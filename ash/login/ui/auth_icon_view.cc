// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/auth_icon_view.h"

#include "ash/login/ui/horizontal_image_sequence_animation_decoder.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

struct ShakeAnimationStep {
  int x_offset;
  int duration_ms;
};

constexpr int kAuthIconSizeDp = 32;

// See spec:
// https://carbon.googleplex.com/cr-os-motion-work/pages/sign-in/undefined/e05c4091-eea2-4c5a-a6f8-38fd37953e7b#a929eb9f-2840-4b37-be52-97d96ca2aafa
constexpr ShakeAnimationStep kShakeAnimationSteps[] = {
    {-5, 83}, {8, 83}, {-7, 66}, {7, 66}, {-7, 66}, {7, 66}, {-3, 83}};

SkColor GetColor(AuthIconView::Color color) {
  switch (color) {
    case AuthIconView::Color::kPrimary:
      return AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary);
    case AuthIconView::Color::kDisabled:
      return AshColorProvider::Get()->GetDisabledColor(
          GetColor(AuthIconView::Color::kPrimary));
    case AuthIconView::Color::kError:
      // TODO(crbug.com/1233614): Either find a system color to match the color
      // in the Fingerprint animation png sequence, or upload new png files with
      // the right color.
      return AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorAlert);
  }
}

}  // namespace

AuthIconView::AuthIconView()
    : AnimatedRoundedImageView(gfx::Size(kAuthIconSizeDp, kAuthIconSizeDp),
                               /*corner_radius=*/0) {
  // Set up layer to allow for animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

AuthIconView::~AuthIconView() = default;

void AuthIconView::SetIcon(const gfx::VectorIcon& icon, Color color) {
  SetImage(gfx::CreateVectorIcon(icon, kAuthIconSizeDp, GetColor(color)));
}

void AuthIconView::SetAnimation(int animation_resource_id,
                                base::TimeDelta duration,
                                int num_frames) {
  SetAnimationDecoder(
      std::make_unique<HorizontalImageSequenceAnimationDecoder>(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              animation_resource_id),
          duration, num_frames),
      AnimatedRoundedImageView::Playback::kSingle);
}

void AuthIconView::RunErrorShakeAnimation() {
  // Stop any existing animation.
  layer()->GetAnimator()->StopAnimating();

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
  layer()->GetAnimator()->StartAnimation(transform_sequence.release());
}

// views::View:
void AuthIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP &&
      event->type() != ui::ET_GESTURE_TAP_DOWN)
    return;

  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
  }
}

// views::View:
bool AuthIconView::OnMousePressed(const ui::MouseEvent& event) {
  if (on_tap_or_click_callback_) {
    on_tap_or_click_callback_.Run();
    return true;
  }
  return false;
}

}  // namespace ash
