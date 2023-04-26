// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
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

  // Adds this class as an observer for CrasAudioHandler and
  // CameraHalDispatcherImpl.
  // (1) We should not call this in /ash/system/* tests, because we are not
  // using FakeCrasAudioClient or MockCameraHalServer. Currently, we directly
  // mock the VideoConferenceTrayButtons inside
  // FakeVideoConferenceTrayController; which is a simpler approach.
  // (2) We need this initialization in
  // ChromeBrowserMainExtraPartsAsh::PreProfileInit for production code.
  void Initialize(VideoConferenceManagerBase* video_conference_manager);

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
  virtual void SetCameraMuted(bool muted);

  // Gets the state for camera mute. Virtual for testing/mocking.
  virtual bool GetCameraMuted();

  // Sets the state for microphone mute. Virtual for testing/mocking.
  virtual void SetMicrophoneMuted(bool muted);

  // Gets the state for microphone mute. Virtual for testing/mocking.
  virtual bool GetMicrophoneMuted();

  // Returns asynchronously a vector of media apps that will be displayed in the
  // "Return to app" panel of the bubble. Virtual for testing/mocking.
  virtual void GetMediaApps(base::OnceCallback<void(MediaApps)> ui_callback);

  // Brings the app with the given `id` to the foreground.
  virtual void ReturnToApp(const base::UnguessableToken& id);

  // Updates the tray UI with the given `VideoConferenceMediaState`.
  void UpdateWithMediaState(VideoConferenceMediaState state);

  // Returns true if any running media apps have been granted permission for
  // camera/microphone.
  bool HasCameraPermission() const;
  bool HasMicrophonePermission() const;

  // Handles device usage from a VC app while the device is system disabled.
  virtual void HandleDeviceUsedWhileDisabled(
      crosapi::mojom::VideoConferenceMediaDevice device,
      const std::u16string& app_name);

  // media::CameraPrivacySwitchObserver:
  void OnCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) override;
  void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // CrasAudioHandler::AudioObserver:
  void OnInputMuteChanged(
      bool mute_on,
      CrasAudioHandler::InputMuteChangeMethod method) override;

  // CrasAudioHandler::AudioObserver:
  // Pop up a toast when speaking on mute is detected.
  void OnSpeakOnMuteDetected() override;

  // Gets `disable_shelf_autohide_timer_`, used for testing.
  base::OneShotTimer& GetShelfAutoHideTimerForTest();

  VideoConferenceTrayEffectsManager& effects_manager() {
    return effects_manager_;
  }

  bool camera_muted_by_hardware_switch() const {
    return camera_muted_by_hardware_switch_;
  }
  bool camera_muted_by_software_switch() const {
    return camera_muted_by_software_switch_;
  }

  bool initialized() const { return initialized_; }

 private:
  // Updates the state of the camera icons across all `VideoConferenceTray`.
  void UpdateCameraIcons();

  // Callback passed to `VideoConferenceManagerAsh` which reacts to the number
  // of active `MediaApp`'s to force the shelf to show or hide.
  void UpdateShelfAutoHide(MediaApps apps);

  // The number of capturing apps, fetched from `VideoConferenceManagerAsh`.
  int capturing_apps_ = 0;

  // This keeps track the current VC media state. The state is being updated by
  // `UpdateWithMediaState()`, calling from `VideoConferenceManagerAsh`.
  VideoConferenceMediaState state_;

  // This keeps track of the current Camera Privacy Switch state.
  // Updated via `OnCameraHWPrivacySwitchStateChanged()` and
  // `OnCameraSWPrivacySwitchStateChanged()` Fetching this would otherwise take
  // an asynchronous call to `media::CameraHalDispatcherImpl`.
  bool camera_muted_by_hardware_switch_ = false;
  bool camera_muted_by_software_switch_ = false;

  // True if microphone is muted by the hardware switch, false if the microphone
  // is muted through software. If the microphone is not muted, disregards this
  // value.
  bool microphone_muted_by_hardware_switch_ = false;

  // Timer responsible for hiding the shelf after it has been shown to alert the
  // user of a new app accessing the sensors.
  base::OneShotTimer disable_shelf_autohide_timer_;

  // List of locks which force the shelf to show, if the shelf is autohidden.
  std::list<Shelf::ScopedDisableAutoHide> disable_shelf_autohide_locks_;

  // Used by the views to construct and lay out effects in the bubble.
  VideoConferenceTrayEffectsManager effects_manager_;

  // Registered observers.
  base::ObserverList<Observer> observer_list_;

  // The last time speak-on-mute notification showed.
  absl::optional<base::TimeTicks> last_speak_on_mute_notification_time_;

  // video_conference_manager_ should be valid after initialized_.
  // Currently, VideoConferenceTrayController is destroyed inside
  // ChromeBrowserMainParts::PostMainMessageLoopRun() as a chrome_extra_part;
  // VideoConferenceManagerAsh is destroyed inside crosapi_manager_.reset()
  // which is after VideoConferenceTrayController.
  raw_ptr<VideoConferenceManagerBase> video_conference_manager_ = nullptr;
  bool initialized_ = false;

  base::WeakPtrFactory<VideoConferenceTrayController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
