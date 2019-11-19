// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_

#include <memory>
#include <vector>

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/ui/base/assistant_scroll_view.h"

namespace ui {
class CallbackLayerAnimationObserver;
}  // namespace ui

namespace ash {

class AssistantViewDelegate;
class AssistantResponse;
class ElementAnimator;

// A view that will observe the |AssistantResponse| and which will use
// |ElementAnimator| to animate each child view.
//
// To use this you must implement |HandleResponse| and in there
//    - Add the new child views for the given |AssistantResponse|.
//    - Add animators for the new view by calling |AddElementAnimator|.
//
// More in detail, this is what will happen:
//    1) When |AssistantInteractionModelObserver::OnCommittedQueryChanged| is
//       observed, |FadeOut| is called on all |ElementAnimator| instances.
//       Furthermore all views will stop processing events (like click).
//    2) When |AssistantInteractionModelObserver::OnResponseChanged| is
//       observed, we wait until the |FadeOut| animations are complete.
//    3) Next |AnimateOut| is invoked on all |ElementAnimator| instances.
//    4) When these animations are complete, all child views are removed.
//    5) |AnimatedContainerView::OnAllViewsRemoved| is invoked to inform
//       the derived class all child views are removed.
//    6) Next |AnimatedContainerView::HandleResponse| is called with the new
//       |AssistantResponse|.
//       In here the derived class should add the child views for the new
//       response, as well as adding |ElementAnimator| instances by calling
//       |AnimatedContainerView::AddElementAnimator|.
//    7) When all new child views have been added, |AnimateIn| is invoked on
//       all |ElementAnimator| instances.
//    8) Finally when this animation is complete the derived class is informed
//       through |AnimatedContainerView::OnAllViewsAnimatedIn|.
class COMPONENT_EXPORT(ASSISTANT_UI) AnimatedContainerView
    : public AssistantScrollView,
      public AssistantInteractionModelObserver {
 public:
  explicit AnimatedContainerView(AssistantViewDelegate* delegate);
  ~AnimatedContainerView() override;

  // Add an animator for a view that is displayed in this content view.
  // Should be called for each view that needs to be animated, and is usually
  // called from inside the |HandleResponse| callback.
  void AddElementAnimator(std::unique_ptr<ElementAnimator> view_animator);

  // AssistantScrollView:
  void PreferredSizeChanged() override;
  void OnChildViewRemoved(View* observed_view, View* child) override;

  // AssistantInteractionModelObserver:
  void OnCommittedQueryChanged(const AssistantQuery& query) override;
  void OnResponseChanged(
      const scoped_refptr<AssistantResponse>& response) override;
  void OnResponseCleared() override;

  // Remove all current responses/views.
  // This will abort all in progress animations, and remove all the child views
  // and their animators.
  void RemoveAllViews();

  // Manually trigger the animate-in animation.
  // Should only be used if you call |AddElementAnimator| outside of the
  // |HandleResponse| callback.
  void AnimateIn();

 protected:
  // Callback called when all (new) views have been added.
  // This is called when the animate-in animations are done.
  virtual void OnAllViewsAnimatedIn() {}

  // Callback called when all (old) views have been removed.
  // This is called when the exit animations are done.
  virtual void OnAllViewsRemoved() {}

  // Callback called to create the new views.
  // For each new views it should
  //    - Create and add the |views::View|.
  //    - Call |AddElementAnimator| and pass it the |ElementAnimator| to
  //      animate the view.
  virtual void HandleResponse(const AssistantResponse& response) = 0;

  AssistantViewDelegate* delegate() { return delegate_; }

 private:
  void SetPropagatePreferredSizeChanged(bool propagate);

  void FadeOutViews();
  void ChangeResponse(const scoped_refptr<const AssistantResponse>& response);
  void AddResponse(scoped_refptr<const AssistantResponse> response);

  static bool AnimateInObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);
  static bool AnimateOutObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);
  static bool FadeOutObserverCallback(
      const base::WeakPtr<AnimatedContainerView>& weak_ptr,
      const ui::CallbackLayerAnimationObserver& observer);

  AssistantViewDelegate* const delegate_;  // Owned by AssistantController.

  // The animators used to trigger the animations of the individual views.
  std::vector<std::unique_ptr<ElementAnimator>> animators_;

  // Whether we should allow propagation of PreferredSizeChanged events.
  // Because we only animate views in/out in batches, we can prevent
  // over-propagation of PreferredSizeChanged events by waiting until the
  // entirety of a response has been added/removed before propagating. This
  // reduces layout passes.
  bool propagate_preferred_size_changed_ = true;

  // Whether the fade-out animation is in progress.
  bool fade_out_in_progress_ = false;

  // Shared pointers to the response that is currently on stage as well as the
  // pending response to be presented following the former's animated exit. We
  // use shared pointers to ensure that underlying views are not destroyed
  // before we have an opportunity to remove their associated views.
  scoped_refptr<const AssistantResponse> response_;
  scoped_refptr<const AssistantResponse> pending_response_;

  base::WeakPtrFactory<AnimatedContainerView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AnimatedContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ANIMATED_CONTAINER_VIEW_H_
