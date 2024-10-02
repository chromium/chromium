// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_disk_space_monitor.h"

#include "ash/components/arc/arc_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/logging.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace arc {

namespace {

// Returns whether ArcDiskSpaceMonitor should be activated.
bool ShouldActivate() {
  DCHECK(ArcSessionManager::Get());
  DCHECK(ArcSessionManager::Get()->profile());
  // Activate if and only if virtio-blk is used for /data.
  return ShouldUseVirtioBlkData(
      ArcSessionManager::Get()->profile()->GetPrefs());
}

}  // namespace

ArcDiskSpaceMonitor::ArcDiskSpaceMonitor() {
  ArcSessionManager::Get()->AddObserver(this);
}

ArcDiskSpaceMonitor::~ArcDiskSpaceMonitor() {
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcDiskSpaceMonitor::OnArcStarted() {
  if (!ShouldActivate()) {
    VLOG(1) << "Skipping Activation of ArcDiskSpaceMonitor because virtio-blk "
               "is not used for /data";
    return;
  }

  VLOG(1) << "ARC started. Activating ArcDiskSpaceMonitor.";

  // Calling ScheduleCheckDiskSpace(Seconds(0)) instead of CheckDiskSpace()
  // because ArcSessionManager::RequestStopOnLowDiskSpace() doesn't work if it
  // is called directly inside OnArcStarted().
  ScheduleCheckDiskSpace(base::Seconds(0));
}

void ArcDiskSpaceMonitor::OnArcSessionStopped(ArcStopReason stop_reason) {
  if (!ShouldActivate()) {
    return;
  }
  VLOG(1) << "ARC stopped. Deactivating ArcDiskSpaceMonitor.";
  timer_.Stop();
}

void ArcDiskSpaceMonitor::ScheduleCheckDiskSpace(base::TimeDelta delay) {
  timer_.Start(FROM_HERE, delay,
               base::BindOnce(&ArcDiskSpaceMonitor::CheckDiskSpace,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcDiskSpaceMonitor::CheckDiskSpace() {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      "/home/chronos/user",
      base::BindOnce(&ArcDiskSpaceMonitor::OnGetFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDiskSpaceMonitor::OnGetFreeDiskSpace(std::optional<int64_t> reply) {
  if (!reply.has_value() || reply.value() < 0) {
    LOG(ERROR) << "spaced::GetFreeDiskSpace failed. "
               << "Deactivating ArcDiskSpaceMonitor.";
    return;
  }
  const int64_t free_disk_space = reply.value();

  arc::ArcSessionManager* const arc_session_manager =
      arc::ArcSessionManager::Get();
  const ArcSessionManager::State state = arc_session_manager->state();

  VLOG(1) << "ArcSessionManager::State:" << state
          << ", free_disk_space:" << free_disk_space;

  if (state != ArcSessionManager::State::ACTIVE) {
    LOG(WARNING) << "ARC is not active.";
    // No need to call ScheduleCheckDiskSpace() because
    // OnArcStarted() will trigger CheckDiskSpace() when ARC starts.
    return;
  }

  if (free_disk_space < kDiskSpaceThresholdForStoppingArc) {
    LOG(WARNING) << "Stopping ARC due to low disk space. free_disk_space:"
                 << free_disk_space;
    arc_session_manager->RequestStopOnLowDiskSpace();

    // Show a post-stop warning notification.
    MaybeShowNotification(/*is_pre_stop=*/false);

    // ArcDiskSpaceMonitor will be deactivated after ARC is stopeed.
    return;
  }

  if (free_disk_space < kDiskSpaceThresholdForPreStopNotification) {
    // Show a pre-stop warning notification.
    MaybeShowNotification(/*is_pre_stop=*/true);

    ScheduleCheckDiskSpace(kDiskSpaceCheckIntervalShort);
  } else {
    ScheduleCheckDiskSpace(kDiskSpaceCheckIntervalLong);
  }
}

void ArcDiskSpaceMonitor::MaybeShowNotification(bool is_pre_stop) {
  if (is_pre_stop) {
    if (!pre_stop_notification_last_shown_time_.is_null() &&
        base::Time::Now() - pre_stop_notification_last_shown_time_ <
            kPreStopNotificationReshowInterval) {
      // Don't reshow a pre-stop warning notification yet.
      return;
    }
    pre_stop_notification_last_shown_time_ = base::Time::Now();
  }

  const std::string notification_id = is_pre_stop
                                          ? kLowDiskSpacePreStopNotificationId
                                          : kLowDiskSpacePostStopNotificationId;
  const ash::NotificationCatalogName catalog_name =
      is_pre_stop ? ash::NotificationCatalogName::kArcLowDiskSpacePreStop
                  : ash::NotificationCatalogName::kArcLowDiskSpacePostStop;
  const int title_id =
      is_pre_stop ? IDS_ARC_LOW_DISK_SPACE_PRE_STOP_NOTIFICATION_TITLE
                  : IDS_ARC_LOW_DISK_SPACE_POST_STOP_NOTIFICATION_TITLE;
  const int message_id =
      is_pre_stop ? IDS_ARC_LOW_DISK_SPACE_PRE_STOP_NOTIFICATION_MESSAGE
                  : IDS_ARC_LOW_DISK_SPACE_POST_STOP_NOTIFICATION_MESSAGE;

  message_center::Notification notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(title_id),
      l10n_util::GetStringUTF16(message_id),
      l10n_util::GetStringUTF16(IDS_ARC_NOTIFICATION_DISPLAY_SOURCE),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDiskSpaceMonitorNotifierId, catalog_name),
      /*optional_fields=*/message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](std::optional<int> button_index) {})),
      kNotificationStorageFullIcon,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);

  Profile* profile = arc::ArcSessionManager::Get()->profile();
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

}  // namespace arc
