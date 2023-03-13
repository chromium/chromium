// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include "ash/public/cpp/sensor_disabled_notification_delegate.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

// Returns true if a notification with id `id` is in the message center.
bool HasNotification(const std::string& id) {
  return message_center::MessageCenter::Get()->FindNotificationById(id);
}

}  // namespace

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
    const absl::optional<int>& button_index_opt,
    const absl::optional<std::u16string>& reply) {
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
  DCHECK(delegate);
  DCHECK(message_ids.size() < 2u || !sensors.Empty())
      << "Specify at least one sensor when providing more than one message ID";
  DCHECK_LE(button_ids.size(), 2u) << "Privacy hub notifications are not "
                                      "supposed to have more than two buttons.";
}

PrivacyHubNotificationDescriptor::PrivacyHubNotificationDescriptor(
    const PrivacyHubNotificationDescriptor& other) = default;

PrivacyHubNotificationDescriptor& PrivacyHubNotificationDescriptor::operator=(
    const PrivacyHubNotificationDescriptor& other) = default;

PrivacyHubNotificationDescriptor::~PrivacyHubNotificationDescriptor() = default;

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

PrivacyHubNotification::~PrivacyHubNotification() = default;

void PrivacyHubNotification::Show() {
  SetNotificationContent();
  if (HasNotification(id_)) {
    // The notification is already in the message center. Update the content and
    // pop it up again.
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr());
    message_center::MessageCenter::Get()->ResetSinglePopup(id_);
  } else {
    message_center::MessageCenter::Get()->AddNotification(builder_.BuildPtr());
  }
}

void PrivacyHubNotification::Hide() {
  message_center::MessageCenter::Get()->RemoveNotification(id_,
                                                           /*by_user=*/false);
}

void PrivacyHubNotification::Update() {
  if (HasNotification(id_)) {
    SetNotificationContent();
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr());
  }
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

std::vector<std::u16string> PrivacyHubNotification::GetAppsAccessingSensors(
    const size_t number_of_apps) const {
  std::vector<std::u16string> app_names;

  if (SensorDisabledNotificationDelegate* delegate =
          SensorDisabledNotificationDelegate::Get()) {
    for (SensorDisabledNotificationDelegate::Sensor sensor : sensors_) {
      for (const std::u16string& app :
           delegate->GetAppsAccessingSensor(sensor)) {
        if (!base::Contains(app_names, app)) {
          app_names.push_back(app);
        }
        if (app_names.size() == number_of_apps) {
          return app_names;
        }
      }
    }
  }

  return app_names;
}

void PrivacyHubNotification::SetNotificationContent() {
  auto descriptor = notification_descriptors_.find(sensors_.ToEnumBitmask());
  DCHECK(descriptor != notification_descriptors_.end());

  if (has_sensors_changed_) {
    message_center::RichNotificationData optional_fields;
    optional_fields.remove_on_click = true;

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
    builder_.SetMessageWithArgs(descriptor->message_ids().at(num_apps), apps);
  } else {
    builder_.SetMessageId(descriptor->message_ids().at(0));
  }
}

}  // namespace ash
