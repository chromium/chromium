// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_label_view.h"

#include "ash/capture_mode/capture_button_view.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

// Capture label button rounded corner radius.
constexpr int kCaptureLabelRadius = 18;

constexpr int kCountDownStartSeconds = 3;

constexpr base::TimeDelta kCaptureLabelOpacityFadeoutDuration =
    base::Milliseconds(33);

// Delay to enter number 3 to start count down.
constexpr base::TimeDelta kStartCountDownDelay = base::Milliseconds(233);

// The duration of the counter (e.g. "3", "2", etc.) fade in animation. The
// counter also scales up as it fades in with the same duration.
constexpr base::TimeDelta kCounterFadeInDuration = base::Milliseconds(250);

// The delay we wait before we fade out the counter after it fades in with the
// above duration.
constexpr base::TimeDelta kCounterFadeOutDuration = base::Milliseconds(150);

// The duration of the fade out and scale up animation of the counter when its
// value is `kCountDownStartSeconds`.
constexpr base::TimeDelta kStartCounterFadeOutDelay = base::Milliseconds(900);

// Same as above but for all other counters (i.e. "2" and "1").
constexpr base::TimeDelta kAllCountersFadeOutDelay = base::Milliseconds(850);

// The duration of the fade out animation applied on the label widget once the
// count down value reaches 1.
constexpr base::TimeDelta kWidgetFadeOutDuration = base::Milliseconds(333);

// The duration of drop to stop recording button position animation.
constexpr base::TimeDelta kDropToStopRecordingButtonDuration =
    base::Milliseconds(500);

// The counter starts at 80% scale as it fades in, and animates to a scale of
// 100%.
constexpr float kCounterInitialFadeInScale = 0.8f;

// The counter ends at 120% when it finishes its fade out animation.
constexpr float kCounterFinalFadeOutScale = 1.2f;

// The label widget scales down to 80% as it fades out at the very end of the
// count down.
constexpr float kWidgetFinalFadeOutScale = 0.8f;

}  // namespace

// -----------------------------------------------------------------------------
// DropToStopRecordingButtonAnimation:

// Defines an animation that calculates the transform of the label widget at
// each step of the drop towards the stop recording button position animation.
class DropToStopRecordingButtonAnimation : public gfx::LinearAnimation {
 public:
  DropToStopRecordingButtonAnimation(gfx::AnimationDelegate* delegate,
                                     const gfx::Point& start_position,
                                     const gfx::Point& target_position)
      : LinearAnimation(kDropToStopRecordingButtonDuration,
                        gfx::LinearAnimation::kDefaultFrameRate,
                        delegate),
        start_position_(start_position),
        target_position_(target_position) {}
  DropToStopRecordingButtonAnimation(
      const DropToStopRecordingButtonAnimation&) = delete;
  DropToStopRecordingButtonAnimation& operator=(
      const DropToStopRecordingButtonAnimation&) = delete;
  ~DropToStopRecordingButtonAnimation() override = default;

  const gfx::Transform& current_transform() const { return current_transform_; }

  // gfx::LinearAnimation:
  void AnimateToState(double state) override {
    // Note that this animation moves the widget at different speeds in X and Y.
    // This results in motion on a curve.
    const int new_x = gfx::Tween::IntValueBetween(
        gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_LINEAR_IN, state),
        start_position_.x(), target_position_.x());
    const int new_y = gfx::Tween::IntValueBetween(
        gfx::Tween::CalculateValue(gfx::Tween::ACCEL_30_DECEL_20_85, state),
        start_position_.y(), target_position_.y());

    current_transform_.MakeIdentity();
    current_transform_.Translate(gfx::Point(new_x, new_y) - start_position_);
  }

 private:
  // Note that the coordinate system of both `start_position_` and
  // `target_position_` must be the same. They can be both in screen, or both in
  // root. They're used to calculate and offset for a translation transform, so
  // it doesn't matter which coordinate system as long as they has the same.

  // The origin of the label widget at the start of this animation.
  const gfx::Point start_position_;

  // The origin of the stop recording button, which is the target position of
  // this animation.
  const gfx::Point target_position_;

  // The current value of the transform at each step of this animation which
  // will be applied on the label widget's layer.
  gfx::Transform current_transform_;
};

