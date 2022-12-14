// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class Canvas;
struct VectorIcon;
}  // namespace gfx

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

namespace video_conference {
class BubbleViewTest;
}  // namespace video_conference

class Shelf;
class TrayBubbleView;
class TrayBubbleWrapper;

// A toggle icon button in the VC tray, which is used for toggling camera,
// microphone, and screen sharing.
class VideoConferenceTrayButton : public IconButton {
 public:
  VideoConferenceTrayButton(PressedCallback callback,
                            const gfx::VectorIcon* icon,
                            const gfx::VectorIcon* toggled_icon,
                            const int accessible_name_id);

  VideoConferenceTrayButton(const VideoConferenceTrayButton&) = delete;
  VideoConferenceTrayButton& operator=(const VideoConferenceTrayButton&) =
      delete;

  ~VideoConferenceTrayButton() override;

  bool show_privacy_indicator() const { return show_privacy_indicator_; }

  // Sets the state of showing the privacy indicator in the button.
  void SetShowPrivacyIndicator(bool show);

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  bool show_privacy_indicator_ = false;
};

// This class represents the VC Controls tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT VideoConferenceTray
    : public TrayBackgroundView,
      public VideoConferenceTrayController::Observer {
 public:
  METADATA_HEADER(VideoConferenceTray);

  explicit VideoConferenceTray(Shelf* shelf);
  VideoConferenceTray(const VideoConferenceTray&) = delete;
  VideoConferenceTray& operator=(const VideoConferenceTray&) = delete;
  ~VideoConferenceTray() override;

  VideoConferenceTrayButton* audio_icon() { return audio_icon_; }
  VideoConferenceTrayButton* camera_icon() { return camera_icon_; }

  // TrayBackgroundView:
  void CloseBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  std::u16string GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void HandleLocaleChange() override;
  void UpdateAfterLoginStatusChange() override;

  // VideoConferenceTrayController::Observer:
  void OnCameraCapturingStateChange(bool is_capturing) override;
  void OnMicrophoneCapturingStateChange(bool is_capturing) override;

  // The expand indicator of the toggle bubble button needs to rotate according
  // to shelf alignment and whether the bubble is opened. This function will
  // calculate that rotation value.
  SkScalar GetRotationValueForToggleBubbleButton();

 private:
  friend class video_conference::BubbleViewTest;
  friend class VideoConferenceTrayTest;

  // Callback function for `toggle_bubble_button_`.
  void ToggleBubble(const ui::Event& event);

  // Callback functions for the icons when being clicked.
  void OnCameraButtonClicked(const ui::Event& event);
  void OnAudioButtonClicked(const ui::Event& event);
  void OnScreenShareButtonClicked(const ui::Event& event);

  // Owned by the views hierarchy.
  VideoConferenceTrayButton* audio_icon_ = nullptr;
  VideoConferenceTrayButton* camera_icon_ = nullptr;
  VideoConferenceTrayButton* screen_share_icon_ = nullptr;
  IconButton* toggle_bubble_button_ = nullptr;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  base::WeakPtrFactory<VideoConferenceTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
