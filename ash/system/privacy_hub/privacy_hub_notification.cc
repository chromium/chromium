// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include <iterator>
#include <optional>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/public/cpp/projector/projector_session.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/privacy_hub/privacy_hub_controller.h"
#include "ash/system/privacy_hub/privacy_hub_metrics.h"
#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "ash/system/privacy_hub/sensor_disabled_notification_delegate.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "components/vector_icons/vector_icons.h"
#include "privacy_hub_notification_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {
constexpr size_t kMaxNotificationMessageLength = 150;
}

namespace ash {

bool operator<(const PrivacyHubNotificationDescriptor& descriptor1,
               const PrivacyHubNotificationDescriptor& descriptor2) {
  return descriptor1.sensors().ToEnumBitmask() <
         descriptor2.sensors().ToEnumBitmask();
}

bool operator<(const PrivacyHubNotificationDescriptor& descriptor,
               const uint64_t& sensors_bitmask) {
  return descriptor.sensors().ToEnumBitmask() < sensors_bitmask;
}

bool operator<(const uint64_t& sensors_bitmask,
               const PrivacyHubNotificationDescriptor& descriptor) {
  return sensors_bitmask < descriptor.sensors().ToEnumBitmask();
}

PrivacyHubNotificationClickDelegate::PrivacyHubNotificationClickDelegate(
    base::RepeatingClosure button_click) {
  button_callbacks_[0] = std::move(button_click);
}

PrivacyHubNotificationClickDelegate::~PrivacyHubNotificationClickDelegate() =
    default;

void PrivacyHubNotificationClickDelegate::Click(
    const std::optional<int>& button_index_opt,
    const std::optional<std::u16string>& reply) {
  if (button_index_opt.has_value()) {
    const unsigned int button_index = button_index_opt.value();
    CHECK_GT(button_callbacks_.size(), button_index);
    DCHECK(!button_callbacks_[button_index].is_null())
        << "button_index=" << button_index;
    RunCallbackIfNotNull(button_callbacks_[button_index]);
  } else {
    RunCallbackIfNotNull(message_callback_);
  }
}

void PrivacyHubNotificationClickDelegate::SetMessageClickCallback(
    base::RepeatingClosure callback) {
  message_callback_ = std::move(callback);
}

void PrivacyHubNotificationClickDelegate::SetSecondButtonCallback(
    base::RepeatingClosure callback) {
  button_callbacks_[1] = std::move(callback);
}

void PrivacyHubNotificationClickDelegate::RunCallbackIfNotNull(
    const base::RepeatingClosure& callback) {
  if (!callback.is_null()) {
    callback.Run();
  }
}

PrivacyHubNotificationDescriptor::PrivacyHubNotificationDescriptor(
    const SensorDisabledNotificationDelegate::SensorSet& sensors,
    const int title_id,
    const std::vector<int>& button_ids,
    const std::vector<int>& message_ids,
    const scoped_refptr<PrivacyHubNotificationClickDelegate> delegate)
    : title_id_(title_id),
      button_ids_(button_ids),
      sensors_(sensors),
      message_ids_(message_ids),
      delegate_(delegate) {
  DCHECK(!message_ids.empty());
  DCHECK(message_ids.size() < 2u || !sensors.empty())
      << "Specify at least one sensor when providing more than one message ID";
  DCHECK_LE(button_ids.size(), 2u) << "Privacy hub notifications are not "
                                      "supposed to have more than two buttons.";
  if (!delegate_) {
    delegate_ = base::MakeRefCounted<PrivacyHubNotificationClickDelegate>(
        base::BindRepeating(
            [](SensorDisabledNotificationDelegate::SensorSet sensors) {
              for (const auto sensor : sensors) {
                PrivacyHubNotificationController::
                    SetAndLogSensorPreferenceFromNotification(sensor, true);
              }
            },
            sensors));
  }
}

PrivacyHubNotificationDescriptor::PrivacyHubNotificationDescriptor(
    const PrivacyHubNotificationDescriptor& other) = default;

PrivacyHubNotificationDescriptor& PrivacyHubNotificationDescriptor::operator=(
    const PrivacyHubNotificationDescriptor& other) = default;

PrivacyHubNotificationDescriptor::~PrivacyHubNotificationDescriptor() = default;

namespace {

// Default throttler implementation that never suppress a notification.
class DefaultThrottler : public PrivacyHubNotification::Throttler {
 public:
  // PrivacyHubNotification::Throttler:
  bool ShouldThrottle() final { return false; }

