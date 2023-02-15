// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_media_state.h"
#include "base/auto_reset.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// Controller that will act as a "bridge" between VC apps management and the VC
// UI layers. The singleton instance is constructed immediately before and
// destructed immediately after the UI, so any code that keeps a reference to
// it must be prepared to accommodate this specific lifetime in order to prevent
// any use-after-free bugs.
class ASH_EXPORT VideoConferenceTrayController
    : public media::CameraPrivacySwitchObserver,
      public CrasAudioHandler::AudioObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when the state of `has_media_app` within
    // `VideoConferenceMediaState` is changed.
    virtual void OnHasMediaAppStateChange() = 0;

    // Called when the state of camera permission is changed.
    virtual void OnCameraPermissionStateChange() = 0;

    // Called when the state of microphone permission is changed.
    virtual void OnMicrophonePermissionStateChange() = 0;

    // Called when the state of camera capturing is changed.
    virtual void OnCameraCapturingStateChange(bool is_capturing) = 0;

    // Called when the state of microphone capturing is changed.
    virtual void OnMicrophoneCapturingStateChange(bool is_capturing) = 0;

    // Called when the state of screen sharing is changed.
    virtual void OnScreenSharingStateChange(bool is_capturing_screen) = 0;
  };

  VideoConferenceTrayController();

  VideoConferenceTrayController(const VideoConferenceTrayController&) = delete;
  VideoConferenceTrayController& operator=(
      const VideoConferenceTrayController&) = delete;

  ~VideoConferenceTrayController() override;

  // Returns the singleton instance.
  static VideoConferenceTrayController* Get();

  // Observer functions.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Whether the tray should be shown.
  bool ShouldShowTray() const;

  // Returns whether `state_` indicates permissions are granted for different
  // mediums.
  bool GetHasCameraPermissions() const;
  bool GetHasMicrophonePermissions() const;

  // Returns whether `state_` indicates a capture session is in progress for
  // different mediums.
  bool IsCapturingScreen() const;
  bool IsCapturingCamera() const;
  bool IsCapturingMicrophone() const;

  // Sets the state for camera mute. Virtual for testing/mocking.
  virtual void SetCameraMuted(bool muted) = 0;

  // Gets the state for camera mute. Virtual for testing/mocking.
  virtual bool GetCameraMuted() = 0;

  // Sets the state for microphone mute. Virtual for testing/mocking.
  virtual void SetMicrophoneMuted(bool muted) = 0;

  // Gets the state for microphone mute. Virtual for testing/mocking.
  virtual bool GetMicrophoneMuted() = 0;

  // Returns asynchronously a vector of media apps that will be displayed in the
  // "Return to app" panel of the bubble. Virtual for testing/mocking.
  virtual void GetMediaApps(
      base::OnceCallback<void(MediaApps)> ui_callback) = 0;

  // Brings the app with the given `id` to the foreground.
  virtual void ReturnToApp(const base::UnguessableToken& id) = 0;

  // Updates the tray UI with the given `VideoConferenceMediaState`.
  void UpdateWithMediaState(VideoConferenceMediaState state);

  // Handles device usage from a VC app while the device is system disabled.
  virtual void HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name);

  // media::CameraPrivacySwitchObserver:
  void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;

  // CrasAudioHandler::AudioObserver:
  // Pop up a toast when speaking on mute is detected.
  void OnSpeakOnMuteDetected() override;

  VideoConferenceTrayEffectsManager& effects_manager() {
    return effects_manager_;
  }

  bool camera_muted_by_software_switch() const {
    return camera_muted_by_software_switch_;
  }

 private:
  // Callback for `tray_hide_delay_timer_`.
  void SetTraysVisibilityAfterDelayHiding();

  // This keeps track the current VC media state. The state is being updated by
  // `UpdateWithMediaState()`, calling from `VideoConferenceManagerAsh`.
  VideoConferenceMediaState state_;

  // This keeps track of the current Camera Software Privacy Switch state.
  // Updated via `OnCameraSWPrivacySwitchStateChanged()` Fetching this would
  // otherwise take an asynchronous call to `media::CameraHalDispatcherImpl`.
  bool camera_muted_by_software_switch_ = false;

  // Used by the views to construct and lay out effects in the bubble.
  VideoConferenceTrayEffectsManager effects_manager_;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;

  // The last time speak-on-mute notification showed.
  absl::optional<base::TimeTicks> last_speak_on_mute_notification_time_;

  // Timer to delay hiding all the VC trays for 12 seconds.
  base::OneShotTimer tray_hide_delay_timer_;

  // These are used to cache the old state of camera/microphone permissions.
  // When the timer is running, we will use these old states to display the
  // icons accordingly.
  bool camera_permission_during_timer_ = false;
  bool microphone_permission_during_timer_ = false;

  base::WeakPtrFactory<VideoConferenceTrayController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
