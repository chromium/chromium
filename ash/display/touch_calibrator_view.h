// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_TOUCH_CALIBRATOR_VIEW_H_
#define ASH_DISPLAY_TOUCH_CALIBRATOR_VIEW_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace views {
class Label;
class Widget;
}  // namespace views

namespace gfx {
class Animation;
class LinearAnimation;
}  // namespace gfx

namespace ui {
class LayerAnimationSequence;
}  // namespace ui

namespace ash {

class CircularThrobberView;

// An overlay view used during touch calibration. This view is responsible for
// all animations and UX during touch calibration on all displays currently
// active on the device. The view on the display being calibrated is the primary
// touch calibration view.
// |TouchCalibratorView| acts as a state machine and has an API to toggle its
// state or get the current state.
class ASH_EXPORT TouchCalibratorView : public views::View,
                                       public views::AnimationDelegateViews,
                                       public ui::LayerAnimationObserver {
 public:
  // Different states of |TouchCalibratorView| in order.
  enum State {
    UNKNOWN = 0,
    BACKGROUND_FADING_IN,  // Transition state where the background is fading
                           // in.
    DISPLAY_POINT_1,       // Static state where the touch point is at its first
                           // location.
    ANIMATING_1_TO_2,  // Transition state when the touch point is being moved
                       // from one location to another.
    DISPLAY_POINT_2,   // Static state where the touch point is at its second
                       // location.
    ANIMATING_2_TO_3,
    DISPLAY_POINT_3,  // Static state where the touch point is at its third
                      // location.
    ANIMATING_3_TO_4,
    DISPLAY_POINT_4,  // Static state where the touch point is at its final
                      // location.
    ANIMATING_FINAL_MESSAGE,  // Transition state when the calibration complete
                              // message is being transitioned into view.
    CALIBRATION_COMPLETE,  // Static state when the calibration complete message
                           // is displayed to the user.
    BACKGROUND_FADING_OUT  // Transition state where the background is fading
                           // out
  };

  TouchCalibratorView(const display::Display& target_display,
                      bool is_primary_view);
  ~TouchCalibratorView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // ui::LayerAnimationObserver
  void OnLayerAnimationStarted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override;
  void OnLayerAnimationScheduled(ui::LayerAnimationSequence* sequence) override;

  // Moves the touch calibrator view to its next state.
  void AdvanceToNextState();

  // Skips to the final state. Should be used to cancel calibration and hide all
  // views from the screen with a smooth transition out animation.
  void SkipToFinalState();

  // Returns true if |location| is set by the end of this function call. If set,
  // |location| will point to the center of the circle that the user sees during
  // the touch calibration UX.
  bool GetDisplayPointLocation(gfx::Point* location);

  // Skips/cancels any ongoing animation to its end.
  void SkipCurrentAnimation();

  // Returns the current state of the view.
  State state() { return state_; }

 private:
  void InitViewContents();

  // The target display on which this view is rendered on.
  const display::Display display_;

  // True if this view is on the display that is being calibrated.
  bool is_primary_view_ = false;

  std::unique_ptr<views::Widget> widget_;

  cc::PaintFlags flags_;

  // Defines the bounds for the background animation.
  gfx::RectF background_rect_;

  // Text label indicating how to exit the touch calibration.
  views::Label* exit_label_;
  // Text label indicating the significance of the touch point on screen.
  views::Label* tap_label_;

  // Start and end opacity values used during the fade animation. This is set
  // before the animation begins.
  float start_opacity_value_;
  float end_opacity_value_;

  // Linear animation used for various aniations including fade-in, fade out,
  // and view translation.
  std::unique_ptr<gfx::LinearAnimation> animator_;

  // View responsible for displaying the animated circular icon that the user
  // touches to calibrate the screen.
  CircularThrobberView* throbber_circle_;

  // A hint box displayed next to the first touch point to assist user with
  // information about the next step.
  views::View* hint_box_view_;

  // Final view containing the calibration complete message along with an icon.
  views::View* completion_message_view_;

  // View that contains the animated throbber circle and a text label informing
  // the user to tap the circle to continue calibration.
  views::View* touch_point_view_;

  State state_ = UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorView);
};

}  // namespace ash

#endif  // ASH_DISPLAY_TOUCH_CALIBRATOR_VIEW_H_