// -----------------------------------------------------------------------------
// CaptureLabelView:

CaptureLabelView::CaptureLabelView(
    CaptureModeSession* capture_mode_session,
    views::Button::PressedCallback on_capture_button_pressed,
    views::Button::PressedCallback on_drop_down_button_pressed)
    : capture_mode_session_(capture_mode_session),
      // Since this view has fully circular rounded corners, we can't use a nine
      // patch layer for the shadow. We have to use the `ShadowOnTextureLayer`.
      // For more info, see https://crbug.com/1308800.
      shadow_(SystemShadow::CreateShadowOnTextureLayer(
          SystemShadow::Type::kElevation12)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetBackground(views::CreateThemedSolidBackground(kColorAshShieldAndBase80));
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCaptureLabelRadius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  capture_button_container_ = AddChildView(std::make_unique<CaptureButtonView>(
      std::move(on_capture_button_pressed),
      std::move(on_drop_down_button_pressed),
      capture_mode_session_->active_behavior()));
  capture_button_container_->SetPaintToLayer();
  capture_button_container_->layer()->SetFillsBoundsOpaquely(false);
  capture_button_container_->SetNotifyEnterExitOnChild(true);

  label_ = AddChildView(std::make_unique<views::Label>(std::u16string()));
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetEnabledColorId(kColorAshTextColorPrimary);
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);

  capture_mode_util::SetHighlightBorder(
      this, kCaptureLabelRadius,
      views::HighlightBorder::Type::kHighlightBorderNoShadow);

  shadow_->SetRoundedCornerRadius(kCaptureLabelRadius);
}

CaptureLabelView::~CaptureLabelView() = default;

bool CaptureLabelView::IsViewInteractable() const {
  return capture_button_container_->GetVisible();
}

bool CaptureLabelView::IsPointOnRecordingTypeDropDownButton(
    const gfx::Point& screen_location) const {
  auto* drop_down_button = capture_button_container_->drop_down_button();
  return drop_down_button &&
         drop_down_button->GetBoundsInScreen().Contains(screen_location);
}

bool CaptureLabelView::IsRecordingTypeDropDownButtonVisible() const {
  auto* drop_down_button = capture_button_container_->drop_down_button();
  return capture_button_container_->GetVisible() && drop_down_button &&
         drop_down_button->GetVisible();
}

void CaptureLabelView::UpdateIconAndText() {
  CaptureModeController* controller = CaptureModeController::Get();
  const CaptureModeSource source = controller->source();
  const bool is_capturing_image = controller->type() == CaptureModeType::kImage;
  const bool in_tablet_mode = display::Screen::GetScreen()->InTabletMode();

  // Depending on the current state, only one of the two views
  // `capture_button_container_` or `label_` can be visible at a time.
  // Note that they can both be hidden in the case of `kWindow` source in
  // clamshell mode.
  bool capture_button_visibility = false;

  std::u16string text;
  switch (source) {
    case CaptureModeSource::kFullscreen:
      text = l10n_util::GetStringUTF16(
          is_capturing_image
              ? (in_tablet_mode
                     ? IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_IMAGE_CAPTURE_TABLET
                     : IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_IMAGE_CAPTURE_CLAMSHELL)
              : (in_tablet_mode
                     ? IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_VIDEO_RECORD_TABLET
                     : IDS_ASH_SCREEN_CAPTURE_LABEL_FULLSCREEN_VIDEO_RECORD_CLAMSHELL));
      break;
    case CaptureModeSource::kWindow: {
      // If the bar is anchored to the window, then we already have a pre-
      // selected game window for the game dashboard, and there is no need to
      // show the label.
      if (in_tablet_mode && !capture_mode_session_->IsBarAnchoredToWindow()) {
        text = l10n_util::GetStringUTF16(
            is_capturing_image
                ? IDS_ASH_SCREEN_CAPTURE_LABEL_WINDOW_IMAGE_CAPTURE
                : IDS_ASH_SCREEN_CAPTURE_LABEL_WINDOW_VIDEO_RECORD);
      }
      break;
    }
    case CaptureModeSource::kRegion: {
      if (!capture_mode_session_->is_selecting_region()) {
        if (CaptureModeController::Get()->user_capture_region().IsEmpty()) {
          // We're now in waiting to select a capture region phase.
          text = capture_mode_session_->active_behavior()
                     ->GetCaptureLabelRegionText();
        } else if (capture_mode_session_->active_behavior()
                       ->ShouldShowCaptureButtonAfterRegionSelected()) {
          // We're now in fine-tuning phase (i.e. there's a valid region, and
          // therefore we can show the capture button).
          capture_button_visibility = true;
        }
      }
      break;
    }
  }

  capture_button_container_->SetVisible(capture_button_visibility);
  if (capture_button_visibility)
    capture_button_container_->UpdateViewVisuals();

  const bool label_visibility = !text.empty();
  label_->SetVisible(label_visibility);
  if (label_visibility && (label_->GetText() != text)) {
    label_->SetText(text);
    capture_mode_util::TriggerAccessibilityAlertSoon(base::UTF16ToUTF8(text));
  }
}

