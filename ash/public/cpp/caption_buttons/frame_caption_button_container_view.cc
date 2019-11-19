// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"

#include <cmath>
#include <map>

#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/caption_buttons/frame_size_button.h"
#include "ash/public/cpp/caption_buttons/snap_controller.h"
#include "ash/public/cpp/gesture_action_type.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_sink.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"  // Accessibility names
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/frame_caption_button.h"

namespace ash {

namespace {

// Duration of the animation of the position of buttons to the left of
// |size_button_|.
constexpr auto kPositionAnimationDuration =
    base::TimeDelta::FromMilliseconds(500);

// Duration of the animation of the alpha of |size_button_|.
constexpr auto kAlphaAnimationDuration = base::TimeDelta::FromMilliseconds(250);

// Delay during |tablet_mode_animation_| hide to wait before beginning to
// animate the position of buttons to the left of |size_button_|.
constexpr auto kHidePositionDelay = base::TimeDelta::FromMilliseconds(100);

// Duration of |tablet_mode_animation_| hiding.
// Hiding size button 250
// |------------------------|
// Delay 100      Slide other buttons 500
// |---------|-------------------------------------------------|
constexpr auto kHideAnimationDuration =
    kHidePositionDelay + kPositionAnimationDuration;

// Delay during |tablet_mode_animation_| show to wait before beginning to
// animate the alpha of |size_button_|.
constexpr auto kShowAnimationAlphaDelay =
    base::TimeDelta::FromMilliseconds(100);

// Duration of |tablet_mode_animation_| showing.
// Slide other buttons 500
// |-------------------------------------------------|
// Delay 100   Show size button 250
// |---------|-----------------------|
constexpr auto kShowAnimationDuration = kPositionAnimationDuration;

// Value of |tablet_mode_animation_| showing to begin animating alpha of
// |size_button_|.
float SizeButtonShowStartValue() {
  return kShowAnimationAlphaDelay.InMillisecondsF() /
         kShowAnimationDuration.InMillisecondsF();
}

// Amount of |tablet_mode_animation_| showing to animate the alpha of
// |size_button_|.
float SizeButtonShowDuration() {
  return kAlphaAnimationDuration.InMillisecondsF() /
         kShowAnimationDuration.InMillisecondsF();
}

// Amount of |tablet_mode_animation_| hiding to animate the alpha of
// |size_button_|.
float SizeButtonHideDuration() {
  return kAlphaAnimationDuration.InMillisecondsF() /
         kHideAnimationDuration.InMillisecondsF();
}

// Value of |tablet_mode_animation_| hiding to begin animating the position of
// buttons to the left of |size_button_|.
float HidePositionStartValue() {
  return 1.0f - kHidePositionDelay.InMillisecondsF() /
                    kHideAnimationDuration.InMillisecondsF();
}

// Bounds animation values to the range 0.0 - 1.0. Allows for mapping of offset
// animations to the expected range so that gfx::Tween::CalculateValue() can be
// used.
double CapAnimationValue(double value) {
  return base::ClampToRange(value, 0.0, 1.0);
}

// Returns a |views::BoxLayout| layout manager with the settings needed by
// FrameCaptionButtonContainerView.
std::unique_ptr<views::BoxLayout> MakeBoxLayoutManager(
    int minimum_cross_axis_size) {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
  layout->set_minimum_cross_axis_size(minimum_cross_axis_size);
  return layout;
}

// A default CaptionButtonModel that uses the widget delegate's state
// to determine if each button should be visible and enabled.
class DefaultCaptionButtonModel : public CaptionButtonModel {
 public:
  explicit DefaultCaptionButtonModel(views::Widget* frame) : frame_(frame) {}
  ~DefaultCaptionButtonModel() override {}

  // CaptionButtonModel:
  bool IsVisible(views::CaptionButtonIcon type) const override {
    switch (type) {
      case views::CAPTION_BUTTON_ICON_MINIMIZE:
        return frame_->widget_delegate()->CanMinimize();
      case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
        return frame_->widget_delegate()->CanMaximize();
      // Resizable widget can be snapped.
      case views::CAPTION_BUTTON_ICON_LEFT_SNAPPED:
      case views::CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
        return frame_->widget_delegate()->CanResize();
      case views::CAPTION_BUTTON_ICON_CLOSE:
        return frame_->widget_delegate()->ShouldShowCloseButton();

      // No back or menu button by default.
      case views::CAPTION_BUTTON_ICON_BACK:
      case views::CAPTION_BUTTON_ICON_MENU:
      case views::CAPTION_BUTTON_ICON_ZOOM:
        return false;
      case views::CAPTION_BUTTON_ICON_LOCATION:
        // not used
        return false;
      case views::CAPTION_BUTTON_ICON_COUNT:
        break;
    }
    NOTREACHED();
    return false;
  }
  bool IsEnabled(views::CaptionButtonIcon type) const override { return true; }
  bool InZoomMode() const override { return false; }

