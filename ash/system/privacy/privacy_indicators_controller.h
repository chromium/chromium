// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

// An interface for the delegate of the privacy indicators notification,
// handling launching the app and its settings. Clients that use privacy
// indicators should provide this delegate when calling the privacy indicators
// controller API so that the API can add correct buttons to the notification
// based on the callbacks provided and appropriate actions are performed when
// clicking the buttons.
class ASH_EXPORT PrivacyIndicatorsNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  explicit PrivacyIndicatorsNotificationDelegate(
      std::optional<base::RepeatingClosure> launch_settings_callback =
          std::nullopt);

  PrivacyIndicatorsNotificationDelegate(
      const PrivacyIndicatorsNotificationDelegate&) = delete;
  PrivacyIndicatorsNotificationDelegate& operator=(
      const PrivacyIndicatorsNotificationDelegate&) = delete;

  const std::optional<base::RepeatingClosure>& launch_settings_callback()
      const {
    return launch_settings_callback_;
  }

  void SetLaunchSettingsCallback(
      const base::RepeatingClosure& launch_settings_callback);

  // message_center::NotificationDelegate:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 protected:
  ~PrivacyIndicatorsNotificationDelegate() override;

 private:
  // Callback for clicking the launch settings button.
  std::optional<base::RepeatingClosure> launch_settings_callback_;
};

// This enum contains all the sources that use privacy indicators. This enum is
// used for metrics collection. Note to keep in sync with enum in
// tools/metrics/histograms/enums.xml.
enum class PrivacyIndicatorsSource {
  kApps = 0,
  kLinuxVm = 1,
  kScreenCapture = 2,
  kMaxValue = kScreenCapture
};

// Get the id of the privacy indicators notification associated with `app_id`.
std::string ASH_EXPORT
GetPrivacyIndicatorsNotificationId(const std::string& app_id);

// Struct that stores info of an app that is being tracked by privacy
// indicators.
struct PrivacyIndicatorsAppInfo {
  PrivacyIndicatorsAppInfo();
  PrivacyIndicatorsAppInfo(PrivacyIndicatorsAppInfo&&);
  PrivacyIndicatorsAppInfo& operator=(PrivacyIndicatorsAppInfo&&) = default;
  ~PrivacyIndicatorsAppInfo();

  std::optional<std::u16string> app_name;
  scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate;
};

// A controller that manages the logic of modifying the tray item privacy
// indicator dot and the privacy indicators notification when camera/microphone
// access state changes.
class ASH_EXPORT PrivacyIndicatorsController
    : public CrasAudioHandler::AudioObserver,
      public media::CameraPrivacySwitchObserver {
 public:
  PrivacyIndicatorsController();

  PrivacyIndicatorsController(const PrivacyIndicatorsController&) = delete;
  PrivacyIndicatorsController& operator=(const PrivacyIndicatorsController&) =
      delete;

  ~PrivacyIndicatorsController() override;

  // Returns the singleton instance.
  static PrivacyIndicatorsController* Get();

  // Updates privacy indicators, including:
  // * Updates camera and microphone access indicators for
  //   `PrivacyIndicatorsTrayItemView`(s) across all status area widgets.
  // * Adds, updates, or removes the privacy notification associated with the
  //   given `app_id`. The given scoped_refptr for `delegate` will be passed as
  //   a parameter for the function creating the privacy indicators
  //   notification.
  // Note that if camera/microphone are muted, privacy indicators will not
  // indicate its usage via the tray item indicator dot and the notification.
  void UpdatePrivacyIndicators(
      const std::string& app_id,
      std::optional<std::u16string> app_name,
      bool is_camera_used,
      bool is_microphone_used,
      scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate,
      PrivacyIndicatorsSource source);

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

  // Specifies whether camera/microphone is in use by at least one app.
  bool IsCameraUsed() const;
  bool IsMicrophoneUsed() const;

  const std::map<std::string, PrivacyIndicatorsAppInfo>& apps_using_camera()
      const {
    return apps_using_camera_;
  }

  const std::map<std::string, PrivacyIndicatorsAppInfo>& apps_using_microphone()
      const {
    return apps_using_microphone_;
  }

  // A minimum delay before privacy indicator disappears.
  static constexpr base::TimeDelta kPrivacyIndicatorsMinimumHoldDuration =
      base::Seconds(4);
  // A delay before the privacy indicator disappears if they were previously
  // used longer than `kPrivacyIndicatorsMinimumHoldDuration`.
  static constexpr base::TimeDelta kPrivacyIndicatorsHoldAfterUseDuration =
      base::Seconds(1);

 private:
  // Updates privacy indicators after camera mute state changed.
  void UpdateForCameraMuteStateChanged();

  // `indicators_hiding_delay_timer_` is triggering this function when the timer
  // expires.
  void TriggerPrivacyIndicators(
      bool is_camera_used,
      bool is_microphone_used,
      bool is_new_app,
      bool was_camera_in_use,
      bool was_microphone_in_use,
      const std::string& app_id,
      std::optional<std::u16string> app_name,
      scoped_refptr<PrivacyIndicatorsNotificationDelegate> delegate);

  // If neither camera nor microphone is in use, calculates the delay for the
  // hiding timer.
  base::TimeDelta CalculateIndicatorsDelayTime() const;

  // If another app is using a camera and the new app tries to use the same
  // camera but fails, returns true to indicate that the privacy indicators
  // should be skipped.
  bool ShouldSkipShowPrivacyIndicators(bool is_camera_used,
                                       bool is_microphone_used,
                                       bool is_new_app,
                                       bool was_camera_in_use) const;

  // Stores the app(s) info that are currently accessing camera/microphone. The
  // key represents the app id.
  std::map<std::string, PrivacyIndicatorsAppInfo> apps_using_camera_;
  std::map<std::string, PrivacyIndicatorsAppInfo> apps_using_microphone_;

  // This keeps track of the current Camera Privacy Switch state.
  // Updated via `OnCameraHWPrivacySwitchStateChanged()` and
  // `OnCameraSWPrivacySwitchStateChanged()` We use these variables since
  // fetching this directly through `media::CameraHalDispatcherImpl` would
  // otherwise need an asynchronous call.
  bool camera_muted_by_hardware_switch_ = false;
  bool camera_muted_by_software_switch_ = false;

  // The time when the privacy indicator was active.
  base::TimeTicks privacy_indicator_time_;

  // The most recent state when the privacy indicator was active.
  std::pair<bool /*camera_state*/, bool /*microphone_state*/>
      recent_active_state_ = {false, false};

  // A timer to delay hiding a privacy indicator.
  base::OneShotTimer indicator_hiding_delay_timer_;
};

// Update `PrivacyIndicatorsTrayItemView` screen share status across all status
// area widgets.
void ASH_EXPORT
UpdatePrivacyIndicatorsScreenShareStatus(bool is_screen_sharing);

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