  void RecordDismissalByUser() final {}
};

}  // namespace

PrivacyHubNotification::PrivacyHubNotification(
    const std::string& id,
    const ash::NotificationCatalogName catalog_name,
    const PrivacyHubNotificationDescriptor& descriptor)
    : id_(id), sensors_(descriptor.sensors()), catalog_name_(catalog_name) {
  notification_descriptors_.emplace(descriptor);
  SetNotificationContent();

  builder_.SetId(id)
      .SetCatalogName(catalog_name)
      .SetSmallImage(vector_icons::kSettingsIcon)
      .SetWarningLevel(message_center::SystemNotificationWarningLevel::NORMAL);

  // Sets up the observation / throttling logic
  SetThrottler(std::make_unique<DefaultThrottler>());
  StartDismissalObservation();
}

PrivacyHubNotification::PrivacyHubNotification(
    const std::string& id,
    const ash::NotificationCatalogName catalog_name,
    const std::vector<PrivacyHubNotificationDescriptor>& descriptors)
    : PrivacyHubNotification(id, catalog_name, descriptors.at(0)) {
  DCHECK_GT(descriptors.size(), 1u);

  for (unsigned int i = 1; i < descriptors.size(); ++i) {
    notification_descriptors_.emplace(descriptors.at(i));
  }
}

PrivacyHubNotification::~PrivacyHubNotification() {
  StopDismissalObservation();
}

void PrivacyHubNotification::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (by_user && notification_id == id_) {
    CHECK(throttler_);
    throttler_->RecordDismissalByUser();
  }
}

void PrivacyHubNotification::Show() {
  CHECK(throttler_);
  if (throttler_->ShouldThrottle()) {
    return;
  }
  SetNotificationContent();
  if (IsShown()) {
    // The notification is already in the message center. Update the content and
    // pop it up again.
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr(false));
    message_center::MessageCenter::Get()->ResetSinglePopup(id_);
  } else {
    message_center::MessageCenter::Get()->AddNotification(
        builder_.BuildPtr(false));
  }
}

void PrivacyHubNotification::Hide() {
  message_center::MessageCenter::Get()->RemoveNotification(id_,
                                                           /*by_user=*/false);
}

bool PrivacyHubNotification::IsShown() {
  return message_center::MessageCenter::Get()->FindNotificationById(id_);
}

void PrivacyHubNotification::Update() {
  if (IsShown()) {
    SetNotificationContent();
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr(true));
  }
}

void PrivacyHubNotification::SetPriority(
    message_center::NotificationPriority priority) {
  message_center::RichNotificationData optional_fields =
      builder_.GetOptionalFields();
  optional_fields.priority = priority;
  builder_.SetOptionalFields(optional_fields);
}

void PrivacyHubNotification::SetSensors(
    const SensorDisabledNotificationDelegate::SensorSet sensors) {
  DCHECK_GT(notification_descriptors_.size(), 1u)
      << "`sensors_` should only be updated when multiple notification "
         "descriptors are provided.";

  if (sensors_ != sensors) {
    sensors_ = sensors;
    has_sensors_changed_ = true;
  }
}

void PrivacyHubNotification::SetThrottler(
    std::unique_ptr<Throttler> throttler) {
  throttler_ = std::move(throttler);
}

void PrivacyHubNotification::StartDismissalObservation() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center) {
    message_center->AddObserver(this);
  }
}

void PrivacyHubNotification::StopDismissalObservation() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center) {
    message_center->RemoveObserver(this);
  }
}