 private:
  views::Widget* frame_;
  DISALLOW_COPY_AND_ASSIGN(DefaultCaptionButtonModel);
};

}  // namespace

// static
const char FrameCaptionButtonContainerView::kViewClassName[] =
    "FrameCaptionButtonContainerView";

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::Widget* frame)
    : views::AnimationDelegateViews(frame->GetRootView()),
      frame_(frame),
      model_(std::make_unique<DefaultCaptionButtonModel>(frame)) {
  SetLayoutManager(MakeBoxLayoutManager(/*minimum_cross_axis_size=*/0));
  tablet_mode_animation_.reset(new gfx::SlideAnimation(this));
  tablet_mode_animation_->SetTweenType(gfx::Tween::LINEAR);

  // Ensure animation tracks visibility of size button.
  if (model_->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
      model_->InZoomMode()) {
    tablet_mode_animation_->Reset(1.0f);
  }

  // Insert the buttons left to right.
  menu_button_ = new views::FrameCaptionButton(
      this, views::CAPTION_BUTTON_ICON_MENU, HTMENU);
  menu_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MENU));
  AddChildView(menu_button_);

  minimize_button_ = new views::FrameCaptionButton(
      this, views::CAPTION_BUTTON_ICON_MINIMIZE, HTMINBUTTON);
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  AddChildView(minimize_button_);

  size_button_ = new FrameSizeButton(this, this);
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  AddChildView(size_button_);

  close_button_ = new views::FrameCaptionButton(
      this, views::CAPTION_BUTTON_ICON_CLOSE, HTCLOSE);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);

  UpdateCaptionButtonState(false /* animate */);
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() = default;

void FrameCaptionButtonContainerView::TestApi::EndAnimations() {
  container_view_->tablet_mode_animation_->End();
}

void FrameCaptionButtonContainerView::SetButtonImage(
    views::CaptionButtonIcon icon,
    const gfx::VectorIcon& icon_definition) {
  button_icon_map_[icon] = &icon_definition;

  views::FrameCaptionButton* buttons[] = {menu_button_, minimize_button_,
                                          size_button_, close_button_};
  for (size_t i = 0; i < base::size(buttons); ++i) {
    if (buttons[i]->icon() == icon)
      buttons[i]->SetImage(icon, views::FrameCaptionButton::ANIMATE_NO,
                           icon_definition);
  }
}

void FrameCaptionButtonContainerView::SetPaintAsActive(bool paint_as_active) {
  menu_button_->set_paint_as_active(paint_as_active);
  minimize_button_->set_paint_as_active(paint_as_active);
  size_button_->set_paint_as_active(paint_as_active);
  close_button_->set_paint_as_active(paint_as_active);
}

void FrameCaptionButtonContainerView::SetBackgroundColor(
    SkColor background_color) {
  menu_button_->SetBackgroundColor(background_color);
  minimize_button_->SetBackgroundColor(background_color);
  size_button_->SetBackgroundColor(background_color);
  close_button_->SetBackgroundColor(background_color);
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  SetButtonsToNormal(ANIMATE_NO);
}

