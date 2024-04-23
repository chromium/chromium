// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/sms_observer.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"

namespace ash {

const char SmsObserver::kNotificationPrefix[] = "chrome://network/sms";

namespace {

const char kNotifierSms[] = "ash.sms";

// Send the |message| to notification center to display to users. Note that each
// notification will be assigned with different |message_id| as notification id.
void ShowNotification(const base::Value::Dict* message,
                      const std::string& message_text,
                      const std::string& message_number,
                      int message_id) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (!message_center)
    return;

  std::unique_ptr<message_center::Notification> notification;

  // TODO(estade): should SMS notifications really be shown to all users?
  notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      SmsObserver::kNotificationPrefix + base::NumberToString(message_id),
      base::ASCIIToUTF16(message_number),
      base::CollapseWhitespace(base::UTF8ToUTF16(message_text),
                               false /* trim_sequences_with_line_breaks */),
      std::u16string(), GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierSms, NotificationCatalogName::kSMS),
      message_center::RichNotificationData(), nullptr, kNotificationSmsSyncIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  message_center->AddNotification(std::move(notification));
}

}  // namespace

SmsObserver::SmsObserver() {
  // TODO(armansito): SMS could be a special case for cellular that requires a
  // user (perhaps the owner) to be logged in. If that is the case, then an
  // additional check should be done before subscribing for SMS notifications.
  if (NetworkHandler::IsInitialized()) {
      NetworkHandler::Get()->text_message_provider()->AddObserver(this);
  }
}

SmsObserver::~SmsObserver() {
  if (NetworkHandler::IsInitialized()) {
      NetworkHandler::Get()->text_message_provider()->RemoveObserver(this);
  }
}

void SmsObserver::MessageReceived(const std::string& guid,
                                  const TextMessageData& message_data) {
  // TODO(b/328445717): A message might be due to a special "Message Waiting"
  // state that the message is in. Once SMS handling moves to shill, such
  // messages should be filtered there so that this check becomes unnecessary.
  if (!message_data.text.has_value() || message_data.text->empty()) {
    NET_LOG(ERROR) << "SMS message contains no or empty content.";
    return;
  }
  if (!message_data.number.has_value() || message_data.number->empty()) {
    NET_LOG(DEBUG) << "SMS contains no number. Ignoring.";
    return;
  }
  message_id_++;
  // TODO(b/295169036) Remove unused message dictionary once suppress text
  // messages feature flag is removed.
  ShowNotification(/*message=*/nullptr, *message_data.text,
                   *message_data.number, message_id_);

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()
        ->text_message_provider()
        ->LogTextMessageNotificationMetrics(guid);
  }
}

}  // namespace ash
