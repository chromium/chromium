// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/frame_header.h"

#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/window_properties.h"
#include "base/logging.h"  // DCHECK
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/class_property.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
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

constexpr base::TimeDelta kFrameActivationAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

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

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, public:

// static
FrameHeader* FrameHeader::Get(views::Widget* widget) {
  return widget->GetNativeView()->GetProperty(kFrameHeaderKey);
}

FrameHeader::~FrameHeader() {
  auto* target_window = target_widget_->GetNativeView();
  if (target_window && target_window->GetProperty(kFrameHeaderKey) == this)
    target_window->ClearProperty(kFrameHeaderKey);
}

int FrameHeader::GetMinimumHeaderWidth() const {
  // Ensure we have enough space for the window icon and buttons. We allow
  // the title string to collapse to zero width.
  return GetTitleBounds().x() +
         caption_button_container_->GetMinimumSize().width();
}

// An invisible view that drives the frame's animation. This holds the animating
// layer as a layer beneath this view so that it's behind all other child layers
// of the window to avoid hiding their contents.
class FrameHeader::FrameAnimatorView : public views::View,
                                       public views::ViewObserver,
                                       public ui::ImplicitAnimationObserver {
 public:
  FrameAnimatorView(FrameHeader* frame_header, views::View* parent)
      : frame_header_(frame_header), parent_(parent) {
    SetPaintToLayer(ui::LAYER_NOT_DRAWN);
    parent_->AddChildViewAt(this, 0);
    parent_->AddObserver(this);
  }
  FrameAnimatorView(const FrameAnimatorView&) = delete;
  FrameAnimatorView& operator=(const FrameAnimatorView&) = delete;
  ~FrameAnimatorView() override {
    StopAnimation();
    // A child view should always be removed first.
    parent_->RemoveObserver(this);
  }

  void StartAnimation(base::TimeDelta duration) {
    if (layer_owner_) {
      // If animation is already running, just update the content of the new
      // layer.
      parent_->SchedulePaint();
      return;
    }
    aura::Window* window = frame_header_->target_widget()->GetNativeWindow();

    // Make sure the this view is at the bottom of root view's children.
    parent_->ReorderChildView(this, 0);

    std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
        std::make_unique<ui::LayerTreeOwner>(window->RecreateLayer());
    ui::Layer* old_layer = old_layer_owner->root();
    ui::Layer* new_layer = window->layer();
    new_layer->SetName(old_layer->name());
    old_layer->SetName(old_layer->name() + ":Old");
    old_layer->SetTransform(gfx::Transform());

    layer_owner_ = std::move(old_layer_owner);

    AddLayerBeneathView(old_layer);

    // The old layer is on top and should fade out.
    old_layer->SetOpacity(1.f);
    new_layer->SetOpacity(1.f);
    {
      ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      settings.AddObserver(this);
      settings.SetTransitionDuration(duration);
      old_layer->SetOpacity(0.f);
      settings.SetTweenType(gfx::Tween::EASE_OUT);
    }
  }

  // views::Views:
  const char* GetClassName() const override { return "FrameAnimatorView"; }
  std::unique_ptr<ui::Layer> RecreateLayer() override {
    // A layer may be recreated for another animation (maximize/restore).
    // Just cancel the animation if that happens during animation.
    StopAnimation();
    return views::View::RecreateLayer();
  }

  // ViewObserver::
  void OnChildViewReordered(views::View* observed_view,
                            views::View* child) override {
    // Stop animation if the child view order has changed during animation.
    StopAnimation();
  }
  void OnViewBoundsChanged(views::View* observed_view) override {
    // Stop animation if the frame size changed during animation.
    StopAnimation();
    SetBoundsRect(parent_->GetLocalBounds());
  }

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override {
    RemoveLayerBeneathView(layer_owner_->root());
    layer_owner_ = nullptr;
  }

 private:
  void StopAnimation() {
    if (layer_owner_) {
      layer_owner_->root()->GetAnimator()->StopAnimating();
      layer_owner_ = nullptr;
    }
  }

  FrameHeader* frame_header_;
  views::View* parent_;
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;
};

void FrameHeader::PaintHeader(gfx::Canvas* canvas) {
  painted_ = true;
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
  // No need to animate if already active.
  const bool already_active = (mode_ == Mode::MODE_ACTIVE);

  if (already_active == paint_as_active)
    return;

  mode_ = paint_as_active ? MODE_ACTIVE : MODE_INACTIVE;

  // The frame has no content yet to animatie.
  if (painted_)
    StartTransitionAnimation(kFrameActivationAnimationDuration);

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
                           chromeos::kWindowControlBackIcon);
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
// FrameHeader, protected:

FrameHeader::FrameHeader(views::Widget* target_widget, views::View* view)
    : target_widget_(target_widget), view_(view) {
  DCHECK(target_widget);
  DCHECK(view);
  UpdateFrameHeaderKey();
  frame_animator_ = new FrameAnimatorView(this, view);
}

void FrameHeader::UpdateFrameHeaderKey() {
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
                                            chromeos::kWindowControlMenuIcon);
  caption_button_container_->SetButtonImage(views::CAPTION_BUTTON_ICON_CLOSE,
                                            views::kWindowControlCloseIcon);
  caption_button_container_->SetButtonImage(
      views::CAPTION_BUTTON_ICON_LEFT_SNAPPED,
      chromeos::kWindowControlLeftSnappedIcon);
  caption_button_container_->SetButtonImage(
      views::CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
      chromeos::kWindowControlRightSnappedIcon);

  // Perform layout to ensure the container height is correct.
  LayoutHeaderInternal();
}

void FrameHeader::StartTransitionAnimation(base::TimeDelta duration) {
  aura::Window* window = target_widget_->GetNativeWindow();
  // Don't start another animation if the window is already animating
  // such as maximize/restore/unminimize.
  if (window->layer()->GetAnimator()->is_animating())
    return;

  frame_animator_->StartAnimation(duration);

  frame_animator_->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// FrameHeader, private:

void FrameHeader::LayoutHeaderInternal() {
  // The animator's position can change when the frame is moved from overlay.
  // Make sure the animator view is at the bottom.
  view_->ReorderChildView(frame_animator_, 0);

  bool use_zoom_icons = caption_button_container()->model()->InZoomMode();
  const gfx::VectorIcon& restore_icon = use_zoom_icons
                                            ? chromeos::kWindowControlDezoomIcon
                                            : views::kWindowControlRestoreIcon;
  const gfx::VectorIcon& maximize_icon =
      use_zoom_icons ? chromeos::kWindowControlZoomIcon
                     : views::kWindowControlMaximizeIcon;
  // TODO(crbug.com/1092005): Investigate if we can move this to
  // CaptionButtonModel and just check the model in
  // FrameCaptionButtonContainerView.
  const bool use_restore_frame = ash::ShouldUseRestoreFrame(target_widget_);
  caption_button_container()->SetButtonImage(
      views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
      use_restore_frame ? maximize_icon : restore_icon);
  caption_button_container()->UpdateSizeButtonTooltip(use_restore_frame);

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
