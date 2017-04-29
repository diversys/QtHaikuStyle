/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qhaikustyle.h"

#include <qcombobox.h>
#include <qpushbutton.h>
#include <qpainter.h>
#include <qdir.h>
#include <qhash.h>
#include <qstyleoption.h>
#include <qapplication.h>
#include <qmainwindow.h>
#include <qfont.h>
#include <qgroupbox.h>
#include <qprocess.h>
#include <qpixmapcache.h>
#include <qdialogbuttonbox.h>
#include <qscrollbar.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qsplitter.h>
#include <qprogressbar.h>
#include <qtoolbar.h>
#include <qwizard.h>
#include <qlibrary.h>
#include <qstylefactory.h>
#include <qmdisubwindow.h>
#include <qlabel.h>

#include <QDebug>

#include <AppKit.h>
#include <StorageKit.h>
#include <InterfaceKit.h>
#include <NodeInfo.h>
#include <Bitmap.h>
#include <View.h>
#include <ControlLook.h>
#include <ScrollBar.h>
#include <Bitmap.h>
#include <IconUtils.h>

#include "qstylehelper_p.h"
#include "qstylecache_p.h"

QT_BEGIN_NAMESPACE

using namespace QStyleHelper;

enum Direction {
    TopDown,
    FromLeft,
    BottomUp,
    FromRight
};

typedef enum {
	ARROW_LEFT = 0,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	ARROW_NONE
} arrow_direction;

// Haiku BBitmap surface
class TemporarySurface
{
public:
	TemporarySurface(const BRect& bounds)
		: mBitmap(bounds, B_BITMAP_ACCEPTS_VIEWS, B_RGBA32)
		, mView(bounds, "Qt temporary surface", 0, 0)
		, mImage(reinterpret_cast<const uchar*>(mBitmap.Bits()),
			bounds.IntegerWidth() + 1, bounds.IntegerHeight() + 1,
			mBitmap.BytesPerRow(), QImage::Format_ARGB32)
	{
		mBitmap.Lock();
		mBitmap.AddChild(&mView);
	}

	~TemporarySurface()
	{
		mBitmap.RemoveChild(&mView);
		mBitmap.Unlock();
	}

	BView* view()
	{
		return &mView;
	}

	QImage& image()
	{
		if(mView.Window())
			mView.Sync();
		return mImage;
	}

private:
	BBitmap		mBitmap;
	BView		mView;
	QImage		mImage;
};

// convert Haiku rgb_color to QColor
static QColor mkQColor(rgb_color rgb)
{
	return QColor(rgb.red, rgb.green, rgb.blue);
}

// from windows style
static const int windowsItemFrame        =  2; // menu item frame width
static const int windowsItemHMargin      =  3; // menu item hor text margin
static const int windowsItemVMargin      =  8; // menu item ver text margin
static const int windowsRightBorder      = 15; // right border on windows
static const int progressAnimationFps    = 24;
static const int mdiTabWidthMin			 = 100; //minimal tab size for mid window
static const int mdiTabTextMarginLeft	 =  32;
static const int mdiTabTextMarginRight	 =  24;


/* XPM */
static const char * const dock_widget_close_xpm[] = {
    "11 13 7 1",
    " 	c None",
    ".	c #D5CFCB",
    "+	c #8F8B88",
    "@	c #6C6A67",
    "#	c #ABA6A3",
    "$	c #B5B0AC",
    "%	c #A4A09D",
    "           ",
    " +@@@@@@@+ ",
    "+#       #+",
    "@ $@   @$ @",
    "@ @@@ @@@ @",
    "@  @@@@@  @",
    "@   @@@   @",
    "@  @@@@@  @",
    "@ @@@ @@@ @",
    "@ $@   @$ @",
    "+%       #+",
    " +@@@@@@@+ ",
    "           "};

static const char * const qt_haiku_arrow_down_xpm[] = {
    "11 7 2 1",
    " 	c None",
    "x	c #000000",
    "           ",
    "  x     x  ",
    " xxx   xxx ",
    "  xxxxxxx  ",
    "   xxxxx   ",
    "    xxx    ",
    "     x     "};

static const char * const qt_haiku_arrow_up_xpm[] = {
    "11 7 2 1",
    " 	c None",
    "x	c #000000",
    "     x     ",
    "    xxx    ",
    "   xxxxx   ",
    "  xxxxxxx  ",
    " xxx   xxx ",
    "  x     x  ",
    "           "};

static const char * const dock_widget_restore_xpm[] = {
    "11 13 7 1",
    " 	c None",
    ".	c #D5CFCB",
    "+	c #8F8B88",
    "@	c #6C6A67",
    "#	c #ABA6A3",
    "$	c #B5B0AC",
    "%	c #A4A09D",
    "           ",
    " +@@@@@@@+ ",
    "+#       #+",
    "@   #@@@# @",
    "@   @   @ @",
    "@ #@@@# @ @",
    "@ @   @ @ @",
    "@ @   @@@ @",
    "@ @   @   @",
    "@ #@@@#   @",
    "+%       #+",
    " +@@@@@@@+ ",
    "           "};

static const char * const workspace_minimize[] = {
    "11 13 7 1",
    " 	c None",
    ".	c #D5CFCB",
    "+	c #8F8B88",
    "@	c #6C6A67",
    "#	c #ABA6A3",
    "$	c #B5B0AC",
    "%	c #A4A09D",
    "           ",
    " +@@@@@@@+ ",
    "+#       #+",
    "@         @",
    "@         @",
    "@         @",
    "@ @@@@@@@ @",
    "@ @@@@@@@ @",
    "@         @",
    "@         @",
    "+%       #+",
    " +@@@@@@@+ ",
    "           "};

static const char * const qt_spinbox_button_arrow_down[] = {
    "7 4 2 1",
    "   c None",
    "*  c #BFBFBF",
    "*******",
    " ***** ",
    "  ***  ",
    "   *   "};

static const char * const qt_spinbox_button_arrow_up[] = {
    "7 4 2 1",
    "   c None",
    "*  c #BFBFBF",
    "   *   ",
    "  ***  ",
    " ***** ",
    "*******"};


static const char * const qt_haiku_menuitem_checkbox_checked[] = {
    "8 7 6 1",
    " 	g None",
    ".	g #959595",
    "+	g #676767",
    "@	g #454545",
    "#	g #1D1D1D",
    "0	g #101010",
    "      ..",
    "     .+ ",
    "    .+  ",
    "0  .@   ",
    "@#++.   ",
    "  @#    ",
    "   .    "};

static QImage get_haiku_alert_icon(uint32 fType, int32 iconSize)
{
	QImage image;

	if (fType == B_EMPTY_ALERT)
		return image;

	BBitmap* icon = NULL;
	BPath path;
	status_t status = find_directory(B_BEOS_SERVERS_DIRECTORY, &path);
	if (status != B_OK)
		return image;

	path.Append("app_server");
	BFile file;
	status = file.SetTo(path.Path(), B_READ_ONLY);
	if (status != B_OK)
		return image;

	BResources resources;
	status = resources.SetTo(&file);
	if (status != B_OK)
		return image;

	const char* iconName;
	switch (fType) {
		case B_INFO_ALERT:
			iconName = "info";
			break;
		case B_IDEA_ALERT:
			iconName = "idea";
			break;
		case B_WARNING_ALERT:
			iconName = "warn";
			break;
		case B_STOP_ALERT:
			iconName = "stop";
			break;
		default:
			return image;
	}

	icon = new(std::nothrow) BBitmap(BRect(0, 0, iconSize - 1, iconSize - 1), 0, B_RGBA32);
	if (icon == NULL || icon->InitCheck() < B_OK) {
		delete icon;
		return image;
	}

	size_t size = 0;
	const uint8* rawIcon;

	rawIcon = (const uint8*)resources.LoadResource(B_VECTOR_ICON_TYPE, iconName, &size);
	if (rawIcon != NULL	&& BIconUtils::GetVectorIcon(rawIcon, size, icon) == B_OK) {
		image = QImage((unsigned char *) icon->Bits(),
			icon->Bounds().Width() + 1, icon->Bounds().Height() + 1, QImage::Format_ARGB32);
		delete icon;
		return image;
	}

	rawIcon = (const uint8*)resources.LoadResource(B_LARGE_ICON_TYPE, iconName, &size);
	if (rawIcon == NULL) {
		delete icon;
		return image;
	}

	if (icon->ColorSpace() != B_CMAP8)
		BIconUtils::ConvertFromCMAP8(rawIcon, iconSize, iconSize, iconSize, icon);	
	
	image = QImage((unsigned char *) icon->Bits(), icon->Bounds().Width() + 1,
		icon->Bounds().Height() + 1, QImage::Format_ARGB32);
	delete icon;
	return image;
}

