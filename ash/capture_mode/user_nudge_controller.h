// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_
#define ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace ash {

class CaptureModeSession;

// Controls the user nudge animation and toast widget which are used to draw the
// user's attention towards the given `view_to_be_highlighted`. In the current
// iteration, this view is the settings button on the capture mode bar, and the
// desire to inform the users about the newly added ability to change where
// captured images and videos are saved. In upcoming iteration, the camera nudge
// will be shown instead once the selfie camera feature is enabled.
class UserNudgeController {
 public:
  explicit UserNudgeController(CaptureModeSession* session,
                               views::View* view_to_be_highlighted);
  UserNudgeController(const UserNudgeController&) = delete;
  UserNudgeController& operator=(const UserNudgeController&) = delete;
  ~UserNudgeController();

  bool is_visible() const { return is_visible_; }
  void set_should_dismiss_nudge_forever(bool value) {
    should_dismiss_nudge_forever_ = value;
  }
  bool should_dismiss_nudge_forever() const {
    return should_dismiss_nudge_forever_;
  }

  // Repositions the animation layers such that they're correctly parented with
  // the correct bounds on the correct display.
  void Reposition();

  // Animates the animation layers towards the given visibility state `visible`.
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

  // This is the window that will be used to as the parent of our animation
  // layers (`base_ring_` and `ripple_ring_`).
  aura::Window* GetParentWindow() const;

  // The session that owns `this`. Guaranteed to be non null for the lifetime of
  // `this`.
  const raw_ptr<CaptureModeSession> capture_session_;

  // The view to which we're trying to grab the user's attention.
  const raw_ptr<views::View> view_to_be_highlighted_;

  // These are the two animation layers that will be used to highlight
  // `view_to_be_highlighted_` to nudge the user towards it.
  ui::Layer base_ring_{ui::LAYER_SOLID_COLOR};
  ui::Layer ripple_ring_{ui::LAYER_SOLID_COLOR};

  // The timer used to repeat the nudge animation after a certain delay.
  base::OneShotTimer timer_;

  // The current visibility state of the nudge elements.
  bool is_visible_ = false;

  // If set to true, we will set a user pref to disable this nudge forever at
  // the time when `this` is destroyed.
  bool should_dismiss_nudge_forever_ = false;

  base::WeakPtrFactory<UserNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_USER_NUDGE_CONTROLLER_H_
