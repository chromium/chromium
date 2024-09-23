// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/geolocation_access_level.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/system/privacy_hub/geolocation_privacy_switch_controller.h"
#include "ash/system/privacy_hub/microphone_privacy_switch_controller.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "ash/system/system_notification_controller.h"
#include "base/containers/ring_buffer.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

namespace ash {
using Sensor = SensorDisabledNotificationDelegate::Sensor;

namespace {

constexpr char kLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=privacy_hub";

void LogInvalidSensor(const Sensor sensor) {
  NOTREACHED() << "Invalid sensor: "
               << static_cast<std::underlying_type_t<Sensor>>(sensor);
}

// Throttler for geolocation notification. Limits notification to be displayed
// no more than once in an hour and no more that 3x in 24 hours.
class GeolocationThrottler : public PrivacyHubNotification::Throttler {
 public:
  // PrivacyHubNotification::Throttler
  bool ShouldThrottle() final {
    if (dismissals_.CurrentIndex() == 0) {
      // No dismissals recorded yet. -> No suppression.
      return false;
    }
    const base::TimeTicks current_time = base::TimeTicks::Now();
    const base::TimeDelta time_since_last_dismissal =
        current_time - **dismissals_.End();
    if (time_since_last_dismissal < base::Hours(1)) {
      // There has been a dismissal recorded within the last hour. ->
      // Notification suppressed.
      return true;
    }
    // Now we should suppress if there have been at least 3 accesses within last
    // 24 hours.
    if (dismissals_.CurrentIndex() < dismissals_.BufferSize()) {
      // Buffer is not full. -> There have not been 3 accesses. -> No
      // suppression.
      return false;
    }
    // There are 3 recorded accesses.
    const base::TimeDelta time_since_oldest_dismissal =
        current_time - **dismissals_.Begin();
    if (time_since_oldest_dismissal >= base::Hours(24)) {
      // The oldest one is older than 24 hours. -> No suppression.
      return false;
    }
    // All 3 must be within the 24 hours window. -> Notification suppressed.
    return true;
  }

  void RecordDismissalByUser() final {
    dismissals_.SaveToBuffer(base::TimeTicks::Now());
  }

