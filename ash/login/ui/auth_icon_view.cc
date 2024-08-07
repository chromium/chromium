// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_icon_view.h"

#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/lottie/animation.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

struct ShakeAnimationStep {
  int x_offset;
  int duration_ms;
};

constexpr int kAuthIconSizeDp = 32;
constexpr int kIconMarginDp = 10;
constexpr int kAuthIconViewDp = kAuthIconSizeDp + 2 * kIconMarginDp;
constexpr int kProgressAnimationStrokeWidth = 3;

// This determines how frequently we paint the progress spinner.
// 1 frame / 30 msec = about 30 frames per second
// 30 fps seems to be smooth enough to look good without excessive painting.
constexpr base::TimeDelta kProgressFrameDuration = base::Milliseconds(30);
// How long the nudge animation takes to scale up and fade out.
constexpr base::TimeDelta kNudgeAnimationScalingDuration =
    base::Milliseconds(2000);
// How long the delay between the repeating nudge animation takes in between
// cycles.
constexpr base::TimeDelta kNudgeAnimationDelayDuration =
    base::Milliseconds(1000);
// How opaque the nudge animation will reset the view to.
constexpr float kOpacityReset = 0.5;
// Size that nudge animation scales view up and down by.
constexpr SkScalar kTransformScaleUpSize = 3;
// The interpolation of transform fails when scaling all the way down to 0.
constexpr SkScalar kTransformScaleDownSize = 0.01;

// See spec:
// https://carbon.googleplex.com/cr-os-motion-work/pages/sign-in/undefined/e05c4091-eea2-4c5a-a6f8-38fd37953e7b#a929eb9f-2840-4b37-be52-97d96ca2aafa
constexpr ShakeAnimationStep kShakeAnimationSteps[] = {
    {-5, 83}, {8, 83}, {-7, 66}, {7, 66}, {-7, 66}, {7, 66}, {-3, 83}};

}  // namespace

// static
ui::ColorId AuthIconView::GetColorId(AuthIconView::Status status) {
  switch (status) {
    case AuthIconView::Status::kPrimary:
      return cros_tokens::kCrosSysOnSurface;
    case AuthIconView::Status::kDisabled:
      return cros_tokens::kCrosSysDisabled;
    case AuthIconView::Status::kError:
      return cros_tokens::kCrosSysError;
    case AuthIconView::Status::kPositive:
      return cros_tokens::kCrosSysPositive;
  }
}

AuthIconView::AuthIconView() {
  SetLayoutManager(std::make_unique<views::BoxLayout>());

  icon_ = AddChildView(std::make_unique<AnimatedRoundedImageView>(
      gfx::Size(kAuthIconSizeDp, kAuthIconSizeDp),
      /*corner_radius=*/0));
  icon_->SetProperty(views::kMarginsKey, gfx::Insets(kIconMarginDp));

  // Set up layer to allow for animation.
  icon_->SetPaintToLayer();
  icon_->layer()->SetFillsBoundsOpaquely(false);
  icon_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  lottie_animation_view_ =
      AddChildView(std::make_unique<views::AnimatedImageView>());
  lottie_animation_view_->SetImageSize(
      gfx::Size(kAuthIconSizeDp, kAuthIconSizeDp));
  lottie_animation_view_->SetProperty(views::kMarginsKey,
                                      gfx::Insets(kIconMarginDp));
  lottie_animation_view_->SetVisible(false);
}

AuthIconView::~AuthIconView() = default;

void AuthIconView::AddedToWidget() {
  RasterizeIcon();
}

void AuthIconView::OnThemeChanged() {
  views::View::OnThemeChanged();
  RasterizeIcon();
}

void AuthIconView::SetIcon(const gfx::VectorIcon& icon, Status status) {
  icon_image_model_ =
      ui::ImageModel::FromVectorIcon(icon, GetColorId(status), kAuthIconSizeDp);
  if (GetColorProvider()) {
    icon_->SetImage(icon_image_model_.Rasterize(GetColorProvider()));
  }
  icon_->SetVisible(true);
  lottie_animation_view_->SetVisible(false);
}

void AuthIconView::RasterizeIcon() {
  if (!icon_image_model_.IsEmpty()) {
    icon_->SetImage(icon_image_model_.Rasterize(GetColorProvider()));
  }
}

void AuthIconView::SetCircleImage(int size, SkColor color) {
  gfx::ImageSkia circle_icon =
      gfx::CanvasImageSource::MakeImageSkia<CircleImageSource>(size, color);
  icon_->SetImage(circle_icon);
}

void AuthIconView::SetAnimation(int animation_resource_id,
                                base::TimeDelta duration,
                                int num_frames) {
  icon_->SetAnimationDecoder(
      std::make_unique<HorizontalImageSequenceAnimationDecoder>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              animation_resource_id),
          duration, num_frames),
      AnimatedRoundedImageView::Playback::kSingle);
  icon_->SetVisible(true);
  lottie_animation_view_->SetVisible(false);
}

void AuthIconView::SetLottieAnimation(
    std::unique_ptr<lottie::Animation> animation) {
  // This config prevents the animation from looping.
  auto playback_config = lottie::Animation::PlaybackConfig::CreateWithStyle(
      lottie::Animation::Style::kLinear, *animation);

  lottie_animation_view_->SetAnimatedImage(std::move(animation));
  lottie_animation_view_->Play(playback_config);
  icon_->SetVisible(false);
  lottie_animation_view_->SetVisible(true);
}

