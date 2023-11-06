// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/coral/coral_controller.h"

#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/command_line.h"
#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

// The hash value for the secret key of the coral feature.
constexpr char kCoralKeyHash[] =
    "\x3A\x92\xA0\xFA\x8A\x13\x53\x56\x8B\x7A\x14\x7C\x18\x7B\x0B\x31\x0A\x3B"
    "\xE9\x59";

// Every 4 hours, we try to collect the data.
constexpr base::TimeDelta record_duration = base::Hours(4);

}  // namespace

CoralController::CoralController() {
  // If it's created, the secret key must have matched.
  CHECK(IsSecretKeyMatched());

  data_collection_timer_.Start(
      FROM_HERE, record_duration,
      base::BindRepeating(&CoralController::CollectDataPeriodically,
                          weak_factory_.GetWeakPtr()));
}

CoralController::~CoralController() {}

// basic
const char CoralController::kDataCollectionNotificationId[] =
    "data_collection_notification_id";

// static
bool CoralController::IsSecretKeyMatched() {
  // Commandline looks like:
  //  out/Default/chrome --user-data-dir=/tmp/tmp123
  //  --coral-feature-key="INSERT KEY HERE" --enable-features=CoralFeature
  const std::string& provided_key_hash = base::SHA1HashString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kCoralFeatureKey));

  bool coral_key_matched = (provided_key_hash == kCoralKeyHash);
  if (!coral_key_matched) {
    LOG(ERROR) << "Provided secret key does not match with the expected one.";
  }

  return coral_key_matched;
}

void CoralController::Click(const absl::optional<int>& button_index,
                            const absl::optional<std::u16string>& reply) {
  if (!button_index) {
    return;
  }
  // TODO: bring up
}

void CoralController::CollectDataPeriodically() {
  // Show a notification to users.
  message_center::RichNotificationData notification_data;

  message_center::ButtonInfo confirm_button(u"Yes");
  notification_data.buttons.push_back(confirm_button);
  message_center::ButtonInfo cancel_button(u"No");
  notification_data.buttons.push_back(cancel_button);

  std::u16string title(u"Share your task groups with us?");
  std::u16string message(u"Share your tabs and apps groups with us. ");

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          CoralController::kDataCollectionNotificationId, title, message,
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              CoralController::kDataCollectionNotificationId,
              NotificationCatalogName::kCoralFeature),
          notification_data,
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_factory_.GetWeakPtr()),
          kUnifiedMenuInfoIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_priority(message_center::SYSTEM_PRIORITY);

  // Add the notification to the message center
  auto* message_center = message_center::MessageCenter::Get();
  message_center->RemoveNotification(notification->id(),
                                     /*by_user=*/false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
