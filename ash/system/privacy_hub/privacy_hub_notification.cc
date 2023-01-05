// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

PrivacyHubNotificationClickDelegate::PrivacyHubNotificationClickDelegate(
    base::RepeatingClosure button_click)
    : button_callback_(std::move(button_click)) {}
PrivacyHubNotificationClickDelegate::~PrivacyHubNotificationClickDelegate() =
    default;

void PrivacyHubNotificationClickDelegate::Click(
    const absl::optional<int>& button_index,
    const absl::optional<std::u16string>& reply) {
  if (button_index.has_value()) {
    button_callback_.Run();
  } else {
    if (!message_callback_.is_null()) {
      message_callback_.Run();
    }

    PrivacyHubNotificationController::OpenPrivacyHubSettingsPage();
  }
}

void PrivacyHubNotificationClickDelegate::SetMessageClickCallback(
    base::RepeatingClosure callback) {
  message_callback_ = std::move(callback);
}

PrivacyHubNotification::PrivacyHubNotification(
    const std::string& id,
    const int title_id,
    const MessageIds& message_ids,
    const SensorSet sensors_for_apps,
    const scoped_refptr<PrivacyHubNotificationClickDelegate> delegate,
    const ash::NotificationCatalogName catalog_name,
    const int button_id)
    : id_(id), message_ids_(message_ids), sensors_for_apps_(sensors_for_apps) {
  DCHECK(!message_ids_.empty());
  DCHECK(message_ids_.size() < 2u || !sensors_for_apps_.Empty())
      << "Specify at least one sensor when providing more than one message ID";
  DCHECK(delegate);

  message_center::RichNotificationData optional_fields;
  optional_fields.remove_on_click = true;
  optional_fields.buttons.emplace_back(l10n_util::GetStringUTF16(button_id));

  builder_.SetId(id)
      .SetCatalogName(catalog_name)
      .SetDelegate(std::move(delegate))
      .SetTitleId(title_id)
      .SetOptionalFields(optional_fields)
      .SetSmallImage(vector_icons::kSettingsIcon)
      .SetWarningLevel(message_center::SystemNotificationWarningLevel::NORMAL);
}

PrivacyHubNotification::PrivacyHubNotification(PrivacyHubNotification&&) =
    default;
PrivacyHubNotification& PrivacyHubNotification::operator=(
    PrivacyHubNotification&&) = default;
PrivacyHubNotification::~PrivacyHubNotification() = default;

void PrivacyHubNotification::Show() {
  const std::vector<std::u16string> apps = GetAppsAccessingSensors();

  if (const size_t num_apps = apps.size(); num_apps < message_ids_.size()) {
    builder_.SetMessageWithArgs(message_ids_.at(num_apps), apps);
  } else {
    builder_.SetMessageId(message_ids_.at(0));
  }

  message_center::MessageCenter::Get()->AddNotification(builder_.BuildPtr());
}

void PrivacyHubNotification::Hide() {
  message_center::MessageCenter::Get()->RemoveNotification(id_,
                                                           /*by_user=*/false);
}

std::vector<std::u16string> PrivacyHubNotification::GetAppsAccessingSensors()
    const {
  std::vector<std::u16string> app_names;

  if (SensorDisabledNotificationDelegate* delegate =
          SensorDisabledNotificationDelegate::Get()) {
    for (SensorDisabledNotificationDelegate::Sensor sensor :
         sensors_for_apps_) {
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

}  // namespace ash
