// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/privacy_hub/privacy_hub_util.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/privacy_hub_delegate.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/check_deref.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ash/camera_presence_notifier.h"
#include "chrome/browser/ash/privacy_hub/content_block_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/app_access_notifier.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "components/prefs/pref_service.h"

namespace ash::privacy_hub_util {

void SetFrontend(PrivacyHubDelegate* ptr) {
  PrivacyHubController* const controller = PrivacyHubController::Get();
  if (controller != nullptr) {
    // Controller may not be available when used from a test.
    controller->SetFrontend(ptr);
  }
}

bool MicrophoneSwitchState() {
  return ui::MicrophoneMuteSwitchMonitor::Get()->microphone_mute_switch_on();
}

bool ShouldForceDisableCameraSwitch() {
  PrivacyHubController* controller = PrivacyHubController::Get();
  if (!controller || !controller->camera_controller()) {
    return false;
  }
  return controller->camera_controller()->IsCameraAccessForceDisabled();
}

void SetUpCameraCountObserver() {
  auto* camera_controller = CameraPrivacySwitchController::Get();
  CHECK(camera_controller);

  base::RepeatingCallback<void(int)> update_camera_count_in_privacy_hub =
      base::BindRepeating(
          [](CameraPrivacySwitchController* controller, int camera_count) {
            controller->OnCameraCountChanged(camera_count);
          },
          camera_controller);
  auto notifier = std::make_unique<CameraPresenceNotifier>(
      std::move(update_camera_count_in_privacy_hub));
  notifier->Start();

  static const char kUserDataKey = '\0';
  camera_controller->SetUserData(&kUserDataKey, std::move(notifier));
}

// Notifies the Privacy Hub controller.
void TrackGeolocationAttempted(const std::string& name) {
  if (!features::IsCrosPrivacyHubLocationEnabled()) {
    return;
  }
  GeolocationPrivacySwitchController* controller =
      GeolocationPrivacySwitchController::Get();
  // TODO(b/288854399): Remove this if.
  if (controller) {
    controller->TrackGeolocationAttempted(name);
  }
}

// Notifies the Privacy Hub controller.
void TrackGeolocationRelinquished(const std::string& name) {
  if (!features::IsCrosPrivacyHubLocationEnabled()) {
    return;
  }
  GeolocationPrivacySwitchController* controller =
      GeolocationPrivacySwitchController::Get();
  // TODO(b/288854399): Remove this if.
  if (controller) {
    controller->TrackGeolocationRelinquished(name);
  }
}

bool IsCrosLocationOobeNegotiationNeeded() {
  // No negotiation needed, if the PH Location feature is not yet enabled.
  if (!ash::features::IsCrosPrivacyHubLocationEnabled()) {
    return false;
  }

  const Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (profile->GetPrefs()->IsManagedPreference(
          ash::prefs::kUserGeolocationAccessLevel)) {
    return false;
  }

  return true;
}

namespace {
std::optional<bool> camera_led_fallback_for_testing{};
}

// TODO(b/289510726): remove when all cameras fully support the software
// switch.
bool UsingCameraLEDFallback() {
  if (!camera_led_fallback_for_testing.has_value()) {
    CameraPrivacySwitchController* const controller =
        CameraPrivacySwitchController::Get();
    CHECK(controller);
    return controller->UsingCameraLEDFallback();
  }

  // Can happen in some testing environments
  CHECK(camera_led_fallback_for_testing.has_value());
  return camera_led_fallback_for_testing.value();
}

ScopedCameraLedFallbackForTesting::ScopedCameraLedFallbackForTesting(
    bool value) {
  CHECK(!camera_led_fallback_for_testing.has_value());
  camera_led_fallback_for_testing = value;
}

ScopedCameraLedFallbackForTesting::~ScopedCameraLedFallbackForTesting() {
  CHECK(camera_led_fallback_for_testing.has_value());
  camera_led_fallback_for_testing.reset();
  CHECK(!camera_led_fallback_for_testing.has_value());
}

void SetAppAccessNotifier(AppAccessNotifier* app_access_notifier) {
  // Wraps the `AppAccessNotifier` to be used from
  // `PrivacyHubNotificationController`.
  class Wrapper : public SensorDisabledNotificationDelegate {
   public:
    explicit Wrapper(AppAccessNotifier* notifier)
        : notifier_(raw_ref<AppAccessNotifier>::from_ptr(notifier)) {}

    std::vector<std::u16string> GetAppsAccessingSensor(Sensor sensor) override {
      switch (sensor) {
        case Sensor::kCamera:
          return notifier_->GetAppsAccessingCamera();
        case Sensor::kMicrophone:
          return notifier_->GetAppsAccessingMicrophone();
        case Sensor::kLocation:
          break;
      }
      NOTREACHED_NORETURN();
    }

   private:
    raw_ref<AppAccessNotifier> notifier_;
  };

  PrivacyHubNotificationController* controller =
      PrivacyHubNotificationController::Get();
  CHECK(controller);
  controller->SetSensorDisabledNotificationDelegate(
      app_access_notifier ? std::make_unique<Wrapper>(app_access_notifier)
                          : nullptr);
}

std::pair<base::Time, base::Time> SunriseSunsetSchedule() {
  const base::Time default_sunrise_time =
      base::Time::Now().LocalMidnight() + base::Hours(6);
  const base::Time default_sunset_time = default_sunrise_time + base::Hours(12);
  const ash::GeolocationController* geolocation_controller =
      ash::GeolocationController::Get();
  const base::Time sunrise_time =
      geolocation_controller
          ? geolocation_controller->GetSunriseTime().value_or(
                default_sunrise_time)
          : default_sunrise_time;
  const base::Time sunset_time =
      geolocation_controller ? geolocation_controller->GetSunsetTime().value_or(
                                   default_sunset_time)
                             : default_sunrise_time;
  return std::make_pair(sunrise_time, sunset_time);
}

bool ContentBlocked(ContentType type) {
  switch (type) {
    case ContentType::MEDIASTREAM_CAMERA: {
      auto* const controller = CameraPrivacySwitchController::Get();
      CHECK(controller);
      return !controller->IsCameraUsageAllowed();
    }
    case ContentType::MEDIASTREAM_MIC: {
      auto* const controller = MicrophonePrivacySwitchController::Get();
      CHECK(controller);
      return !controller->IsMicrophoneUsageAllowed();
    }
    case ContentType::GEOLOCATION: {
      if (!features::IsCrosPrivacyHubLocationEnabled()) {
        return true;
      }
      auto* const controller = GeolocationPrivacySwitchController::Get();
      CHECK(controller);
      return !controller->IsGeolocationUsageAllowedForApps();
    }
    default: {
      // If the provided content type is not controllable in ChromeOS, then it
      // is not blocked.
      return false;
    }
  }
}

std::unique_ptr<base::CheckedObserver> CreateObservationForBlockedContent(
    ContentBlockCallback callback) {
  return ContentBlockObservation::Create(std::move(callback));
}

void OpenSystemSettings(Profile* profile, ContentType type) {
  const char* settings_path = "";
  switch (type) {
    case ContentType::MEDIASTREAM_CAMERA: {
      settings_path = chromeos::settings::mojom::kPrivacyHubCameraSubpagePath;
      break;
    }
    case ContentType::MEDIASTREAM_MIC: {
      settings_path =
          chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath;
      break;
    }
    case ContentType::GEOLOCATION: {
      settings_path =
          chromeos::settings::mojom::kPrivacyHubGeolocationSubpagePath;
      break;
    }
    default: {
      // This should only be called for camera, microphone, or geolocation.
      NOTREACHED_IN_MIGRATION();
      return;
    }
  }

  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                               settings_path);
}

}  // namespace ash::privacy_hub_util