static void qt_haiku_draw_windows_frame(QPainter *painter, const QRect &qrect, color_which bcolor, uint32 borders = BControlLook::B_ALL_BORDERS, bool sizer = true)
{
	QColor frameColorActive(mkQColor(ui_color(bcolor)));
	QColor bevelShadow1(mkQColor(tint_color(ui_color(bcolor), 1.07)));
	QColor bevelShadow2(mkQColor(tint_color(ui_color(bcolor), B_DARKEN_2_TINT)));
	QColor bevelShadow3(mkQColor(tint_color(ui_color(bcolor), B_DARKEN_3_TINT)));
	QColor bevelLight(mkQColor(tint_color(ui_color(bcolor), B_LIGHTEN_2_TINT)));
	
	QRect rect= qrect;
	painter->setPen(bevelShadow2);
	if ((borders & BControlLook::B_LEFT_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.bottomLeft());
	if ((borders & BControlLook::B_TOP_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.topRight());
	painter->setPen(bevelShadow3);
	if ((borders & BControlLook::B_RIGHT_BORDER) != 0)
		painter->drawLine(rect.topRight(), rect.bottomRight());
	if ((borders & BControlLook::B_BOTTOM_BORDER) != 0)
		painter->drawLine(rect.bottomRight(), rect.bottomLeft());
	rect.adjust(1,1,-1,-1);
	painter->setPen(bevelLight);
	if ((borders & BControlLook::B_LEFT_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.bottomLeft());
	if ((borders & BControlLook::B_TOP_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.topRight());
	painter->setPen(bevelShadow1);
	if ((borders & BControlLook::B_RIGHT_BORDER) != 0)
		painter->drawLine(rect.topRight(), rect.bottomRight());
	if ((borders & BControlLook::B_BOTTOM_BORDER) != 0)
		painter->drawLine(rect.bottomRight(), rect.bottomLeft());
	rect.adjust(1,1,-1,-1);
	painter->setPen(frameColorActive);
	if ((borders & BControlLook::B_LEFT_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.bottomLeft());
	if ((borders & BControlLook::B_TOP_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.topRight());
	if ((borders & BControlLook::B_RIGHT_BORDER) != 0)
		painter->drawLine(rect.topRight(), rect.bottomRight());
	if ((borders & BControlLook::B_BOTTOM_BORDER) != 0)
		painter->drawLine(rect.bottomRight(), rect.bottomLeft());
	rect.adjust(1,1,-1,-1);
	painter->setPen(bevelShadow1);
	if ((borders & BControlLook::B_LEFT_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.bottomLeft());
	if ((borders & BControlLook::B_TOP_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.topRight());
	painter->setPen(bevelLight);
	if ((borders & BControlLook::B_RIGHT_BORDER) != 0)
		painter->drawLine(rect.topRight(), rect.bottomRight());
	if ((borders & BControlLook::B_BOTTOM_BORDER) != 0)
		painter->drawLine(rect.bottomRight(), rect.bottomLeft());
	rect.adjust(1,1,-1,-1);
	painter->setPen(bevelShadow2);
	if ((borders & BControlLook::B_LEFT_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.bottomLeft());
	if ((borders & BControlLook::B_TOP_BORDER) != 0)
		painter->drawLine(rect.topLeft(), rect.topRight());
	if ((borders & BControlLook::B_RIGHT_BORDER) != 0)
		painter->drawLine(rect.topRight(), rect.bottomRight());
	if ((borders & BControlLook::B_BOTTOM_BORDER) != 0)
		painter->drawLine(rect.bottomRight(), rect.bottomLeft());
	if( borders & BControlLook::B_RIGHT_BORDER != 0 && 
		borders & BControlLook::B_BOTTOM_BORDER != 0 && sizer) {
		painter->setPen(bevelShadow2);
		painter->drawLine(rect.bottomRight() + QPoint(0, -18), rect.bottomRight() + QPoint(5,-18));
		painter->drawLine(rect.bottomRight() + QPoint(-18, 0), rect.bottomRight() + QPoint(-18, 5));
	}
}

static void qt_haiku_draw_button(QPainter *painter, const QRect &qrect, bool def, bool flat, bool pushed, bool focus, bool enabled, bool bevel=true, orientation orient = B_HORIZONTAL, arrow_direction arrow = ARROW_NONE)
{
	QRect rect = qrect;
	
	if (be_control_look != NULL) {
		// TODO: If this button is embedded within a different color background, it would be
		// nice to tell this function so the frame can be smoothly blended into the background.
		rgb_color background = ui_color(B_PANEL_BACKGROUND_COLOR);
		rgb_color base = background;
		uint32 flags = 0;
		if (pushed)
			flags |= BControlLook::B_ACTIVATED;
		if (focus)
			flags |= BControlLook::B_FOCUSED;
		if (def) {
			flags |= BControlLook::B_DEFAULT_BUTTON;
			rect = rect.adjusted(-2,-2,2,2);
		}
		if (!enabled)
			flags |= BControlLook::B_DISABLED;

		QRect sRect = bevel?QRect(0,0,rect.width(),rect.height()):QRect(1,1,rect.width()-2,rect.height()-2);
		BRect bRect(0.0f, 0.0f, rect.width() - 1, rect.height() - 1);		

		TemporarySurface surface(bRect);

		surface.view()->SetHighColor(base);
		surface.view()->SetLowColor(base);
		surface.view()->FillRect(bRect);

		be_control_look->DrawButtonFrame(surface.view(), bRect, bRect, base, background, flags);
		be_control_look->DrawButtonBackground(surface.view(), bRect, bRect, base, flags, BControlLook::B_ALL_BORDERS, orient);
		
		if (arrow != ARROW_NONE) {
			bRect.InsetBy(-1, -1);			
			be_control_look->DrawArrowShape(surface.view(), bRect, bRect, base, arrow, 0, B_DARKEN_MAX_TINT);
		}

		painter->drawImage(rect, surface.image(), sRect);

		return;
	}
}

static void qt_haiku_draw_gradient(QPainter *painter, const QRect &rect, const QColor &gradientStart,
                                        const QColor &gradientStop, Direction direction = TopDown, QBrush bgBrush = QBrush())
{
        int x = rect.center().x();
        int y = rect.center().y();
        QLinearGradient *gradient;
        switch (direction) {
            case FromLeft:
                gradient = new QLinearGradient(rect.left(), y, rect.right(), y);
                break;
            case FromRight:
                gradient = new QLinearGradient(rect.right(), y, rect.left(), y);
                break;
            case BottomUp:
                gradient = new QLinearGradient(x, rect.bottom(), x, rect.top());
                break;
            case TopDown:
            default:
                gradient = new QLinearGradient(x, rect.top(), x, rect.bottom());
                break;
        }
        if (bgBrush.gradient())
            gradient->setStops(bgBrush.gradient()->stops());
        else {
            gradient->setColorAt(0, gradientStart);
            gradient->setColorAt(1, gradientStop);
        }
        painter->fillRect(rect, *gradient);
        delete gradient;
}

/*!
    Constructs a QHaikuStyle object.
*/
QHaikuStyle::QHaikuStyle() : QProxyStyle(QStyleFactory::create(QLatin1String("Fusion"))), animateStep(0), animateTimer(0)
{
    setObjectName(QLatin1String("Haiku"));
}

/*!
    Destroys the QHaikuStyle object.
*/
QHaikuStyle::~QHaikuStyle()
{
}

/*!
    \fn void QHaikuStyle::drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette,
                                    bool enabled, const QString& text, QPalette::ColorRole textRole) const

    Draws the given \a text in the specified \a rectangle using the
    provided \a painter and \a palette.

    Text is drawn using the painter's pen. If an explicit \a textRole
    is specified, then the text is drawn using the \a palette's color
    for the specified role.  The \a enabled value indicates whether or
    not the item is enabled; when reimplementing, this value should
    influence how the item is drawn.

    The text is aligned and wrapped according to the specified \a
    alignment.

    \sa Qt::Alignment
*/
void QHaikuStyle::drawItemText(QPainter *painter, const QRect &rect, int alignment, const QPalette &pal,
                                    bool enabled, const QString& text, QPalette::ColorRole textRole) const
{
    if (text.isEmpty())
        return;

    QPen savedPen = painter->pen();
    if (textRole != QPalette::NoRole) {
        painter->setPen(QPen(pal.brush(textRole), savedPen.widthF()));
    }
    if (!enabled) {
        QPen pen = painter->pen();
        painter->setPen(pen);
    }
    painter->drawText(rect, alignment, text);
    painter->setPen(savedPen);
}

static QColor mergedColors(const QColor &colorA, const QColor &colorB, int factor = 50)
{
    const int maxFactor = 100;
    QColor tmp = colorA;
    tmp.setRed((tmp.red() * factor) / maxFactor + (colorB.red() * (maxFactor - factor)) / maxFactor);
    tmp.setGreen((tmp.green() * factor) / maxFactor + (colorB.green() * (maxFactor - factor)) / maxFactor);
    tmp.setBlue((tmp.blue() * factor) / maxFactor + (colorB.blue() * (maxFactor - factor)) / maxFactor);
    return tmp;
}

/*!
    \reimp
*/
void QHaikuStyle::drawPrimitive(PrimitiveElement elem,
                        const QStyleOption *option,
                        QPainter *painter, const QWidget *widget) const
{
    Q_ASSERT(option);
    QRect rect = option->rect;
    int state = option->state;
    QColor button = option->palette.button().color();
    QColor buttonShadow = option->palette.button().color().darker(110);
    QColor buttonShadowAlpha = buttonShadow;
    buttonShadowAlpha.setAlpha(128);
    QColor darkOutline;
    QColor dark;
    darkOutline.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*3.0)),
                qMin(255, (int)(button.value()*0.6)));
    dark.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*1.9)),
                qMin(255, (int)(button.value()*0.7)));
    QColor tabFrameColor = mergedColors(option->palette.background().color(),
                                                dark.lighter(135), 60);

    switch (elem) {
    case PE_IndicatorViewItemCheck:
        {
            QStyleOptionButton button;
            button.QStyleOption::operator=(*option);
            button.state &= ~State_MouseOver;
            proxy()->drawPrimitive(PE_IndicatorCheckBox, &button, painter, widget);
        }
        return;
    case PE_IndicatorHeaderArrow:
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            QRect r = header->rect;
            QImage arrow;
            if (header->sortIndicator & QStyleOptionHeader::SortUp)
                arrow = QImage(qt_haiku_arrow_up_xpm);
            else if (header->sortIndicator & QStyleOptionHeader::SortDown)
                arrow = QImage(qt_haiku_arrow_down_xpm);
            if (!arrow.isNull()) {
                r.setSize(arrow.size());
                r.moveCenter(header->rect.center());
                arrow.setColor(1, header->palette.foreground().color().rgba());
                painter->drawImage(r, arrow);
            }
        }
        break;
    case PE_IndicatorButtonDropDown:
        proxy()->drawPrimitive(PE_PanelButtonCommand, option, painter, widget);
        break;
    case PE_IndicatorToolBarSeparator:
        {
            QRect rect = option->rect;
            const int margin = 6;
            if (option->state & State_Horizontal) {
                const int offset = rect.width()/2;
                painter->setPen(QPen(option->palette.background().color().darker(110)));
                painter->drawLine(rect.bottomLeft().x() + offset,
                            rect.bottomLeft().y() - margin,
                            rect.topLeft().x() + offset,
                            rect.topLeft().y() + margin);
                painter->setPen(QPen(option->palette.background().color().lighter(110)));
                painter->drawLine(rect.bottomLeft().x() + offset + 1,
                            rect.bottomLeft().y() - margin,
                            rect.topLeft().x() + offset + 1,
                            rect.topLeft().y() + margin);
            } else { //Draw vertical separator
                const int offset = rect.height()/2;
                painter->setPen(QPen(option->palette.background().color().darker(110)));
                painter->drawLine(rect.topLeft().x() + margin ,
                            rect.topLeft().y() + offset,
                            rect.topRight().x() - margin,
                            rect.topRight().y() + offset);
                painter->setPen(QPen(option->palette.background().color().lighter(110)));
                painter->drawLine(rect.topLeft().x() + margin ,
                            rect.topLeft().y() + offset + 1,
                            rect.topRight().x() - margin,
                            rect.topRight().y() + offset + 1);
            }
        }
        break;
    case PE_Frame:
        painter->save();
        painter->setPen(dark.lighter(108));
        painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
        painter->restore();
        break;
    case PE_FrameGroupBox:
        painter->save();
	    {    
	        QColor backgroundColor(mkQColor(ui_color(B_PANEL_BACKGROUND_COLOR)));
	        QColor frameColor(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.30)));
	        QColor bevelLight(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 0.8)));
	        QColor bevelShadow(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.03)));
	        
	        QRect frame = option->rect;
	        painter->setPen(bevelShadow);
	        painter->drawLine(frame.topLeft(), frame.bottomLeft());
	        painter->drawLine(frame.topLeft(), frame.topRight());
	        painter->setPen(bevelLight);
	        painter->drawLine(frame.topRight(), frame.bottomRight());
	        painter->drawLine(frame.bottomLeft(), frame.bottomRight());
	
			frame.adjust(1, 1, -1, -1);
	        painter->setPen(frameColor);
	        painter->drawLine(frame.topLeft(), frame.bottomLeft());
	        painter->drawLine(frame.topLeft(), frame.topRight());
	        painter->drawLine(frame.topRight(), frame.bottomRight());
	        painter->drawLine(frame.bottomLeft(), frame.bottomRight());
			
			frame.adjust(1, 1, -1, -1);
	        painter->setPen(bevelLight);
	        painter->drawLine(frame.topLeft(), frame.bottomLeft());
	        painter->drawLine(frame.topLeft(), frame.topRight());
	        painter->setPen(bevelShadow);
	        painter->drawLine(frame.topRight(), frame.bottomRight());
	        painter->drawLine(frame.bottomLeft(), frame.bottomRight());
	    }
        painter->restore();    
    	break;
    case PE_FrameMenu:
        painter->save();
        {
            painter->setPen(QPen(darkOutline, 1));
            painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
            QColor frameLight = option->palette.background().color().lighter(160);
            QColor frameShadow = option->palette.background().color().darker(110);

            //paint beveleffect
            QRect frame = option->rect.adjusted(1, 1, -1, -1);
            painter->setPen(frameLight);
            painter->drawLine(frame.topLeft(), frame.bottomLeft());
            painter->drawLine(frame.topLeft(), frame.topRight());

            painter->setPen(frameShadow);
            painter->drawLine(frame.topRight(), frame.bottomRight());
            painter->drawLine(frame.bottomLeft(), frame.bottomRight());
        }
        painter->restore();
        break;
    case PE_FrameDockWidget:

        painter->save();
        {
            QColor softshadow = option->palette.background().color().darker(120);

            QRect rect= option->rect;
            painter->setPen(softshadow);
            painter->drawRect(option->rect.adjusted(0, 0, -1, -1));
            painter->setPen(QPen(option->palette.light(), 0));
            painter->drawLine(QPoint(rect.left() + 1, rect.top() + 1), QPoint(rect.left() + 1, rect.bottom() - 1));
            painter->setPen(QPen(option->palette.background().color().darker(120), 0));
            painter->drawLine(QPoint(rect.left() + 1, rect.bottom() - 1), QPoint(rect.right() - 2, rect.bottom() - 1));
            painter->drawLine(QPoint(rect.right() - 1, rect.top() + 1), QPoint(rect.right() - 1, rect.bottom() - 1));

        }
        painter->restore();
        break;
    case PE_PanelButtonTool:
        painter->save();
        if ((option->state & State_Enabled || option->state & State_On) || !(option->state & State_AutoRaise)) {
            QPen oldPen = painter->pen();

            if (widget && widget->inherits("QDockWidgetTitleButton")) {
                   if (option->state & State_MouseOver)
                       proxy()->drawPrimitive(PE_PanelButtonCommand, option, painter, widget);
            } else {
                proxy()->drawPrimitive(PE_PanelButtonCommand, option, painter, widget);
            }
        }
        painter->restore();
        break;
    case PE_IndicatorDockWidgetResizeHandle:
        {
            QStyleOption dockWidgetHandle = *option;
            bool horizontal = option->state & State_Horizontal;
            if (horizontal)
                dockWidgetHandle.state &= ~State_Horizontal;
            else
                dockWidgetHandle.state |= State_Horizontal;
            proxy()->drawControl(CE_Splitter, &dockWidgetHandle, painter, widget);
        }
        break;
    case PE_FrameWindow:
        painter->save();
        {
            bool active = (option->state & State_Active);
			int titleBarHeight = proxy()->pixelMetric(PM_TitleBarHeight);
			int frameWidth = proxy()->pixelMetric(PM_MdiSubWindowFrameWidth);
		    qt_haiku_draw_windows_frame(painter, option->rect, active ? B_WINDOW_BORDER_COLOR : B_WINDOW_INACTIVE_BORDER_COLOR, 
		    	BControlLook::B_LEFT_BORDER | BControlLook::B_RIGHT_BORDER | BControlLook::B_BOTTOM_BORDER);
        }
        painter->restore();
        break;
    case PE_FrameLineEdit:
        painter->save();
        {
			QRect r = option->rect;
			rgb_color base = ui_color(B_CONTROL_BACKGROUND_COLOR);;
			uint32 flags = 0;
			if (!option->state & State_Enabled)
				flags |= BControlLook::B_DISABLED;
			if (option->state & State_HasFocus)
				flags |= BControlLook::B_FOCUSED;
		    BRect bRect(0.0f, 0.0f, r.width() - 1, r.height() - 1);
			TemporarySurface surface(bRect);
			bRect.InsetBy(-1, -1);
			be_control_look->DrawTextControlBorder(surface.view(), bRect, bRect, base, flags);
			painter->drawImage(r, surface.image());
        }
        painter->restore();
        break;
    case PE_IndicatorCheckBox:
        painter->save();
        if (const QStyleOptionButton *checkbox = qstyleoption_cast<const QStyleOptionButton*>(option)) {
            
            rect = rect.adjusted(1, 1, -1, -1);
			BRect bRect(0.0f, 0.0f, rect.width() - 1, rect.height() - 1);
			TemporarySurface surface(bRect);
			rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
			surface.view()->SetHighColor(base);
			surface.view()->SetLowColor(base);
			surface.view()->FillRect(bRect);

			uint32 flags = 0;

			if (!(state & State_Enabled))
				flags |= BControlLook::B_DISABLED;
			if (checkbox->state & State_On)
				flags |= BControlLook::B_ACTIVATED;
			if (checkbox->state & State_HasFocus)
				flags |= BControlLook::B_FOCUSED;
			if (checkbox->state & State_Sunken)
				flags |= BControlLook::B_CLICKED;
			if (checkbox->state & State_NoChange)
				flags |= BControlLook::B_DISABLED | BControlLook::B_ACTIVATED;

			be_control_look->DrawCheckBox(surface.view(), bRect, bRect, base, flags);
			painter->drawImage(rect, surface.image());
        }
        painter->restore();
        break;  
    case PE_IndicatorRadioButton:
        painter->save();
        if (const QStyleOptionButton *radiobutton = qstyleoption_cast<const QStyleOptionButton*>(option)) {
            
            rect = rect.adjusted(1, -1, 3, 1);
			BRect bRect(0.0f, 0.0f, rect.width() - 1, rect.height() - 1);
			TemporarySurface surface(bRect);
			rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
			surface.view()->SetHighColor(base);
			surface.view()->SetLowColor(base);
			surface.view()->FillRect(bRect);

			uint32 flags = 0;

			if (!(state & State_Enabled))
				flags |= BControlLook::B_DISABLED;
			if (radiobutton->state & State_On)
				flags |= BControlLook::B_ACTIVATED;
			if (radiobutton->state & State_HasFocus)
				flags |= BControlLook::B_FOCUSED;
			if (radiobutton->state & State_Sunken)
				flags |= BControlLook::B_CLICKED;
			if (radiobutton->state & State_NoChange)
				flags |= BControlLook::B_DISABLED | BControlLook::B_ACTIVATED;

			be_control_look->DrawRadioButton(surface.view(), bRect, bRect, base, flags);
			painter->drawImage(rect, surface.image());
        }
        painter->restore();
        break;  
    case PE_IndicatorToolBarHandle:
        painter->save();
        if (option->state & State_Horizontal) {
            for (int i = rect.height()/5; i <= 4*(rect.height()/5) ; ++i) {
                int y = rect.topLeft().y() + i + 1;
                int x1 = rect.topLeft().x() + 3;
                int x2 = rect.topRight().x() - 2;

                if (i % 2 == 0)
                    painter->setPen(QPen(option->palette.light(), 0));
                else
                    painter->setPen(QPen(dark.lighter(110), 0));
                painter->drawLine(x1, y, x2, y);
            }
        }
        else { //vertical toolbar
            for (int i = rect.width()/5; i <= 4*(rect.width()/5) ; ++i) {
                int x = rect.topLeft().x() + i + 1;
                int y1 = rect.topLeft().y() + 3;
                int y2 = rect.topLeft().y() + 5;

                if (i % 2 == 0)
                    painter->setPen(QPen(option->palette.light(), 0));
                else
                    painter->setPen(QPen(dark.lighter(110), 0));
                painter->drawLine(x, y1, x, y2);
            }
        }
        painter->restore();
        break;
	case PE_FrameDefaultButton:
    case PE_FrameFocusRect:
    	break;
    case PE_PanelButtonCommand:
        painter->save();
        {   
        	bool isDefault = false;
        	bool isFlat = false;
        	bool isDown = (option->state & State_Sunken) || (option->state & State_On);
        	bool isTool = (elem == PE_PanelButtonTool);        	
        	if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton*>(option)) {
            	isDefault = btn->features & QStyleOptionButton::DefaultButton;
            	isFlat = (btn->features & QStyleOptionButton::Flat);
        	}

        	if (isTool && !(option->state & State_Enabled || option->state & State_On) && (option->state & State_AutoRaise))
            	break;

			bool hasFocus = option->state & State_HasFocus;
            bool isEnabled = option->state & State_Enabled;

			qt_haiku_draw_button(painter, option->rect,
				isDefault, isFlat, isDown, hasFocus, isEnabled);
	     	painter->restore();
        }
        break;
        case PE_FrameTabWidget:        
            painter->save();
            if (const QStyleOptionTabWidgetFrame *twf = qstyleoption_cast<const QStyleOptionTabWidgetFrame *>(option)) {
            	            	
		        QColor backgroundColor(mkQColor(ui_color(B_PANEL_BACKGROUND_COLOR)));
		        QColor frameColor(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.30)));
		        QColor bevelLight(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 0.8)));
		        QColor bevelShadow(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.03)));
		        
		        QRect frame = option->rect;
		        
		        switch(twf->shape) {
		        	case QTabBar::RoundedNorth:
					case QTabBar::TriangularNorth:
						frame.adjust(-1,0,1,1);
						break;
					case QTabBar::RoundedSouth:
					case QTabBar::TriangularSouth:
						frame.adjust(-1,-1,1,0);
						break;
					case QTabBar::RoundedWest:
					case QTabBar::TriangularWest:
						frame.adjust(0,-1,1,1);
						break;
					case QTabBar::RoundedEast:
					case QTabBar::TriangularEast:
						frame.adjust(-1,-1,0,1);
						break;
				}

		        painter->setPen(bevelShadow);
		        painter->drawLine(frame.topLeft(), frame.bottomLeft());
		        painter->drawLine(frame.topLeft(), frame.topRight());
		        painter->setPen(bevelLight);
		        painter->drawLine(frame.topRight(), frame.bottomRight());
		        painter->drawLine(frame.bottomLeft(), frame.bottomRight());

				frame.adjust(1, 1, -1, -1);
		        painter->setPen(frameColor);
		        painter->drawLine(frame.topLeft(), frame.bottomLeft());
		        painter->drawLine(frame.topLeft(), frame.topRight());
		        painter->drawLine(frame.topRight(), frame.bottomRight());
		        painter->drawLine(frame.bottomLeft(), frame.bottomRight());

				frame.adjust(1, 1, -1, -1);
		        painter->setPen(bevelLight);
		        painter->drawLine(frame.topLeft(), frame.bottomLeft());
		        painter->drawLine(frame.topLeft(), frame.topRight());
		        painter->setPen(bevelShadow);
		        painter->drawLine(frame.topRight(), frame.bottomRight());
		        painter->drawLine(frame.bottomLeft(), frame.bottomRight());            	
            }
    painter->restore();
    break ;

    case PE_FrameStatusBarItem:
        break;
    case PE_IndicatorTabClose:
        {
            static QIcon tabBarcloseButtonIcon;
            if (tabBarcloseButtonIcon.isNull())
                tabBarcloseButtonIcon = standardIcon(SP_DialogCloseButton, option, widget);
            if ((option->state & State_Enabled) && (option->state & State_MouseOver))
                proxy()->drawPrimitive(PE_PanelButtonCommand, option, painter, widget);
            QPixmap pixmap = tabBarcloseButtonIcon.pixmap(QSize(16, 16), QIcon::Normal, QIcon::On);
            proxy()->drawItemPixmap(painter, option->rect, Qt::AlignCenter, pixmap);
        }
        break;

    default:
        QProxyStyle::drawPrimitive(elem, option, painter, widget);
        break;
    }
}

/*!
  \reimp
*/
void QHaikuStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                                   const QWidget *widget) const
{
    QColor button = option->palette.button().color();
    QColor dark;
    dark.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*1.9)),
                qMin(255, (int)(button.value()*0.7)));
    QColor darkOutline;
    darkOutline.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*2.0)),
                qMin(255, (int)(button.value()*0.6)));
    QRect rect = option->rect;
    QColor shadow = mergedColors(option->palette.background().color().darker(120),
                                 dark.lighter(130), 60);
    QColor tabFrameColor = mergedColors(option->palette.background().color(),
                                                dark.lighter(135), 60);

    QColor highlight = option->palette.highlight().color();

    switch (element) {
     case CE_RadioButton: //fall through
     case CE_CheckBox:
        if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            QStyleOptionButton copy = *btn;
            copy.rect.adjust(1, -1, 3, 2);
            QProxyStyle::drawControl(element, &copy, painter, widget);
        }
        break;
    case CE_Splitter:
        painter->save();
        {
        	orientation orient = (option->state & State_Horizontal)?B_HORIZONTAL:B_VERTICAL;
        	
			if (be_control_look != NULL) {
				QRect r = option->rect;
				rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);;
				uint32 flags = 0;            
		        BRect bRect(0.0f, 0.0f, r.width() - 1, r.height() - 1);
				TemporarySurface surface(bRect);
				surface.view()->SetHighColor(base);
				surface.view()->SetLowColor(base);
				surface.view()->FillRect(bRect);
				be_control_look->DrawSplitter(surface.view(), bRect, bRect, base, orient, flags);
				painter->drawImage(r, surface.image());			    
			}
        }
        painter->restore();
        break;
