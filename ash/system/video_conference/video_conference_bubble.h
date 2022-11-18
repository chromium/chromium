// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_

#include "ash/system/tray/tray_bubble_view.h"

namespace views {
class ToggleButton;
}  // namespace views

namespace ash {

// The bubble that contains controls for camera and microphone effects,
// and for easy navigation to apps performing video/audio capture.
class VideoConferenceBubbleView : public TrayBubbleView {
 public:
  explicit VideoConferenceBubbleView(const InitParams& init_params);

  VideoConferenceBubbleView(const VideoConferenceBubbleView&) = delete;
  VideoConferenceBubbleView& operator=(const VideoConferenceBubbleView&) =
      delete;
  ~VideoConferenceBubbleView() override = default;

 private:
  // A view that combines a `views::Label` and a `views::ToggleButton`,
  // for toggling a camera/mic effect from the VC bubble.
  class LabeledToggleButton : public views::View {
   public:
    explicit LabeledToggleButton(views::Button::PressedCallback callback,
                                 const std::u16string& effect_name);

    LabeledToggleButton(const LabeledToggleButton&) = delete;
    LabeledToggleButton& operator=(const LabeledToggleButton&) = delete;
    ~LabeledToggleButton() override = default;

    // Returns `true` if the `ToggleButton` is "on," `false` otherwise.
    bool GetIsOn() const;

   private:
    views::ToggleButton* button_;
  };

  // Invoked when the "background replace" toggle button is pressed.
  void OnBackgroundReplaceToggleButtonPressed();

  LabeledToggleButton* background_replace_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_BUBBLE_H_