 private:
  // Contains the times of up to the last three dismissals (in order).
  base::RingBuffer<base::TimeTicks, 3u> dismissals_;
};

}  // namespace

PrivacyHubNotificationController::PrivacyHubNotificationController() {
  PrivacyHubController* privacy_hub_controller = PrivacyHubController::Get();
  CHECK(privacy_hub_controller);

  // If privacy hub is on and the camera fallback mechanism is active, we need
  // to use a different set of messages.
  // TODO(b/289510726): remove when all cameras fully support the software
  // switch.
  // Note: if the privacy hub is not enabled, this object may still exist as it
  // is used by privacy indicators as well.

  std::vector<int> camera_messages;
  std::vector<int> combined_messages;

  if (privacy_hub_controller->UsingCameraLEDFallback()) {
    camera_messages = {
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_DISCLAIMER,
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME_WITH_DISCLAIMER,
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES_WITH_DISCLAIMER};
    combined_messages = {
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_DISCLAIMER,
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME_WITH_DISCLAIMER,
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES_WITH_DISCLAIMER};
  } else {
    camera_messages = {
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE,
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
        IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};
    combined_messages = {
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE,
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
        IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};
  }

  std::vector<int> microphone_messages = {
      IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
      IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
      IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES};

  auto camera_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kCamera}, IDS_PRIVACY_HUB_CAMERA_OFF_NOTIFICATION_TITLE,
      std::vector<int>{IDS_PRIVACY_HUB_TURN_ON_CAMERA_ACTION_BUTTON},
      camera_messages);

  auto microphone_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kMicrophone},
      IDS_MICROPHONE_MUTED_BY_SW_SWITCH_NOTIFICATION_TITLE,
      std::vector<int>{IDS_MICROPHONE_MUTED_NOTIFICATION_ACTION_BUTTON},
      microphone_messages);

  auto combined_notification_descriptor = PrivacyHubNotificationDescriptor(
      SensorSet{Sensor::kCamera, Sensor::kMicrophone},
      IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_TITLE,
      std::vector<int>{
          IDS_PRIVACY_HUB_MICROPHONE_AND_CAMERA_OFF_NOTIFICATION_BUTTON,
          IDS_PRIVACY_HUB_OPEN_SETTINGS_PAGE_BUTTON},
      combined_messages);

  combined_notification_descriptor.delegate()->SetSecondButtonCallback(
      base::BindRepeating(
          &PrivacyHubNotificationController::OpenPrivacyHubSettingsPage));

  combined_notification_ = std::make_unique<PrivacyHubNotification>(
      kCombinedNotificationId, NotificationCatalogName::kPrivacyHubMicAndCamera,
      std::vector<PrivacyHubNotificationDescriptor>{
          camera_notification_descriptor, microphone_notification_descriptor,
          combined_notification_descriptor});

  microphone_hw_switch_notification_ = std::make_unique<PrivacyHubNotification>(
      kMicrophoneHardwareSwitchNotificationId,
      NotificationCatalogName::kMicrophoneMute,
      PrivacyHubNotificationDescriptor{
          SensorSet{Sensor::kMicrophone},
          IDS_MICROPHONE_MUTED_BY_HW_SWITCH_NOTIFICATION_TITLE,
          std::vector<int>{IDS_ASH_LEARN_MORE},
          std::vector<int>{
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
              IDS_MICROPHONE_MUTED_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES},
          base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
              base::BindRepeating(
                  PrivacyHubNotificationController::OpenSupportUrl,
                  Sensor::kMicrophone))});

  PrivacyHubNotificationDescriptor geolocation_notification_descriptor(
      SensorSet{Sensor::kLocation},
      IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_TITLE,
      std::vector<int>{IDS_PRIVACY_HUB_TURN_ON_GEOLOCATION_ACTION_BUTTON,
                       IDS_PRIVACY_HUB_TURN_ON_GEOLOCATION_LEARN_MORE_BUTTON},
      std::vector<int>{
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE,
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE_WITH_ONE_APP_NAME,
          IDS_PRIVACY_HUB_GEOLOCATION_OFF_NOTIFICATION_MESSAGE_WITH_TWO_APP_NAMES});
  geolocation_notification_descriptor.delegate()->SetSecondButtonCallback(
      base::BindRepeating(PrivacyHubNotificationController::OpenSupportUrl,
                          Sensor::kLocation));
  geolocation_notification_ = std::make_unique<PrivacyHubNotification>(
      kGeolocationSwitchNotificationId,
      NotificationCatalogName::kGeolocationSwitch,
      std::move(geolocation_notification_descriptor));
  geolocation_notification_->SetThrottler(
      std::make_unique<GeolocationThrottler>());
}

PrivacyHubNotificationController::~PrivacyHubNotificationController() = default;

// static
PrivacyHubNotificationController* PrivacyHubNotificationController::Get() {
  SystemNotificationController* system_notification_controller =
      Shell::Get()->system_notification_controller();
  return system_notification_controller
             ? system_notification_controller->privacy_hub()
             : nullptr;
}