#ifndef QT_NO_SIZEGRIP
    case CE_SizeGrip:
        painter->save();
        {
            int x, y, w, h;
            option->rect.getRect(&x, &y, &w, &h);
            int sw = qMin(h, w);
            if (h > w)
                painter->translate(0, h - w);
            else
                painter->translate(w - h, 0);

            int sx = x;
            int sy = y;
            int s = 4;
            if (option->direction == Qt::RightToLeft) {
                sx = x + sw;
                for (int i = 0; i < 4; ++i) {
                    painter->setPen(QPen(option->palette.light().color(), 1));
                    painter->drawLine(x, sy - 1 , sx + 1, sw);
                    painter->setPen(QPen(dark.lighter(120), 1));
                    painter->drawLine(x, sy, sx, sw);
                    sx -= s;
                    sy += s;
                }
            } else {
                for (int i = 0; i < 4; ++i) {
                    painter->setPen(QPen(option->palette.light().color(), 1));
                    painter->drawLine(sx - 1, sw, sw, sy - 1);
                    painter->setPen(QPen(dark.lighter(120), 1));
                    painter->drawLine(sx, sw, sw, sy);
                    sx += s;
                    sy += s;
                }
            }
        }
        painter->restore();
        break;
#endif // QT_NO_SIZEGRIP
#ifndef QT_NO_TOOLBAR
    case CE_ToolBar:
        // Reserve the beveled appearance only for mainwindow toolbars
        if (!(widget && qobject_cast<const QMainWindow*> (widget->parentWidget())))
            break;

        painter->save();
        if (const QStyleOptionToolBar *toolbar = qstyleoption_cast<const QStyleOptionToolBar *>(option)) {
            QRect rect = option->rect;

            bool paintLeftBorder = true;
            bool paintRightBorder = true;
            bool paintBottomBorder = true;

            switch (toolbar->toolBarArea) {
            case Qt::BottomToolBarArea:
                switch (toolbar->positionOfLine) {
                case QStyleOptionToolBar::Beginning:
                case QStyleOptionToolBar::OnlyOne:
                    paintBottomBorder = false;
                default:
                    break;
                }
            case Qt::TopToolBarArea:
                switch (toolbar->positionWithinLine) {
                case QStyleOptionToolBar::Beginning:
                    paintLeftBorder = false;
                    break;
                case QStyleOptionToolBar::End:
                    paintRightBorder = false;
                    break;
                case QStyleOptionToolBar::OnlyOne:
                    paintRightBorder = false;
                    paintLeftBorder = false;
                default:
                    break;
                }
                if (toolbar->direction == Qt::RightToLeft) { //reverse layout changes the order of Beginning/end
                    bool tmp = paintLeftBorder;
                    paintRightBorder=paintLeftBorder;
                    paintLeftBorder=tmp;
                }
                break;
            case Qt::RightToolBarArea:
                switch (toolbar->positionOfLine) {
                case QStyleOptionToolBar::Beginning:
                case QStyleOptionToolBar::OnlyOne:
                    paintRightBorder = false;
                    break;
                default:
                    break;
                }
                break;
            case Qt::LeftToolBarArea:
                switch (toolbar->positionOfLine) {
                case QStyleOptionToolBar::Beginning:
                case QStyleOptionToolBar::OnlyOne:
                    paintLeftBorder = false;
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }

            QColor light = option->palette.background().color().lighter(110);

            //draw top border
            painter->setPen(QPen(light));
            painter->drawLine(rect.topLeft().x(),
                        rect.topLeft().y(),
                        rect.topRight().x(),
                        rect.topRight().y());

            if (paintLeftBorder) {
                painter->setPen(QPen(light));
                painter->drawLine(rect.topLeft().x(),
                            rect.topLeft().y(),
                            rect.bottomLeft().x(),
                            rect.bottomLeft().y());
            }

            if (paintRightBorder) {
                painter->setPen(QPen(shadow));
                painter->drawLine(rect.topRight().x(),
                            rect.topRight().y(),
                            rect.bottomRight().x(),
                            rect.bottomRight().y());
            }

            if (paintBottomBorder) {
                painter->setPen(QPen(shadow));
                painter->drawLine(rect.bottomLeft().x(),
                            rect.bottomLeft().y(),
                            rect.bottomRight().x(),
                            rect.bottomRight().y());
            }
        }
        painter->restore();
        break;
#endif // QT_NO_TOOLBAR
#ifndef QT_NO_DOCKWIDGET
    case CE_DockWidgetTitle:
        painter->save();
        if (const QStyleOptionDockWidget *dwOpt = qstyleoption_cast<const QStyleOptionDockWidget *>(option)) {
            bool verticalTitleBar = dwOpt->verticalTitleBar;

            QRect titleRect = subElementRect(SE_DockWidgetTitleBarText, option, widget);
            if (verticalTitleBar) {
                QRect rect = dwOpt->rect;
                QRect r = rect;
                QSize s = r.size();
                s.transpose();
                r.setSize(s);
                titleRect = QRect(r.left() + rect.bottom()
                                    - titleRect.bottom(),
                                r.top() + titleRect.left() - rect.left(),
                                titleRect.height(), titleRect.width());
            }

            if (!dwOpt->title.isEmpty()) {
                QString titleText
                    = painter->fontMetrics().elidedText(dwOpt->title,
                                            Qt::ElideRight, titleRect.width());
                proxy()->drawItemText(painter,
                             titleRect,
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic, dwOpt->palette,
                             dwOpt->state & State_Enabled, titleText,
                             QPalette::WindowText);
                }
        }
        painter->restore();
        break;
#endif // QT_NO_DOCKWIDGET
    case CE_HeaderSection:
        painter->save();
        // Draws the header in tables.
        if (const QStyleOptionHeader *header = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            QPixmap cache;
            QString pixmapName = QStyleHelper::uniqueName(QLatin1String("headersection"), option, option->rect.size());
            pixmapName += QString::number(- int(header->position));
            pixmapName += QString::number(- int(header->orientation));
            QRect r = option->rect;
            QColor gradientStopColor;
            QColor gradientStartColor = option->palette.button().color();
            gradientStopColor.setHsv(gradientStartColor.hue(),
                                     qMin(255, (int)(gradientStartColor.saturation()*2)),
                                     qMin(255, (int)(gradientStartColor.value()*0.96)));
            QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
            if (option->palette.background().gradient()) {
                gradient.setStops(option->palette.background().gradient()->stops());
            } else {
                gradient.setColorAt(0, gradientStartColor);
                gradient.setColorAt(0.8, gradientStartColor);
                gradient.setColorAt(1, gradientStopColor);
            }
            painter->fillRect(r, gradient);

            if (!QPixmapCache::find(pixmapName, cache)) {
                cache = QPixmap(r.size());
                cache.fill(Qt::transparent);
                QRect pixmapRect(0, 0, r.width(), r.height());
                QPainter cachePainter(&cache);
                if (header->orientation == Qt::Vertical) {
                    cachePainter.setPen(QPen(dark));
                    cachePainter.drawLine(pixmapRect.topRight(), pixmapRect.bottomRight());
                    if (header->position != QStyleOptionHeader::End) {
                        cachePainter.setPen(QPen(shadow));
                        cachePainter.drawLine(pixmapRect.bottomLeft() + QPoint(3, -1), pixmapRect.bottomRight() + QPoint(-3, -1));                                cachePainter.setPen(QPen(option->palette.light().color()));
                        cachePainter.drawLine(pixmapRect.bottomLeft() + QPoint(3, 0), pixmapRect.bottomRight() + QPoint(-3, 0));                              }
                } else {
                    cachePainter.setPen(QPen(dark));
                    cachePainter.drawLine(pixmapRect.bottomLeft(), pixmapRect.bottomRight());
                    cachePainter.setPen(QPen(shadow));
                    cachePainter.drawLine(pixmapRect.topRight() + QPoint(-1, 3), pixmapRect.bottomRight() + QPoint(-1, -3));                                  cachePainter.setPen(QPen(option->palette.light().color()));
                    cachePainter.drawLine(pixmapRect.topRight() + QPoint(0, 3), pixmapRect.bottomRight() + QPoint(0, -3));                                }
                cachePainter.end();
                QPixmapCache::insert(pixmapName, cache);
            }
            painter->drawPixmap(r.topLeft(), cache);
        }
        painter->restore();
        break;
    case CE_ProgressBarGroove:
        painter->save();
        {
            painter->fillRect(rect, option->palette.base());
            QColor borderColor = dark.lighter(110);
            painter->setPen(QPen(borderColor, 0));
            const QLine lines[4] = {
                QLine(QPoint(rect.left() + 1, rect.top()), QPoint(rect.right() - 1, rect.top())),
                QLine(QPoint(rect.left() + 1, rect.bottom()), QPoint(rect.right() - 1, rect.bottom())),
                QLine(QPoint(rect.left(), rect.top() + 1), QPoint(rect.left(), rect.bottom() - 1)),
                QLine(QPoint(rect.right(), rect.top() + 1), QPoint(rect.right(), rect.bottom() - 1))
            };
            painter->drawLines(lines, 4);
            QColor alphaCorner = mergedColors(borderColor, option->palette.background().color());
            QColor innerShadow = mergedColors(borderColor, option->palette.base().color());

            //corner smoothing
            painter->setPen(alphaCorner);
            const QPoint points[4] = {
                rect.topRight(),
                rect.topLeft(),
                rect.bottomRight(),
                rect.bottomLeft()
            };
            painter->drawPoints(points, 4);

            //inner shadow
            painter->setPen(innerShadow);
            painter->drawLine(QPoint(rect.left() + 1, rect.top() + 1),
                              QPoint(rect.right() - 1, rect.top() + 1));
            painter->drawLine(QPoint(rect.left() + 1, rect.top() + 1),
                              QPoint(rect.left() + 1, rect.bottom() + 1));

        }
        painter->restore();
        break;
    case CE_ProgressBarContents:
        painter->save();
        if (const QStyleOptionProgressBar *bar = qstyleoption_cast<const QStyleOptionProgressBar *>(option)) {
            QRect rect = bar->rect;
            bool vertical = (bar->orientation == Qt::Vertical);
            bool inverted = bar->invertedAppearance;
            bool indeterminate = (bar->minimum == 0 && bar->maximum == 0);

            // If the orientation is vertical, we use a transform to rotate
            // the progress bar 90 degrees clockwise.  This way we can use the
            // same rendering code for both orientations.
            if (vertical) {
                rect = QRect(rect.left(), rect.top(), rect.height(), rect.width()); // flip width and height
                QTransform m = QTransform::fromTranslate(rect.height()-1, -1.0);
                m.rotate(90.0);
                painter->setTransform(m, true);
            }

            int maxWidth = rect.width() - 4;
            int minWidth = 4;
            qreal progress = qMax(bar->progress, bar->minimum); // workaround for bug in QProgressBar
            int progressBarWidth = (progress - bar->minimum) * qreal(maxWidth) / qMax(qreal(1.0), qreal(bar->maximum) - bar->minimum);
            int width = indeterminate ? maxWidth : qMax(minWidth, progressBarWidth);

            bool reverse = (!vertical && (bar->direction == Qt::RightToLeft)) || vertical;
            if (inverted)
                reverse = !reverse;

            QRect progressBar;
            if (!indeterminate) {
                if (!reverse) {
                    progressBar.setRect(rect.left() + 1, rect.top() + 1, width + 1, rect.height() - 3);
                } else {
                    progressBar.setRect(rect.right() - 1 - width, rect.top() + 1, width + 1, rect.height() - 3);
                }
            } else {
                int slideWidth = (qMax(rect.width() - 4, minWidth) * 2) / 3;
                int step = ((animateStep * slideWidth) / progressAnimationFps) % slideWidth;
                if ((((animateStep * slideWidth) / progressAnimationFps) % (2 * slideWidth)) >= slideWidth)
                    step = slideWidth - step;
                progressBar.setRect(rect.left() + 1 + step, rect.top() + 1,
                                    slideWidth / 2, rect.height() - 3);
            }
            QColor highlight = option->palette.color(QPalette::Normal, QPalette::Highlight);
            painter->setPen(QPen(highlight.darker(140), 0));

            QColor highlightedGradientStartColor = highlight.lighter(100);
            QColor highlightedGradientStopColor  = highlight.lighter(130);

            QLinearGradient gradient(rect.topLeft(), QPoint(rect.bottomLeft().x(),
                                                            rect.bottomLeft().y()*2));

            gradient.setColorAt(0, highlightedGradientStartColor);
            gradient.setColorAt(1, highlightedGradientStopColor);

            painter->setBrush(gradient);
            painter->drawRect(progressBar);

            painter->setPen(QPen(highlight.lighter(120), 0));
            painter->drawLine(QPoint(progressBar.left() + 1, progressBar.top() + 1),
                              QPoint(progressBar.right(), progressBar.top() + 1));
            painter->drawLine(QPoint(progressBar.left() + 1, progressBar.top() + 1),
                              QPoint(progressBar.left() + 1, progressBar.bottom() - 1));

            painter->setPen(QPen(highlightedGradientStartColor, 7.0));//QPen(option->palette.highlight(), 3));

            painter->save();
            painter->setClipRect(progressBar.adjusted(2, 2, -1, -1));
            for (int x = progressBar.left() - 32; x < rect.right() ; x+=18) {
                painter->drawLine(x, progressBar.bottom() + 1, x + 23, progressBar.top() - 2);
            }
            painter->restore();

        }
        painter->restore();
        break;
    case CE_MenuBarItem:
        painter->save();
        if (const QStyleOptionMenuItem *mbi = qstyleoption_cast<const QStyleOptionMenuItem *>(option))
        {
            QStyleOptionMenuItem item = *mbi;
            item.rect = mbi->rect;

            bool act = mbi->state & State_Selected && mbi->state & State_Sunken;
            bool dis = !(mbi->state & State_Enabled);

			if (be_control_look != NULL) {
				rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);

				uint32 flags = 0;

				if (act)
					flags |= BControlLook::B_ACTIVATED;
				if (dis)
					flags |= BControlLook::B_DISABLED;

		        BRect bRect(0.0f, 0.0f, option->rect.width() - 1, option->rect.height() - 1);
				TemporarySurface surface(bRect);

				if (act) {
					base = ui_color(B_MENU_SELECTED_BACKGROUND_COLOR);
					surface.view()->SetLowColor(base);
					be_control_look->DrawMenuItemBackground(surface.view(), bRect, bRect, base, flags, BControlLook::B_ALL_BORDERS);
				} else {
					be_control_look->DrawMenuBarBackground(surface.view(), bRect, bRect, base, flags, 8);
				}
				painter->drawImage(option->rect, surface.image());					
			}

            QPalette::ColorRole textRole = QPalette::Text;
            uint alignment = Qt::AlignCenter  | Qt::TextHideMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
            drawItemText(painter, item.rect, alignment, mbi->palette, mbi->state & State_Enabled, mbi->text, textRole);
        }
        painter->restore();
        break;
    case CE_MenuItem:
        painter->save();
        // Draws one item in a popup menu.
        if (const QStyleOptionMenuItem *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            QColor highlightOutline = highlight.darker(125);
            QColor menuBackground = option->palette.background().color().lighter(104);
            QColor borderColor = option->palette.background().color().darker(160);
            QColor alphaCornerColor;

            if (widget) {
                // ### backgroundrole/foregroundrole should be part of the style option
                alphaCornerColor = mergedColors(option->palette.color(widget->backgroundRole()), borderColor);
            } else {
                alphaCornerColor = mergedColors(option->palette.background().color(), borderColor);
            }
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
                painter->fillRect(menuItem->rect, menuBackground);
                int w = 0;
                if (!menuItem->text.isEmpty()) {
                    painter->setFont(menuItem->font);
                    proxy()->drawItemText(painter, menuItem->rect.adjusted(5, 0, -5, 0), Qt::AlignLeft | Qt::AlignVCenter,
                                 menuItem->palette, menuItem->state & State_Enabled, menuItem->text,
                                 QPalette::Text);
                    w = menuItem->fontMetrics.width(menuItem->text) + 5;
                }
                painter->setPen(shadow.lighter(106));
                bool reverse = menuItem->direction == Qt::RightToLeft;
                painter->drawLine(menuItem->rect.left() + 5 + (reverse ? 0 : w), menuItem->rect.center().y(),
                                  menuItem->rect.right() - 5 - (reverse ? w : 0), menuItem->rect.center().y());
                painter->restore();
                break;
            }
            bool selected = menuItem->state & State_Selected && menuItem->state & State_Enabled;
            if (selected) {
            	rgb_color base = ui_color(B_MENU_SELECTED_BACKGROUND_COLOR);
				uint32 flags = BControlLook::B_ACTIVATED;;
		        BRect bRect(0.0f, 0.0f, option->rect.width() - 1, option->rect.height() - 1);
				TemporarySurface surface(bRect);
				surface.view()->SetLowColor(base);
				be_control_look->DrawMenuItemBackground(surface.view(), bRect, bRect, base, flags);
				painter->drawImage(option->rect, surface.image());
            } else {
                painter->fillRect(option->rect, menuBackground);
            }

            bool checkable = menuItem->checkType != QStyleOptionMenuItem::NotCheckable;
            bool checked = menuItem->checked;
            bool sunken = menuItem->state & State_Sunken;
            bool enabled = menuItem->state & State_Enabled;

            bool ignoreCheckMark = false;
            int checkcol = qMax(menuItem->maxIconWidth, 20);