void FrameCaptionButtonContainerView::UpdateCaptionButtonState(bool animate) {
  bool size_button_visible =
      (model_->IsVisible(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
       model_->InZoomMode());
  if (size_button_visible) {
    size_button_->SetVisible(true);
    if (animate) {
      tablet_mode_animation_->SetSlideDuration(kShowAnimationDuration);
      tablet_mode_animation_->Show();
    }
  } else {
    if (animate) {
      tablet_mode_animation_->SetSlideDuration(kHideAnimationDuration);
      tablet_mode_animation_->Hide();
    } else {
      size_button_->SetVisible(false);
    }
  }
  size_button_->SetEnabled(
      (model_->IsEnabled(views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE) ||
       model_->InZoomMode()));
  minimize_button_->SetVisible(
      model_->IsVisible(views::CAPTION_BUTTON_ICON_MINIMIZE));
  minimize_button_->SetEnabled(
      model_->IsEnabled(views::CAPTION_BUTTON_ICON_MINIMIZE));
  menu_button_->SetVisible(model_->IsVisible(views::CAPTION_BUTTON_ICON_MENU));
  menu_button_->SetEnabled(model_->IsEnabled(views::CAPTION_BUTTON_ICON_MENU));
  close_button_->SetVisible(
      model_->IsVisible(views::CAPTION_BUTTON_ICON_CLOSE));
}

void FrameCaptionButtonContainerView::SetButtonSize(const gfx::Size& size) {
  menu_button_->SetPreferredSize(size);
  minimize_button_->SetPreferredSize(size);
  size_button_->SetPreferredSize(size);
  close_button_->SetPreferredSize(size);

  SetLayoutManager(MakeBoxLayoutManager(size.height()));
}

void FrameCaptionButtonContainerView::SetModel(
    std::unique_ptr<CaptionButtonModel> model) {
  model_ = std::move(model);
}

void FrameCaptionButtonContainerView::Layout() {
  views::View::Layout();

  // This ensures that the first frame of the animation to show the size button
  // pushes the buttons to the left of the size button into the center.
  if (tablet_mode_animation_->is_animating())
    AnimationProgressed(tablet_mode_animation_.get());

#if DCHECK_IS_ON()
  if (close_button_->GetVisible()) {
    // The top right corner must be occupied by the close button for easy mouse
    // access. This check is agnostic to RTL layout.
    DCHECK_EQ(close_button_->y(), 0);
    DCHECK_EQ(close_button_->bounds().right(), width());
  }
#endif  // DCHECK_IS_ON()
}

const char* FrameCaptionButtonContainerView::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButtonContainerView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void FrameCaptionButtonContainerView::ChildVisibilityChanged(View* child) {
  PreferredSizeChanged();
}

void FrameCaptionButtonContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  // Ensure that position is calculated at least once.
  AnimationProgressed(animation);

  double current_value = tablet_mode_animation_->GetCurrentValue();
  if (current_value == 0.0)
    size_button_->SetVisible(false);
}

void FrameCaptionButtonContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  double current_value = animation->GetCurrentValue();
  int size_alpha = 0;
  int x_slide = 0;
  if (tablet_mode_animation_->IsShowing()) {
    double scaled_value_alpha =
        CapAnimationValue((current_value - SizeButtonShowStartValue()) /
                          SizeButtonShowDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value_alpha);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 0, 255);

    double tweened_value_slide =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, current_value);
    x_slide = gfx::Tween::LinearIntValueBetween(tweened_value_slide,
                                                size_button_->width(), 0);
  } else {
    double scaled_value_alpha =
        CapAnimationValue((1.0f - current_value) / SizeButtonHideDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, scaled_value_alpha);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 255, 0);

    double scaled_value_position = CapAnimationValue(
        (HidePositionStartValue() - current_value) / HidePositionStartValue());
    double tweened_value_slide =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value_position);
    x_slide = gfx::Tween::LinearIntValueBetween(tweened_value_slide, 0,
                                                size_button_->width());
  }
  size_button_->SetAlpha(size_alpha);

  // Slide all buttons to the left of the size button. Usually this is just the
  // minimize button but it can also include a PWA menu button.
  int previous_x = 0;
  for (auto* button : children()) {
    if (button == size_button_)
      break;
    button->SetX(previous_x + x_slide);
    previous_x += button->width();
  }
}

void FrameCaptionButtonContainerView::SetButtonIcon(
    views::FrameCaptionButton* button,
    views::CaptionButtonIcon icon,
    Animate animate) {
  // The early return is dependent on |animate| because callers use
  // SetButtonIcon() with ANIMATE_NO to progress |button|'s crossfade animation
  // to the end.
  if (button->icon() == icon &&
      (animate == ANIMATE_YES || !button->IsAnimatingImageSwap())) {
    return;
  }

  views::FrameCaptionButton::Animate fcb_animate =
      (animate == ANIMATE_YES) ? views::FrameCaptionButton::ANIMATE_YES
                               : views::FrameCaptionButton::ANIMATE_NO;
  auto it = button_icon_map_.find(icon);
  if (it != button_icon_map_.end())
    button->SetImage(icon, fcb_animate, *it->second);
}

