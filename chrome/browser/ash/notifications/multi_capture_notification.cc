// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/multi_capture_notification.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/safe_ref.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notifier_id.h"
namespace {

const char kMultiCaptureId[] = "multi_capture";
const char kNotifierMultiCapture[] = "ash.multi_capture";

constexpr base::TimeDelta kMinimumNotificationPresenceTime = base::Seconds(6);

std::unique_ptr<message_center::Notification> CreateNotification(
    const url::Origin& origin) {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierMultiCapture,
      ash::NotificationCatalogName::kMultiCapture);

  const std::string host = origin.host();
  std::u16string converted_host;
  if (!base::UTF8ToUTF16(host.c_str(), host.size(), &converted_host)) {
    NOTREACHED();
    return nullptr;
  }

  // TODO(crbug.com/1356101): Add "Don't show again" for managed sessions.
  return ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      base::StrCat({kMultiCaptureId, ":", host}),
      /*title=*/
      l10n_util::GetStringFUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_TITLE,
                                 converted_host),
      /*message=*/
      l10n_util::GetStringFUTF16(IDS_MULTI_CAPTURE_NOTIFICATION_MESSAGE,
                                 converted_host),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(), notifier_id,
      /*optional_fields=*/message_center::RichNotificationData(),
      /*delegate=*/
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          message_center::HandleNotificationClickDelegate::ButtonClickCallback(
              base::DoNothing())),
      ash::kPrivacyIndicatorsScreenShareIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

}  // namespace

namespace ash {

MultiCaptureNotification::NotificationMetadata::NotificationMetadata(
    std::string id,
    base::TimeTicks time_created)
    : id(std::move(id)), time_created(std::move(time_created)) {}
MultiCaptureNotification::NotificationMetadata::NotificationMetadata(
    MultiCaptureNotification::NotificationMetadata&& metadata) = default;
MultiCaptureNotification::NotificationMetadata&
MultiCaptureNotification::NotificationMetadata::operator=(
    MultiCaptureNotification::NotificationMetadata&& metadata) = default;
MultiCaptureNotification::NotificationMetadata::~NotificationMetadata() =
    default;

MultiCaptureNotification::MultiCaptureNotification() {
  DCHECK(Shell::HasInstance());
  multi_capture_service_client_observation_.Observe(
      Shell::Get()->multi_capture_service_client());
}

MultiCaptureNotification::~MultiCaptureNotification() = default;

void MultiCaptureNotification::MultiCaptureStarted(const std::string& label,
                                                   const url::Origin& origin) {
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification(origin);
  notifications_metadata_.emplace(
      label, NotificationMetadata(notification->id(), base::TimeTicks::Now()));
  // TODO(crbug.com/1356102): Make sure the notification does not disappear
  // automatically after some time.
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void MultiCaptureNotification::MultiCaptureStopped(const std::string& label) {
  const auto notifications_metadata_iterator =
      notifications_metadata_.find(label);
  if (notifications_metadata_iterator == notifications_metadata_.end()) {
    LOG(ERROR) << "Label could not be found";
    return;
  }

  NotificationMetadata& metadata = notifications_metadata_iterator->second;
  const base::TimeDelta time_already_shown =
      base::TimeTicks::Now() - metadata.time_created;
  if (time_already_shown >= kMinimumNotificationPresenceTime) {
    SystemNotificationHelper::GetInstance()->Close(
        /*notification_id=*/metadata.id);
    notifications_metadata_.erase(notifications_metadata_iterator);
  } else if (!metadata.closing_timer) {
    metadata.closing_timer = std::make_unique<base::OneShotTimer>();
    metadata.closing_timer->Start(
        FROM_HERE, kMinimumNotificationPresenceTime - time_already_shown,
        base::BindOnce(&MultiCaptureNotification::MultiCaptureStopped,
                       weak_factory_.GetSafeRef(), label));
  }
}

void MultiCaptureNotification::MultiCaptureServiceClientDestroyed() {
  multi_capture_service_client_observation_.Reset();
  for (const auto& [label, notification_metadata] : notifications_metadata_) {
    SystemNotificationHelper::GetInstance()->Close(
        /*notification_id=*/notification_metadata.id);
  }
  notifications_metadata_.clear();
}

}  // namespace ash