#ifndef QT_NO_COMBOBOX
            if (qobject_cast<const QComboBox*>(widget))
                ignoreCheckMark = true; //ignore the checkmarks provided by the QComboMenuDelegate
#endif

            if (!ignoreCheckMark) {
                // Check
                QRect checkRect(option->rect.left() + 7, option->rect.center().y() - 6, 13, 13);
                checkRect = visualRect(menuItem->direction, menuItem->rect, checkRect);
                if (checkable) {
                    if (menuItem->checkType & QStyleOptionMenuItem::Exclusive) {
                        // Radio button
                        if (checked || sunken) {
                            painter->setRenderHint(QPainter::Antialiasing);
                            painter->setPen(Qt::NoPen);

                            QPalette::ColorRole textRole = !enabled ? QPalette::Text:
                                                        selected ? QPalette::HighlightedText : QPalette::ButtonText;
                            painter->setBrush(option->palette.brush( option->palette.currentColorGroup(), textRole));
                            painter->drawEllipse(checkRect.adjusted(4, 4, -4, -4));
                        }
                    } else {
                        // Check box
                        if (menuItem->icon.isNull()) {
                            if (checked || sunken) {
                                QImage image(qt_haiku_menuitem_checkbox_checked);
                                if (enabled && (menuItem->state & State_Selected)) {
                                    image.setColor(1, 0x55ffffff);
                                    image.setColor(2, 0xAAffffff);
                                    image.setColor(3, 0xBBffffff);
                                    image.setColor(4, 0xFFffffff);
                                    image.setColor(5, 0x33ffffff);
                                } else {
                                    image.setColor(1, 0x55000000);
                                    image.setColor(2, 0xAA000000);
                                    image.setColor(3, 0xBB000000);
                                    image.setColor(4, 0xFF000000);
                                    image.setColor(5, 0x33000000);
                                }
                                painter->drawImage(QPoint(checkRect.center().x() - image.width() / 2,
                                                        checkRect.center().y() - image.height() / 2), image);
                            }
                        }
                    }
                }
            } else { //ignore checkmark
                if (menuItem->icon.isNull())
                    checkcol = 0;
                else
                    checkcol = menuItem->maxIconWidth;
            }

            // Text and icon, ripped from windows style
            bool dis = !(menuItem->state & State_Enabled);
            bool act = menuItem->state & State_Selected;
            const QStyleOption *opt = option;
            const QStyleOptionMenuItem *menuitem = menuItem;

            QPainter *p = painter;
            QRect vCheckRect = visualRect(opt->direction, menuitem->rect,
                                          QRect(menuitem->rect.x(), menuitem->rect.y(),
                                                checkcol, menuitem->rect.height()));
            if (!menuItem->icon.isNull()) {
                QIcon::Mode mode = dis ? QIcon::Disabled : QIcon::Normal;
                if (act && !dis)
                    mode = QIcon::Active;
                QPixmap pixmap;

                int smallIconSize = proxy()->pixelMetric(PM_SmallIconSize, option, widget);
                QSize iconSize(smallIconSize, smallIconSize);
#ifndef QT_NO_COMBOBOX
                if (const QComboBox *combo = qobject_cast<const QComboBox*>(widget))
                    iconSize = combo->iconSize();
#endif // QT_NO_COMBOBOX
                if (checked)
                    pixmap = menuItem->icon.pixmap(iconSize, mode, QIcon::On);
                else
                    pixmap = menuItem->icon.pixmap(iconSize, mode);

                int pixw = pixmap.width();
                int pixh = pixmap.height();

                QRect pmr(0, 0, pixw, pixh);
                pmr.moveCenter(vCheckRect.center());
                painter->setPen(menuItem->palette.text().color());
                if (checkable && checked) {
                    QStyleOption opt = *option;
                    if (act) {
                        QColor activeColor = mergedColors(option->palette.background().color(),
                                                        option->palette.highlight().color());
                        opt.palette.setBrush(QPalette::Button, activeColor);
                    }
                    opt.state |= State_Sunken;
                    opt.rect = vCheckRect;
                    proxy()->drawPrimitive(PE_PanelButtonCommand, &opt, painter, widget);
                }
                painter->drawPixmap(pmr.topLeft(), pixmap);
            }
            if (selected) {
                painter->setPen(menuItem->palette.highlightedText().color());
            } else {
                painter->setPen(menuItem->palette.text().color());
            }
            int x, y, w, h;
            menuitem->rect.getRect(&x, &y, &w, &h);
            int tab = menuitem->tabWidth;
            QColor discol;
            if (dis) {
                discol = menuitem->palette.text().color();
                p->setPen(discol);
            }
            int xm = windowsItemFrame + checkcol + windowsItemHMargin;
            int xpos = menuitem->rect.x() + xm;

            QRect textRect(xpos, y + windowsItemVMargin, w - xm - windowsRightBorder - tab + 1, h - 2 * windowsItemVMargin);
            QRect vTextRect = visualRect(opt->direction, menuitem->rect, textRect);
            QString s = menuitem->text;
            if (!s.isEmpty()) {                     // draw text
                p->save();
                int t = s.indexOf(QLatin1Char('\t'));
                int text_flags = Qt::AlignVCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
                if (!styleHint(SH_UnderlineShortcut, menuitem, widget))
                    text_flags |= Qt::TextHideMnemonic;
                text_flags |= Qt::AlignLeft;
                if (t >= 0) {
                    QRect vShortcutRect = visualRect(opt->direction, menuitem->rect,
                                                     QRect(textRect.topRight(), QPoint(menuitem->rect.right(), textRect.bottom())));
                    if (dis && !act && proxy()->styleHint(SH_EtchDisabledText, option, widget)) {
                        p->setPen(menuitem->palette.light().color());
                        p->drawText(vShortcutRect.adjusted(1, 1, 1, 1), text_flags, s.mid(t + 1));
                        p->setPen(discol);
                    }
                    p->drawText(vShortcutRect, text_flags, s.mid(t + 1));
                    s = s.left(t);
                }
                QFont font = menuitem->font;
                // font may not have any "hard" flags set. We override
                // the point size so that when it is resolved against the device, this font will win.
                // This is mainly to handle cases where someone sets the font on the window
                // and then the combo inherits it and passes it onward. At that point the resolve mask
                // is very, very weak. This makes it stonger.
                font.setPointSizeF(QFontInfo(menuItem->font).pointSizeF());

                if (menuitem->menuItemType == QStyleOptionMenuItem::DefaultItem)
                    font.setBold(true);

                p->setFont(font);
                if (dis && !act && proxy()->styleHint(SH_EtchDisabledText, option, widget)) {
                    p->setPen(menuitem->palette.light().color());
                    p->drawText(vTextRect.adjusted(1, 1, 1, 1), text_flags, s.left(t));
                    p->setPen(discol);
                }
                p->drawText(vTextRect, text_flags, s.left(t));
                p->restore();
            }

            // Arrow
            if (menuItem->menuItemType == QStyleOptionMenuItem::SubMenu) {// draw sub menu arrow
                int dim = (menuItem->rect.height() - 4) / 2;
                PrimitiveElement arrow;
                arrow = QApplication::isRightToLeft() ? PE_IndicatorArrowLeft : PE_IndicatorArrowRight;
                int xpos = menuItem->rect.left() + menuItem->rect.width() - 3 - dim;
                QRect  vSubMenuRect = visualRect(option->direction, menuItem->rect,
                                                 QRect(xpos, menuItem->rect.top() + menuItem->rect.height() / 2 - dim / 2, dim, dim));
                QStyleOptionMenuItem newMI = *menuItem;
                newMI.rect = vSubMenuRect;
                newMI.state = !enabled ? State_None : State_Enabled;
                if (selected)
                    newMI.palette.setColor(QPalette::ButtonText,
                                           newMI.palette.highlightedText().color());
                proxy()->drawPrimitive(arrow, &newMI, painter, widget);
            }
        }
        painter->restore();
        break;
    case CE_MenuHMargin:
    case CE_MenuVMargin:
        break;
    case CE_MenuEmptyArea:
        break;
    case CE_PushButtonLabel:
        if (const QStyleOptionButton *button = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            QRect ir = button->rect;
            uint tf = Qt::AlignVCenter;
            if (styleHint(SH_UnderlineShortcut, button, widget))
                tf |= Qt::TextShowMnemonic;
            else
               tf |= Qt::TextHideMnemonic;
			
			bool isDown = (option->state & State_Sunken) || (option->state & State_On);
			
            if (!button->icon.isNull()) {
                //Center both icon and text
                QPoint point;

                QIcon::Mode mode = button->state & State_Enabled ? QIcon::Normal
                                                              : QIcon::Disabled;
                if (mode == QIcon::Normal && button->state & State_HasFocus)
                    mode = QIcon::Active;
                QIcon::State state = QIcon::Off;
                if (button->state & State_On)
                    state = QIcon::On;

                QPixmap pixmap = button->icon.pixmap(button->iconSize, mode, state);
                int w = pixmap.width();
                int h = pixmap.height();

                if (!button->text.isEmpty())
                    w += button->fontMetrics.boundingRect(option->rect, tf, button->text).width() + 2;

                point = QPoint(ir.x() + ir.width() / 2 - w / 2,
                               ir.y() + ir.height() / 2 - h / 2);

                if (button->direction == Qt::RightToLeft)
                    point.rx() += pixmap.width();

                painter->drawPixmap(visualPos(button->direction, button->rect, point), pixmap);

                if (button->direction == Qt::RightToLeft)
                    ir.translate(-point.x() - 2, 0);
                else
                    ir.translate(point.x() + pixmap.width(), 0);

                // left-align text if there is
                if (!button->text.isEmpty())
                    tf |= Qt::AlignLeft;

            } else {
                tf |= Qt::AlignHCenter;
            }

            if (button->features & QStyleOptionButton::HasMenu)
                ir = ir.adjusted(0, 0, -proxy()->pixelMetric(PM_MenuButtonIndicator, button, widget), 0);
            if (isDown)
            	ir.adjust(1, 1, 1, 1);
            proxy()->drawItemText(painter, ir, tf, button->palette, (button->state & State_Enabled),
                         button->text, QPalette::ButtonText);
        }
        break;
    case CE_MenuBarEmptyArea:
        painter->save();
        {
			if (be_control_look != NULL) {
				QRect r = rect.adjusted(0,0,0,-1);
				rgb_color base = ui_color(B_MENU_BACKGROUND_COLOR);;
				uint32 flags = 0;            
		        BRect bRect(0.0f, 0.0f, r.width() - 1, r.height() - 1);
				TemporarySurface surface(bRect);
				be_control_look->DrawMenuBarBackground(surface.view(), bRect, bRect, base, flags);
				painter->drawImage(r, surface.image());			    
			}
			
   	        painter->setPen(QPen(QColor(152,152,152)));
            painter->drawLine(rect.bottomLeft(), rect.bottomRight());			
        }
        painter->restore();
        break;
#ifndef QT_NO_TABBAR
    case CE_TabBarTabShape:
        painter->save();
		if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(option)) {

            bool rtlHorTabs = (tab->direction == Qt::RightToLeft
                               && (tab->shape == QTabBar::RoundedNorth
                                   || tab->shape == QTabBar::RoundedSouth));
            bool selected = tab->state & State_Selected;
            bool lastTab = ((!rtlHorTabs && tab->position == QStyleOptionTab::End)
                            || (rtlHorTabs
                                && tab->position == QStyleOptionTab::Beginning));
            bool onlyTab = tab->position == QStyleOptionTab::OnlyOneTab;
            bool leftCornerWidget = (tab->cornerWidgets & QStyleOptionTab::LeftCornerWidget);

            bool atBeginning = ((tab->position == (tab->direction == Qt::LeftToRight ?
                                QStyleOptionTab::Beginning : QStyleOptionTab::End)) || onlyTab);

            bool onlyOne = tab->position == QStyleOptionTab::OnlyOneTab;
            bool previousSelected =
                ((!rtlHorTabs
                  && tab->selectedPosition == QStyleOptionTab::PreviousIsSelected)
                 || (rtlHorTabs
                     && tab->selectedPosition == QStyleOptionTab::NextIsSelected));
            bool nextSelected =
                ((!rtlHorTabs
                  && tab->selectedPosition == QStyleOptionTab::NextIsSelected)
                 || (rtlHorTabs
                     && tab->selectedPosition
                     == QStyleOptionTab::PreviousIsSelected));
            int tabBarAlignment = proxy()->styleHint(SH_TabBar_Alignment, tab, widget);
            bool leftAligned = (!rtlHorTabs && tabBarAlignment == Qt::AlignLeft)
                               || (rtlHorTabs
                                   && tabBarAlignment == Qt::AlignRight);

            bool rightAligned = (!rtlHorTabs && tabBarAlignment == Qt::AlignRight)
                                || (rtlHorTabs
                                    && tabBarAlignment == Qt::AlignLeft);

	        QColor backgroundColor(mkQColor(ui_color(B_PANEL_BACKGROUND_COLOR)));
	        QColor frameColor(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.30)));
	        QColor bevelLight(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 0.8)));
    	    QColor bevelShadow(mkQColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), 1.03)));

                                    
			if (be_control_look != NULL) {
				QRect r = option->rect;
				rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);;
				uint32 flags = 0; 
				uint32 side = BControlLook::B_TOP_BORDER;
				uint32 borders = BControlLook::B_RIGHT_BORDER|BControlLook::B_TOP_BORDER|BControlLook::B_BOTTOM_BORDER;
		        BRect bRect(0.0f, 0.0f, r.width() - 1, r.height() - 1);
				TemporarySurface surface(bRect);

				BRect bRect1 = bRect;				

				switch (tab->shape) {
					case QTabBar::TriangularNorth:
            		case QTabBar::RoundedNorth:
            			side = BControlLook::B_TOP_BORDER;
            			borders = (lastTab?BControlLook::B_RIGHT_BORDER:0) |
            					  (previousSelected?0:BControlLook::B_LEFT_BORDER) |
            					  BControlLook::B_TOP_BORDER |
            					  BControlLook::B_BOTTOM_BORDER;
            			if (lastTab || selected)
            				bRect1.right++;
            			if(!previousSelected || selected)
            				bRect1.left--;
    	            	break;
	                case QTabBar::TriangularSouth:
                	case QTabBar::RoundedSouth:
                		side = BControlLook::B_BOTTOM_BORDER;
            			borders = (lastTab?BControlLook::B_RIGHT_BORDER:0) |
            					  (previousSelected?0:BControlLook::B_LEFT_BORDER) |
            					  BControlLook::B_TOP_BORDER |
            					  BControlLook::B_BOTTOM_BORDER;
            			if (lastTab || selected)
            				bRect1.right++;
            			if(!previousSelected || selected)
            				bRect1.left--;
    	            	break;                		
 					case QTabBar::TriangularWest:
                	case QTabBar::RoundedWest:
                		side = BControlLook::B_LEFT_BORDER;                		
            			borders = (lastTab?BControlLook::B_BOTTOM_BORDER:0) |
            					  (previousSelected?0:BControlLook::B_TOP_BORDER) |
            					  BControlLook::B_LEFT_BORDER |
            					  BControlLook::B_RIGHT_BORDER;
            			if (lastTab || selected)
            				bRect1.bottom++;
            			if(!previousSelected || selected)
            				bRect1.top--;
	                	break;
    	            case QTabBar::TriangularEast:
	                case QTabBar::RoundedEast:
	                	side = BControlLook::B_RIGHT_BORDER;
            			borders = (lastTab?BControlLook::B_BOTTOM_BORDER:0) |
            					  (previousSelected?0:BControlLook::B_TOP_BORDER) |
            					  BControlLook::B_LEFT_BORDER |
            					  BControlLook::B_RIGHT_BORDER;
            			if (lastTab || selected)
            				bRect1.bottom++;
            			if(!previousSelected || selected)
            				bRect1.top--;
						break;
				}

				if(selected)
					be_control_look->DrawActiveTab(surface.view(), bRect1, bRect, base, flags, BControlLook::B_ALL_BORDERS, side);
				else
					be_control_look->DrawInactiveTab(surface.view(), bRect1, bRect, base, flags, borders, side);		    	    

				painter->drawImage(r, surface.image());
				if (!selected) {					
   	        		painter->setPen(QPen(frameColor));
   	        		switch(side) {
   	        			case BControlLook::B_TOP_BORDER:
   	        				painter->drawLine(rect.bottomLeft(), rect.bottomRight());
   	        				break;
   	        			case BControlLook::B_BOTTOM_BORDER:
   	        				painter->drawLine(rect.topLeft(), rect.topRight());
   	        				break;
   	        			case BControlLook::B_LEFT_BORDER:
   	        				painter->drawLine(rect.topRight(), rect.bottomRight());
   	        				break;
   	        			case BControlLook::B_RIGHT_BORDER:
   	        				painter->drawLine(rect.topLeft(), rect.bottomLeft());
   	        				break;
   	        		}            		
				}
			}
		}
        painter->restore();
        break;

