// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/animated_auth_factors_label_wrapper.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

// TODO(b/219594317): Const variables identical to the ones used in
// LoginAuthFactorsView should be factored in a single location.
constexpr int kAuthFactorsViewWidthDp = 280;
constexpr int kSpacingBetweenIconsAndLabelDp = 8;
constexpr int kLabelAnimationOffsetDp = 20;
constexpr base::TimeDelta kLabelAnimationDuration = base::Milliseconds(300);
constexpr base::TimeDelta kLabelAnimationPreviousLabelFadeOutDuration =
    kLabelAnimationDuration / 6;
constexpr base::TimeDelta kLabelAnimationCurrentLabelFadeInDuration =
    kLabelAnimationDuration / 2;
constexpr base::TimeDelta kLabelAnimationCurrentLabelFadeInDelay =
    kLabelAnimationDuration / 6;
// In English, the number of lines should not exceed 2. kLabelMaxLines is set to
// 3 in order to prevent truncation in other languages (e.g., Persian, Dutch).
constexpr int kLabelMaxLines = 3;
constexpr int kLabelLineHeightDp = 20;
constexpr int kLabelWrapperHeightDp = kLabelMaxLines * kLabelLineHeightDp;

class AuthFactorsLabel : public views::Label {
  METADATA_HEADER(AuthFactorsLabel, views::Label)

 public:
  AuthFactorsLabel(bool visible_to_screen_reader) {
    SetSubpixelRenderingEnabled(false);
    SetAutoColorReadabilityEnabled(false);
    SetEnabledColorId(kColorAshTextColorSecondary);
    SetMultiLine(true);
    SetMaxLines(kLabelMaxLines);
    SetLineHeight(kLabelLineHeightDp);
    SizeToFit(kAuthFactorsViewWidthDp);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_TOP);
    GetViewAccessibility().SetIsIgnored(!visible_to_screen_reader);
  }

  AuthFactorsLabel(const AuthFactorsLabel&) = delete;
  AuthFactorsLabel& operator=(const AuthFactorsLabel&) = delete;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kAuthFactorsViewWidthDp, kLabelWrapperHeightDp);
  }
};

BEGIN_METADATA(AuthFactorsLabel)
END_METADATA

}  // namespace

