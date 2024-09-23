// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shell_observer.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "components/prefs/pref_registry_simple.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

struct AnchoredNudgeData;
class VideoConferenceTray;

using MediaApps = std::vector<crosapi::mojom::VideoConferenceMediaAppInfoPtr>;

// Controller that will act as a "bridge" between VC apps management and the VC
// UI layers. The singleton instance is constructed immediately before and
// destructed immediately after the UI, so any code that keeps a reference to
// it must be prepared to accommodate this specific lifetime in order to prevent
// any use-after-free bugs.
class ASH_EXPORT VideoConferenceTrayController
    : public media::CameraPrivacySwitchObserver,
      public CrasAudioHandler::AudioObserver,
      public SessionObserver,
      public ShellObserver {
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

    // Called when the Dlc download state is changed for `feature_tile_title` if
    // any DLC was registered for that effect.
    virtual void OnDlcDownloadStateChanged(
        bool error,
        const std::u16string& feature_tile_title) = 0;
  };

  VideoConferenceTrayController();

  VideoConferenceTrayController(const VideoConferenceTrayController&) = delete;
  VideoConferenceTrayController& operator=(
      const VideoConferenceTrayController&) = delete;

  ~VideoConferenceTrayController() override;

  // Called inside ash/ash_prefs.cc to register related prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

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

  // Caches a nudge data object for nudges that attempt to show while the tray
  // is animating in, so they only show once the tray animation has ended. The
  // request will be run immediately if the tray is not animating.
  void CreateNudgeRequest(std::unique_ptr<AnchoredNudgeData> nudge_data);

  // Shows the cached `requested_nudge_data_` object, if one exists.
  void MaybeRunNudgeRequest();

  // Attempts showing the speak-on-mute opt-in nudge.
  void MaybeShowSpeakOnMuteOptInNudge();

  // Returns true if we can show the animation to help users to discover the new
  // feature.
  bool ShouldShowImageButtonAnimation() const;
  bool ShouldShowCreateWithAiButtonAnimation() const;

  // Disables showing the animation for the button from now on. Calling the
  // above ShouldShow...() will return false for the current active user going
  // forward.
  void DismissImageButtonAnimationForever();
  void DismissCreateWithAiButtonAnimationForever();

  // Callback used to update prefs whenever a user opts in or out of the
  // speak-on-mute feature. An `opt_in` value of false means the user opted out.
  void OnSpeakOnMuteNudgeOptInAction(bool opt_in);

  void OnDlcDownloadStateFetched(bool add_warning,
                                 const std::u16string& feature_tile_title);

  // Closes all nudges that are shown anchored to the VC tray, if any.
  void CloseAllVcNudges();

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

  // Stops all screen sharing. Virtual for testing/mocking.
  virtual void StopAllScreenShare();

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

  // Enable or disable input stream ewma power report.
  void SetEwmaPowerReportEnabled(bool enabled);

  // Return the last reported ewma power.
  double GetEwmaPower();

  // Enable or disable sidetone.
  void SetSidetoneEnabled(bool enabled);

  // Gets the state for sidetone.
  bool GetSidetoneEnabled() const;

  // Gets whether sidetone is supported.
  bool IsSidetoneSupported() const;

  // Update the sidetone supported value.
  // Should be called before calling IsSidetoneSupported.
  void UpdateSidetoneSupportedState();

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

  // SessionObserver:
  void OnUserSessionAdded(const AccountId& account_id) override;

  // ShellObserver:
  void OnShellDestroying() override;

  // Handles client updates such as a change of title or addition / removal of a
  // VC app. Virtual to allow mock classes to override for testing.
  virtual void HandleClientUpdate(
      crosapi::mojom::VideoConferenceClientUpdatePtr update);

  // Handles showing the shelf when a new app is added.
  void OnAppAdded();

  // Gets `disable_shelf_autohide_timer_`, used for testing.
  base::OneShotTimer& GetShelfAutoHideTimerForTest();

  virtual VideoConferenceTrayEffectsManager& GetEffectsManager();

  // Passes create background image action to `video_conference_manager_`.
  void CreateBackgroundImage();

  bool camera_muted_by_hardware_switch() const {
    return camera_muted_by_hardware_switch_;
  }
  bool camera_muted_by_software_switch() const {
    return camera_muted_by_software_switch_;
  }

  bool initialized() const { return initialized_; }

 private:
  // All the types of the use while disabled nudge.
  enum class UsedWhileDisabledNudgeType {
    kCamera = 0,
    kMicrophone = 1,
    kBoth = 2,
    kMaxValue = kBoth
  };

  // Updates the state of the camera icons across all `VideoConferenceTray`.
  void UpdateCameraIcons();

  // Records repeated shows metric when the timer is stop.
  void RecordRepeatedShows();

  // Returns true if any of the VC nudges are visible on screen.
  bool IsAnyVcNudgeShown();

  // Displays the use while disabled nudge according to the given `type`.
  void DisplayUsedWhileDisabledNudge(UsedWhileDisabledNudgeType type,
                                     const std::u16string& app_name);

  UsedWhileDisabledNudgeType GetUsedWhileDisabledNudgeType(
      crosapi::mojom::VideoConferenceMediaDevice device);

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

  // The last time speak-on-mute nudge shown.
  // The cool down periods for nudges:
  // 1. No cool down for the first nudge,
  // 2. 2 mins for the second nudge,
  // 3. 4 mins for the third nudge,
  // 4. 8 mins for the forth nudge.
  base::TimeTicks last_speak_on_mute_nudge_shown_time_;

  // The counter of how many time the speak-on-mute nudge has shown in the
  // current session.
  int speak_on_mute_nudge_shown_count_ = 0;

  // `video_conference_manager_` should be valid after `initialized_`.
  // Currently, `VideoConferenceTrayController` is destroyed inside
  // `ChromeBrowserMainParts::PostMainMessageLoopRun()` as a chrome_extra_part;
  // `VideoConferenceManagerAsh` is destroyed inside crosapi_manager_.reset()
  // which is after `VideoConferenceTrayController`.
  raw_ptr<VideoConferenceManagerBase> video_conference_manager_ = nullptr;
  bool initialized_ = false;

  // Used to record metrics of repeated shows per 100 ms.
  int count_repeated_shows_ = 0;
  base::DelayTimer repeated_shows_timer_;

  // Due to some constraint in `VideoConferenceManagerAsh`, when both microphone
  // and camera is being accessed when disabled,`HandleDeviceUsedWhileDisabled`
  // will be called twice for each device. Thus, we need to wait for both 2
  // calls and display one nudge for both. These are the timer and the cache
  // type to make that happen.
  base::OneShotTimer use_while_disabled_signal_waiter_;
  UsedWhileDisabledNudgeType use_while_disabled_nudge_on_wait_;

  // The contents of a nudge data object that is cached so it can be shown once
  // the tray has fully animated in.
  std::unique_ptr<AnchoredNudgeData> requested_nudge_data_;

  base::WeakPtrFactory<VideoConferenceTrayController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
