// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/animated_container_view.h"

#include <utility>

#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

// AnimatedContainerView::ScopedDisablePreferredSizeChanged --------------------

class AnimatedContainerView::ScopedDisablePreferredSizeChanged {
 public:
  explicit ScopedDisablePreferredSizeChanged(AnimatedContainerView* view)
      : view_(view), original_value_(view_->propagate_preferred_size_changed_) {
    view_->SetPropagatePreferredSizeChanged(false);
  }

  ~ScopedDisablePreferredSizeChanged() {
    view_->SetPropagatePreferredSizeChanged(original_value_);
  }

 private:
  const raw_ptr<AnimatedContainerView> view_;
  const bool original_value_;
};

// AnimatedContainerView -------------------------------------------------------

AnimatedContainerView::AnimatedContainerView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantInteractionController::Get()->GetModel()->AddObserver(this);

  AddScrollViewObserver(this);
}

AnimatedContainerView::~AnimatedContainerView() {
  if (response_)
    response_.get()->RemoveObserver(this);

  if (AssistantInteractionController::Get())
    AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);

  RemoveScrollViewObserver(this);
}

void AnimatedContainerView::PreferredSizeChanged() {
  // Because views are added/removed in batches, we attempt to prevent
  // over-propagation of the PreferredSizeChanged event during batched view
  // hierarchy add/remove operations. This helps to reduce layout passes.
  if (propagate_preferred_size_changed_)
    AssistantScrollView::PreferredSizeChanged();
}

void AnimatedContainerView::OnChildViewRemoved(View* observed_view,
                                               View* child) {
  for (auto it = animators_.begin(); it != animators_.end(); ++it) {
    if (it->get()->view() == child) {
      animators_.erase(it);
      return;
    }
  }
}

void AnimatedContainerView::OnAssistantControllerDestroying() {
  AssistantInteractionController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AnimatedContainerView::OnCommittedQueryChanged(
    const AssistantQuery& query) {
  FadeOutViews();
}

void AnimatedContainerView::OnResponseChanged(
    const scoped_refptr<AssistantResponse>& response) {
  ChangeResponse(response);
}

void AnimatedContainerView::OnResponseCleared() {
  RemoveAllViews();
  queued_response_ = nullptr;
}

void AnimatedContainerView::OnUiElementAdded(
    const AssistantUiElement* ui_element) {
  std::unique_ptr<ElementAnimator> animator = HandleUiElement(ui_element);
  if (animator)
    AddElementAnimatorAndAnimateInView(std::move(animator));
}

void AnimatedContainerView::OnSuggestionsAdded(
    const std::vector<AssistantSuggestion>& suggestions) {
  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  ScopedDisablePreferredSizeChanged disable_preferred_size_changed(this);
  for (const auto& suggestion : suggestions) {
    auto animator = HandleSuggestion(suggestion);
    if (animator)
      AddElementAnimatorAndAnimateInView(std::move(animator));
  }
}

void AnimatedContainerView::RemoveAllViews() {
  if (response_)
    response_.get()->RemoveObserver(this);

  // We explicitly abort all in progress animations here because we will remove
  // their views immediately and we want to ensure that any animation observers
  // will be notified of an abort, not an animation completion.  Otherwise there
  // is potential to enter into a bad state (see crbug/952996).
  for (const auto& animator : animators_) {
    animator->AbortAnimation();
    // TODO(b/237704325): Fix ChromeVox focusing on removed chip views
    animator->view()->SetVisible(false);
  }

  animators_.clear();

  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  ScopedDisablePreferredSizeChanged disable_preferred_size_changed(this);
  content_view()->RemoveAllChildViews();

  // We inform our derived class all views have been removed.
  OnAllViewsRemoved();

  // Once the response has been cleared from the stage, we are free to release
  // our shared pointer. This allows resources associated with the underlying
  // views to be freed, provided there are no other usages.
  response_.reset();
}

void AnimatedContainerView::SetPropagatePreferredSizeChanged(bool propagate) {
  if (propagate == propagate_preferred_size_changed_)
    return;

  propagate_preferred_size_changed_ = propagate;

  // When we are no longer stopping propagation of PreferredSizeChanged events,
  // we fire an event off to ensure the view hierarchy is properly laid out.
  if (propagate_preferred_size_changed_)
    PreferredSizeChanged();
}

std::unique_ptr<ElementAnimator> AnimatedContainerView::HandleUiElement(
    const AssistantUiElement* ui_element) {
  return nullptr;
}

std::unique_ptr<ElementAnimator> AnimatedContainerView::HandleSuggestion(
    const AssistantSuggestion& suggestion) {
  return nullptr;
}

void AnimatedContainerView::ChangeResponse(
    const scoped_refptr<const AssistantResponse>& response) {
  if (response_)
    response_.get()->RemoveObserver(this);

  // We may have to postpone the response while we animate the previous response
  // off stage. We use a shared pointer to ensure that any views we add to the
  // view hierarchy can be removed before the underlying views are destroyed.
  queued_response_ = response;

  // If we are currently animating-/fading-out the old content, don't interrupt
  // it. When the animating-/fading-out is completed, it will detect we've got a
  // queued response and animate it in.
  if (animate_out_in_progress_ || fade_out_in_progress_)
    return;

  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we can proceed to add the new response.
  if (content_view()->children().empty()) {
    AddResponse(std::move(queued_response_));
    return;
  }

  animate_out_in_progress_ = true;

  // There is a previous response on stage, so we'll animate it off before
  // adding the new response. The new response will be added upon invocation
  // of the exit animation ended callback.
  auto* animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          AnimatedContainerView::AnimateOutObserverCallback,
          weak_factory_.GetWeakPtr()));

  for (const auto& animator : animators_)
    animator->AnimateOut(animation_observer);

  // Set the observer to active so that we receive callback events.
  animation_observer->SetActive();
}

