// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_pinned_notification.h"

#include "base/containers/contains.h"
#include "base/strings/strcat.h"
#include "build/build_config.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

HidPinnedNotification::HidPinnedNotification() = default;

HidPinnedNotification::~HidPinnedNotification() = default;

// static
std::string HidPinnedNotification::GetNotificationId(Profile* profile) {
  return base::StrCat({"chrome://device_indicator/hid/", profile->UniqueId()});
}

std::unique_ptr<message_center::Notification>
HidPinnedNotification::CreateNotification(Profile* profile) {
  message_center::RichNotificationData data;
  data.buttons.emplace_back(GetManageHidDeviceButtonLabel(profile));
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](Profile* profile, absl::optional<int> button_index) {
                // HidConnectionTracker guarantees that RemoveProfile() is
                // called on Profile destruction so it is impossible to interact
                // with the notification after |profile| becomes a dangling
                // pointer.
                if (!button_index)
                  return;

                DCHECK_EQ(*button_index, 0);
                auto* hid_connection_tracker =
                    HidConnectionTrackerFactory::GetForProfile(
                        profile, /*create=*/false);
                DCHECK(hid_connection_tracker);
                hid_connection_tracker->ShowHidContentSettingsExceptions();
              },
              profile));
  auto notification_id = GetNotificationId(profile);
  auto* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/false);
  DCHECK(hid_connection_tracker);
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      GetTooltipLabel(hid_connection_tracker->connection_count()),
      /*message=*/std::u16string(), /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id,
                                 ash::NotificationCatalogName::kWebHid),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id),
#endif
      data, std::move(delegate));
  notification->set_small_image(gfx::Image(GetStatusTrayIcon()));
  notification->set_pinned(true);
  // Set to system priority so it will never timeout.
  notification->SetSystemPriority();
  return notification;
}

void HidPinnedNotification::DisplayNotification(
    std::unique_ptr<message_center::Notification> notification) {
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void HidPinnedNotification::AddProfile(Profile* profile) {
  DCHECK(!base::Contains(profiles_, profile));
  profiles_.emplace(profile);
  DisplayNotification(CreateNotification(profile));
}

void HidPinnedNotification::RemoveProfile(Profile* profile) {
  DCHECK(base::Contains(profiles_, profile));
  profiles_.erase(profile);
  SystemNotificationHelper::GetInstance()->Close(GetNotificationId(profile));
}

void HidPinnedNotification::NotifyConnectionCountUpdated(Profile* profile) {
  DCHECK(base::Contains(profiles_, profile));
  DisplayNotification(CreateNotification(profile));
}
