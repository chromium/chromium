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
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"

namespace ash {

// Enumeration of camera switch states.
enum class CameraSWPrivacySwitchSetting { kDisabled, kEnabled };

// Abstraction for communication with the backend camera switch.
class CameraPrivacySwitchAPI {
 public:
  virtual ~CameraPrivacySwitchAPI() = default;

  // Sets the SW Privacy Switch value in the CrOS Camera service.
  virtual void SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting) = 0;
};

class ASH_EXPORT CameraPrivacySwitchSynchronizer
    : public SessionObserver,
      public media::CameraPrivacySwitchObserver {
 public:
  CameraPrivacySwitchSynchronizer();
  CameraPrivacySwitchSynchronizer(const CameraPrivacySwitchSynchronizer&) =
      delete;
  CameraPrivacySwitchSynchronizer& operator=(
      const CameraPrivacySwitchSynchronizer&) = delete;
  ~CameraPrivacySwitchSynchronizer() override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  // media::CameraPrivacySwitchObserver:
  void OnCameraSWPrivacySwitchStateChanged(
      cros::mojom::CameraPrivacySwitchState state) override;

  // Handles changes in the user pref ( e.g. toggling the camera switch on
  // Privacy Hub UI).
  void OnPreferenceChanged(const std::string& pref_name);

  // Retrieves the current value of the user pref.
  CameraSWPrivacySwitchSetting GetUserSwitchPreference();

  // Sets Privacy switch API for testing.
  void SetCameraPrivacySwitchAPIForTest(
      std::unique_ptr<CameraPrivacySwitchAPI> switch_api);

 protected:
  // Sets the value of the global camera permission in the camera backend.
  void SetCameraSWPrivacySwitch(CameraSWPrivacySwitchSetting value);

  // Sets the value of the user pref in the pref service.
  void SetUserSwitchPreference(CameraSWPrivacySwitchSetting value);

 private:
  virtual void OnPreferenceChangedImpl() = 0;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::unique_ptr<CameraPrivacySwitchAPI> switch_api_;
  bool is_camera_observer_added_ = false;
};

// A singleton class that acts as a bridge between Privacy Hub UI and backend.
// It listens on both ends and changes UI to reflect changes in
// the backend and notifies the backend of changes in the user
// preference setting.
class ASH_EXPORT CameraPrivacySwitchController
    : public CameraPrivacySwitchSynchronizer,
      public base::SupportsUserData {
 public:
  CameraPrivacySwitchController();
  CameraPrivacySwitchController(const CameraPrivacySwitchController&) = delete;
  CameraPrivacySwitchController& operator=(
      const CameraPrivacySwitchController&) = delete;
  ~CameraPrivacySwitchController() override;

  // Gets the instance from Shell.
  static CameraPrivacySwitchController* Get();

  // Handles the change in the number of cameras.
  void OnCameraCountChanged(int new_camera_count);

  // This is called when the set of applications accessing the camera changes.
  // `application_added` being true means a new applications has started
  // accessing the camera. `application_added` being false means one of the
  // active applications has stopped accessing the camera.
  void ActiveApplicationsChanged(bool application_added);

  // Checks if we use the fallback solution for the camera LED.
  // Returns the boolean value via callback.
  // (go/privacy-hub:camera-led-fallback).
  // TODO(b/289510726): remove when all cameras fully support the software
  // switch.
  bool UsingCameraLEDFallback();

  // This checks whether the LED fallback mechanism is used directly (using the
  // filesystem). Is used during initialization and can be used externally in
  // case that the controller object does not exist. (E.g. to initialize the
  // PrivacyHubNotificationController, which exists even if privacy hub is
  // disabled). Should be used only in that case to avoid repeated blocking
  // calls to the filesystem.
  static bool CheckCameraLEDFallbackDirectly();

 private:
  // Handles changes in the user pref ( e.g. toggling the camera switch on
  // Privacy Hub UI).
  void OnPreferenceChangedImpl() override;

  // Used for first time initialization of the cached value.
  // Can be called only once.
  void InitUsingCameraLEDFallback();

  void ShowNotification() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void RemoveNotification() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void UpdateNotification() VALID_CONTEXT_REQUIRED(sequence_checker_);
  void ScheduleNotificationRemoval() VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool InNotificationExtensionPeriod()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  int GUARDED_BY_CONTEXT(sequence_checker_)
      active_applications_using_camera_count_ = 0;
  int camera_count_ = -1;
  bool using_camera_led_fallback_ = true;
  base::Time GUARDED_BY_CONTEXT(sequence_checker_)
      last_active_notification_update_time_;

  base::WeakPtrFactory<CameraPrivacySwitchController> weak_ptr_factory_{this};
};

// A class that makes sure the camera is initially enabled in case the Privacy
// Hub feature is disabled.
class ASH_EXPORT CameraPrivacySwitchDisabled
    : public CameraPrivacySwitchSynchronizer {
 private:
  // Handles changes in the user pref ( e.g. toggling the camera switch on
  // Privacy Hub UI).
  void OnPreferenceChangedImpl() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_HUB_CAMERA_PRIVACY_SWITCH_CONTROLLER_H_
