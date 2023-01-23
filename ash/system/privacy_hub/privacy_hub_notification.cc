// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_hub/privacy_hub_notification.h"

#include "ash/system/privacy_hub/privacy_hub_notification_controller.h"
#include "base/containers/contains.h"
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

}  // namespace

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

PrivacyHubNotification::~PrivacyHubNotification() = default;

void PrivacyHubNotification::Show() {
  if (remove_timer_.IsRunning()) {
    // Calling `Show()` soon after calling `Hide()` for the same notification
    // usually happens for two cases. In both the update should not be a silent
    // update of just the text but instead resurface the notifiaction:
    // 1. We're updating the app names in the notification and want to make the
    // user aware that the app they just launched also tries to use a sensor
    // that is currently disabled.
    // 2. The user misclicked the app in the tray and closed the 'wrong' app
    // again just to launch the right app a few seconds later. Both apps use
    // the same sensor that is currently disabled.
    remove_timer_.Stop();
    RemoveNotification(id_);
  }

  SetNotificationMessage();

  message_center::MessageCenter::Get()->AddNotification(builder_.BuildPtr());
  last_time_shown_ = last_time_shown_.value_or(base::Time::Now());
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
  if (message_center::MessageCenter::Get()->FindNotificationById(id_)) {
    SetNotificationMessage();
    message_center::MessageCenter::Get()->UpdateNotification(
        id_, builder_.BuildPtr());
  }
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

void PrivacyHubNotification::SetNotificationMessage() {
  const std::vector<std::u16string> apps = GetAppsAccessingSensors();

  if (const size_t num_apps = apps.size(); num_apps < message_ids_.size()) {
    builder_.SetMessageWithArgs(message_ids_.at(num_apps), apps);
  } else {
    builder_.SetMessageId(message_ids_.at(0));
  }
}

}  // namespace ash