bool CaptureLabelView::ShouldHandleEvent() {
  return IsViewInteractable() && !IsInCountDownAnimation();
}

void CaptureLabelView::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  countdown_finished_callback_ = std::move(countdown_finished_callback);

  // The view that needs to fade out will be decided depending on the visibility
  // of `capture_button_container_` and `label_`.
  ui::Layer* animation_layer = nullptr;
  if (capture_button_container_->GetVisible())
    animation_layer = capture_button_container_->layer();
  else if (label_->GetVisible())
    animation_layer = label_->layer();

  if (animation_layer) {
    // Fade out the opacity.
    animation_layer->SetOpacity(1.f);
    views::AnimationBuilder()
        .SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
        .Once()
        .SetDuration(kCaptureLabelOpacityFadeoutDuration)
        .SetOpacity(animation_layer, 0.f);
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureLabelView::FadeInAndOutCounter,
                     weak_factory_.GetWeakPtr(), kCountDownStartSeconds),
      kStartCountDownDelay);
}

bool CaptureLabelView::IsInCountDownAnimation() const {
  return !!countdown_finished_callback_;
}

void CaptureLabelView::AddedToWidget() {
  // Since the layer of the shadow has to be added as a sibling to this view's
  // layer, we need to wait until the view is added to the widget.
  auto* parent = layer()->parent();
  parent->Add(shadow_->GetLayer());
  parent->StackAtBottom(shadow_->GetLayer());

  // Make the shadow observe the color provider source change to update the
  // colors.
  shadow_->ObserveColorProviderSource(GetWidget());
}

void CaptureLabelView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // The shadow layer is a sibling of this view's layer, and should have the
  // same bounds.
  shadow_->SetContentBounds(layer()->bounds());
}

void CaptureLabelView::Layout(PassKey) {
  gfx::Rect label_bounds = GetLocalBounds();
  capture_button_container_->SetBoundsRect(label_bounds);

  label_bounds.ClampToCenteredSize(
      label_->GetPreferredSize(views::SizeBounds(label_->width(), {})));
  label_->SetBoundsRect(label_bounds);

  // This is necessary to update the focus ring, which is a child view of
  // `this`.
  LayoutSuperclass<views::View>(this);
}

gfx::Size CaptureLabelView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (countdown_finished_callback_)
    return gfx::Size(kCaptureLabelRadius * 2, kCaptureLabelRadius * 2);

  const bool is_label_button_visible = capture_button_container_->GetVisible();
  const bool is_label_visible = label_->GetVisible();

  if (!is_label_button_visible && !is_label_visible)
    return gfx::Size();

  if (is_label_button_visible) {
    DCHECK(!is_label_visible);
    return gfx::Size(capture_button_container_->GetPreferredSize().width(),
                     kCaptureLabelRadius * 2);
  }

  DCHECK(is_label_visible && !is_label_button_visible);
  return gfx::Size(
      label_->GetPreferredSize(views::SizeBounds(label_->width(), {})).width() +
          kCaptureLabelRadius * 2,
      kCaptureLabelRadius * 2);
}

