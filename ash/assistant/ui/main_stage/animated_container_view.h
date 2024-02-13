// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_response_observer.h"
#include "ash/assistant/ui/base/assistant_scroll_view.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_controller_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace ash {

class AssistantResponse;
class AssistantUiElement;
class AssistantViewDelegate;
class ElementAnimator;

// A view that will observe the AssistantResponse and which will use
// ElementAnimator to animate each child view.
//
// To use this you should implement HandleUiElement() and/or HandleSuggestion()
// to:
//    - Add new child views as appropriate.
//    - Return animators for any newly created views.
//
// More in detail, this is what will happen:
//    1) When AssistantInteractionModelObserver::OnCommittedQueryChanged() is
//       observed, FadeOut() is called on all ElementAnimator instances.
//       Furthermore all views will stop processing events (like click).
//    2) When AssistantInteractionModelObserver::OnResponseChanged() is
//       observed, we wait until the FadeOut() animations are complete.
//    3) Next AnimateOut() is invoked on all ElementAnimator instances.
//    4) When these animations are complete, all child views are removed.
//    5) OnAllViewsRemoved() is invoked to inform the derived class all child
//       views are removed.
//    6) Next HandleSuggestion() and HandleUiElement() is called for the new
//       AssistantResponse. In those methods, the derived class should add the
//       child views for the new response, as well as return ElementAnimator
//       instances.
//    7) When all new child views have been added, AnimateIn() is invoked on
//       all ElementAnimator instances.
//    8) Finally when this animation is complete the derived class is informed
//       through OnAllViewsAnimatedIn().
class COMPONENT_EXPORT(ASSISTANT_UI) AnimatedContainerView
    : public AssistantScrollView,
      public AssistantScrollView::Observer,
      public AssistantControllerObserver,
      public AssistantInteractionModelObserver,
      public AssistantResponseObserver {
  METADATA_HEADER(AnimatedContainerView, AssistantScrollView)

 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  explicit AnimatedContainerView(AssistantViewDelegate* delegate);
  AnimatedContainerView(const AnimatedContainerView&) = delete;
  AnimatedContainerView& operator=(const AnimatedContainerView&) = delete;
  ~AnimatedContainerView() override;

  // AssistantScrollView:
  void PreferredSizeChanged() override;
  void OnChildViewRemoved(View* observed_view, View* child) override;

  // AssistantControllerObserver:
  void OnAssistantControllerDestroying() override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnResponseChanged(const scoped_refptr<AssistantResponse>&) override;
  void OnResponseCleared() override;

  // AssistantResponseObserver:
  void OnUiElementAdded(const AssistantUiElement* ui_element) override;
  void OnSuggestionsAdded(
      const std::vector<AssistantSuggestion>& suggestions) override;

  // Remove all current responses/views.
  // This will abort all in progress animations, and remove all the child views
  // and their animators.
  void RemoveAllViews();

 protected:
  // Callback called when all (new) views have been added.
  // This is called when the animate-in animations are done.
  virtual void OnAllViewsAnimatedIn() {}

  // Callback called when all (old) views have been removed.
  // This is called when the exit animations are done.
  virtual void OnAllViewsRemoved() {}

  // Callback called to create a view for a UI element.
  // The implementer should:
  //    - Create and add the appropriate views::View.
  //    - Return an ElementAnimator to animate the view. Note that it is
  //      permissible to return |nullptr| if no managed animation is desired.
  virtual std::unique_ptr<ElementAnimator> HandleUiElement(
      const AssistantUiElement* ui_element);

  // Callback called to create a view for a suggestion.
  // The implementer should:
  //    - Create and add the appropriate views::View.
  //    - Return an ElementAnimator to animate the view. Note that it is
  //      permissible to return |nullptr| if no managed animation is desired.
  virtual std::unique_ptr<ElementAnimator> HandleSuggestion(
      const AssistantSuggestion& suggestion);

  AssistantViewDelegate* delegate() { return delegate_; }

 private:
  class ScopedDisablePreferredSizeChanged;
  void SetPropagatePreferredSizeChanged(bool propagate);

  void ChangeResponse(const scoped_refptr<const AssistantResponse>& response);
  void AddResponse(scoped_refptr<const AssistantResponse> response);

  bool IsAnimatingViews() const;
  void AddElementAnimatorAndAnimateInView(std::unique_ptr<ElementAnimator>);
  void FadeOutViews();

  void EnableInteractions() { SetInteractionsEnabled(true); }
  void DisableInteractions() { SetInteractionsEnabled(false); }
  void SetInteractionsEnabled(bool enabled);

  static bool AnimateInObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);
  static bool AnimateOutObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);
  static bool FadeOutObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);

  const raw_ptr<AssistantViewDelegate>
      delegate_;  // Owned by AssistantController.

  // The animators used to trigger the animations of the individual views.
  std::vector<std::unique_ptr<ElementAnimator>> animators_;

  // Whether we should allow propagation of PreferredSizeChanged events.
  // Because we only animate views in/out in batches, we can prevent
  // over-propagation of PreferredSizeChanged events by waiting until the
  // entirety of a response has been added/removed before propagating. This
  // reduces layout passes.
  bool propagate_preferred_size_changed_ = true;

  // Whether the animate-out animation is in progress.
  bool animate_out_in_progress_ = false;

  // Whether the fade-out animation is in progress.
  bool fade_out_in_progress_ = false;

  // Shared pointers to the response that is currently on stage as well as the
  // queued response to be presented following the former's animated exit. We
  // use shared pointers to ensure that underlying views are not destroyed
  // before we have an opportunity to remove their associated views.
  scoped_refptr<const AssistantResponse> response_;
  scoped_refptr<const AssistantResponse> queued_response_;

  base::ScopedObservation<AssistantController, AssistantControllerObserver>
      assistant_controller_observation_{this};

  base::WeakPtrFactory<AnimatedContainerView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_