#endif // QT_NO_TABBAR
    default:
        QCommonStyle::drawControl(element,option,painter,widget);
        break;
    }
}

/*!
  \reimp
*/
QPalette QHaikuStyle::standardPalette () const
{
    QPalette palette = QProxyStyle::standardPalette();
    palette.setBrush(QPalette::Active, QPalette::Highlight, QColor(98, 140, 178));
    palette.setBrush(QPalette::Inactive, QPalette::Highlight, QColor(145, 141, 126));
    palette.setBrush(QPalette::Disabled, QPalette::Highlight, QColor(145, 141, 126));

    QColor backGround(mkQColor(ui_color(B_PANEL_BACKGROUND_COLOR)));

    QColor light = backGround.lighter(150);
    QColor base = Qt::white;
    QColor dark = QColor(170, 156, 143).darker(110);
    dark = backGround.darker(150);
    QColor darkDisabled = QColor(209, 200, 191).darker(110);

    //### Find the correct disabled text color
    palette.setBrush(QPalette::Disabled, QPalette::Text, QColor(190, 190, 190));

    palette.setBrush(QPalette::Window, backGround);
    palette.setBrush(QPalette::Mid, backGround.darker(130));
    palette.setBrush(QPalette::Light, light);

    palette.setBrush(QPalette::Active, QPalette::Base, base);
    palette.setBrush(QPalette::Inactive, QPalette::Base, base);
    palette.setBrush(QPalette::Disabled, QPalette::Base, backGround);

    palette.setBrush(QPalette::Midlight, palette.mid().color().lighter(110));

    palette.setBrush(QPalette::All, QPalette::Dark, dark);
    palette.setBrush(QPalette::Disabled, QPalette::Dark, darkDisabled);

    QColor button = backGround;

    palette.setBrush(QPalette::Button, button);

    QColor shadow = dark.darker(135);
    palette.setBrush(QPalette::Shadow, shadow);
    palette.setBrush(QPalette::Disabled, QPalette::Shadow, shadow.lighter(150));
    palette.setBrush(QPalette::HighlightedText, QColor(QRgb(0xffffffff)));
    return palette;
}

/*!
  \reimp
*/
void QHaikuStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                         QPainter *painter, const QWidget *widget) const
{
    QColor button = option->palette.button().color();
    QColor dark;
    QColor grooveColor;
    QColor darkOutline;
    dark.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*1.9)),
                qMin(255, (int)(button.value()*0.7)));
    grooveColor.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*2.6)),
                qMin(255, (int)(button.value()*0.9)));
    darkOutline.setHsv(button.hue(),
                qMin(255, (int)(button.saturation()*3.0)),
                qMin(255, (int)(button.value()*0.6)));

    QColor alphaCornerColor;
    if (widget) {
        // ### backgroundrole/foregroundrole should be part of the style option
        alphaCornerColor = mergedColors(option->palette.color(widget->backgroundRole()), darkOutline);
    } else {
        alphaCornerColor = mergedColors(option->palette.background().color(), darkOutline);
    }
    QColor gripShadow = grooveColor.darker(110);
    QColor buttonShadow = option->palette.button().color().darker(110);

    QColor gradientStartColor = option->palette.button().color().lighter(108);
    QColor gradientStopColor = mergedColors(option->palette.button().color().darker(108), dark.lighter(150), 70);

    QPalette palette = option->palette;

    switch (control) {
#ifndef QT_NO_SPINBOX
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            QPixmap cache;
            QString pixmapName = QStyleHelper::uniqueName(QLatin1String("spinbox"), spinBox, spinBox->rect.size());
            if (!QPixmapCache::find(pixmapName, cache)) {
                cache = QPixmap(spinBox->rect.size());
                cache.fill(Qt::transparent);
                QRect pixmapRect(0, 0, spinBox->rect.width(), spinBox->rect.height());
                QPainter cachePainter(&cache);

                bool isEnabled = (spinBox->state & State_Enabled);
                //bool focus = isEnabled && (spinBox->state & State_HasFocus);
                bool hover = isEnabled && (spinBox->state & State_MouseOver);
                bool sunken = (spinBox->state & State_Sunken);
                bool upIsActive = (spinBox->activeSubControls == SC_SpinBoxUp);
                bool downIsActive = (spinBox->activeSubControls == SC_SpinBoxDown);

                QRect rect = pixmapRect;
                QStyleOptionSpinBox spinBoxCopy = *spinBox;
                spinBoxCopy.rect = pixmapRect;
                QRect upRect = proxy()->subControlRect(CC_SpinBox, &spinBoxCopy, SC_SpinBoxUp, widget);
                QRect downRect = proxy()->subControlRect(CC_SpinBox, &spinBoxCopy, SC_SpinBoxDown, widget);

                int fw = spinBoxCopy.frame ? proxy()->pixelMetric(PM_SpinBoxFrameWidth, &spinBoxCopy, widget) : 0;
                cachePainter.fillRect(rect.adjusted(1, qMax(fw - 1, 0), -1, -fw),
                                      option->palette.base());

                QRect r = rect.adjusted(0, 1, 0, -1);
                if (spinBox->frame) {

                    QColor topShadow = darkOutline;
                    topShadow.setAlpha(60);
                    cachePainter.setPen(topShadow);

                    // antialias corners
                    const QPoint points[8] = {
                        QPoint(r.right(), r.top() + 1),
                        QPoint(r.right() - 1, r.top() ),
                        QPoint(r.right(), r.bottom() - 1),
                        QPoint(r.right() - 1, r.bottom() ),
                        QPoint(r.left() + 1, r.bottom()),
                        QPoint(r.left(), r.bottom() - 1),
                        QPoint(r.left() + 1, r.top()),
                        QPoint(r.left(), r.top() + 1)
                    };
                    cachePainter.drawPoints(points, 8);

                    // draw frame
                    topShadow.setAlpha(30);
                    cachePainter.setPen(topShadow);
                    cachePainter.drawLine(QPoint(r.left() + 2, r.top() - 1), QPoint(r.right() - 2, r.top() - 1));

                    cachePainter.setPen(QPen(option->palette.background().color(), 1));
                    cachePainter.drawLine(QPoint(r.left() + 2, r.top() + 1), QPoint(r.right() - 2, r.top() + 1));
                    QColor highlight = Qt::white;
                    highlight.setAlpha(130);
                    cachePainter.setPen(option->palette.base().color().darker(120));
                    cachePainter.drawLine(QPoint(r.left() + 1, r.top() + 1),
                                  QPoint(r.right() - 1, r.top() + 1));
                    cachePainter.drawLine(QPoint(r.left() + 1, r.top() + 1),
                                  QPoint(r.left() + 1, r.bottom() - 1));
                    cachePainter.setPen(option->palette.base().color());
                    cachePainter.drawLine(QPoint(r.right() - 1, r.top() + 1),
                                  QPoint(r.right() - 1, r.bottom() - 1));
                    cachePainter.drawLine(QPoint(r.left() + 1, r.bottom() - 1),
                                  QPoint(r.right() - 1, r.bottom() - 1));
                    cachePainter.setPen(highlight);
                    cachePainter.drawLine(QPoint(r.left() + 3, r.bottom() + 1),
                                  QPoint(r.right() - 3, r.bottom() + 1));

                    cachePainter.setPen(QPen(darkOutline, 1));

                    // top and bottom lines
                    const QLine lines[4] = {
                        QLine(QPoint(r.left() + 2, r.bottom()), QPoint(r.right()- 2, r.bottom())),
                        QLine(QPoint(r.left() + 2, r.top()), QPoint(r.right() - 2, r.top())),
                        QLine(QPoint(r.right(), r.top() + 2), QPoint(r.right(), r.bottom() - 2)),
                        QLine(QPoint(r.left(), r.top() + 2), QPoint(r.left(), r.bottom() - 2))
                    };
                    cachePainter.drawLines(lines, 4);
                }

                    // gradients
                    qt_haiku_draw_gradient(&cachePainter, upRect,
                                            gradientStartColor.darker(106),
                                            gradientStopColor, TopDown, option->palette.button());
                    qt_haiku_draw_gradient(&cachePainter, downRect.adjusted(0, 0, 0, 1),
                                            gradientStartColor.darker(106),
                                            gradientStopColor, TopDown, option->palette.button());
                if (isEnabled) {
                    if (upIsActive) {
                        if (sunken) {
                            cachePainter.fillRect(upRect.adjusted(1, 0, 0, 0), gradientStopColor.darker(110));
                        } else if (hover) {
                            qt_haiku_draw_gradient(&cachePainter, upRect.adjusted(1, 0, 0, 0),
                                                    gradientStartColor.lighter(110),
                                                    gradientStopColor.lighter(110), TopDown, option->palette.button());
                        }
                    }
                    if (downIsActive) {
                        if (sunken) {
                            cachePainter.fillRect(downRect.adjusted(1, 0, 0, 1), gradientStopColor.darker(110));

                        } else if (hover) {
                                qt_haiku_draw_gradient(&cachePainter, downRect.adjusted(1, 0, 0, 1),
                                                        gradientStartColor.lighter(110),
                                                        gradientStopColor.lighter(110), TopDown, option->palette.button());
                        }
                    }
                }

                if (spinBox->frame) {
                    // rounded corners
                    const QPoint points[4] = {
                        QPoint(r.left() + 1, r.bottom() - 1),
                        QPoint(r.left() + 1, r.top() + 1),
                        QPoint(r.right() - 1, r.bottom() - 1),
                        QPoint(r.right() - 1, r.top() + 1)
                    };
                    cachePainter.drawPoints(points, 4);

                    if (option->state & State_HasFocus) {
                        QColor darkoutline = option->palette.highlight().color().darker(150);
                        QColor innerline = mergedColors(option->palette.highlight().color(), Qt::white);
                        cachePainter.setPen(QPen(innerline, 0));
                        if (spinBox->direction == Qt::LeftToRight) {
                            cachePainter.drawRect(rect.adjusted(1, 2, -3 -downRect.width(), -3));
                            cachePainter.setPen(QPen(darkoutline, 0));
                            const QLine lines[4] = {
                                QLine(QPoint(r.left() + 2, r.bottom()), QPoint(r.right()- downRect.width() - 1, r.bottom())),
                                QLine(QPoint(r.left() + 2, r.top()), QPoint(r.right() - downRect.width() - 1, r.top())),
                                QLine(QPoint(r.right() - downRect.width() - 1, r.top() + 1), QPoint(r.right()- downRect.width() - 1, r.bottom() - 1)),
                                QLine(QPoint(r.left(), r.top() + 2), QPoint(r.left(), r.bottom() - 2))
                            };
                            cachePainter.drawLines(lines, 4);
                            cachePainter.drawPoint(QPoint(r.left() + 1, r.bottom() - 1));
                            cachePainter.drawPoint(QPoint(r.left() + 1, r.top() + 1));
                            cachePainter.drawLine(QPoint(r.left(), r.top() + 2), QPoint(r.left(), r.bottom() - 2));
                        } else {
                            cachePainter.drawRect(rect.adjusted(downRect.width() + 2, 2, -2, -3));
                            cachePainter.setPen(QPen(darkoutline, 0));
                            cachePainter.drawLine(QPoint(r.left() + downRect.width(), r.bottom()), QPoint(r.right()- 2 - 1, r.bottom()));
                            cachePainter.drawLine(QPoint(r.left() + downRect.width(), r.top()), QPoint(r.right() - 2 - 1, r.top()));

                            cachePainter.drawLine(QPoint(r.right(), r.top() + 2), QPoint(r.right(), r.bottom() - 2));
                            cachePainter.drawPoint(QPoint(r.right() - 1, r.bottom() - 1));
                            cachePainter.drawPoint(QPoint(r.right() - 1, r.top() + 1));
                            cachePainter.drawLine(QPoint(r.left() + downRect.width() + 1, r.top()),
                                                  QPoint(r.left() + downRect.width() + 1, r.bottom()));
                        }
                    }
                }

                // outline the up/down buttons
                cachePainter.setPen(darkOutline);
                QColor light = option->palette.light().color().lighter();

                if (spinBox->direction == Qt::RightToLeft) {
                    cachePainter.drawLine(upRect.right(), upRect.top() - 1, upRect.right(), downRect.bottom() + 1);
                    cachePainter.setPen(light);
                    cachePainter.drawLine(upRect.right() - 1, upRect.top() + 3, upRect.right() - 1, downRect.bottom() );
                } else {
                    cachePainter.drawLine(upRect.left(), upRect.top() - 1, upRect.left(), downRect.bottom() + 1);
                    cachePainter.setPen(light);
                    cachePainter.drawLine(upRect.left() + 1, upRect.top() , upRect.left() + 1, downRect.bottom() );
                }
                if (upIsActive && sunken) {
                    cachePainter.setPen(gradientStopColor.darker(130));
                    cachePainter.drawLine(upRect.left() + 1, upRect.top(), upRect.left() + 1, upRect.bottom());
                    cachePainter.drawLine(upRect.left(), upRect.top() - 1, upRect.right(), upRect.top() - 1);
                } else {
                    cachePainter.setPen(light);
                    cachePainter.drawLine(upRect.topLeft() + QPoint(1, -1), upRect.topRight() + QPoint(-1, -1));
                    cachePainter.setPen(darkOutline);
                    cachePainter.drawLine(upRect.bottomLeft(), upRect.bottomRight());
                }
                if (downIsActive && sunken) {
                    cachePainter.setPen(gradientStopColor.darker(130));
                    cachePainter.drawLine(downRect.left() + 1, downRect.top(), downRect.left() + 1, downRect.bottom() + 1);
                    cachePainter.drawLine(downRect.left(), downRect.top(), downRect.right(), downRect.top());
                    cachePainter.setPen(gradientStopColor.darker(110));
                    cachePainter.drawLine(downRect.left(), downRect.bottom() + 1, downRect.right(), downRect.bottom() + 1);
                } else {
                    cachePainter.setPen(light);
                    cachePainter.drawLine(downRect.topLeft() + QPoint(2,0), downRect.topRight());
                }

                if (spinBox->buttonSymbols == QAbstractSpinBox::PlusMinus) {
                    int centerX = upRect.center().x();
                    int centerY = upRect.center().y();
                    cachePainter.setPen(spinBox->palette.foreground().color());

                    // plus/minus
                    if (spinBox->activeSubControls == SC_SpinBoxUp && sunken) {
                        cachePainter.drawLine(1 + centerX - 2, 1 + centerY, 1 + centerX + 2, 1 + centerY);
                        cachePainter.drawLine(1 + centerX, 1 + centerY - 2, 1 + centerX, 1 + centerY + 2);
                    } else {
                        cachePainter.drawLine(centerX - 2, centerY, centerX + 2, centerY);
                        cachePainter.drawLine(centerX, centerY - 2, centerX, centerY + 2);
                    }

                    centerX = downRect.center().x();
                    centerY = downRect.center().y();
                    if (spinBox->activeSubControls == SC_SpinBoxDown && sunken) {
                        cachePainter.drawLine(1 + centerX - 2, 1 + centerY, 1 + centerX + 2, 1 + centerY);
                    } else {
                        cachePainter.drawLine(centerX - 2, centerY, centerX + 2, centerY);
                    }
                } else if (spinBox->buttonSymbols == QAbstractSpinBox::UpDownArrows){
                    // arrows
                    QImage upArrow(qt_spinbox_button_arrow_up);
                    upArrow.setColor(1, spinBox->palette.foreground().color().rgba());

                    cachePainter.drawImage(upRect.center().x() - upArrow.width() / 2,
                                            upRect.center().y() - upArrow.height() / 2,
                                            upArrow);

                    QImage downArrow(qt_spinbox_button_arrow_down);
                    downArrow.setColor(1, spinBox->palette.foreground().color().rgba());

                    cachePainter.drawImage(downRect.center().x() - downArrow.width() / 2,
                                            downRect.center().y() - downArrow.height() / 2 + 1,
                                            downArrow);
                }

                QColor disabledColor = option->palette.background().color();
                disabledColor.setAlpha(150);
                if (!(spinBox->stepEnabled & QAbstractSpinBox::StepUpEnabled))
                    cachePainter.fillRect(upRect.adjusted(1, 0, 0, 0), disabledColor);
                if (!(spinBox->stepEnabled & QAbstractSpinBox::StepDownEnabled)) {
                    cachePainter.fillRect(downRect.adjusted(1, 0, 0, 0), disabledColor);
                }
                cachePainter.end();
                QPixmapCache::insert(pixmapName, cache);
            }
            painter->drawPixmap(spinBox->rect.topLeft(), cache);
        }
        break;
