// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
class Label;
}  // namespace views

namespace ash {

class CaptureModeSession;
class DropToStopRecordingButtonAnimation;

// A view that displays (optional) icon and text message to the user depending
// on current capture source and type. In video capture mode, it will later
// transform into a 3 second countdown timer.
class ASH_EXPORT CaptureLabelView
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView,
      public gfx::AnimationDelegate {
 public:
  METADATA_HEADER(CaptureLabelView);

  CaptureLabelView(CaptureModeSession* capture_mode_session,
                   base::RepeatingClosure on_capture_button_pressed);
  CaptureLabelView(const CaptureLabelView&) = delete;
  CaptureLabelView& operator=(const CaptureLabelView&) = delete;
  ~CaptureLabelView() override;

  views::LabelButton* label_button() { return label_button_; }

  // Update icon and text according to current capture source and type.
  void UpdateIconAndText();

  // Returns true if CaptureLabelView should handle event that falls in the
  // bounds of this view. This should only return true when |label_button_| is
  // visible.
  bool ShouldHandleEvent();

  // Called when starting 3-seconds count down before recording video.
  void StartCountDown(base::OnceClosure countdown_finished_callback);

  // Returns true if count down animation is in progress.
  bool IsInCountDownAnimation() const;

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;
  std::unique_ptr<views::HighlightPathGenerator> CreatePathGenerator() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
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

  // The label button that displays an icon and a text message. Can be user
  // interactable. When clicking/tapping on the button, start perform image or
  // video capture.
  views::LabelButton* label_button_ = nullptr;

  // The label that displays a text message. Not user interactable.
  views::Label* label_ = nullptr;

  // Callback function to be called after countdown if finished.
  base::OnceClosure countdown_finished_callback_;

  // Pointer to the current capture mode session. Not nullptr during this
  // lifecycle.
  CaptureModeSession* capture_mode_session_;

  // Animates the widget of this view towards the position of the stop recording
  // button at the end of the count down.
  std::unique_ptr<DropToStopRecordingButtonAnimation>
      drop_to_stop_button_animation_;

  base::WeakPtrFactory<CaptureLabelView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
