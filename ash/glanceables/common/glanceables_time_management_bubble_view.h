// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_
#define ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/glanceables/common/glanceables_error_message_view.h"
#include "base/functional/callback_forward.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"

namespace ash {

// Glanceables Time Management bubble container that is a child of
// `GlanceableTrayChildBubble`.
class ASH_EXPORT GlanceablesTimeManagementBubbleView
    : public views::FlexLayoutView,
      public gfx::AnimationDelegate {
  METADATA_HEADER(GlanceablesTimeManagementBubbleView, views::FlexLayoutView)

 public:
  // The attribute that describes what type this view is used for.
  // Note that the enum values should not be reordered or reused as the values
  // are used in prefs (kGlanceablesTimeManagementLastExpandedBubble).
  enum class Context { kTasks = 0, kClassroom = 1 };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnExpandStateChanged(Context context, bool is_expanded) = 0;
  };

  GlanceablesTimeManagementBubbleView();
  GlanceablesTimeManagementBubbleView(
      const GlanceablesTimeManagementBubbleView&) = delete;
  GlanceablesTimeManagementBubbleView& operator=(
      const GlanceablesTimeManagementBubbleView&) = delete;
  ~GlanceablesTimeManagementBubbleView() override;

  // views::View:
  void ChildPreferredSizeChanged(View* child) override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns the preferred height of `this` in the collapsed state. This is used
  // to calculate the available size for glanceables. This should be constant
  // after the view is laid out.
  virtual int GetCollapsedStatePreferredHeight() const = 0;

  // Returns the expanded/collapsed state of the bubble view.
  virtual bool IsExpanded() const = 0;

  bool is_animating_resize() const {
    return resize_animation_ && resize_animation_->is_animating();
  }

  void SetAnimationEndedClosureForTest(base::OnceClosure closure);

 protected:
  // Linear animation to track time management bubble resize animation - as the
  // animation progresses, the bubble view preferred size will change causing
  // bubble bounds updates. `ResizeAnimation` will provide the expected
  // preferred time management bubble height.
  class ResizeAnimation : public gfx::LinearAnimation {
   public:
    // The context of the animation that determines the type of tweens and
    // duration to use.
    enum class Type {
      kContainerExpandStateChanged,
      kChildResize,
    };

    ResizeAnimation(int start_height,
                    int end_height,
                    gfx::AnimationDelegate* delegate,
                    Type type);

    int GetCurrentHeight() const;

   private:
    const Type type_;

    const int start_height_;
    const int end_height_;
  };

  void SetUpResizeThroughputTracker(const std::string& histogram_name);

  // Removes an active `error_message_` from the view, if any.
  void MaybeDismissErrorMessage();
  void ShowErrorMessage(const std::u16string& error_message,
                        views::Button::PressedCallback callback,
                        GlanceablesErrorMessageView::ButtonActionType type);

  GlanceablesErrorMessageView* error_message() { return error_message_; }

  // Linear animation that drive time management bubble resize animation - the
  // animation updates the time management bubble view preferred size, which
  // causes layout updates. Runs when the bubble preferred size changes.
  std::unique_ptr<ResizeAnimation> resize_animation_;

  base::ObserverList<Observer> observers_;

 private:
  // Measure animation smoothness metrics for `resize_animation_`.
  std::optional<ui::ThroughputTracker> resize_throughput_tracker_;

  // Called when `resize_animation_` ends or is canceled. This is currently only
  // used in test.
  base::OnceClosure resize_animation_ended_closure_;

  // Owned by views hierarchy.
  raw_ptr<GlanceablesErrorMessageView> error_message_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_COMMON_GLANCEABLES_TIME_MANAGEMENT_BUBBLE_VIEW_H_
