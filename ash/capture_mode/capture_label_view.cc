// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_label_view.h"

#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
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
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Capture label button rounded corner radius.
constexpr int kCaptureLabelRadius = 18;

constexpr int kCountDownStartSeconds = 3;
constexpr int kCountDownEndSeconds = 1;

constexpr base::TimeDelta kCaptureLabelOpacityFadeoutDuration =
    base::Milliseconds(33);
// Opacity fade in animation duration and scale up animation duration when the
// timeout label enters 3.
constexpr base::TimeDelta kCountDownEnter3Duration = base::Milliseconds(267);
// Opacity fade out animation duration and scale down animation duration when
// the timeout label exits 1.
constexpr base::TimeDelta kCountDownExit1Duration = base::Milliseconds(333);
// For other number enter/exit fade in/out, scale up/down animation duration.
constexpr base::TimeDelta kCountDownEnterExitDuration = base::Milliseconds(167);

// Delay to enter number 3 to start count down.
constexpr base::TimeDelta kStartCountDownDelay = base::Milliseconds(233);
// Delay to exit a number after entering animation is completed.
constexpr base::TimeDelta kCountDownExitDelay = base::Milliseconds(667);

// Different scales for enter/exiting countdown numbers.
constexpr float kEnterLabelScaleDown = 0.8f;
constexpr float kExitLabelScaleUp = 1.2f;
// Scale when exiting the number 1, unlike the other numbers, it will shrink
// down a bit and fade out.
constexpr float kExitLabel1ScaleDown = 0.8f;

void GetOpacityCountDownAnimationSetting(int count_down_number,
                                         bool enter,
                                         base::TimeDelta* duration,
                                         gfx::Tween::Type* tween_type) {
  if (count_down_number == kCountDownStartSeconds && enter) {
    *duration = kCountDownEnter3Duration;
    *tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
  } else if (count_down_number == kCountDownEndSeconds && !enter) {
    *duration = kCountDownExit1Duration;
    *tween_type = gfx::Tween::EASE_OUT_3;
  } else {
    *duration = kCountDownEnterExitDuration;
    *tween_type = gfx::Tween::LINEAR;
  }
}

void GetTransformCountDownAnimationSetting(int count_down_number,
                                           bool enter,
                                           base::TimeDelta* duration,
                                           gfx::Tween::Type* tween_type) {
  if (count_down_number == kCountDownStartSeconds && enter) {
    *duration = kCountDownEnter3Duration;
    *tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
  } else if (count_down_number == kCountDownEndSeconds && !enter) {
    *duration = kCountDownExit1Duration;
    *tween_type = gfx::Tween::EASE_OUT_3;
  } else if (enter) {
    *duration = kCountDownEnterExitDuration;
    *tween_type = gfx::Tween::LINEAR_OUT_SLOW_IN;
  } else {
    *duration = kCountDownEnterExitDuration;
    *tween_type = gfx::Tween::FAST_OUT_LINEAR_IN;
  }
}

std::unique_ptr<ui::LayerAnimationElement> CreateOpacityLayerAnimationElement(
    float target_opacity,
    base::TimeDelta duration,
    gfx::Tween::Type tween_type) {
  std::unique_ptr<ui::LayerAnimationElement> opacity_element =
      ui::LayerAnimationElement::CreateOpacityElement(target_opacity, duration);
  opacity_element->set_tween_type(tween_type);
  return opacity_element;
}

std::unique_ptr<ui::LayerAnimationElement> CreateTransformLayerAnimationElement(
    const gfx::Transform& target_transform,
    base::TimeDelta duration,
    gfx::Tween::Type tween_type) {
  std::unique_ptr<ui::LayerAnimationElement> transform_element =
      ui::LayerAnimationElement::CreateTransformElement(target_transform,
                                                        duration);
  transform_element->set_tween_type(tween_type);
  return transform_element;
}

// Returns the transform that can scale |bounds| around its center point.
gfx::Transform GetScaleTransform(const gfx::Rect& bounds, float scale) {
  const gfx::Point center_point = bounds.CenterPoint();
  return gfx::GetScaleTransform(
      gfx::Point(center_point.x() - bounds.x(), center_point.y() - bounds.y()),
      scale);
}

}  // namespace

CaptureLabelView::CaptureLabelView(
    CaptureModeSession* capture_mode_session,
    base::RepeatingClosure on_capture_button_pressed)
    : timeout_count_down_(kCountDownStartSeconds),
      capture_mode_session_(capture_mode_session) {
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
    ui::ScopedLayerAnimationSettings settings(animation_layer->GetAnimator());
    settings.SetTweenType(gfx::Tween::LINEAR);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(kCaptureLabelOpacityFadeoutDuration);
    animation_layer->SetOpacity(0.f);
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureLabelView::ScheduleCountDownAnimation,
                     weak_factory_.GetWeakPtr()),
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