void CaptureLabelView::OnThemeChanged() {
  views::View::OnThemeChanged();

  UpdateIconAndText();
}

void CaptureLabelView::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(drop_to_stop_button_animation_.get(), animation);
  OnCountDownAnimationFinished();
}

void CaptureLabelView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(drop_to_stop_button_animation_.get(), animation);
  GetWidget()->GetLayer()->SetTransform(
      drop_to_stop_button_animation_->current_transform());
}

void CaptureLabelView::FadeInAndOutCounter(int counter_value) {
  if (counter_value == 0) {
    DropWidgetToStopRecordingButton();
    return;
  }

  label_->SetVisible(true);
  label_->SetText(base::FormatNumber(counter_value));
  DeprecatedLayoutImmediately();

  // The counter should be initially fully transparent and scaled down 80%.
  ui::Layer* layer = label_->layer();
  layer->SetOpacity(0.f);
  layer->SetTransform(capture_mode_util::GetScaleTransformAboutCenter(
      layer, kCounterInitialFadeInScale));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CaptureLabelView::FadeInAndOutCounter,
                              weak_factory_.GetWeakPtr(), counter_value - 1))
      .Once()
      .SetDuration(kCounterFadeInDuration)
      .SetOpacity(layer, 1.f)
      .SetTransform(layer, gfx::Transform(), gfx::Tween::LINEAR_OUT_SLOW_IN)
      .At(counter_value == kCountDownStartSeconds ? kStartCounterFadeOutDelay
                                                  : kAllCountersFadeOutDelay)
      .SetDuration(kCounterFadeOutDuration)
      .SetOpacity(layer, 0.f)
      .SetTransform(layer,
                    capture_mode_util::GetScaleTransformAboutCenter(
                        layer, kCounterFinalFadeOutScale),
                    gfx::Tween::FAST_OUT_LINEAR_IN);
}

void CaptureLabelView::DropWidgetToStopRecordingButton() {
  auto* widget_window = GetWidget()->GetNativeWindow();
  StopRecordingButtonTray* stop_recording_button =
      capture_mode_util::GetStopRecordingButtonForRoot(
          widget_window->GetRootWindow());

  // Fall back to the fade out animation of the widget in case the button is not
  // available.
  if (!stop_recording_button) {
    FadeOutWidget();
    return;
  }

  // Temporarily show the button (without animation, i.e. don't use
  // `SetVisiblePreferred()`) in order to layout and get the position in which
  // it will be placed when we actually show it. `ShelfLayoutManager` will take
  // care of updating the layout when the visibility changes.
  stop_recording_button->SetVisible(true);
  stop_recording_button->UpdateLayout();
  const auto target_position =
      stop_recording_button->GetBoundsInScreen().origin();
  stop_recording_button->SetVisible(false);

  drop_to_stop_button_animation_ =
      std::make_unique<DropToStopRecordingButtonAnimation>(
          this, widget_window->GetBoundsInScreen().origin(), target_position);
  drop_to_stop_button_animation_->Start();
}

void CaptureLabelView::FadeOutWidget() {
  const auto tween = gfx::Tween::EASE_OUT_3;
  auto* widget_layer = GetWidget()->GetLayer();
  views::AnimationBuilder builder;
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&CaptureLabelView::OnCountDownAnimationFinished,
                              weak_factory_.GetWeakPtr()))
      .Once()
      .At(kCounterFadeInDuration + kAllCountersFadeOutDelay)
      .SetDuration(kWidgetFadeOutDuration)
      .SetOpacity(widget_layer, 0.f, tween)
      .SetTransform(widget_layer,
                    capture_mode_util::GetScaleTransformAboutCenter(
                        widget_layer, kWidgetFinalFadeOutScale),
                    tween);
}

void CaptureLabelView::OnCountDownAnimationFinished() {
  DCHECK(countdown_finished_callback_);
  std::move(countdown_finished_callback_).Run();  // `this` is destroyed here.
}

BEGIN_METADATA(CaptureLabelView)
END_METADATA

}  // namespace ash