void FrameCaptionButtonContainerView::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  // Abort any animations of the button icons.
  SetButtonsToNormal(ANIMATE_NO);

  using base::RecordAction;
  using base::UserMetricsAction;
  if (sender == minimize_button_) {
    frame_->Minimize();
    RecordAction(UserMetricsAction("MinButton_Clk"));
  } else if (sender == size_button_) {
    if (frame_->IsFullscreen()) {  // Can be clicked in immersive fullscreen.
      frame_->Restore();
      RecordAction(UserMetricsAction("MaxButton_Clk_ExitFS"));
    } else if (frame_->IsMaximized()) {
      frame_->Restore();
      RecordAction(UserMetricsAction("MaxButton_Clk_Restore"));
    } else {
      frame_->Maximize();
      RecordAction(UserMetricsAction("MaxButton_Clk_Maximize"));
    }

    if (event.IsGestureEvent()) {
      UMA_HISTOGRAM_ENUMERATION("Ash.GestureTarget", GESTURE_FRAMEMAXIMIZE_TAP,
                                GESTURE_ACTION_COUNT);
    }
  } else if (sender == close_button_) {
    frame_->Close();
    if (TabletMode::Get()->InTabletMode())
      RecordAction(UserMetricsAction("Tablet_WindowCloseFromCaptionButton"));
    else
      RecordAction(UserMetricsAction("CloseButton_Clk"));
  } else if (sender == menu_button_) {
    // Send up event as well as down event as ARC++ clients expect this
    // sequence.
    aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
    ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED, ui::VKEY_APPS,
                                 ui::EF_NONE);
    ignore_result(root_window->GetHost()->event_sink()->OnEventFromSource(
        &press_key_event));
    ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED, ui::VKEY_APPS,
                                   ui::EF_NONE);
    ignore_result(root_window->GetHost()->event_sink()->OnEventFromSource(
        &release_key_event));
    // TODO(oshima): Add metrics
  }
}

bool FrameCaptionButtonContainerView::IsMinimizeButtonVisible() const {
  return minimize_button_->GetVisible();
}

void FrameCaptionButtonContainerView::SetButtonsToNormal(Animate animate) {
  SetButtonIcons(views::CAPTION_BUTTON_ICON_MINIMIZE,
                 views::CAPTION_BUTTON_ICON_CLOSE, animate);
  menu_button_->SetState(views::Button::STATE_NORMAL);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  size_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
}

void FrameCaptionButtonContainerView::SetButtonIcons(
    views::CaptionButtonIcon minimize_button_icon,
    views::CaptionButtonIcon close_button_icon,
    Animate animate) {
  SetButtonIcon(minimize_button_, minimize_button_icon, animate);
  SetButtonIcon(close_button_, close_button_icon, animate);
}

const views::FrameCaptionButton*
FrameCaptionButtonContainerView::GetButtonClosestTo(
    const gfx::Point& position_in_screen) const {
  // Since the buttons all have the same size, the closest button is the button
  // with the center point closest to |position_in_screen|.
  // TODO(pkotwicz): Make the caption buttons not overlap.
  gfx::Point position(position_in_screen);
  views::View::ConvertPointFromScreen(this, &position);

  views::FrameCaptionButton* buttons[] = {menu_button_, minimize_button_,
                                          size_button_, close_button_};
  int min_squared_distance = INT_MAX;
  views::FrameCaptionButton* closest_button = NULL;
  for (size_t i = 0; i < base::size(buttons); ++i) {
    views::FrameCaptionButton* button = buttons[i];
    if (!button->GetVisible())
      continue;

    gfx::Point center_point = button->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(button, this, &center_point);
    int squared_distance = static_cast<int>(
        pow(static_cast<double>(position.x() - center_point.x()), 2) +
        pow(static_cast<double>(position.y() - center_point.y()), 2));
    if (squared_distance < min_squared_distance) {
      min_squared_distance = squared_distance;
      closest_button = button;
    }
  }
  return closest_button;
}

void FrameCaptionButtonContainerView::SetHoveredAndPressedButtons(
    const views::FrameCaptionButton* to_hover,
    const views::FrameCaptionButton* to_press) {
  views::FrameCaptionButton* buttons[] = {menu_button_, minimize_button_,
                                          size_button_, close_button_};
  for (size_t i = 0; i < base::size(buttons); ++i) {
    views::FrameCaptionButton* button = buttons[i];
    views::Button::ButtonState new_state = views::Button::STATE_NORMAL;
    if (button == to_hover)
      new_state = views::Button::STATE_HOVERED;
    else if (button == to_press)
      new_state = views::Button::STATE_PRESSED;
    button->SetState(new_state);
  }
}

bool FrameCaptionButtonContainerView::CanSnap() {
  return SnapController::Get()->CanSnap(frame_->GetNativeWindow());
}

void FrameCaptionButtonContainerView::ShowSnapPreview(SnapDirection snap) {
  SnapController::Get()->ShowSnapPreview(frame_->GetNativeWindow(), snap);
}

void FrameCaptionButtonContainerView::CommitSnap(SnapDirection snap) {
  SnapController::Get()->CommitSnap(frame_->GetNativeWindow(), snap);
}

}  // namespace ash
