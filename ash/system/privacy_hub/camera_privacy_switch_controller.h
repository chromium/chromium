// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/session/session_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

// The ID for a notification shown when the user tries to use a camera while the
// camera is disabled in Privacy Hub.
inline constexpr char kPrivacyHubCameraOffNotificationId[] =
    "ash.media.privacy_hub.activity_with_disabled_camera";
// The ID for a notification shown when the user enables camera via a HW switch
// but it is still disabled in PrivacyHub.
inline constexpr char
    kPrivacyHubHWCameraSwitchOffSWCameraSwitchOnNotificationId[] =
        "ash.media.privacy_hub.want_to_turn_off_camera";

// Enumeration of camera switch states.
enum class CameraSWPrivacySwitchSetting { kDisabled, kEnabled };

// Abstraction for communication with the backend camera switch.
class CameraPrivacySwitchAPI {
 public:
  virtual ~CameraPrivacySwitchAPI() = default;

  // Sets the SW Privacy Switch value in the CrOS Camera service.
  virtual void SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting) = 0;
};

// A singleton class that acts as a bridge between Privacy Hub UI and backend.
// It listens on both ends and changes UI to reflect changes in
// the backend and notifies the backend of changes in the user
// preference setting.
class ASH_EXPORT CameraPrivacySwitchController
    : public SessionObserver,
      public media::CameraPrivacySwitchObserver,
      public media::CameraActiveClientObserver {
 public:
  CameraPrivacySwitchController();

  CameraPrivacySwitchController(const CameraPrivacySwitchController&) = delete;
  CameraPrivacySwitchController& operator=(
      const CameraPrivacySwitchController&) = delete;

  ~CameraPrivacySwitchController() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // media::CameraPrivacySwitchObserver:
  void OnCameraHWPrivacySwitchStateChanged(
      const std::string& device_id,
      cros::mojom::CameraPrivacySwitchState state) override;
  void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // media::CameraActiveClientObserver:
  void OnActiveClientChange(
      cros::mojom::CameraClientType type,
      bool is_active,
      const base::flat_set<std::string>& device_ids) override;

  // Handles user toggling the camera switch on Privacy Hub UI.
  void OnPreferenceChanged(const std::string& pref_name);

  // Returns the last observed HW switch state for the camera.
  cros::mojom::CameraPrivacySwitchState HWSwitchState() const;

  // Sets Privacy switch API for testing.
  void SetCameraPrivacySwitchAPIForTest(
      std::unique_ptr<CameraPrivacySwitchAPI> switch_api);

  // Displays the camera off notification.
  void ShowCameraOffNotification();

  // Retrieves the current value of the user pref.
  CameraSWPrivacySwitchSetting GetUserSwitchPreference();

  // Set `prefs::kUserCameraAllowed` to the value of `enabled` and log the
  // interaction from a notification. TODO(b/248211321) find a better location
  // for this.
  static void SetAndLogCameraPreferenceFromNotification(bool enabled);

 private:
  // Displays the "Do you want to turn the camera off" notification.
  void ShowHWCameraSwitchOffSWCameraSwitchOnNotification();

  // Displays a notification with an action that can enable/disable the camera.
  void ShowNotification(bool action_enables_camera,
                        const char* kNotificationId,
                        const int notification_title_id,
                        const int notification_message_id,
                        const NotificationCatalogName catalog);

  // Clears all notifications related to the camera SW switch
  void ClearSWSwitchNotifications();

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CameraPrivacySwitchAPI> switch_api_;
  cros::mojom::CameraPrivacySwitchState camera_privacy_switch_state_ =
      cros::mojom::CameraPrivacySwitchState::UNKNOWN;
  int active_camera_client_count_ = 0;
  bool is_camera_observer_added_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