AnimatedAuthFactorsLabelWrapper::AnimatedAuthFactorsLabelWrapper() {
  SetUseDefaultFillLayout(true);

  previous_label_ = AddChildView(
      std::make_unique<AuthFactorsLabel>(/*visible_to_screen_reader=*/false));
  previous_label_->SetPaintToLayer();
  previous_label_->layer()->SetFillsBoundsOpaquely(false);
  previous_label_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  previous_label_->layer()->GetAnimator()->SetOpacity(0.0);

  current_label_ = AddChildView(
      std::make_unique<AuthFactorsLabel>(/*visible_to_screen_reader=*/true));
  current_label_->SetPaintToLayer();
  current_label_->layer()->SetFillsBoundsOpaquely(false);
  current_label_->layer()->GetAnimator()->set_preemption_strategy(
      ui::LayerAnimator::PreemptionStrategy::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

AnimatedAuthFactorsLabelWrapper::~AnimatedAuthFactorsLabelWrapper() = default;

void AnimatedAuthFactorsLabelWrapper::SetLabelTextAndAccessibleName(
    int label_id,
    int accessible_name_id,
    bool animate) {
  if (label_id == previous_label_id_ &&
      accessible_name_id == previous_accessible_name_id_) {
    // If both are unchanged, avoid animating.
    return;
  }
  // Ensure that both are changed; it's unexpected that only one be changed.
  DCHECK(label_id != previous_label_id_ &&
         accessible_name_id != previous_accessible_name_id_);

  previous_label_id_ = label_id;
  previous_accessible_name_id_ = accessible_name_id;
  std::u16string previous_text = current_label_->GetText();

  current_label_->SetText(l10n_util::GetStringUTF16(label_id));
  current_label_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(accessible_name_id));
  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(kSpacingBetweenIconsAndLabelDp, 0, 0, 0));

  // If |previous_text_| is empty, then this is the first time the text is
  // being set. Avoid animating because it looks janky to have an animation in
  // progress when the lock screen first becomes visible.
  if (!animate || previous_text.empty()) {
    return;
  }

  // Set the text/transform/opacity of the previous label to match the
  // appearance of the current label before the animation.
  previous_label_->SetText(std::move(previous_text));
  previous_label_->layer()->GetAnimator()->SetTransform(gfx::Transform());
  previous_label_->layer()->GetAnimator()->SetOpacity(1.0);

  // Hide the current label and move it downward to prepare for fading/sliding
  // it upward.
  gfx::Transform label_transform;
  label_transform.Translate(/*x=*/0, /*y=*/kLabelAnimationOffsetDp);
  current_label_->layer()->GetAnimator()->SetTransform(label_transform);
  current_label_->layer()->GetAnimator()->SetOpacity(0.0);

  // Prepare an animation sequence that will slide the previous label upward.
  // Note: ACCEL_20_DECEL_100 is equivalent to cubic-bezier(0.20, 0, 0, 1.0).
  auto previous_label_transform_seq =
      std::make_unique<ui::LayerAnimationSequence>();
  gfx::Transform previous_label_transform;
  previous_label_transform.Translate(/*x=*/0,
                                     /*y=*/-kLabelAnimationOffsetDp);
  auto transform_element = ui::LayerAnimationElement::CreateTransformElement(
      previous_label_transform, kLabelAnimationDuration);
  transform_element->set_tween_type(gfx::Tween::Type::ACCEL_20_DECEL_100);
  previous_label_transform_seq->AddElement(std::move(transform_element));

  // Prepare an animation sequence that will fade out the previous label.
  auto previous_label_opacity_seq =
      std::make_unique<ui::LayerAnimationSequence>();
  auto opacity_element = ui::LayerAnimationElement::CreateOpacityElement(
      0.0, kLabelAnimationPreviousLabelFadeOutDuration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);
  previous_label_opacity_seq->AddElement(std::move(opacity_element));

  // Prepare an animation sequence that will slide the current label upward.
  auto current_label_transform_seq =
      std::make_unique<ui::LayerAnimationSequence>();
  transform_element = ui::LayerAnimationElement::CreateTransformElement(
      gfx::Transform(), kLabelAnimationDuration);
  transform_element->set_tween_type(gfx::Tween::Type::ACCEL_20_DECEL_100);
  current_label_transform_seq->AddElement(std::move(transform_element));

  // Prepare an animation sequence that will fade in the current label.
  auto current_label_opacity_seq =
      std::make_unique<ui::LayerAnimationSequence>();
  current_label_opacity_seq->AddElement(
      ui::LayerAnimationElement::CreateOpacityElement(
          0.0, kLabelAnimationCurrentLabelFadeInDelay));
  opacity_element = ui::LayerAnimationElement::CreateOpacityElement(
      1.0, kLabelAnimationCurrentLabelFadeInDuration);
  opacity_element->set_tween_type(gfx::Tween::Type::LINEAR);
  current_label_opacity_seq->AddElement(std::move(opacity_element));

  // Apply the animations. The layer animators take ownership of of the
  // animation sequences.
  previous_label_->layer()->GetAnimator()->StartAnimation(
      previous_label_transform_seq.release());
  previous_label_->layer()->GetAnimator()->StartAnimation(
      previous_label_opacity_seq.release());
  current_label_->layer()->GetAnimator()->StartAnimation(
      current_label_transform_seq.release());
  current_label_->layer()->GetAnimator()->StartAnimation(
      current_label_opacity_seq.release());
}

gfx::Size AnimatedAuthFactorsLabelWrapper::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kAuthFactorsViewWidthDp, kLabelWrapperHeightDp);
}

BEGIN_METADATA(AnimatedAuthFactorsLabelWrapper)
END_METADATA

}  // namespace ash