void AuthIconView::RunErrorShakeAnimation() {
  StopAnimating();

  auto transform_sequence = std::make_unique<ui::LayerAnimationSequence>();
  gfx::Transform transform;
  for (const ShakeAnimationStep& step : kShakeAnimationSteps) {
    transform.Translate(step.x_offset, /*y=*/0);
    auto element = ui::LayerAnimationElement::CreateTransformElement(
        transform, base::Milliseconds(step.duration_ms));
    element->set_tween_type(gfx::Tween::Type::EASE_IN_OUT_2);
    transform_sequence->AddElement(std::move(element));
  }

  // Animator takes ownership of transform_sequence.
  icon_->layer()->GetAnimator()->StartAnimation(transform_sequence.release());
}

void AuthIconView::RunNudgeAnimation() {
  StopAnimating();

  // Create two separate animation sequences and run in parallel.
  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();
  auto transform_sequence = std::make_unique<ui::LayerAnimationSequence>();

  // Fade out view by gradually setting opacity to 0.
  auto element = ui::LayerAnimationElement::CreateOpacityElement(
      0, kNudgeAnimationScalingDuration);
  element->set_tween_type(gfx::Tween::Type::ACCEL_0_80_DECEL_80);
  opacity_sequence->AddElement(std::move(element));

  // Reset opacity so that |opacity_sequence| can be repeated.
  element = ui::LayerAnimationElement::CreateOpacityElement(kOpacityReset,
                                                            base::TimeDelta());
  opacity_sequence->AddElement(std::move(element));

  element = ui::LayerAnimationElement::CreatePauseElement(
      0, kNudgeAnimationDelayDuration);
  opacity_sequence->AddElement(std::move(element));

  opacity_sequence->set_is_repeating(true);

  // Every time it scales, translate by |center_offset| so that the view scales
  // outward from center point.
  int half_icon_size = kAuthIconSizeDp / 2;
  auto center_offset = gfx::Vector2d(half_icon_size, half_icon_size);

  gfx::Transform transform;
  transform.Translate(center_offset);
  // Make view larger.
  transform.Scale(/*x=*/kTransformScaleUpSize, /*y=*/kTransformScaleUpSize);
  transform.Translate(-center_offset);
  element = ui::LayerAnimationElement::CreateTransformElement(
      transform, kNudgeAnimationScalingDuration);
  element->set_tween_type(gfx::Tween::Type::ACCEL_0_40_DECEL_100);
  transform_sequence->AddElement(std::move(element));

  transform = gfx::Transform();
  transform.Translate(center_offset);
  // Make view smaller.
  transform.Scale(/*x=*/kTransformScaleDownSize, /*y=*/kTransformScaleDownSize);
  transform.Translate(-center_offset);
  element = ui::LayerAnimationElement::CreateTransformElement(
      transform, base::TimeDelta());
  transform_sequence->AddElement(std::move(element));

  element = ui::LayerAnimationElement::CreatePauseElement(
      0, kNudgeAnimationDelayDuration);
  transform_sequence->AddElement(std::move(element));

  transform_sequence->set_is_repeating(true);

  // Animator takes ownership of opacity_sequence and transform_sequence.
  icon_->layer()->GetAnimator()->StartAnimation(opacity_sequence.release());
  icon_->layer()->GetAnimator()->StartAnimation(transform_sequence.release());
}

void AuthIconView::StartProgressAnimation() {
  // Progress animation already running.
  if (progress_animation_timer_.IsRunning()) {
    return;
  }

  progress_animation_start_time_ = base::TimeTicks::Now();
  progress_animation_timer_.Start(
      FROM_HERE, kProgressFrameDuration,
      base::BindRepeating(&AuthIconView::SchedulePaint,
                          base::Unretained(this)));
  SchedulePaint();
}

void AuthIconView::StopProgressAnimation() {
  // Progress already stopped.
  if (!progress_animation_timer_.IsRunning()) {
    return;
  }

  progress_animation_timer_.Stop();
  SchedulePaint();
}

void AuthIconView::StopAnimating() {
  icon_->layer()->GetAnimator()->StopAnimating();
}

void AuthIconView::OnPaint(gfx::Canvas* canvas) {
  // Draw the icon first.
  views::View::OnPaint(canvas);

  // Draw the progress spinner on top if it's currently running.
  if (progress_animation_timer_.IsRunning()) {
    SkColor color = GetColorProvider()->GetColor(cros_tokens::kCrosSysPrimary);
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - progress_animation_start_time_;
    gfx::PaintThrobberSpinning(canvas, GetContentsBounds(), color, elapsed_time,
                               kProgressAnimationStrokeWidth);
  }
}

gfx::Size AuthIconView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kAuthIconViewDp, kAuthIconViewDp);
}

void AuthIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::EventType::kGestureTap &&
      event->type() != ui::EventType::kGestureTapDown) {
    return;
  }

  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
  }
}

bool AuthIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
    return true;
  }
  return false;
}

AuthIconView::CircleImageSource::CircleImageSource(int size, SkColor color)
    : gfx::CanvasImageSource(gfx::Size(size, size)), color_(color) {}

void AuthIconView::CircleImageSource::Draw(gfx::Canvas* canvas) {
  float radius = size().width() / 2.0f;
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(color_);
  canvas->DrawCircle(gfx::PointF(radius, radius), radius, flags);
}

BEGIN_METADATA(AuthIconView)
END_METADATA

}  // namespace ash