void CaptureLabelView::ScheduleCountDownAnimation() {
  label_->SetVisible(true);
  label_->SetText(base::FormatNumber(timeout_count_down_));

  // Initial setup for entering |timeout_count_down_|:
  ui::Layer* label_layer = label_->layer();
  label_layer->SetOpacity(0.f);
  // Use target bounds as when this function is called, we're still in bounds
  // change animation, Widget::GetBoundsInScreen() won't return correct value.
  gfx::Rect bounds = GetWidget()->GetLayer()->GetTargetBounds();
  bounds.ClampToCenteredSize(label_->GetPreferredSize());
  label_layer->SetTransform(GetScaleTransform(bounds, kEnterLabelScaleDown));
  if (!animation_observer_) {
    animation_observer_ = std::make_unique<ui::CallbackLayerAnimationObserver>(
        base::BindRepeating(&CaptureLabelView::OnCountDownAnimationCompleted,
                            base::Unretained(this)));
  }

  StartLabelLayerAnimationSequences();
  StartWidgetLayerAnimationSequences();
  animation_observer_->SetActive();
}

bool CaptureLabelView::OnCountDownAnimationCompleted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // If animation was aborted, return directly to avoid crash as |this| may
  // no longer be valid.
  if (observer.aborted_count())
    return false;

  if (timeout_count_down_ == kCountDownEndSeconds) {
    std::move(countdown_finished_callback_).Run();  // |this| is destroyed here.
  } else {
    timeout_count_down_--;
    ScheduleCountDownAnimation();
  }

  // Return false to prevent the observer from destroying itself.
  return false;
}

void CaptureLabelView::StartLabelLayerAnimationSequences() {
  // Create |label_opacity_sequence|. Note we don't need the exit animation for
  // the last countdown number 1, since when exiting number 1, we'll fade out
  // the entire widget, not just the label.
  std::unique_ptr<ui::LayerAnimationSequence> label_opacity_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  base::TimeDelta enter_duration, exit_duration;
  gfx::Tween::Type enter_type, exit_type;
  GetOpacityCountDownAnimationSetting(timeout_count_down_, /*enter=*/true,
                                      &enter_duration, &enter_type);
  GetOpacityCountDownAnimationSetting(timeout_count_down_, /*enter=*/false,
                                      &exit_duration, &exit_type);

  label_opacity_sequence->AddElement(
      CreateOpacityLayerAnimationElement(1.f, enter_duration, enter_type));
  label_opacity_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::OPACITY, kCountDownExitDelay));
  const bool is_final_second = timeout_count_down_ == kCountDownEndSeconds;
  if (!is_final_second) {
    label_opacity_sequence->AddElement(
        CreateOpacityLayerAnimationElement(0.f, exit_duration, exit_type));
  }

  // Construct |label_transfrom_sequence|. Same reason above, we don't need
  // the exit animation for the last countdown number 1.
  std::unique_ptr<ui::LayerAnimationSequence> label_transfrom_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  GetTransformCountDownAnimationSetting(timeout_count_down_, /*enter=*/true,
                                        &enter_duration, &enter_type);
  GetTransformCountDownAnimationSetting(timeout_count_down_, /*enter=*/false,
                                        &exit_duration, &exit_type);

  label_transfrom_sequence->AddElement(CreateTransformLayerAnimationElement(
      gfx::Transform(), enter_duration, enter_type));
  label_transfrom_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::TRANSFORM, kCountDownExitDelay));
  if (!is_final_second) {
    gfx::Rect bounds = GetWidget()->GetLayer()->GetTargetBounds();
    bounds.ClampToCenteredSize(label_->GetPreferredSize());
    label_transfrom_sequence->AddElement(CreateTransformLayerAnimationElement(
        GetScaleTransform(bounds, kExitLabelScaleUp), exit_duration,
        exit_type));
  }

  label_opacity_sequence->AddObserver(animation_observer_.get());
  label_transfrom_sequence->AddObserver(animation_observer_.get());
  label_->layer()->GetAnimator()->StartTogether(
      {label_opacity_sequence.release(), label_transfrom_sequence.release()});
}

void CaptureLabelView::StartWidgetLayerAnimationSequences() {
  // Only need animate the widget layer when exiting the last countdown number.
  if (timeout_count_down_ != kCountDownEndSeconds)
    return;

  std::unique_ptr<ui::LayerAnimationSequence> widget_opacity_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  base::TimeDelta exit_duration;
  gfx::Tween::Type exit_type;
  GetOpacityCountDownAnimationSetting(timeout_count_down_, /*enter=*/false,
                                      &exit_duration, &exit_type);
  widget_opacity_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::OPACITY,
          kCountDownEnterExitDuration + kCountDownExitDelay));
  widget_opacity_sequence->AddElement(
      CreateOpacityLayerAnimationElement(0.f, exit_duration, exit_type));

  std::unique_ptr<ui::LayerAnimationSequence> widget_transform_sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  GetTransformCountDownAnimationSetting(timeout_count_down_, /*enter=*/false,
                                        &exit_duration, &exit_type);
  widget_transform_sequence->AddElement(
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::TRANSFORM,
          kCountDownEnterExitDuration + kCountDownExitDelay));
  const gfx::Rect bounds = GetWidget()->GetLayer()->GetTargetBounds();
  widget_transform_sequence->AddElement(CreateTransformLayerAnimationElement(
      GetScaleTransform(bounds, kExitLabel1ScaleDown), exit_duration,
      exit_type));

  widget_opacity_sequence->AddObserver(animation_observer_.get());
  widget_transform_sequence->AddObserver(animation_observer_.get());
  GetWidget()->GetLayer()->GetAnimator()->StartTogether(
      {widget_opacity_sequence.release(), widget_transform_sequence.release()});
}

BEGIN_METADATA(CaptureLabelView, views::View)
END_METADATA

}  // namespace ash
