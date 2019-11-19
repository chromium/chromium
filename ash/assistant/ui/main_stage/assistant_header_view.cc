// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_header_view.h"

#include <memory>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/logo_view/logo_view.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "base/time/time.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kPaddingHorizontalDip = 32;

// Appear animation.
constexpr base::TimeDelta kAppearAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kAppearAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAppearAnimationTranslateUpDuration =
    base::TimeDelta::FromMilliseconds(250);
constexpr int kAppearAnimationTranslationUpDip = 115;

// Response animation.
constexpr base::TimeDelta kResponseAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(50);
constexpr base::TimeDelta kResponseAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kResponseAnimationFadeOutDelay =
    base::TimeDelta::FromMilliseconds(33);
constexpr base::TimeDelta kResponseAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kResponseAnimationTranslateLeftDuration =
    base::TimeDelta::FromMilliseconds(333);

}  // namespace

AssistantHeaderView::AssistantHeaderView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  InitLayout();

  // The AssistantViewDelegate should outlive AssistantHeaderView.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantHeaderView::~AssistantHeaderView() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantHeaderView::GetClassName() const {
  return "AssistantHeaderView";
}

gfx::Size AssistantHeaderView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantHeaderView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets(0, 0, kSpacingDip, 0)));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Molecule icon.
  molecule_icon_ = LogoView::Create();
  molecule_icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon_->SetState(LogoView::State::kMoleculeWavy,
                           /*animate=*/false);

  // The molecule icon will be animated on its own layer.
  molecule_icon_->SetPaintToLayer();
  molecule_icon_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(molecule_icon_);
}

void AssistantHeaderView::OnResponseChanged(
    const scoped_refptr<AssistantResponse>& response) {
  // We only handle the first response when animating the molecule icon. For
  // all subsequent responses the molecule icon remains unchanged.
  if (!is_first_response_)
    return;

  is_first_response_ = false;

  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::CreateTransformElement;

  // The molecule icon will be animated from the center of its parent, to the
  // left hand side.
  gfx::Transform transform;
  transform.Translate(
      -(width() - molecule_icon_->width()) / 2 + kPaddingHorizontalDip, 0);

  // Animate the molecule icon.
  molecule_icon_->layer()->GetAnimator()->StartTogether(
      {// Animate the translation.
       CreateLayerAnimationSequence(CreateTransformElement(
           transform, kResponseAnimationTranslateLeftDuration)),
       // Animate the opacity.
       CreateLayerAnimationSequence(
           // Pause...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kResponseAnimationFadeOutDelay),
           // ...then fade out...
           CreateOpacityElement(0.f, kResponseAnimationFadeOutDuration),
           // ...hold...
           ui::LayerAnimationElement::CreatePauseElement(
               ui::LayerAnimationElement::AnimatableProperty::OPACITY,
               kResponseAnimationFadeInDelay),
           // ...and fade back in.
           CreateOpacityElement(1.f, kResponseAnimationFadeInDuration))});
}

void AssistantHeaderView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the molecule icon.
    using assistant::util::CreateLayerAnimationSequence;
    using assistant::util::CreateOpacityElement;
    using assistant::util::CreateTransformElement;

    // We're going to animate the molecule icon up into position so we'll need
    // to apply an initial transformation.
    gfx::Transform transform;
    transform.Translate(0, kAppearAnimationTranslationUpDip);

    // Set up our pre-animation values.
    molecule_icon_->layer()->SetOpacity(0.f);
    molecule_icon_->layer()->SetTransform(transform);

    // Start animating molecule icon.
    molecule_icon_->layer()->GetAnimator()->StartTogether(
        {// Animate the transformation.
         CreateLayerAnimationSequence(CreateTransformElement(
             gfx::Transform(), kAppearAnimationTranslateUpDuration,
             gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
         // Animate the opacity to 100% with delay.
         CreateLayerAnimationSequence(
             ui::LayerAnimationElement::CreatePauseElement(
                 ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                 kAppearAnimationFadeInDelay),
             CreateOpacityElement(1.f, kAppearAnimationFadeInDuration))});

    return;
  }

  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  is_first_response_ = true;

  molecule_icon_->layer()->SetTransform(gfx::Transform());
}

}  // namespace ash
