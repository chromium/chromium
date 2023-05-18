// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace gfx {
class Canvas;
struct VectorIcon;
}  // namespace gfx

namespace session_manager {
enum class SessionState;
}  // namespace session_manager

namespace ui {
class Event;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

namespace video_conference {
class BubbleViewTest;
class ReturnToAppPanelTest;
class ResourceDependencyTest;
class ToggleEffectsViewTest;
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

  void set_toggle_is_one_way() { toggle_is_one_way_ = true; }

  bool show_privacy_indicator() const { return show_privacy_indicator_; }
  bool is_capturing() const { return is_capturing_; }

  // Set the state of `is_capturing_`.
  void SetIsCapturing(bool is_capturing);

  // Updates the capturing state and show/hide the privacy indicator
  // accordingly.
  void UpdateCapturingState();

  // IconButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  // Updates the tooltip according to the medium the button is for, the toggle
  // state, and the capture state.
  void UpdateTooltip();

  // Cache of the capturing state received from `VideoConferenceManagerAsh`.
  bool is_capturing_ = false;

  // Indicates whether we are showing the privacy indicator (the green dot) in
  // the button.
  bool show_privacy_indicator_ = false;

  // Whether the toggle is a one way operation (like Screen Share). Toggling it
  // off makes it dissapear.
  bool toggle_is_one_way_ = false;

  // The accessible name for this button's capture type (camera, microphone, or
  // screen share).
  const int accessible_name_id_;
};

// This class represents the VC Controls tray button in the status area and
// controls the bubble that is shown when the tray button is clicked.
class ASH_EXPORT VideoConferenceTray
    : public SessionObserver,
      public TrayBackgroundView,
      public VideoConferenceTrayController::Observer,
      public VideoConferenceTrayEffectsManager::Observer {
 public:
  METADATA_HEADER(VideoConferenceTray);

  explicit VideoConferenceTray(Shelf* shelf);
  VideoConferenceTray(const VideoConferenceTray&) = delete;
  VideoConferenceTray& operator=(const VideoConferenceTray&) = delete;
  ~VideoConferenceTray() override;

  VideoConferenceTrayButton* audio_icon() { return audio_icon_; }
  VideoConferenceTrayButton* camera_icon() { return camera_icon_; }
  VideoConferenceTrayButton* screen_share_icon() { return screen_share_icon_; }
  IconButton* toggle_bubble_button() { return toggle_bubble_button_; }

  // TrayBackgroundView:
  void CloseBubble() override;
  TrayBubbleView* GetBubbleView() override;
  views::Widget* GetBubbleWidget() const override;
  std::u16string GetAccessibleNameForTray() override;
  std::u16string GetAccessibleNameForBubble() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  void HandleLocaleChange() override;
  void AnchorUpdated() override;

  // VideoConferenceTrayController::Observer:
  void OnHasMediaAppStateChange() override;
  void OnCameraPermissionStateChange() override;
  void OnMicrophonePermissionStateChange() override;
  void OnCameraCapturingStateChange(bool is_capturing) override;
  void OnMicrophoneCapturingStateChange(bool is_capturing) override;
  void OnScreenSharingStateChange(bool is_capturing_screen) override;

  // VideoConferenceTrayEffectsManager::Observer:
  void OnEffectSupportStateChanged(VcEffectId effect_id,
                                   bool is_supported) override;

  // The expand indicator of the toggle bubble button needs to rotate according
  // to shelf alignment and whether the bubble is opened. This function will
  // calculate that rotation value.
  SkScalar GetRotationValueForToggleBubbleButton();

  // Update the visibility and capturing state of the tray and icons according
  // to the state in `VideoConferenceTrayController`.
  void UpdateTrayAndIconsState();

 private:
  friend class video_conference::BubbleViewTest;
  friend class video_conference::ReturnToAppPanelTest;
  friend class video_conference::ResourceDependencyTest;
  friend class video_conference::ToggleEffectsViewTest;
  friend class VideoConferenceTrayTest;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Callback function for `toggle_bubble_button_`.
  void ToggleBubble(const ui::Event& event);

  // Callback functions for the icons when being clicked.
  void OnCameraButtonClicked(const ui::Event& event);
  void OnAudioButtonClicked(const ui::Event& event);
  void OnScreenShareButtonClicked(const ui::Event& event);

  // Callback function for the settings buttons or the speak-on-mute toast when
  // being clicked. Opens the the Privacy Hub settings page with speak-on-mute
  // switch focused.
  static void OpenSpeakOnMuteDetectionSettingsPage();

  // Owned by the views hierarchy.
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> audio_icon_ = nullptr;
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> camera_icon_ = nullptr;
  raw_ptr<VideoConferenceTrayButton, ExperimentalAsh> screen_share_icon_ =
      nullptr;
  raw_ptr<IconButton, ExperimentalAsh> toggle_bubble_button_ = nullptr;

  // The bubble that appears after clicking the tray button.
  std::unique_ptr<TrayBubbleWrapper> bubble_;

  base::WeakPtrFactory<VideoConferenceTray> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_H_
