// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/style/system_shadow.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class CaptureButtonView;
class CaptureModeSession;
class DropToStopRecordingButtonAnimation;

// A view that displays (optional) icon and text message to the user depending
// on current capture source and type. In video capture mode, it will later
// transform into a 3 second countdown timer.
class ASH_EXPORT CaptureLabelView : public views::View,
                                    public gfx::AnimationDelegate {
  METADATA_HEADER(CaptureLabelView, views::View)

 public:
  CaptureLabelView(CaptureModeSession* capture_mode_session,
                   views::Button::PressedCallback on_capture_button_pressed,
                   views::Button::PressedCallback on_drop_down_button_pressed);
  CaptureLabelView(const CaptureLabelView&) = delete;
  CaptureLabelView& operator=(const CaptureLabelView&) = delete;
  ~CaptureLabelView() override;

  CaptureButtonView* capture_button_container() {
    return capture_button_container_;
  }

  // Returns true if the given `screen_location` is on the drop down button,
  // which when clicked opens the recording type menu.
  bool IsPointOnRecordingTypeDropDownButton(
      const gfx::Point& screen_location) const;

  // Returns true if the recording drop down button is available and visible.
  bool IsRecordingTypeDropDownButtonVisible() const;

  // Returns true if this view is hosting the capture button instead of just a
  // label, and can be interacted with by the user. In this case, this view has
  // views that are a11y highlightable.
  bool IsViewInteractable() const;

  // Update icon and text according to current capture source and type.
  void UpdateIconAndText();

  // Returns true if CaptureLabelView should handle event that falls in the
  // bounds of this view. This should only return true when the view is
  // interactable before the count down animation starts.
  bool ShouldHandleEvent();

  // Called when starting 3-seconds count down before recording video.
  void StartCountDown(base::OnceClosure countdown_finished_callback);

  // Returns true if count down animation is in progress.
  bool IsInCountDownAnimation() const;

  // views::View:
  void AddedToWidget() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  friend class CaptureModeSessionTestApi;

  // Fades in and out the given `counter_value` (e.g. "3", "2", or "1") as it
  // performs a step in the count down animation.
  void FadeInAndOutCounter(int counter_value);

  // At the end of the count down animation, we drop the widget of this view to
  // the position where the stop button will be shown.
  void DropWidgetToStopRecordingButton();

  // This is a fallback animation in case the stop recording button is not
  // available (e.g. during shutdown or root window removal). In this case, we
  // fade out the widget of this view as the last step in the count down
  // animation.
  void FadeOutWidget();

  // Called once the entire count down animation finishes.
  void OnCountDownAnimationFinished();

  // The view that contains the button that when pressed, capture will be
  // performed. If we are in video recording mode, and GIF recording is enabled,
  // this view will also host a drop down button to allow the user to choose the
  // type of the recording format.
  raw_ptr<CaptureButtonView> capture_button_container_ = nullptr;

  // The label that displays a text message. Not user interactable.
  raw_ptr<views::Label> label_ = nullptr;

  // Callback function to be called after countdown if finished.
  base::OnceClosure countdown_finished_callback_;

  // Pointer to the current capture mode session. Not nullptr during this
  // lifecycle.
  raw_ptr<CaptureModeSession> capture_mode_session_;

  // Animates the widget of this view towards the position of the stop recording
  // button at the end of the count down.
  std::unique_ptr<DropToStopRecordingButtonAnimation>
      drop_to_stop_button_animation_;

  std::unique_ptr<SystemShadow> shadow_;

  base::WeakPtrFactory<CaptureLabelView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