void AnimatedContainerView::AddResponse(
    scoped_refptr<const AssistantResponse> response) {
  // All children should be animated out and removed before the new response is
  // added.
  DCHECK(content_view()->children().empty());

  // We cache a reference to the |response| to ensure that the instance is not
  // destroyed before we have removed associated views from the view hierarchy.
  response_ = std::move(response);

  // In response processing v2, we observe the |response_| so that we handle
  // new suggestions and UI elements that continue to stream in.
  response_.get()->AddObserver(this);

  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  ScopedDisablePreferredSizeChanged disable_preferred_size_changed(this);

  // Create views/animators for the suggestions and UI elements belonging to the
  // |response_|. Note that this will also cause them to begin animating in.
  OnSuggestionsAdded(response_->GetSuggestions());
  for (const auto& ui_element : response_->GetUiElements())
    OnUiElementAdded(ui_element.get());
}

bool AnimatedContainerView::IsAnimatingViews() const {
  return base::ranges::any_of(
      animators_, [](const std::unique_ptr<ElementAnimator>& animator) {
        return animator->layer()->GetAnimator()->is_animating();
      });
}

void AnimatedContainerView::AddElementAnimatorAndAnimateInView(
    std::unique_ptr<ElementAnimator> animator) {
  DCHECK_EQ(animator->view()->parent(), content_view());
  animators_.push_back(std::move(animator));

  // We don't allow interactions while animating.
  DisableInteractions();

  auto* animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          AnimatedContainerView::AnimateInObserverCallback,
          weak_factory_.GetWeakPtr()));

  // Start animating in the view.
  animators_.back()->AnimateIn(animation_observer);

  // Set the observer to active so that we receive callback events.
  animation_observer->SetActive();
}

void AnimatedContainerView::FadeOutViews() {
  // If there's already an animation in progress, there's nothing for us to do.
  if (fade_out_in_progress_)
    return;

  fade_out_in_progress_ = true;

  // We don't allow interactions while waiting for the next query response. The
  // contents will be faded out, so it should not be interactive.
  DisableInteractions();

  auto* animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          AnimatedContainerView::FadeOutObserverCallback,
          weak_factory_.GetWeakPtr()));

  for (const auto& animator : animators_)
    animator->FadeOut(animation_observer);

  // Set the observer to active so that we receive callback events.
  animation_observer->SetActive();
}

void AnimatedContainerView::SetInteractionsEnabled(bool enabled) {
  SetCanProcessEventsWithinSubtree(enabled);
  // We also need to enable/disable the individual views, to enable/disable
  // processing of key events.
  for (const auto& animator : animators_)
    animator->view()->SetEnabled(enabled);
}

bool AnimatedContainerView::AnimateInObserverCallback(
    const base::WeakPtr<AnimatedContainerView>& weak_ptr,
    const ui::CallbackLayerAnimationObserver& observer) {
  // If the AnimatedContainerView is destroyed we just return true to delete our
  // observer. No further action is needed.
  if (!weak_ptr)
    return true;

  // If the animation was aborted, we just return true to delete our observer.
  // No further action is needed.
  if (observer.aborted_count())
    return true;

  // If there are no further animations in progress, we can make our view
  // interactive again and notify derived classes that all views have animated
  // in. Note that in response processing v2, another animation may have kicked
  // off prior to this animation finishing. Once all animations have completed
  // interactivity will be restored and derivate classes notified.
  if (!weak_ptr->IsAnimatingViews()) {
    weak_ptr->EnableInteractions();
    weak_ptr->OnAllViewsAnimatedIn();
  }

  // We return true to delete our observer.
  return true;
}

bool AnimatedContainerView::AnimateOutObserverCallback(
    const base::WeakPtr<AnimatedContainerView>& weak_ptr,
    const ui::CallbackLayerAnimationObserver& observer) {
  // If the AnimatedContainerView is destroyed we just return true to delete our
  // observer. No further action is needed.
  if (!weak_ptr)
    return true;

  weak_ptr->animate_out_in_progress_ = false;

  // If the exit animation was aborted, we just return true to delete our
  // observer. No further action is needed.
  if (observer.aborted_count())
    return true;

  // All views have finished their exit animations so it's safe to perform
  // clearing of their views and managed resources.
  weak_ptr->RemoveAllViews();

  // It is safe to add our queued response, if one exists, to the view
  // hierarchy now that we've cleared the previous response from the stage.
  if (weak_ptr->queued_response_)
    weak_ptr->AddResponse(std::move(weak_ptr->queued_response_));

  // We return true to delete our observer.
  return true;
}

bool AnimatedContainerView::FadeOutObserverCallback(
    const base::WeakPtr<AnimatedContainerView>& weak_ptr,
    const ui::CallbackLayerAnimationObserver& observer) {
  // If the AnimatedContainerView is destroyed we just return true to delete our
  // observer. No further action is needed.
  if (!weak_ptr)
    return true;

  weak_ptr->fade_out_in_progress_ = false;

  // If the exit animation was aborted, we just return true to delete our
  // observer. No further action is needed.
  if (observer.aborted_count())
    return true;

  // If the new response arrived while the fade-out was in progress, we will
  // start handling it now.
  if (weak_ptr->queued_response_)
    weak_ptr->ChangeResponse(std::move(weak_ptr->queued_response_));

  // We return true to delete our observer.
  return true;
}

BEGIN_METADATA(AnimatedContainerView)
END_METADATA

}  // namespace ash
