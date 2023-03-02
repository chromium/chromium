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

void RemoveNotification(const std::string& id) {
  message_center::MessageCenter::Get()->RemoveNotification(id,
                                                           /*by_user=*/false);
}

// Returns true if a notification with id `id` is in the message center.
bool HasNotification(const std::string& id) {
  return message_center::MessageCenter::Get()->FindNotificationById(id);
}

}  // namespace

namespace ash {

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
    const int button_id,
    const std::vector<int>& message_ids,
    const scoped_refptr<PrivacyHubNotificationClickDelegate> delegate)
    : title_id_(title_id),
      button_id_(button_id),
      sensors_(sensors),
      message_ids_(message_ids),
      delegate_(delegate) {
  DCHECK(!message_ids.empty());
  DCHECK(delegate);
  DCHECK(message_ids.size() < 2u || !sensors.Empty())
      << "Specify at least one sensor when providing more than one message ID";
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
    : id_(id),
      message_ids_(descriptor.message_ids()),
      sensors_(descriptor.sensors()),
      delegate_(descriptor.delegate()),
      button_text_(l10n_util::GetStringUTF16(descriptor.button_id_)) {
  builder_.SetId(id)
      .SetCatalogName(catalog_name)
      .SetDelegate(descriptor.delegate())
      .SetTitleId(descriptor.title_id_)
      .SetOptionalFields(MakeOptionalFields())
      .SetSmallImage(vector_icons::kSettingsIcon)
      .SetWarningLevel(message_center::SystemNotificationWarningLevel::NORMAL);
}

PrivacyHubNotification::~PrivacyHubNotification() = default;

void PrivacyHubNotification::Show() {
  if (remove_timer_.IsRunning()) {
    remove_timer_.Stop();
  }

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

  last_time_shown_ = base::Time::Now();
}

void PrivacyHubNotification::Hide() {
  if (!last_time_shown_) {
    return;
  }

  if (const base::TimeDelta remaining_show_time =
          kMinShowTime - (base::Time::Now() - last_time_shown_.value());
      remaining_show_time.is_positive()) {
    remove_timer_.Start(FROM_HERE, remaining_show_time,
                        base::BindOnce(RemoveNotification, id_));
  } else {
    RemoveNotification(id_);
  }

  last_time_shown_.reset();
}

void PrivacyHubNotification::Update() {
  if (HasNotification(id_)) {
    SetNotificationContent();
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr());
  }
}

void PrivacyHubNotification::SetSecondButton(base::RepeatingClosure callback,
                                             int title_id) {
  message_center::RichNotificationData optional_fields = MakeOptionalFields();
  optional_fields.buttons.emplace_back(l10n_util::GetStringUTF16(title_id));
  builder_.SetOptionalFields(optional_fields);
  delegate_->SetSecondButtonCallback(std::move(callback));
}

std::vector<std::u16string> PrivacyHubNotification::GetAppsAccessingSensors()
    const {
  std::vector<std::u16string> app_names;

  if (SensorDisabledNotificationDelegate* delegate =
          SensorDisabledNotificationDelegate::Get()) {
    for (SensorDisabledNotificationDelegate::Sensor sensor : sensors_) {
      for (const std::u16string& app :
           delegate->GetAppsAccessingSensor(sensor)) {
        if (!base::Contains(app_names, app)) {
          app_names.push_back(app);
        }
        if (app_names.size() == message_ids_.size()) {
          return app_names;
        }
      }
    }
  }

  return app_names;
}

void PrivacyHubNotification::SetNotificationContent() {
  const std::vector<std::u16string> apps = GetAppsAccessingSensors();

  if (const size_t num_apps = apps.size(); num_apps < message_ids_.size()) {
    builder_.SetMessageWithArgs(message_ids_.at(num_apps), apps);
  } else {
    builder_.SetMessageId(message_ids_.at(0));
  }
}

message_center::RichNotificationData
PrivacyHubNotification::MakeOptionalFields() const {
  message_center::RichNotificationData optional_fields;
  optional_fields.remove_on_click = true;
  optional_fields.buttons.emplace_back(button_text_);

  return optional_fields;
}

}  // namespace ash