#endif // QT_NO_SPINBOX
    case CC_TitleBar:
        painter->save();
        if (const QStyleOptionTitleBar *titleBar = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            bool active = (titleBar->titleBarState & State_Active);

			int titleBarHeight = proxy()->pixelMetric(PM_TitleBarHeight);
			int frameWidth = proxy()->pixelMetric(PM_MdiSubWindowFrameWidth);
			
            QRect fullRect = titleBar->rect;

            QPalette palette = option->palette;
            QColor highlight = option->palette.highlight().color();

            QColor titleBarFrameBorder(active ? highlight.darker(180): dark.darker(110));
            QColor titleBarHighlight(active ? highlight.lighter(120): palette.background().color().lighter(120));
            QColor textAlphaColor(active ? 0xffffff : 0xff000000 );

            QColor textColorActive(mkQColor(ui_color(B_WINDOW_TEXT_COLOR)));
		    QColor textColorInactive(mkQColor(ui_color(B_WINDOW_INACTIVE_TEXT_COLOR)));
            QColor textColor(active ? textColorActive : textColorInactive);

            QColor tabColorActive(mkQColor(ui_color(B_WINDOW_TAB_COLOR)));
		    QColor tabColorInactive(mkQColor(ui_color(B_WINDOW_INACTIVE_TAB_COLOR)));

            QColor tabColorActiveLight(mkQColor(tint_color(ui_color(B_WINDOW_TAB_COLOR), (B_LIGHTEN_MAX_TINT + B_LIGHTEN_2_TINT) / 2)));
		    QColor tabColorInactiveLight(mkQColor(tint_color(ui_color(B_WINDOW_INACTIVE_TAB_COLOR), (B_LIGHTEN_MAX_TINT + B_LIGHTEN_2_TINT) / 2)));
            
            QColor titlebarColor = QColor(active ? tabColorActive : tabColorInactive);
            QColor titlebarColor2 = QColor(active ? tabColorActiveLight : tabColorInactiveLight);
            
            color_which bcolor = active ? B_WINDOW_BORDER_COLOR : B_WINDOW_INACTIVE_BORDER_COLOR;
            color_which tcolor = active ? B_WINDOW_TAB_COLOR : B_WINDOW_INACTIVE_TAB_COLOR;
            
			QColor frameColorActive(mkQColor(ui_color(bcolor)));
			QColor bevelShadow1(mkQColor(tint_color(ui_color(bcolor), 1.07)));
			QColor bevelShadow2(mkQColor(tint_color(ui_color(bcolor), B_DARKEN_2_TINT)));
			QColor bevelShadow3(mkQColor(tint_color(ui_color(bcolor), B_DARKEN_3_TINT)));
			QColor bevelLight(mkQColor(tint_color(ui_color(bcolor), B_LIGHTEN_2_TINT)));
			
			QColor tabColor(mkQColor(ui_color(tcolor)));
			QColor tabBevelLight(mkQColor(tint_color(ui_color(tcolor), B_LIGHTEN_2_TINT)));
			QColor tabShadow(mkQColor(tint_color(ui_color(tcolor), (B_DARKEN_1_TINT + B_NO_TINT) / 2)));
			QColor buttonFrame(mkQColor(tint_color(ui_color(tcolor), B_DARKEN_2_TINT)));

			qt_haiku_draw_windows_frame(painter, fullRect.adjusted(0, titleBarHeight - frameWidth, 0, titleBarHeight - frameWidth),
				active ? B_WINDOW_BORDER_COLOR : B_WINDOW_INACTIVE_BORDER_COLOR,
				BControlLook::B_LEFT_BORDER | BControlLook::B_RIGHT_BORDER | BControlLook::B_TOP_BORDER, false);
			
			// tab
            QRect tabRect = fullRect;
            
            QRect textRect = proxy()->subControlRect(CC_TitleBar, titleBar, SC_TitleBarLabel, widget);

            int tabWidth = textRect.width() + mdiTabTextMarginLeft + mdiTabTextMarginRight;            
            tabRect.setWidth(tabWidth);
			
            QLinearGradient gradient(option->rect.topLeft(), option->rect.bottomLeft());
            gradient.setColorAt(0, titlebarColor2);
            gradient.setColorAt(1, titlebarColor);

			painter->setPen(bevelShadow2);
			painter->drawLine(tabRect.topLeft(), tabRect.bottomLeft());
			painter->drawLine(tabRect.topLeft(), tabRect.topRight());
			painter->setPen(bevelShadow3);
			painter->drawLine(tabRect.topRight(), tabRect.bottomRight() - QPoint(0, frameWidth));
            
            painter->setPen(tabBevelLight);
			painter->drawLine(tabRect.topLeft() + QPoint(1, 1), tabRect.bottomLeft() + QPoint(1, -4));
			painter->drawLine(tabRect.topLeft() + QPoint(1, 1), tabRect.topRight() + QPoint(-1, 1));
			painter->setPen(tabShadow);
			painter->drawLine(tabRect.topRight() + QPoint(-1, 2), tabRect.bottomRight() - QPoint(1, frameWidth - 1));

            painter->fillRect(tabRect.adjusted(2, 2, -2, 1 - frameWidth), gradient);

            // draw title
            QFont font = widget->font();
            font.setBold(true);
            painter->setFont(font);
            painter->setPen(textColor);
            // Note workspace also does elliding but it does not use the correct font
            QString title = QFontMetrics(font).elidedText(titleBar->text, Qt::ElideMiddle, textRect.width() - 14);
            painter->drawText(textRect, title, QTextOption(Qt::AlignLeft | Qt::AlignVCenter));
            
            // max button
            if ((titleBar->subControls & SC_TitleBarMaxButton) && (titleBar->titleBarFlags & Qt::WindowMaximizeButtonHint) &&
                !(titleBar->titleBarState & Qt::WindowMaximized)) {
                QRect maxButtonRect = proxy()->subControlRect(CC_TitleBar, titleBar, SC_TitleBarMaxButton, widget);
                if (maxButtonRect.isValid()) {
                    bool hover = (titleBar->activeSubControls & SC_TitleBarMaxButton) && (titleBar->state & State_MouseOver);
                    bool sunken = (titleBar->activeSubControls & SC_TitleBarMaxButton) && (titleBar->state & State_Sunken);

					QRect bigBox = maxButtonRect.adjusted(4, 4, 0, 0);
					QRect smallBox = maxButtonRect.adjusted(0, 0, -7, -7);
                    
					QLinearGradient gradient(bigBox.left(), bigBox.top(), bigBox.right(), bigBox.bottom());
            		gradient.setColorAt(sunken?1:0, Qt::white);
            		gradient.setColorAt(sunken?0:1, tabColor);

                    painter->setPen(buttonFrame);
            		painter->fillRect(bigBox, gradient);
                    painter->drawRect(bigBox);
                    gradient.setStart(smallBox.left(), smallBox.top());
                    gradient.setFinalStop(smallBox.right(), smallBox.bottom());
            		painter->fillRect(smallBox, gradient);
                    painter->drawRect(smallBox);
                }
            }

            // close button
            if ((titleBar->subControls & SC_TitleBarCloseButton) && (titleBar->titleBarFlags & Qt::WindowSystemMenuHint)) {
                QRect closeButtonRect = proxy()->subControlRect(CC_TitleBar, titleBar, SC_TitleBarCloseButton, widget);
                if (closeButtonRect.isValid()) {
                    bool hover = (titleBar->activeSubControls & SC_TitleBarCloseButton) && (titleBar->state & State_MouseOver);
                    bool sunken = (titleBar->activeSubControls & SC_TitleBarCloseButton) && (titleBar->state & State_Sunken);
					QLinearGradient gradient(closeButtonRect.left(), closeButtonRect.top(), closeButtonRect.right(), closeButtonRect.bottom());
            		gradient.setColorAt(sunken?1:0, Qt::white);
            		gradient.setColorAt(sunken?0:1, tabColor);
            		painter->fillRect(closeButtonRect, gradient);
                    painter->setPen(buttonFrame);
                    painter->drawRect(closeButtonRect);
                }
            }

            // normalize button
            if ((titleBar->subControls & SC_TitleBarNormalButton) &&
               (((titleBar->titleBarFlags & Qt::WindowMinimizeButtonHint) &&
               (titleBar->titleBarState & Qt::WindowMinimized)) ||
               ((titleBar->titleBarFlags & Qt::WindowMaximizeButtonHint) &&
               (titleBar->titleBarState & Qt::WindowMaximized)))) {
                QRect normalButtonRect = proxy()->subControlRect(CC_TitleBar, titleBar, SC_TitleBarNormalButton, widget);
                if (normalButtonRect.isValid()) {
                    bool hover = (titleBar->activeSubControls & SC_TitleBarNormalButton) && (titleBar->state & State_MouseOver);
                    bool sunken = (titleBar->activeSubControls & SC_TitleBarNormalButton) && (titleBar->state & State_Sunken);

					QRect bigBox = normalButtonRect.adjusted(4, 4, 0, 0);
					QRect smallBox = normalButtonRect.adjusted(0, 0, -7, -7);

					QLinearGradient gradient(bigBox.left(), bigBox.top(), bigBox.right(), bigBox.bottom());
            		gradient.setColorAt(sunken?1:0, Qt::white);
            		gradient.setColorAt(sunken?0:1, tabColor);

                    painter->setPen(buttonFrame);
            		painter->fillRect(bigBox, gradient);
                    painter->drawRect(bigBox);
                    gradient.setStart(smallBox.left(), smallBox.top());
                    gradient.setFinalStop(smallBox.right(), smallBox.bottom());
            		painter->fillRect(smallBox, gradient);
                    painter->drawRect(smallBox);                                     
                }
            }
        }
        painter->restore();
        break;
#ifndef QT_NO_SCROLLBAR
    case CC_ScrollBar:
        painter->save();
        if (const QStyleOptionSlider *scrollBar = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            bool isEnabled = scrollBar->state & State_Enabled;
            bool reverse = scrollBar->direction == Qt::RightToLeft;
            bool horizontal = scrollBar->orientation == Qt::Horizontal;
            bool sunken = scrollBar->state & State_Sunken;

            painter->fillRect(option->rect, option->palette.background());

            QRect scrollBarSubLine = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSubLine, widget);
            QRect scrollBarAddLine = proxy()->subControlRect(control, scrollBar, SC_ScrollBarAddLine, widget);
            QRect scrollBarSlider = proxy()->subControlRect(control, scrollBar, SC_ScrollBarSlider, widget);
            QRect grooveRect = proxy()->subControlRect(control, scrollBar, SC_ScrollBarGroove, widget);
            
            if (scrollBar->subControls & SC_ScrollBarSubLine) {
				QRect pixmapRect = scrollBarSubLine;;
            
            	bool pushed = (scrollBar->activeSubControls & SC_ScrollBarSubLine) && sunken;
           	            
				qt_haiku_draw_button(painter, pixmapRect, false, false, pushed, false, isEnabled, false,
					horizontal?B_HORIZONTAL:B_VERTICAL, horizontal?ARROW_LEFT:ARROW_UP);	
            }
            if (scrollBar->subControls & SC_ScrollBarAddLine) {
				QRect pixmapRect = scrollBarAddLine;;

            	bool pushed = (scrollBar->activeSubControls & SC_ScrollBarAddLine) && sunken;

				qt_haiku_draw_button(painter, pixmapRect, false, false, pushed, false, isEnabled, false,
					horizontal?B_HORIZONTAL:B_VERTICAL, horizontal?ARROW_RIGHT:ARROW_DOWN);
            }
			// paint groove
            if (scrollBar->subControls & SC_ScrollBarGroove) {
                if (horizontal) {
                	scrollBarSlider.adjust(1,0,-1,0);
                	grooveRect.adjust(-1,0,1,0);
                	QRect thumbRect = scrollBarSlider;
                	rgb_color normal = ui_color(B_PANEL_BACKGROUND_COLOR);
                	BRect bRectGroove(0.0f, 0.0f, grooveRect.width() - 1, grooveRect.height() - 1);
					BRect leftOfThumb(bRectGroove.left, bRectGroove.top, (thumbRect.left() - grooveRect.left()) - 2, bRectGroove.bottom);
					BRect rightOfThumb((thumbRect.right() - grooveRect.left()) + 2, bRectGroove.top, bRectGroove.right, bRectGroove.bottom);
					TemporarySurface surfaceGroove(bRectGroove);
					surfaceGroove.view()->SetDrawingMode(B_OP_COPY);
					be_control_look->DrawScrollBarBackground(surfaceGroove.view(), leftOfThumb, rightOfThumb, bRectGroove, normal, 0, B_HORIZONTAL);
					surfaceGroove.view()->SetHighColor(tint_color(normal, B_DARKEN_2_TINT));
					surfaceGroove.view()->StrokeRect(surfaceGroove.view()->Bounds());
					painter->drawImage(grooveRect, surfaceGroove.image());
                } else {
                	scrollBarSlider.adjust(0,-1,0,1);
                	grooveRect.adjust(0,-1,0,1);
                	QRect thumbRect = scrollBarSlider;
                	rgb_color normal = ui_color(B_PANEL_BACKGROUND_COLOR);
                	BRect bRectGroove(0.0f, 0.0f, grooveRect.width() - 1, grooveRect.height() - 1);
					BRect upOfThumb(bRectGroove.left, bRectGroove.top, bRectGroove.right, (thumbRect.top()-grooveRect.top()));
					BRect downOfThumb(bRectGroove.left, (thumbRect.bottom()-grooveRect.top()), bRectGroove.right, bRectGroove.bottom);
					TemporarySurface surfaceGroove(bRectGroove);
					surfaceGroove.view()->SetDrawingMode(B_OP_COPY);
					be_control_look->DrawScrollBarBackground(surfaceGroove.view(), upOfThumb, downOfThumb, bRectGroove, normal, 0, B_VERTICAL);
					surfaceGroove.view()->SetHighColor(tint_color(normal, B_DARKEN_2_TINT));
					surfaceGroove.view()->StrokeRect(surfaceGroove.view()->Bounds());
					painter->drawImage(grooveRect, surfaceGroove.image());                	
                }                
            }            
            //paint slider
            if (scrollBar->subControls & SC_ScrollBarSlider) {
                QRect pixmapRect = scrollBarSlider;

                if (horizontal)
                    pixmapRect.adjust(-2, 0, 2, 0);

                qt_haiku_draw_button(painter, pixmapRect, false, false, false, false, isEnabled, false, 
                	horizontal?B_HORIZONTAL:B_VERTICAL);
            }
        }
        painter->restore();
        break;
#endif // QT_NO_SCROLLBAR
#ifndef QT_NO_COMBOBOX
    case CC_ComboBox:
        painter->save();
        if (const QStyleOptionComboBox *comboBox = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            bool sunken = comboBox->state & State_On; // play dead, if combobox has no items
            bool isEnabled = (comboBox->state & State_Enabled);
            bool focus = isEnabled && (comboBox->state & State_HasFocus);
           
           	QRect rect = comboBox->rect.adjusted(0,1,0,-1);
			BRect bRect(0.0f, 0.0f, rect.width() - 1, rect.height() - 1);
			TemporarySurface surface(bRect);
			rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
			rgb_color color = tint_color(base, B_DARKEN_2_TINT);

			uint32 flags = 0;

			if (!isEnabled)
				flags |= BControlLook::B_DISABLED;
			if (comboBox->state & State_On)
				flags |= BControlLook::B_ACTIVATED;
			if (comboBox->state & State_HasFocus)
				flags |= BControlLook::B_FOCUSED;
			if (comboBox->state & State_Sunken)
				flags |= BControlLook::B_CLICKED;
			if (comboBox->state & State_NoChange)
				flags |= BControlLook::B_DISABLED | BControlLook::B_ACTIVATED;

			BRect bRect2 = bRect;
			bRect2.InsetBy(1, 1);
            be_control_look->DrawMenuFieldBackground(surface.view(), bRect2, bRect2, base, true, flags);
            be_control_look->DrawMenuFieldFrame(surface.view(), bRect, bRect, base, base, flags);
            
			painter->drawImage(rect, surface.image());
        }
        painter->restore();
        break;
#endif // QT_NO_COMBOBOX
#ifndef QT_NO_GROUPBOX
    case CC_GroupBox:
        painter->save();
        if (const QStyleOptionGroupBox *groupBox = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            QRect textRect = proxy()->subControlRect(CC_GroupBox, groupBox, SC_GroupBoxLabel, widget);
            QRect checkBoxRect = proxy()->subControlRect(CC_GroupBox, groupBox, SC_GroupBoxCheckBox, widget);
            bool flat = groupBox->features & QStyleOptionFrame::Flat;

            if (!flat) {
                if (groupBox->subControls & QStyle::SC_GroupBoxFrame) {
                    QStyleOptionFrame frame;
                    frame.QStyleOption::operator=(*groupBox);
                    frame.features = groupBox->features;
                    frame.lineWidth = groupBox->lineWidth;
                    frame.midLineWidth = groupBox->midLineWidth;
                    frame.rect = proxy()->subControlRect(CC_GroupBox, option, SC_GroupBoxFrame, widget);

                    painter->save();
                    QRegion region(groupBox->rect);
                    bool ltr = groupBox->direction == Qt::LeftToRight;
                    region -= checkBoxRect.united(textRect).adjusted(ltr ? -4 : 0, 0, ltr ? 0 : 4, 0);
                    if (!groupBox->text.isEmpty() ||  groupBox->subControls & SC_GroupBoxCheckBox)
                        painter->setClipRegion(region);
                    frame.palette.setBrush(QPalette::Dark, option->palette.mid().color().lighter(110));
                    proxy()->drawPrimitive(PE_FrameGroupBox, &frame, painter);
                    painter->restore();
                }
            }
            // Draw title
            if ((groupBox->subControls & QStyle::SC_GroupBoxLabel) && !groupBox->text.isEmpty()) {
                if (!groupBox->text.isEmpty()) {
                    QColor textColor = groupBox->textColor;
                    if (textColor.isValid())
                        painter->setPen(textColor);
					QFont font = widget->font();
					font.setBold(true);
					painter->setFont(font);
                    painter->drawText(textRect, Qt::TextHideMnemonic | Qt::AlignLeft| groupBox->textAlignment, groupBox->text);
                }
            }
            if (groupBox->subControls & SC_GroupBoxCheckBox) {
                QStyleOptionButton box;
                box.QStyleOption::operator=(*groupBox);
                box.rect = checkBoxRect;
                proxy()->drawPrimitive(PE_IndicatorCheckBox, &box, painter, widget);
            }
        }
        painter->restore();
        break;
