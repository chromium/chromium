// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_

#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Label;
class View;
class Widget;
}  // namespace views

namespace ash {

// Controls the user nudge animation and toast widget which are used to draw the
// user's attention towards the given `view_to_be_highlighted`. In the current
// iteration, this view is the settings button on the capture mode bar, and the
// desire to inform the users about the newly added ability to change where
// captured images and videos are saved. In upcoming iteration, the camera nudge
// will be shown instead once the selfie camera feature is enabled.
class UserNudgeController {
 public:
  explicit UserNudgeController(views::View* view_to_be_highlighted);
  UserNudgeController(const UserNudgeController&) = delete;
  UserNudgeController& operator=(const UserNudgeController&) = delete;
  ~UserNudgeController();

  views::Widget* toast_widget() { return toast_widget_.get(); }
  bool is_visible() const { return is_visible_; }
  void set_should_dismiss_nudge_forever(bool value) {
    should_dismiss_nudge_forever_ = value;
  }

  // Repositions the animation layers and the toast widget such that they're
  // correctly parented with the correct bounds on the correct display.
  void Reposition();

  // Animates the animation layers and the toast widget towards the given
  // visibility state `visible`.
  void SetVisible(bool visible);

 private:
  // Triggers all the nudge animations performed by the below functions.
  void PerformNudgeAnimations();

  // Animates the base ring (which is a circle highlight around the
  // `view_to_be_highlighted_`) in a way that grabs the user's attention.
  void PerformBaseRingAnimation();

  // Animates the ripple ring (which is another circle highlight around the
  // `view_to_be_highlighted_` but animates to a bigger size while fading out).
  void PerformRippleRingAnimation();

  // Scales up the `view_to_be_highlighted_` itself so its icon appears to be
  // growing with the base ring animation.
  void PerformViewScaleAnimation();

  // Called back when the base ring animation finishes so that we can schedule
  // a repeat of this animation after a certain delay using `timer_`.
  void OnBaseRingAnimationEnded();

  // This is the window that will be used to as the parent of the toast widget,
  // and its layer as the parent of our animation layers (`base_ring_` and
  // `ripple_ring_`).
  aura::Window* GetParentWindow() const;

  // Calculates and returns the current screen bounds that should be set on the
  // `toast_widget_` based on where `view_to_be_highlighted_`'s widget (which is
  // the capture bar) is.
  gfx::Rect CalculateToastWidgetScreenBounds() const;

  // Initializes the toast widget and its contents.
  void BuildToastWidget();

  // The view to which we're trying to grab the user's attention.
  views::View* const view_to_be_highlighted_;

  // These are the two animation layers that will be used to highlight
  // `view_to_be_highlighted_` to nudge the user towards it.
  ui::Layer base_ring_{ui::LAYER_SOLID_COLOR};
  ui::Layer ripple_ring_{ui::LAYER_SOLID_COLOR};

  // The toast widget and its contents view.
  views::UniqueWidgetPtr toast_widget_ = std::make_unique<views::Widget>();
  views::Label* toast_label_view_ = nullptr;

  // The timer used to repeat the nudge animation after a certain delay.
  base::OneShotTimer timer_;

  // The current visibility state of the nudge elements.
  bool is_visible_ = false;

  // If set to true, we will set a user pref to disable this nudge forever at
  // the time when `this` is destroyed.
  bool should_dismiss_nudge_forever_ = false;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_