void PrivacyHubNotificationController::ShowSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      // Microphone software switch notification will be displayed now. If the
      // hardware switch notification is still not cleared, clear it first.
      microphone_hw_switch_notification_->Hide();
      [[fallthrough]];
    }
    case Sensor::kCamera: {
      AddSensor(sensor);
      combined_notification_->Show();
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Show();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::RemoveSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kCamera: {
      [[fallthrough]];
    }
    case Sensor::kMicrophone: {
      RemoveSensor(sensor);
      if (!sensors_.empty()) {
        combined_notification_->Update();
      } else {
        combined_notification_->Hide();
      }
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Hide();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::UpdateSoftwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kCamera: {
      [[fallthrough]];
    }
    case Sensor::kMicrophone: {
      combined_notification_->Update();
      break;
    }
    case Sensor::kLocation: {
      geolocation_notification_->Update();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

// TODO(janlanik): Does this support geolocation?
bool PrivacyHubNotificationController::
    IsSoftwareSwitchNotificationDisplayedForSensor(Sensor sensor) {
  return combined_notification_->IsShown() && sensors_.Has(sensor);
}

void PrivacyHubNotificationController::
    SetPriorityForMicrophoneHardwareNotification(
        message_center::NotificationPriority priority) {
  microphone_hw_switch_notification_->SetPriority(priority);
}

void PrivacyHubNotificationController::ShowHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      RemoveSensor(sensor);
      if (!sensors_.empty()) {
        combined_notification_->Update();
      } else {
        // As the hardware switch notification for microphone will be displayed
        // now, remove the sw switch notification.
        combined_notification_->Hide();
      }
      microphone_hw_switch_notification_->Show();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::RemoveHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      microphone_hw_switch_notification_->Hide();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::UpdateHardwareSwitchNotification(
    const Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone: {
      microphone_hw_switch_notification_->Update();
      break;
    }
    default: {
      LogInvalidSensor(sensor);
      break;
    }
  }
}

void PrivacyHubNotificationController::OpenPrivacyHubSettingsPage() {
  privacy_hub_metrics::LogPrivacyHubOpenedFromNotification();
  Shell::Get()->system_tray_model()->client()->ShowPrivacyHubSettings();
}

void PrivacyHubNotificationController::OpenSupportUrl(Sensor sensor) {
  switch (sensor) {
    case Sensor::kMicrophone:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kMicrophone);
      break;
    case Sensor::kCamera:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kCamera);
      break;
    case Sensor::kLocation:
      privacy_hub_metrics::LogPrivacyHubLearnMorePageOpened(
          privacy_hub_metrics::PrivacyHubLearnMoreSensor::kGeolocation);
      return;
  }
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kLearnMoreUrl), NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

// static
void PrivacyHubNotificationController::
    SetAndLogSensorPreferenceFromNotification(Sensor sensor,
                                              const bool enabled) {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!pref_service) {
    return;
  }

  switch (sensor) {
    case Sensor::kCamera: {
      pref_service->SetBoolean(prefs::kUserCameraAllowed, enabled);
      break;
    }
    case Sensor::kMicrophone: {
      pref_service->SetBoolean(prefs::kUserMicrophoneAllowed, enabled);
      break;
    }
    case Sensor::kLocation: {
      // Geolocation notification asks user to allow geolocation for everything
      // (not only system services).
      if (auto* controller = ash::GeolocationPrivacySwitchController::Get()) {
        controller->SetAccessLevel(enabled
                                       ? GeolocationAccessLevel::kAllowed
                                       : GeolocationAccessLevel::kDisallowed);
      }
      break;
    }
  }

  privacy_hub_metrics::LogSensorEnabledFromNotification(sensor, enabled);
}

std::unique_ptr<SensorDisabledNotificationDelegate>
PrivacyHubNotificationController::SetSensorDisabledNotificationDelegate(
    std::unique_ptr<SensorDisabledNotificationDelegate> delegate) {
  std::swap(sensor_disabled_notification_delegate_, delegate);
  return delegate;
}

SensorDisabledNotificationDelegate*
PrivacyHubNotificationController::sensor_disabled_notification_delegate() {
  SensorDisabledNotificationDelegate* delegate =
      sensor_disabled_notification_delegate_.get();
  CHECK(delegate);
  return delegate;
}

void PrivacyHubNotificationController::AddSensor(Sensor sensor) {
  sensors_.Put(sensor);
  combined_notification_->SetSensors(sensors_);
}

void PrivacyHubNotificationController::RemoveSensor(Sensor sensor) {
  sensors_.Remove(sensor);
  combined_notification_->SetSensors(sensors_);
}

}  // namespace ash
