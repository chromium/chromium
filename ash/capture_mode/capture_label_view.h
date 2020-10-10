// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class LabelButton;
class Label;
}  // namespace views

namespace ash {

class CaptureModeSession;

// A view that displays (optional) icon and text message to the user depending
// on current capture source and type. In video capture mode, it will later
// transform into a 3 second countdown timer.
class ASH_EXPORT CaptureLabelView : public views::View,
                                    public views::ButtonListener {
 public:
  METADATA_HEADER(CaptureLabelView);

  explicit CaptureLabelView(CaptureModeSession* capture_mode_session);
  CaptureLabelView(const CaptureLabelView&) = delete;
  CaptureLabelView& operator=(const CaptureLabelView&) = delete;
  ~CaptureLabelView() override;

  // Function to be called to set a short time interval for countdown in tests
  // so that we don't have to wait over 3 seconds to start video recording.
  static void SetUseDelayForTesting(bool use_delay);

  // Update icon and text according to current capture source and type.
  void UpdateIconAndText();

  // Returns true if CaptureLabelView should handle event that falls in the
  // bounds of this view. This should only return true when |label_button_| is
  // visible.
  bool ShouldHandleEvent();

  // Called when starting 3-seconds count down before recording video.
  void StartCountDown(base::OnceClosure countdown_finished_callback);

  // views::View:
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 private:
  static constexpr int kCountDownSeconds = 3;

  void CountDown();

  // The label button that displays an icon and a text message. Can be user
  // interactable. When clicking/tapping on the button, start perform image or
  // video capture.
  views::LabelButton* label_button_ = nullptr;

  // The label that displays a text message. Not user interactable.
  views::Label* label_ = nullptr;

  // Count down timer.
  base::RepeatingTimer count_down_timer_;
  int timeout_count_down_ = kCountDownSeconds;

  // Callback function to be called after countdown if finished.
  base::OnceClosure countdown_finished_callback_;

  // Pointer to the current capture mode session. Not nullptr during this
  // lifecycle.
  CaptureModeSession* capture_mode_session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_LABEL_VIEW_H_
