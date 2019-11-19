// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_footer_view.h"

#include <utility>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"
#include "ash/assistant/ui/main_stage/suggestion_container_view.h"
#include "ash/assistant/util/animation_util.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 48;

// Animation.
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(167);

}  // namespace

AssistantFooterView::AssistantFooterView(AssistantViewDelegate* delegate)
    : delegate_(delegate),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*animation_started_callback=*/base::BindRepeating(
              &AssistantFooterView::OnAnimationStarted,
              base::Unretained(this)),
          /*animation_ended_callback=*/base::BindRepeating(
              &AssistantFooterView::OnAnimationEnded,
              base::Unretained(this)))) {
  SetID(AssistantViewID::kFooterView);
  InitLayout();
  AssistantState::Get()->AddObserver(this);
}

AssistantFooterView::~AssistantFooterView() {
  AssistantState::Get()->RemoveObserver(this);
}

const char* AssistantFooterView::GetClassName() const {
  return "AssistantFooterView";
}

gfx::Size AssistantFooterView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int AssistantFooterView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantFooterView::InitLayout() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Initial view state is based on user consent state.
  const bool consent_given =
      AssistantState::Get()->consent_status().value_or(
          chromeos::assistant::prefs::ConsentStatus::kUnknown) ==
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted;

  // Suggestion container.
  suggestion_container_ = new SuggestionContainerView(delegate_);
  suggestion_container_->set_can_process_events_within_subtree(consent_given);

  // Suggestion container will be animated on its own layer.
  suggestion_container_->SetPaintToLayer();
  suggestion_container_->layer()->SetFillsBoundsOpaquely(false);
  suggestion_container_->layer()->SetOpacity(consent_given ? 1.f : 0.f);
  suggestion_container_->SetVisible(consent_given);

  AddChildView(suggestion_container_);

  // Opt in view.
  opt_in_view_ = new AssistantOptInView(delegate_);
  opt_in_view_->set_can_process_events_within_subtree(!consent_given);

  // Opt in view will be animated on its own layer.
  opt_in_view_->SetPaintToLayer();
  opt_in_view_->layer()->SetFillsBoundsOpaquely(false);
  opt_in_view_->layer()->SetOpacity(consent_given ? 0.f : 1.f);
  opt_in_view_->SetVisible(!consent_given);

  AddChildView(opt_in_view_);
}

void AssistantFooterView::OnAssistantConsentStatusChanged(int consent_status) {
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;
  using assistant::util::StartLayerAnimationSequence;

  const bool consent_given =
      consent_status ==
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted;

  // When the consent state changes, we need to hide/show the appropriate views.
  views::View* hide_view =
      consent_given ? static_cast<views::View*>(opt_in_view_)
                    : static_cast<views::View*>(suggestion_container_);

  views::View* show_view =
      consent_given ? static_cast<views::View*>(suggestion_container_)
                    : static_cast<views::View*>(opt_in_view_);

  // Reset visibility to enable animation.
  hide_view->SetVisible(true);
  show_view->SetVisible(true);

  // Hide the view for the previous consent state by fading to 0% opacity.
  StartLayerAnimationSequence(hide_view->layer()->GetAnimator(),
                              CreateLayerAnimationSequence(CreateOpacityElement(
                                  0.f, kAnimationFadeOutDuration)),
                              // Observe the animation.
                              animation_observer_.get());

  // Show the view for the next consent state by fading to 100% opacity with
  // delay.
  StartLayerAnimationSequence(
      show_view->layer()->GetAnimator(),
      CreateLayerAnimationSequence(
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kAnimationFadeInDelay),
          CreateOpacityElement(1.f, kAnimationFadeInDuration)),
      // Observe the animation.
      animation_observer_.get());

  // Set the observer to active to receive animation callback events.
  animation_observer_->SetActive();
}

void AssistantFooterView::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  // Our views should not process events while animating.
  suggestion_container_->set_can_process_events_within_subtree(false);
  opt_in_view_->set_can_process_events_within_subtree(false);
}

bool AssistantFooterView::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  const bool consent_given =
      AssistantState::Get()->consent_status().value_or(
          chromeos::assistant::prefs::ConsentStatus::kUnknown) ==
      chromeos::assistant::prefs::ConsentStatus::kActivityControlAccepted;

  // Only the view relevant to our consent state should process events.
  suggestion_container_->set_can_process_events_within_subtree(consent_given);
  suggestion_container_->SetVisible(consent_given);
  opt_in_view_->set_can_process_events_within_subtree(!consent_given);
  opt_in_view_->SetVisible(!consent_given);

  // Return false to prevent the observer from destroying itself.
  return false;
}

}  // namespace ash