#endif // QT_NO_GROUPBOX
#ifndef QT_NO_SLIDER
    case CC_Slider:
   	painter->save();
        if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect groove = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
            QRect handle = subControlRect(CC_Slider, option, SC_SliderHandle, widget);
            QRect ticks = subControlRect(CC_Slider, option, SC_SliderTickmarks, widget);
            
            bool ticksAbove = slider->tickPosition & QSlider::TicksAbove;
            bool ticksBelow = slider->tickPosition & QSlider::TicksBelow;

			orientation orient = slider->orientation == Qt::Horizontal?B_HORIZONTAL:B_VERTICAL;

			if (be_control_look != NULL) {
				QRect r = groove;
				rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
				rgb_color fill_color = tint_color(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT);
				uint32 flags = 0;            

		        BRect bRect(0.0f, 0.0f, option->rect.width() - 1,  option->rect.height() - 1);
				TemporarySurface surface(bRect);				
				
				surface.view()->SetHighColor(base);
				surface.view()->SetLowColor(base);
				surface.view()->FillRect(bRect);
				
				if ((option->subControls & SC_SliderGroove) && groove.isValid()) {
					r = groove;
					if (orient==B_HORIZONTAL)
						bRect = BRect(r.x(), r.y(), r.x()+r.width(), r.y()+r.height());
					else
						bRect = BRect(r.x(), r.y(), r.x()+r.width(), r.y()+r.height());
					be_control_look->DrawSliderBar(surface.view(), bRect, bRect, base, fill_color, flags, orient);
					painter->drawImage(r, surface.image());		
				}

				if (option->subControls & SC_SliderTickmarks) {
					int mlocation = B_HASH_MARKS_NONE;
					if (ticksAbove)
						mlocation |= B_HASH_MARKS_TOP;
					if (ticksBelow)
						mlocation |= B_HASH_MARKS_BOTTOM;
					int interval =  slider->tickInterval <= 0 ? 1 : slider->tickInterval;
					int num = 1 + ((slider->maximum-slider->minimum) / interval);
					int len = pixelMetric(PM_SliderLength, slider, widget) / 2;
					r = (orient == B_HORIZONTAL) ? option->rect.adjusted(len, -2, -len, 2) : option->rect.adjusted(0, len, 0, -len);
					bRect = BRect(r.x(), r.y(), r.x() + r.width(), r.y() + r.height());
					be_control_look->DrawSliderHashMarks(surface.view(), bRect, bRect, base, num, (hash_mark_location)mlocation, flags, orient);						
				}

				if (option->subControls & SC_SliderHandle ) {
					bRect = BRect(handle.x(), handle.y(), handle.x() + handle.width(), handle.y() + handle.height());
					be_control_look->DrawSliderTriangle(surface.view(), bRect, bRect, base, flags, orient);
				}
				painter->drawImage(slider->rect, surface.image());
			}
            painter->restore();
        }
        break;
#endif // QT_NO_SLIDER
#ifndef QT_NO_DIAL
    case CC_Dial:
        if (const QStyleOptionSlider *dial = qstyleoption_cast<const QStyleOptionSlider *>(option))
            QStyleHelper::drawDial(dial, painter);
        break;
#endif // QT_NO_DIAL
        default:
            QProxyStyle::drawComplexControl(control, option, painter, widget);
        break;
    }
}

/*!
  \reimp
*/
int QHaikuStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    int ret = -1;
    switch (metric) {
    case PM_ToolTipLabelFrameWidth:
        ret = 2;
        break;
    case PM_ButtonDefaultIndicator:
        ret = 2;
        break;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
        ret = 0;
        break;
    case PM_MessageBoxIconSize:
        ret = 32;
        break;
    case PM_ListViewIconSize:
        ret = 16;
        break;
    case PM_DialogButtonsSeparator:
        ret = 6;
        break;
    case PM_SplitterWidth:
        ret = 10;
        break;
    case PM_ScrollBarSliderMin:
        ret = 26;
        break;
    case PM_MenuPanelWidth: //menu framewidth
        ret = 1;
        break;
    case PM_TitleBarHeight:
        ret = 24 + 5;
        break;
    case PM_ScrollBarExtent:
        ret = B_V_SCROLL_BAR_WIDTH;
        break;
    case PM_SliderThickness:
        ret = 8;
        break;
    case PM_SliderLength:
        ret = 12;
        break;
    case PM_DockWidgetTitleMargin:
        ret = 1;
        break;
    case PM_MenuBarVMargin:
        ret = 0;
        break;
    case PM_DefaultFrameWidth:
        ret = 2;
        break;
    case PM_MdiSubWindowFrameWidth:
		ret = 5;
		break;
    case PM_SpinBoxFrameWidth:
        ret = 3;
        break;
    case PM_MenuBarItemSpacing:
        ret = 0;
        break;
    case PM_MenuBarHMargin:
        ret = 0;
        break;
    case PM_ToolBarHandleExtent:
        ret = 9;
        break;
    case PM_ToolBarItemSpacing:
        ret = 2;
        break;
    case PM_ToolBarFrameWidth:
        ret = 0;
        break;
    case PM_ToolBarItemMargin:
        ret = 1;
        break;
    case PM_SmallIconSize:
        ret = 16;
        break;
    case PM_ButtonIconSize:
        ret = 16;
        break;
    case PM_MenuVMargin:
    case PM_MenuHMargin:
        ret = 0;
        break;
    case PM_DockWidgetTitleBarButtonMargin:
        ret = 4;
        break;
    case PM_MaximumDragDistance:
        return -1;
    case PM_TabCloseIndicatorWidth:
    case PM_TabCloseIndicatorHeight:
        return 18;
    case PM_TabBarTabVSpace:
        return 7;
    case PM_TabBarTabHSpace:
        return 14;
    case PM_TabBarTabShiftVertical:
        return 0;
    case PM_TabBarTabShiftHorizontal:
        return 0;
	case PM_TabBarScrollButtonWidth:
		return 18;
	case PM_TabBar_ScrollButtonOverlap:
		return 0;
    case PM_IndicatorWidth:
    	return 18;
    case PM_IndicatorHeight:
    	return 18;
//    case PM_TabBarTabOverlap:
//        return 50;
//    case PM_TabBarBaseOverlap:
//        return 0;
    default:
        break;
    }

    return ret != -1 ? ret : QProxyStyle::pixelMetric(metric, option, widget);
}

/*!
  \reimp
*/
QSize QHaikuStyle::sizeFromContents(ContentsType type, const QStyleOption *option,
                                        const QSize &size, const QWidget *widget) const
{
    QSize newSize = QProxyStyle::sizeFromContents(type, option, size, widget);
    switch (type) {
    case CT_PushButton:
        if (const QStyleOptionButton *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            if (!btn->text.isEmpty()) {
            	newSize += QSize(7, 2);
				if(newSize.width() < 64)
                	newSize.setWidth(64);
            }
            if (!btn->icon.isNull() && btn->iconSize.height() > 16)
                newSize -= QSize(0, 2);
        }
        break;
#ifndef QT_NO_GROUPBOX
    case CT_GroupBox:
        // Since we use a bold font we have to recalculate base width
        if (const QGroupBox *gb = qobject_cast<const QGroupBox*>(widget)) {
            QFont font = gb->font();
            font.setBold(true);
            QFontMetrics metrics(font);
            int baseWidth = metrics.width(gb->title()) + metrics.width(QLatin1Char(' '));
            if (gb->isCheckable()) {
                baseWidth += proxy()->pixelMetric(QStyle::PM_IndicatorWidth, option, widget);
                baseWidth += proxy()->pixelMetric(QStyle::PM_CheckBoxLabelSpacing, option, widget);
            }
            newSize.setWidth(qMax(baseWidth, newSize.width()));
        }
        newSize += QSize(0, 1);
        break;
#endif //QT_NO_GROUPBOX
    case CT_RadioButton:
    	newSize += QSize(1, 1);
    	break;
    case CT_CheckBox:
        newSize += QSize(1, 1);
        break;
    case CT_ToolButton:
#ifndef QT_NO_TOOLBAR
        if (widget && qobject_cast<QToolBar *>(widget->parentWidget()))
            newSize += QSize(4, 6);
#endif // QT_NO_TOOLBAR
        break;
    case CT_Slider:
    	if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
    		if (slider->orientation == Qt::Horizontal)
    			newSize += QSize(0, 8);
    		else
    			newSize += QSize(6, 0);
    	}
    	break;
    case CT_SpinBox:
        newSize += QSize(0, -2);
        break;
    case CT_ComboBox:
        newSize = sizeFromContents(CT_PushButton, option, size, widget);
        newSize.rwidth() += 28;
        newSize.rheight() += 8;
        break;
    case CT_LineEdit:
        break;
    case CT_MenuBar:
        newSize += QSize(0, 1);
        break;
    case CT_MenuBarItem:
        newSize += QSize(8, -2);
        break;
    case CT_MenuItem:
        if (const QStyleOptionMenuItem *menuItem = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            if (menuItem->menuItemType == QStyleOptionMenuItem::Separator) {
                if (!menuItem->text.isEmpty()) {
                    newSize.setHeight(menuItem->fontMetrics.height());
                }
            }
#ifndef QT_NO_COMBOBOX
            else if (!menuItem->icon.isNull()) {
                if (const QComboBox *combo = qobject_cast<const QComboBox*>(widget)) {
                    newSize.setHeight(qMax(combo->iconSize().height() + 2, newSize.height()));
                }
            }
#endif // QT_NO_COMBOBOX
			if (menuItem->menuItemType == QStyleOptionMenuItem::Separator)
				newSize += QSize(-2, 8);
			else
				newSize += QSize(-2, -4);
        }
        break;
    case CT_SizeGrip:
        newSize += QSize(2, 2);
        break;
    case CT_MdiControls:
       /* if (const QStyleOptionComplex *styleOpt = qstyleoption_cast<const QStyleOptionComplex *>(option)) {
            int width = 0;
            if (styleOpt->subControls & SC_MdiMinButton)
                width += 19 + 1;
            if (styleOpt->subControls & SC_MdiNormalButton)
                width += 19 + 1;
            if (styleOpt->subControls & SC_MdiCloseButton)
                width += 19 + 1;
            newSize = QSize(width, 19);
        } else {
            newSize = QSize(60, 19);
        }*/
        break;
    default:
        break;
    }
    return newSize;
}

/*!
  \reimp
*/
void QHaikuStyle::polish(QApplication *app)
{
    QProxyStyle::polish(app);
}

/*!
  \reimp
*/
void QHaikuStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (qobject_cast<QAbstractButton*>(widget)
#ifndef QT_NO_COMBOBOX
        || qobject_cast<QComboBox *>(widget)
#endif
#ifndef QT_NO_PROGRESSBAR
        || qobject_cast<QProgressBar *>(widget)
#endif
#ifndef QT_NO_SCROLLBAR
        || qobject_cast<QScrollBar *>(widget)
#endif
#ifndef QT_NO_SPLITTER
        || qobject_cast<QSplitterHandle *>(widget)
#endif
        || qobject_cast<QAbstractSlider *>(widget)
#ifndef QT_NO_SPINBOX
        || qobject_cast<QAbstractSpinBox *>(widget)
#endif
        || (widget->inherits("QDockSeparator"))
        || (widget->inherits("QDockWidgetSeparator"))
        ) {
        widget->setAttribute(Qt::WA_Hover, true);
    }
#ifndef QT_NO_PROGRESSBAR
    if (qobject_cast<QProgressBar *>(widget))
        widget->installEventFilter(this);
#endif
    if (widget->inherits("QMdiSubWindow"))
        widget->installEventFilter(this);
}

/*!
  \reimp
*/
void QHaikuStyle::polish(QPalette &pal)
{
    QProxyStyle::polish(pal);
    //this is a workaround for some themes such as Human, where the contrast
    //between text and background is too low.
    QColor highlight = pal.highlight().color();
    QColor highlightText = pal.highlightedText().color();
    if (qAbs(qGray(highlight.rgb()) - qGray(highlightText.rgb())) < 150) {
        if (qGray(highlightText.rgb()) < 128)
            pal.setBrush(QPalette::Highlight, highlight.lighter(145));
    }
}

/*!
  \reimp
*/
void QHaikuStyle::unpolish(QWidget *widget)
{
    QProxyStyle::unpolish(widget);
    if (qobject_cast<QAbstractButton*>(widget)
#ifndef QT_NO_COMBOBOX
        || qobject_cast<QComboBox *>(widget)
#endif
#ifndef QT_NO_PROGRESSBAR
        || qobject_cast<QProgressBar *>(widget)
#endif
#ifndef QT_NO_SCROLLBAR
        || qobject_cast<QScrollBar *>(widget)
#endif
#ifndef QT_NO_SPLITTER
        || qobject_cast<QSplitterHandle *>(widget)
#endif
        || qobject_cast<QAbstractSlider *>(widget)
#ifndef QT_NO_SPINBOX
        || qobject_cast<QAbstractSpinBox *>(widget)
#endif
        || (widget->inherits("QDockSeparator"))
        || (widget->inherits("QDockWidgetSeparator"))
        ) {
        widget->setAttribute(Qt::WA_Hover, false);
    }
#ifndef QT_NO_PROGRESSBAR
    if (qobject_cast<QProgressBar *>(widget))
        widget->removeEventFilter(this);
#endif
	if (widget->inherits("QMdiSubWindow"))
        widget->removeEventFilter(this);
}

/*!
  \reimp
*/
void QHaikuStyle::unpolish(QApplication *app)
{
    QProxyStyle::unpolish(app);
}

/*!
  \reimp
*/
bool QHaikuStyle::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Timer: {
#ifndef QT_NO_PROGRESSBAR
        QTimerEvent *timerEvent = reinterpret_cast<QTimerEvent *>(event);
        if (timerEvent->timerId() == animateTimer) {
            Q_ASSERT(progressAnimationFps > 0);
            animateStep = startTime.elapsed() / (1000 / progressAnimationFps);
            foreach (QProgressBar *bar, animatedProgressBars)
                bar->update();
        }
#endif // QT_NO_PROGRESSBAR
        event->ignore();
    }
    default:
        break;
    }

    return QProxyStyle::event(event);
}

/*!
  \reimp
*/
bool QHaikuStyle::eventFilter(QObject *o, QEvent *e)
{
    switch (e->type())
    {    	
    case QEvent::WindowTitleChange:
    	if(QMdiSubWindow* w = qobject_cast<QMdiSubWindow*>(o)) {
			QStyleHintReturnMask mask;

    		QStyleOptionTitleBar titleBarOptions;

    		titleBarOptions.initFrom(w);
		    titleBarOptions.subControls = QStyle::SC_All;
    		titleBarOptions.titleBarFlags = w->windowFlags();
    		titleBarOptions.titleBarState = w->windowState();
			titleBarOptions.state = QStyle::State_Active;
        	titleBarOptions.titleBarState = QStyle::State_Active;
		    titleBarOptions.rect = QRect(0, 0, w->width(), w->height());
		    titleBarOptions.version = 2;	//TODO: ugly dirty hack for 10px mask error

    		if (!w->windowTitle().isEmpty()) {
		        titleBarOptions.text = w->windowTitle();
		        QFont font = QApplication::font("QMdiSubWindowTitleBar");
		        font.setBold(true);
        		titleBarOptions.fontMetrics = QFontMetrics(font);
		    }

			if (styleHint(SH_WindowFrame_Mask, &titleBarOptions, w, &mask))
				w->setMask(mask.region);

			w->repaint();
    	}
    	break;
#ifndef QT_NO_PROGRESSBAR
    case QEvent::StyleChange:
    case QEvent::Paint:
    case QEvent::Show:
        if (QProgressBar *bar = qobject_cast<QProgressBar *>(o)) {
            // Animation by timer for progress bars that have their min and
            // max values the same
            if (bar->minimum() == bar->maximum())
                startProgressAnimation(this, bar);
            else
                stopProgressAnimation(this, bar);
        }
        break;
    case QEvent::Destroy:
    case QEvent::Hide:
        // Do static_cast because there is no type info when getting
        // the destroy event. We know that it is a QProgressBar, since
        // we only install a widget event filter for QScrollBars.
        stopProgressAnimation(this, static_cast<QProgressBar *>(o));
        break;
#endif // QT_NO_PROGRESSBAR
    default:
        break;
    }
    return QProxyStyle::eventFilter(o, e);
}

void QHaikuStyle::startProgressAnimation(QObject *o, QProgressBar *bar)
{
    if (!animatedProgressBars.contains(bar)) {
        animatedProgressBars << bar;
        if (!animateTimer) {
            Q_ASSERT(progressAnimationFps > 0);
            animateStep = 0;
            startTime.start();
            animateTimer = o->startTimer(1000 / progressAnimationFps);
        }
    }
}

