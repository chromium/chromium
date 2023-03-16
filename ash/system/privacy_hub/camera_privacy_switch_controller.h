// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "base/supports_user_data.h"
#include "components/prefs/pref_change_registrar.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

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
      public base::SupportsUserData {
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

  // Handles user toggling the camera switch on Privacy Hub UI.
  void OnPreferenceChanged(const std::string& pref_name);

  // Handles the change in the number of cameras
  void OnCameraCountChanged(int new_camera_count);

  // Sets Privacy switch API for testing.
  void SetCameraPrivacySwitchAPIForTest(
      std::unique_ptr<CameraPrivacySwitchAPI> switch_api);

  // Retrieves the current value of the user pref.
  CameraSWPrivacySwitchSetting GetUserSwitchPreference();

  // Set `prefs::kUserCameraAllowed` to the value of `enabled` and log the
  // interaction from a notification.
  static void SetAndLogCameraPreferenceFromNotification(bool enabled);

  // This is called when the set of applications accessing the camera changes.
  // `application_added` being true means a new applications has started
  // accessing the camera. `application_added` being false means one of the
  // active applications has stopped accessing the camera.
  void ActiveApplicationsChanged(bool application_added);

 private:
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CameraPrivacySwitchAPI> switch_api_;
  int active_applications_using_camera_count_ = 0;
  bool is_camera_observer_added_ = false;
  int camera_count_ = -1;
  bool camera_used_while_deactivated_ = false;
  PrivacyHubNotification turn_sw_switch_on_notification_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
