// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/animated_container_view.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/main_stage/element_animator.h"
#include "ui/compositor/callback_layer_animation_observer.h"

namespace ash {

AnimatedContainerView::AnimatedContainerView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  // The AssistantViewDelegate should outlive AnimatedContainerView.
  delegate_->AddInteractionModelObserver(this);
}

AnimatedContainerView::~AnimatedContainerView() {
  delegate_->RemoveInteractionModelObserver(this);
}

void AnimatedContainerView::AddElementAnimator(
    std::unique_ptr<ElementAnimator> view_animator) {
  DCHECK_EQ(view_animator->view()->parent(), content_view());
  animators_.push_back(std::move(view_animator));
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
}

void AnimatedContainerView::RemoveAllViews() {
  // We explicitly abort all in progress animations here because we will remove
  // their views immediately and we want to ensure that any animation observers
  // will be notified of an abort, not an animation completion.  Otherwise there
  // is potential to enter into a bad state (see crbug/952996).
  for (const auto& animator : animators_)
    animator->AbortAnimation();

  // We can prevent over-propagation of the PreferredSizeChanged event by
  // stopping propagation during batched view hierarchy add/remove operations.
  SetPropagatePreferredSizeChanged(false);
  animators_.clear();
  content_view()->RemoveAllChildViews(/*delete_children=*/true);
  SetPropagatePreferredSizeChanged(true);

  // We inform our derived class all views have been removed.
  OnAllViewsRemoved();

  // Once the response has been cleared from the stage, we are free to release
  // our shared pointer. This allows resources associated with the underlying
  // views to be freed, provided there are no other usages.
  response_.reset();
}

void AnimatedContainerView::AnimateIn() {
  // We don't allow processing of events while animating.
  set_can_process_events_within_subtree(false);

  auto* animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          AnimatedContainerView::AnimateInObserverCallback,
          weak_factory_.GetWeakPtr()));

  for (const auto& animator : animators_)
    animator->AnimateIn(animation_observer);

  // Set the observer to active so that we receive callback events.
  animation_observer->SetActive();
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

void AnimatedContainerView::FadeOutViews() {
  // If there's already an animation in progress, there's nothing for us to do.
  if (fade_out_in_progress_)
    return;

  // We don't allow processing of events while waiting for the next query
  // response. The contents will be faded out, so it should not be interactive.
  set_can_process_events_within_subtree(false);

  fade_out_in_progress_ = true;

  auto* animation_observer = new ui::CallbackLayerAnimationObserver(
      /*animation_ended_callback=*/base::BindRepeating(
          AnimatedContainerView::FadeOutObserverCallback,
          weak_factory_.GetWeakPtr()));

  for (const auto& animator : animators_)
    animator->FadeOut(animation_observer);

  // Set the observer to active so that we receive callback events.
  animation_observer->SetActive();
}

void AnimatedContainerView::ChangeResponse(
    const scoped_refptr<const AssistantResponse>& response) {
  // We may have to pend the response while we animate the previous response off
  // stage. We use a shared pointer to ensure that any views we add to the view
  // hierarchy can be removed before the underlying views are destroyed.
  pending_response_ = response;

  // If we are currently fading out the old content, don't interrupt it.
  // When the fading out is completed, it will detect we've got a pending
  // response and animate it in.
  if (fade_out_in_progress_)
    return;

  // If we don't have any pre-existing content, there is nothing to animate off
  // stage so we can proceed to add the new response.
  if (content_view()->children().empty()) {
    AddResponse(std::move(pending_response_));
    return;
  }

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
  // The response should be fully processed before it is presented.
  DCHECK_EQ(AssistantResponse::ProcessingState::kProcessed,
            response->processing_state());
  // All children should be animated out and removed before the new response is
  // added.
  DCHECK(content_view()->children().empty());

  // We cache a reference to the |response| to ensure that the instance is not
  // destroyed before we have removed associated views from the view hierarchy.
  response_ = std::move(response);

  // Because the views for the response are animated in together, we can stop
  // propagation of PreferredSizeChanged events until all views have been added
  // to the view hierarchy to reduce layout passes.
  SetPropagatePreferredSizeChanged(false);

  HandleResponse(*response_);

  // Now that the response for the current query has been added to the view
  // hierarchy, we can restart propagation of PreferredSizeChanged events since
  // all views have been added to the view hierarchy. Note we do not re-enable
  // processing of events yet, as that will happen once the enter animations
  // have completed.
  SetPropagatePreferredSizeChanged(true);

  // Now that we've received and added all views for the current query response,
  // we can animate them in.
  AnimateIn();
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

  // Now that all views have been animated in we make it interactive.
  weak_ptr->set_can_process_events_within_subtree(true);

  // Inform our derived class all views have been animated in.
  weak_ptr->OnAllViewsAnimatedIn();

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

  // If the exit animation was aborted, we just return true to delete our
  // observer. No further action is needed.
  if (observer.aborted_count())
    return true;

  // All views have finished their exit animations so it's safe to perform
  // clearing of their views and managed resources.
  weak_ptr->RemoveAllViews();

  // It is safe to add our pending response, if one exists, to the view
  // hierarchy now that we've cleared the previous response from the stage.
  if (weak_ptr->pending_response_)
    weak_ptr->AddResponse(std::move(weak_ptr->pending_response_));

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
  if (weak_ptr->pending_response_)
    weak_ptr->ChangeResponse(std::move(weak_ptr->pending_response_));

  // We return true to delete our observer.
  return true;
}

}  // namespace ash