void QHaikuStyle::stopProgressAnimation(QObject *o, QProgressBar *bar)
{
    if (!animatedProgressBars.isEmpty()) {
        animatedProgressBars.removeOne(bar);
        if (animatedProgressBars.isEmpty() && animateTimer) {
            o->killTimer(animateTimer);
            animateTimer = 0;
        }
    }
}

/*!
  \reimp
*/
QRect QHaikuStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                       SubControl subControl, const QWidget *widget) const
{
    QRect rect = QProxyStyle::subControlRect(control, option, subControl, widget);

    switch (control) {
#ifndef QT_NO_SLIDER
    case CC_Slider:
        if (const QStyleOptionSlider *slider = qstyleoption_cast<const QStyleOptionSlider *>(option)) {        	
            //int tickSize = proxy()->pixelMetric(PM_SliderTickmarkOffset, option, widget);

            QPoint grooveCenter = slider->rect.center();
            if (slider->orientation == Qt::Horizontal)
                rect.setHeight(6);
            else
                rect.setWidth(6);

            switch (subControl) {
            case SC_SliderHandle: {
                if (slider->orientation == Qt::Horizontal) {
                    rect.setHeight(proxy()->pixelMetric(PM_SliderThickness));
                    rect.setWidth(proxy()->pixelMetric(PM_SliderLength));                    
                    int centerY = grooveCenter.y() + 1;
                    rect.moveTop(centerY);
                } else {
                    rect.setWidth(proxy()->pixelMetric(PM_SliderThickness));
                    rect.setHeight(proxy()->pixelMetric(PM_SliderLength));
                    int centerX = grooveCenter.x() + 1;
                    rect.moveLeft(centerX);
                }
            }
            	break;
            case SC_SliderGroove: {
                rect.moveCenter(grooveCenter);
                break;
            }
            default:
                break;
            }
        }
        break;
#endif // QT_NO_SLIDER
    case CC_ScrollBar:
        break;
#ifndef QT_NO_SPINBOX
    case CC_SpinBox:
        if (const QStyleOptionSpinBox *spinbox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            QSize bs;
            int center = spinbox->rect.height() / 2;
            int fw = spinbox->frame ? proxy()->pixelMetric(PM_SpinBoxFrameWidth, spinbox, widget) : 0;
            int y = fw;
            bs.setHeight(qMax(8, spinbox->rect.height()/2 - y));
            bs.setWidth(15);
            int x, lx, rx;
            x = spinbox->rect.width() - y - bs.width() + 2;
            lx = fw;
            rx = x - fw;
            switch (subControl) {
            case SC_SpinBoxUp:
                if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons)
                    return QRect();
                rect = QRect(x, fw, bs.width(), center - fw);
                break;
            case SC_SpinBoxDown:
                if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons)
                    return QRect();

                rect = QRect(x, center, bs.width(), spinbox->rect.bottom() - center - fw + 1);
                break;
            case SC_SpinBoxEditField:
                if (spinbox->buttonSymbols == QAbstractSpinBox::NoButtons) {
                    rect = QRect(lx, fw, spinbox->rect.width() - 2*fw, spinbox->rect.height() - 2*fw);
                } else {
                    rect = QRect(lx, fw, rx - qMax(fw - 1, 0), spinbox->rect.height() - 2*fw);
                }
                break;
            case SC_SpinBoxFrame:
                rect = spinbox->rect;
            default:
                break;
            }
            rect = visualRect(spinbox->direction, spinbox->rect, rect);
        }
        break;
#endif // Qt_NO_SPINBOX
#ifndef QT_NO_GROUPBOX
    case CC_GroupBox:
        if (const QStyleOptionGroupBox * groupBox = qstyleoption_cast<const QStyleOptionGroupBox *>(option)) {
            rect = option->rect;
            int topMargin = 10;
            int topHeight = 14;
            QRect frameRect = rect;
            frameRect.setTop(topMargin);

            if (subControl == SC_GroupBoxFrame)
                return frameRect;
            else if (subControl == SC_GroupBoxContents) {
                int margin = 1;
                int leftMarginExtension = 8;
                return frameRect.adjusted(leftMarginExtension + margin, margin + topHeight, -margin, -margin);
            }

            QFontMetrics fontMetrics = option->fontMetrics;
            if (qobject_cast<const QGroupBox *>(widget)) {
                //Prepare metrics for a bold font
                QFont font = widget->font();
                font.setBold(true);
                fontMetrics = QFontMetrics(font);
            } else if (QStyleHelper::isInstanceOf(groupBox->styleObject, QAccessible::Grouping)) {
                QVariant var = groupBox->styleObject->property("font");
                if (var.isValid() && var.canConvert<QFont>()) {
                    QFont font = var.value<QFont>();
                    font.setBold(true);
                    fontMetrics = QFontMetrics(font);
                }
            }

            QSize textRect = fontMetrics.boundingRect(groupBox->text).size() + QSize(1, 1);
            int indicatorWidth = proxy()->pixelMetric(PM_IndicatorWidth, option, widget);
            int indicatorHeight = proxy()->pixelMetric(PM_IndicatorHeight, option, widget);

            if (subControl == SC_GroupBoxCheckBox) {
                rect.setWidth(indicatorWidth);
                rect.setHeight(indicatorHeight);
                rect.moveTop((textRect.height() - indicatorHeight) / 2);
				rect.adjust(10, 2, 10, 2);
            } else if (subControl == SC_GroupBoxLabel) {
            	rect.adjust(10, 2, 10, 2);
                if (groupBox->subControls & SC_GroupBoxCheckBox) {
                    rect.adjust(indicatorWidth, 0, 0, 0);
                }
                rect.setSize(textRect);
            }
            rect = visualRect(option->direction, option->rect, rect);
        }

        return rect;
#ifndef QT_NO_COMBOBOX
    case CC_ComboBox:
        switch (subControl) {
        case SC_ComboBoxArrow:
            rect = visualRect(option->direction, option->rect, rect);
            rect.setRect(rect.right() - 9, rect.top() - 2,
                         10, rect.height() + 4);
            rect = visualRect(option->direction, option->rect, rect);
            break;
        case SC_ComboBoxEditField: {
            int frameWidth = proxy()->pixelMetric(PM_DefaultFrameWidth);
            rect = visualRect(option->direction, option->rect, rect);
            rect.setRect(option->rect.left() + frameWidth, option->rect.top() + frameWidth + 1,
                         option->rect.width() - 10 - 2 * frameWidth,
                         option->rect.height() - 2 * frameWidth - 2);
            if (const QStyleOptionComboBox *box = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
                if (!box->editable) {
                    rect.adjust(6, 0, 0, 0);
                    if (box->state & (State_Sunken | State_On))
                        rect.translate(1, 1);
                }
            }
            rect = visualRect(option->direction, option->rect, rect);
            break;
        }
        default:
            break;
        }
        break;
#endif // QT_NO_COMBOBOX
#endif //QT_NO_GROUPBOX
        case CC_TitleBar:
        if (const QStyleOptionTitleBar *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
            SubControl sc = subControl;
            QRect &ret = rect;
            const int controlWidthMargin = 2;
            const int controlHeight = 15 ;
            const int delta = controlHeight + controlWidthMargin;
            int offset = 0;

            bool isMinimized = tb->titleBarState & Qt::WindowMinimized;
            bool isMaximized = tb->titleBarState & Qt::WindowMaximized;

			QFontMetrics fontMetrics = option->fontMetrics;
			if (widget) {
				QFont font = widget->font();
				font.setBold(true);
				fontMetrics = QFontMetrics(font);
			}
			
			int textWidth = mdiTabWidthMin;
            if (!tb->text.isEmpty()) {
				textWidth = fontMetrics.width(tb->text) + 20;
	            if (tb->version == 2)		//TODO: ugly dirty hack for 10px mask error
    	        	textWidth -= 10;
				if (textWidth < mdiTabWidthMin)
					textWidth = mdiTabWidthMin;
            }

            int tabWidth = textWidth + mdiTabTextMarginLeft +  mdiTabTextMarginRight;

            if (tabWidth > tb->rect.width())
            	tabWidth = tb->rect.width();
            	
            switch (sc) {
            case SC_TitleBarLabel:
                if (tb->titleBarFlags & (Qt::WindowTitleHint | Qt::WindowSystemMenuHint)) {
                    ret = tb->rect;
                    ret.adjust(0, 2, 0, -2);
                    ret.setWidth(tabWidth - (mdiTabTextMarginLeft + mdiTabTextMarginRight));
                    ret.adjust(mdiTabTextMarginLeft, 0, mdiTabTextMarginLeft, -1);
                } else
                	ret = QRect();
                break;                
            case SC_TitleBarNormalButton:
                if ( (isMinimized && (tb->titleBarFlags & Qt::WindowMinimizeButtonHint)) ||
                	 (isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint))) {
                    ret = tb->rect;
                    ret.adjust(0, 2, 0, -2);
                    ret.setRect(ret.left() + tabWidth - mdiTabTextMarginRight,  tb->rect.top() + 5, controlHeight, controlHeight);
                }
                break;
            case SC_TitleBarMaxButton:
                if (!isMaximized && (tb->titleBarFlags & Qt::WindowMaximizeButtonHint)) {
                    ret = tb->rect;
                    ret.adjust(0, 2, 0, -2);
                    ret.setRect(ret.left() + tabWidth - mdiTabTextMarginRight,  tb->rect.top() + 5, controlHeight, controlHeight);
                }
				break;
            case SC_TitleBarContextHelpButton:
            case SC_TitleBarMinButton:
            case SC_TitleBarShadeButton:
            case SC_TitleBarUnshadeButton:
            case SC_TitleBarSysMenu:
            	ret = QRect();
                break;
            case SC_TitleBarCloseButton:
                ret.setRect(tb->rect.left() + 5, tb->rect.top() + 5, controlHeight, controlHeight);
                break;
            default:
                break;
            }
            ret = visualRect(tb->direction, tb->rect, ret);
        }
        break;
    default:
        break;
    }

    return rect;
}


/*!
  \reimp
*/
QRect QHaikuStyle::itemPixmapRect(const QRect &r, int flags, const QPixmap &pixmap) const
{
    return QProxyStyle::itemPixmapRect(r, flags, pixmap);
}

/*!
  \reimp
*/
void QHaikuStyle::drawItemPixmap(QPainter *painter, const QRect &rect,
                            int alignment, const QPixmap &pixmap) const
{
    QProxyStyle::drawItemPixmap(painter, rect, alignment, pixmap);
}

/*!
  \reimp
*/
QStyle::SubControl QHaikuStyle::hitTestComplexControl(ComplexControl cc, const QStyleOptionComplex *opt,
                              const QPoint &pt, const QWidget *w) const
{
    return QProxyStyle::hitTestComplexControl(cc, opt, pt, w);
}

/*!
  \reimp
*/
QPixmap QHaikuStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap,
                                        const QStyleOption *opt) const
{
    return QProxyStyle::generatedIconPixmap(iconMode, pixmap, opt);
}

/*!
  \reimp
*/
int QHaikuStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                               QStyleHintReturn *returnData) const
{
    int ret = 0;
    switch (hint) {
    case SH_ScrollBar_MiddleClickAbsolutePosition:
        ret = int(true);
        break;
    case SH_EtchDisabledText:
        ret = int(false);
        break;
	case SH_UnderlineShortcut:
        ret = int(false);
        break;
    case SH_Menu_AllowActiveAndDisabled:
        ret = false;
        break;
    case SH_MainWindow_SpaceBelowMenuBar:
        ret = 0;
        break;
    case SH_MenuBar_MouseTracking:
        ret = 1;
        break;
    case SH_TitleBar_AutoRaise:
        ret = 1;
        break;
    case SH_TitleBar_NoBorder:
        ret = 1;
        break;
    case SH_LineEdit_PasswordCharacter:
        ret = 0x00B7;
        break;
    case SH_ItemView_ShowDecorationSelected:
        ret = true;
        break;
    case SH_Table_GridLineColor:
        if (option) {
            ret = option->palette.background().color().darker(120).rgb();
            break;
        }
    case SH_ComboBox_Popup:
        if (const QStyleOptionComboBox *cmb = qstyleoption_cast<const QStyleOptionComboBox *>(option))
            ret = !cmb->editable;
        else
            ret = 0;
        break;
    case SH_WindowFrame_Mask:
        ret = 1;
        if (QStyleHintReturnMask *mask = qstyleoption_cast<QStyleHintReturnMask *>(returnData)) {
        	if (const QStyleOptionTitleBar *titleBar = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
        		QRect textRect = proxy()->subControlRect(CC_TitleBar, titleBar, SC_TitleBarLabel, widget);
           		int frameWidth = proxy()->pixelMetric(PM_MdiSubWindowFrameWidth);
            	int tabHeight = pixelMetric(PM_TitleBarHeight, titleBar, widget) - frameWidth;
				int tabWidth = textRect.width() + mdiTabTextMarginLeft + mdiTabTextMarginRight;
           		mask->region = option->rect;
           		mask->region -= QRect(tabWidth, option->rect.top(), option->rect.width()-tabWidth, tabHeight);
        	}
        }
        break;
    case SH_MessageBox_TextInteractionFlags:
        ret = Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse;
        break;
    case SH_DialogButtonBox_ButtonsHaveIcons:
        ret = false;
        break;
    case SH_MessageBox_CenterButtons:
        ret = false;
        break;
#ifndef QT_NO_WIZARD
    case SH_WizardStyle:
        ret = QWizard::ClassicStyle;
        break;
#endif
    case SH_ItemView_ArrowKeysNavigateIntoChildren:
        ret = false;
        break;
    case SH_Menu_SubMenuPopupDelay:
        ret = 225; // default from GtkMenu
        break;
    default:
        ret = QProxyStyle::styleHint(hint, option, widget, returnData);
        break;
    }
    return ret;
}

/*! \reimp */
QRect QHaikuStyle::subElementRect(SubElement sr, const QStyleOption *opt, const QWidget *w) const
{
    QRect r = QProxyStyle::subElementRect(sr, opt, w);
    switch (sr) {
    case SE_TabWidgetTabBar:
//        r.adjust(10, 0, 10, 0);	//Tab shift
        break;
    case SE_TabBarTabLeftButton:
    case SE_TabBarTabRightButton:
    	{
    		if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
    			bool selected = tab->state & State_Selected;
    			if (!selected)
    				r.adjust(0, 2, 0, 2);
    		}
    	}
    	break;
    case SE_PushButtonFocusRect:
        r.adjust(0, 1, 0, -1);
        break;
    case SE_DockWidgetTitleBarText: {
        const QStyleOptionDockWidget *dwOpt
            = qstyleoption_cast<const QStyleOptionDockWidget*>(opt);
        bool verticalTitleBar = dwOpt && dwOpt->verticalTitleBar;
        if (verticalTitleBar) {
            r.adjust(0, 0, 0, -4);
        } else {
            if (opt->direction == Qt::LeftToRight)
                r.adjust(4, 0, 0, 0);
            else
                r.adjust(0, 0, -4, 0);
        }

        break;
    }
    case SE_ProgressBarContents:
        r = subElementRect(SE_ProgressBarGroove, opt, w);
        break;
    case SE_TabBarTabText:
    	if (const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab *>(opt)) {
   			bool selected = tab->state & State_Selected;
    		if( tab->shape == QTabBar::TriangularSouth || tab->shape == QTabBar::RoundedSouth)
    			r.adjust(0, 0, 0, -5);
    		else
    			r.adjust(0, 5, 0, 0);
   			if (selected) {
   				if( tab->shape == QTabBar::TriangularNorth || tab->shape == QTabBar::RoundedNorth)
	   				r.adjust(-1, -1, -1, -1);
   			}
    	}
    	break;    	
    default:
        break;
    }
    return r;
}

/*!
    \reimp
*/
QIcon QHaikuStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option,
                                     const QWidget *widget) const
{
    return QProxyStyle::standardIcon(standardIcon, option, widget);
}

/*!
 \reimp
 */
QPixmap QHaikuStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *opt,
                                      const QWidget *widget) const
{
    QPixmap pixmap;
	uint32 atype = B_EMPTY_ALERT;

	switch (standardPixmap) {
    case SP_MessageBoxQuestion:
    	atype = B_IDEA_ALERT;
    	break;
    case SP_MessageBoxInformation:
    	atype = B_INFO_ALERT;
    	break;
    case SP_MessageBoxCritical:
    	atype = B_STOP_ALERT;
    	break;
    case SP_MessageBoxWarning:
    	atype = B_WARNING_ALERT;
        break;
    default:
        break;        
	}

    if(	atype != B_EMPTY_ALERT ) {
    	int size = pixelMetric(PM_MessageBoxIconSize, opt, widget);
   		QImage image = get_haiku_alert_icon(atype, size);
   		if(!image.isNull()) {
			pixmap = QPixmap::fromImage(image);
   			return pixmap;
   		}
   	}

#ifndef QT_NO_IMAGEFORMAT_XPM
    switch (standardPixmap) {
    case SP_TitleBarNormalButton:
        return QPixmap((const char **)dock_widget_restore_xpm);
    case SP_TitleBarMinButton:
        return QPixmap((const char **)workspace_minimize);
    case SP_TitleBarCloseButton:
    case SP_DockWidgetCloseButton:
        return QPixmap((const char **)dock_widget_close_xpm);

    default:
        break;
    }
#endif //QT_NO_IMAGEFORMAT_XPM


    return QProxyStyle::standardPixmap(standardPixmap, opt, widget);
}

QT_END_NAMESPACE