std::vector<std::u16string> PrivacyHubNotification::GetAppsAccessingSensors(
    const size_t number_of_apps) const {
  std::vector<std::u16string> app_names;

  for (SensorDisabledNotificationDelegate::Sensor sensor : sensors_) {
    CHECK_LE(app_names.size(), number_of_apps);
    // This forces an implicit conversion of unsigned size_t into signed
    // difference_type, avoiding conversion issues later on.
    const decltype(app_names)::difference_type remaining_capacity =
        number_of_apps - app_names.size();
    if (remaining_capacity == 0) {
      break;
    }
    std::vector<std::u16string> sensor_apps;
    if (SensorDisabledNotificationDelegate::Sensor::kLocation == sensor) {
      const auto* const controller = GeolocationPrivacySwitchController::Get();
      if (controller) {
        sensor_apps = controller->GetActiveApps(remaining_capacity);
      }
    } else {
      if (PrivacyHubNotificationController* controller =
              PrivacyHubNotificationController::Get()) {
        SensorDisabledNotificationDelegate* delegate =
            controller->sensor_disabled_notification_delegate();
        CHECK(delegate);
        sensor_apps = delegate->GetAppsAccessingSensor(sensor);
      }
    }
    // Copy app names for the given sensor.
    std::copy(std::begin(sensor_apps), std::end(sensor_apps),
              std::back_inserter(app_names));
  }

  // For the  microphone sensor, check Assist, Screen cast & capture, and
  // Dictation and add to apps names.
  if (sensors_.Has(SensorDisabledNotificationDelegate::Sensor::kMicrophone)) {
    // Add dictation if it's enabled.
    bool is_dictation_enabled =
        Shell::Get()->accessibility_controller()->dictation_active();
    if (is_dictation_enabled) {
      app_names.push_back(l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_ACCESSIBILITY_DICTATION));
    }

    // Add Screencast (i.e. ProjectorSession) if it's active.
    ProjectorSession* projector_session = ProjectorSession::Get();
    bool is_projector_session_active =
        projector_session != nullptr && projector_session->is_active();
    if (is_projector_session_active) {
      app_names.push_back(
          l10n_util::GetStringUTF16(IDS_ASH_PROJECTOR_DISPLAY_SOURCE));
    } else {
      // Checking the Screen capture (triggered via keyboard) only if the
      // Screencast is not active.
      CaptureModeController* capture_mode_controller =
          CaptureModeController::Get();
      bool is_capture_mode_active =
          capture_mode_controller != nullptr &&
          capture_mode_controller->IsAudioRecordingInProgress();
      if (is_capture_mode_active) {
        app_names.push_back(
            l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE));
      }
    }

    // Consider assistant only if no other apps were added to the list of app
    // names.
    if (app_names.size() == 0) {
      bool is_assist_enabled =
          Shell::Get()->app_list_controller()->IsAssistantAllowedAndEnabled();
      if (is_assist_enabled) {
        app_names.push_back(
            l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_WINDOW));
      }
    }
  }

  // De-duplicate app names.
  std::sort(std::begin(app_names), std::end(app_names));
  app_names.erase(std::unique(std::begin(app_names), std::end(app_names)),
                  std::end(app_names));

  CHECK_LE(app_names.size(), number_of_apps);
  return app_names;
}

void PrivacyHubNotification::SetNotificationContent() {
  auto descriptor = notification_descriptors_.find(sensors_.ToEnumBitmask());
  DCHECK(descriptor != notification_descriptors_.end());

  if (has_sensors_changed_) {
    message_center::RichNotificationData optional_fields =
        builder_.GetOptionalFields();
    optional_fields.remove_on_click = true;

    optional_fields.buttons.clear();
    for (int button_id : descriptor->button_ids()) {
      optional_fields.buttons.emplace_back(
          l10n_util::GetStringUTF16(button_id));
    }

    builder_.SetDelegate(descriptor->delegate())
        .SetOptionalFields(optional_fields);

    if (catalog_name_ != NotificationCatalogName::kCameraPrivacySwitch) {
      builder_.SetTitleId(descriptor->title_id_);
    }

    has_sensors_changed_ = false;
  }

  if (catalog_name_ == NotificationCatalogName::kCameraPrivacySwitch) {
    return;
  }

  const std::vector<std::u16string> apps =
      GetAppsAccessingSensors(descriptor->message_ids().size());

  if (const size_t num_apps = apps.size();
      num_apps < descriptor->message_ids().size()) {
    const std::u16string message =
        l10n_util::GetStringFUTF16(descriptor->message_ids().at(num_apps), apps,
                                   /*offsets=*/nullptr);
    if (message.size() <= kMaxNotificationMessageLength) {
      builder_.SetMessage(message);
      return;
    }
  }
  builder_.SetMessageId(descriptor->message_ids().at(0));
}

}  // namespace ash
