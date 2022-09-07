// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_label_view.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
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
    base::RepeatingClosure on_capture_button_pressed)
    : capture_mode_session_(capture_mode_session) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* color_provider = AshColorProvider::Get();
  SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  SetBackground(views::CreateSolidBackground(background_color));
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(kCaptureLabelRadius));
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  label_button_ = AddChildView(std::make_unique<views::LabelButton>(
      std::move(on_capture_button_pressed), std::u16string()));
  label_button_->SetPaintToLayer();
  label_button_->layer()->SetFillsBoundsOpaquely(false);
  label_button_->SetEnabledTextColors(text_color);
  label_button_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_button_->SetNotifyEnterExitOnChild(true);

  views::InkDrop::Get(label_button_)
      ->SetMode(views::InkDropHost::InkDropMode::ON);
  StyleUtil::ConfigureInkDropAttributes(
      label_button_, StyleUtil::kBaseColor | StyleUtil::kInkDropOpacity);
  label_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);

  label_ = AddChildView(std::make_unique<views::Label>(std::u16string()));
  label_->SetPaintToLayer();
  label_->layer()->SetFillsBoundsOpaquely(false);
  label_->SetEnabledColor(text_color);
  label_->SetBackgroundColor(SK_ColorTRANSPARENT);

  UpdateIconAndText();

  if (features::IsDarkLightModeEnabled()) {
    SetBorder(std::make_unique<views::HighlightBorder>(
        kCaptureLabelRadius, views::HighlightBorder::Type::kHighlightBorder2,
        /*use_light_colors=*/false));
  }
}

CaptureLabelView::~CaptureLabelView() = default;

void CaptureLabelView::UpdateIconAndText() {
  CaptureModeController* controller = CaptureModeController::Get();
  const CaptureModeSource source = controller->source();
  const bool is_capturing_image = controller->type() == CaptureModeType::kImage;
  const bool in_tablet_mode = TabletModeController::Get()->InTabletMode();
  auto* color_provider = AshColorProvider::Get();
  SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  gfx::ImageSkia icon;
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
      if (in_tablet_mode) {
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
          text = l10n_util::GetStringUTF16(
              is_capturing_image
                  ? IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_IMAGE_CAPTURE
                  : IDS_ASH_SCREEN_CAPTURE_LABEL_REGION_VIDEO_RECORD);
        } else {
          // We're now in fine-tuning phase.
          icon = is_capturing_image
                     ? gfx::CreateVectorIcon(kCaptureModeImageIcon, icon_color)
                     : gfx::CreateVectorIcon(kCaptureModeVideoIcon, icon_color);
          text = l10n_util::GetStringUTF16(
              is_capturing_image ? IDS_ASH_SCREEN_CAPTURE_LABEL_IMAGE_CAPTURE
                                 : IDS_ASH_SCREEN_CAPTURE_LABEL_VIDEO_RECORD);
        }
      }
      break;
    }
  }

  if (!icon.isNull()) {
    label_->SetVisible(false);
    label_button_->SetVisible(true);
    // Update the icon only if one is not already present or it has changed to
    // reduce repainting.
    if (!label_button_->HasImage(views::Button::STATE_NORMAL) ||
        !icon.BackedBySameObjectAs(
            label_button_->GetImage(views::Button::STATE_NORMAL))) {
      label_button_->SetImage(views::Button::STATE_NORMAL, icon);
    }
    label_button_->SetText(text);
  } else if (!text.empty()) {
    label_button_->SetVisible(false);
    label_->SetVisible(true);
    label_->SetText(text);
  } else {
    label_button_->SetVisible(false);
    label_->SetVisible(false);
  }
}

bool CaptureLabelView::ShouldHandleEvent() {
  return label_button_->GetVisible() && !IsInCountDownAnimation();
}

void CaptureLabelView::StartCountDown(
    base::OnceClosure countdown_finished_callback) {
  countdown_finished_callback_ = std::move(countdown_finished_callback);

  // Depending on the visibility of |label_button_| and |label_|, decide which
  // view needs to fade out.
  ui::Layer* animation_layer = nullptr;
  if (label_button_->GetVisible())
    animation_layer = label_button_->layer();
  if (label_->GetVisible())
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

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureLabelView::FadeInAndOutCounter,
                     weak_factory_.GetWeakPtr(), kCountDownStartSeconds),
      kStartCountDownDelay);
}

bool CaptureLabelView::IsInCountDownAnimation() const {
  return !!countdown_finished_callback_;
}

void CaptureLabelView::Layout() {
  gfx::Rect label_bounds = GetLocalBounds();
  label_button_->SetBoundsRect(label_bounds);

  label_bounds.ClampToCenteredSize(label_->GetPreferredSize());
  label_->SetBoundsRect(label_bounds);

  // This is necessary to update the focus ring, which is a child view of
  // |this|.
  views::View::Layout();
}

gfx::Size CaptureLabelView::CalculatePreferredSize() const {
  if (countdown_finished_callback_)
    return gfx::Size(kCaptureLabelRadius * 2, kCaptureLabelRadius * 2);

  const bool is_label_button_visible = label_button_->GetVisible();
  const bool is_label_visible = label_->GetVisible();

  if (!is_label_button_visible && !is_label_visible)
    return gfx::Size();

  if (is_label_button_visible) {
    DCHECK(!is_label_visible);
    return gfx::Size(
        label_button_->GetPreferredSize().width() + kCaptureLabelRadius * 2,
        kCaptureLabelRadius * 2);
  }

  DCHECK(is_label_visible && !is_label_button_visible);
  return gfx::Size(label_->GetPreferredSize().width() + kCaptureLabelRadius * 2,
                   kCaptureLabelRadius * 2);
}

views::View* CaptureLabelView::GetView() {
  return label_button_;
}

std::unique_ptr<views::HighlightPathGenerator>
CaptureLabelView::CreatePathGenerator() {
  // Regular focus rings are drawn outside the view's bounds. Since this view is
  // the same size as its widget, inset by half the focus ring thickness to
  // ensure the focus ring is drawn inside the widget bounds.
  return std::make_unique<views::RoundRectHighlightPathGenerator>(
      gfx::Insets(views::FocusRing::kDefaultHaloThickness / 2),
      kCaptureLabelRadius);
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
  Layout();

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

BEGIN_METADATA(CaptureLabelView, views::View)
END_METADATA

}  // namespace ash
