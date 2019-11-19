// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/frame_header.h"

#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/cpp/window_properties.h"
#include "base/logging.h"  // DCHECK
#include "ui/base/class_property.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"
#include "ui/views/window/vector_icons/vector_icons.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::FrameHeader*)

namespace ash {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(FrameHeader*, kFrameHeaderKey, nullptr)

// Returns the available bounds for the header's title given the views to the
// left and right of the title, and the font used. |left_view| should be null
// if there is no view to the left of the title.
gfx::Rect GetAvailableTitleBounds(const views::View* left_view,
                                  const views::View* right_view,
                                  int header_height) {
  // Space between the title text and the caption buttons.
  constexpr int kTitleCaptionButtonSpacing = 5;
  // Space between window icon and title text.
  constexpr int kTitleIconOffsetX = 5;
  // Space between window edge and title text, when there is no icon.
  constexpr int kTitleNoIconOffsetX = 8;

  const int x = left_view ? left_view->bounds().right() + kTitleIconOffsetX
                          : kTitleNoIconOffsetX;
  const int title_height = gfx::FontList().GetHeight();
  DCHECK_LE(right_view->height(), header_height);
  // We want to align the center points of the header and title vertically.
  // Note that we can't just do (header_height - title_height) / 2, since this
  // won't make the center points align perfectly vertically due to rounding.
  // Floor when computing the center of |header_height| and when computing the
  // center of the text.
  const int header_center_y = header_height / 2;
  const int title_center_y = title_height / 2;
  const int y = std::max(0, header_center_y - title_center_y);
  const int width =
      std::max(0, right_view->x() - kTitleCaptionButtonSpacing - x);
  return gfx::Rect(x, y, width, title_height);
}

// Returns true if the header for |widget| can animate to new visuals when the
// widget's activation changes. Returns false if the header should switch to
// new visuals instantaneously.
bool CanAnimateActivation(views::Widget* widget) {
  // Do not animate the header if the parent (e.g. the active desk container) is
  // already animating. All of the implementers of FrameHeader animate
  // activation by continuously painting during the animation. This gives the
  // parent's animation a slower frame rate.
  // TODO(sky): Expose a better way to determine this rather than assuming the
  // parent is a toplevel container.
  aura::Window* window = widget->GetNativeWindow();
  // TODO(sky): parent()->layer() is for mash until animations ported.
  if (!window || !window->parent() || !window->parent()->layer())
    return true;

  ui::LayerAnimator* parent_layer_animator =
      window->parent()->layer()->GetAnimator();
  return !parent_layer_animator->IsAnimatingProperty(
             ui::LayerAnimationElement::OPACITY) &&
         !parent_layer_animator->IsAnimatingProperty(
             ui::LayerAnimationElement::VISIBILITY);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, public:

// static
FrameHeader* FrameHeader::Get(views::Widget* widget) {
  return widget->GetNativeView()->GetProperty(kFrameHeaderKey);
}

FrameHeader::~FrameHeader() {
  if (target_widget_->GetNativeView())
    target_widget_->GetNativeView()->ClearProperty(kFrameHeaderKey);
}

int FrameHeader::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width();
}

void FrameHeader::PaintHeader(gfx::Canvas* canvas, Mode mode) {
  Mode old_mode = mode_;
  mode_ = mode;

  if (mode_ != old_mode) {
    UpdateCaptionButtonColors();

    if (!initial_paint_ && CanAnimateActivation(target_widget_)) {
      activation_animation_.SetSlideDuration(
          base::TimeDelta::FromMilliseconds(200));
      if (mode_ == MODE_ACTIVE)
        activation_animation_.Show();
      else
        activation_animation_.Hide();
    } else {
      if (mode_ == MODE_ACTIVE)
        activation_animation_.Reset(1);
      else
        activation_animation_.Reset(0);
    }
    initial_paint_ = false;
  }

  DoPaintHeader(canvas);
}

void FrameHeader::LayoutHeader() {
  LayoutHeaderInternal();
  // Default to the header height; owning code may override via
  // SetHeaderHeightForPainting().
  painted_height_ = GetHeaderHeight();
}

int FrameHeader::GetHeaderHeight() const {
  return caption_button_container_->height();
}

int FrameHeader::GetHeaderHeightForPainting() const {
  return painted_height_;
}

void FrameHeader::SetHeaderHeightForPainting(int height) {
  painted_height_ = height;
}

void FrameHeader::SchedulePaintForTitle() {
  view_->SchedulePaintInRect(view_->GetMirroredRect(GetTitleBounds()));
}

void FrameHeader::SetPaintAsActive(bool paint_as_active) {
  caption_button_container_->SetPaintAsActive(paint_as_active);
  if (back_button_)
    back_button_->set_paint_as_active(paint_as_active);
  UpdateCaptionButtonColors();
}

void FrameHeader::OnShowStateChanged(ui::WindowShowState show_state) {
  if (show_state == ui::SHOW_STATE_MINIMIZED)
    return;

  LayoutHeaderInternal();
}

void FrameHeader::SetLeftHeaderView(views::View* left_header_view) {
  left_header_view_ = left_header_view;
}

void FrameHeader::SetBackButton(views::FrameCaptionButton* back_button) {
  back_button_ = back_button;
  if (back_button_) {
    back_button_->SetBackgroundColor(GetCurrentFrameColor());
    back_button_->SetImage(views::CAPTION_BUTTON_ICON_BACK,
                           views::FrameCaptionButton::ANIMATE_NO,
                           kWindowControlBackIcon);
  }
}

views::FrameCaptionButton* FrameHeader::GetBackButton() const {
  return back_button_;
}

const CaptionButtonModel* FrameHeader::GetCaptionButtonModel() const {
  return caption_button_container_->model();
}

void FrameHeader::SetFrameTextOverride(
    const base::string16& frame_text_override) {
  frame_text_override_ = frame_text_override;
  SchedulePaintForTitle();
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void FrameHeader::AnimationProgressed(const gfx::Animation* animation) {
  view_->SchedulePaintInRect(GetPaintedBounds());
}

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, protected:

FrameHeader::FrameHeader(views::Widget* target_widget, views::View* view)
    : views::AnimationDelegateViews(view),
      target_widget_(target_widget),
      view_(view) {
  DCHECK(target_widget);
  DCHECK(view);
  target_widget_->GetNativeView()->SetProperty(kFrameHeaderKey, this);
}

gfx::Rect FrameHeader::GetPaintedBounds() const {
  return gfx::Rect(view_->width(), painted_height_);
}

void FrameHeader::UpdateCaptionButtonColors() {
  const SkColor frame_color = GetCurrentFrameColor();
  caption_button_container_->SetBackgroundColor(frame_color);
  if (back_button_)
    back_button_->SetBackgroundColor(frame_color);
}

void FrameHeader::PaintTitleBar(gfx::Canvas* canvas) {
  base::string16 text = frame_text_override_;
  views::WidgetDelegate* target_widget_delegate =
      target_widget_->widget_delegate();
  if (text.empty() && target_widget_delegate &&
      target_widget_delegate->ShouldShowWindowTitle()) {
    text = target_widget_delegate->GetWindowTitle();
  }

  if (!text.empty()) {
    int flags = gfx::Canvas::NO_SUBPIXEL_RENDERING;
    if (target_widget_delegate->ShouldCenterWindowTitleText())
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
    canvas->DrawStringRectWithFlags(text, gfx::FontList(), GetTitleColor(),
                                    view_->GetMirroredRect(GetTitleBounds()),
                                    flags);
  }
}

void FrameHeader::SetCaptionButtonContainer(
    FrameCaptionButtonContainerView* caption_button_container) {
  caption_button_container_ = caption_button_container;
  caption_button_container_->SetButtonImage(views::CAPTION_BUTTON_ICON_MINIMIZE,
                                            views::kWindowControlMinimizeIcon);
  caption_button_container_->SetButtonImage(views::CAPTION_BUTTON_ICON_MENU,
                                            kWindowControlMenuIcon);
  caption_button_container_->SetButtonImage(views::CAPTION_BUTTON_ICON_CLOSE,
                                            views::kWindowControlCloseIcon);
  caption_button_container_->SetButtonImage(
      views::CAPTION_BUTTON_ICON_LEFT_SNAPPED, kWindowControlLeftSnappedIcon);
  caption_button_container_->SetButtonImage(
      views::CAPTION_BUTTON_ICON_RIGHT_SNAPPED, kWindowControlRightSnappedIcon);

  // Perform layout to ensure the container height is correct.
  LayoutHeaderInternal();
}

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, private:

void FrameHeader::LayoutHeaderInternal() {
  bool use_zoom_icons = caption_button_container()->model()->InZoomMode();
  const gfx::VectorIcon& restore_icon = use_zoom_icons
                                            ? kWindowControlDezoomIcon
                                            : views::kWindowControlRestoreIcon;
  const gfx::VectorIcon& maximize_icon =
      use_zoom_icons ? kWindowControlZoomIcon
                     : views::kWindowControlMaximizeIcon;
  const gfx::VectorIcon& icon =
      target_widget_->IsMaximized() || target_widget_->IsFullscreen()
          ? restore_icon
          : maximize_icon;
  caption_button_container()->SetButtonImage(
      views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE, icon);

  caption_button_container()->SetButtonSize(
      views::GetCaptionButtonLayoutSize(GetButtonLayoutSize()));

  const gfx::Size caption_button_container_size =
      caption_button_container()->GetPreferredSize();
  caption_button_container()->SetBounds(
      view_->width() - caption_button_container_size.width(), 0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  caption_button_container()->Layout();

  int origin = 0;
  if (back_button_) {
    gfx::Size size = back_button_->GetPreferredSize();
    back_button_->SetBounds(0, 0, size.width(),
                            caption_button_container_size.height());
    origin = back_button_->bounds().right();
  }

  if (left_header_view_) {
    // Vertically center the left header view (typically the window icon) with
    // respect to the caption button container.
    const gfx::Size icon_size(left_header_view_->GetPreferredSize());
    const int icon_offset_y = (GetHeaderHeight() - icon_size.height()) / 2;
    constexpr int kLeftViewXInset = 9;
    left_header_view_->SetBounds(kLeftViewXInset + origin, icon_offset_y,
                                 icon_size.width(), icon_size.height());
  }
}

gfx::Rect FrameHeader::GetTitleBounds() const {
  views::View* left_view = left_header_view_ ? left_header_view_ : back_button_;
  return GetAvailableTitleBounds(left_view, caption_button_container_,
                                 GetHeaderHeight());
}

}  // namespace ash
