// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_pinned_notification.h"
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

std::u16string GetMessageLabel(HidConnectionTracker* connection_tracker) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<std::u16string> extension_names;
  for (const auto& [origin, state] : connection_tracker->origins()) {
    CHECK_EQ(origin.scheme(), extensions::kExtensionScheme);
    extension_names.push_back(base::UTF8ToUTF16(state.name));
  }
  CHECK(!extension_names.empty());
  if (extension_names.size() == 1) {
    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_EXTENSION_LIST),
        1, extension_names[0]);
  } else if (extension_names.size() == 2) {
    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_EXTENSION_LIST),
        2, extension_names[0], extension_names[1]);
  }
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_EXTENSION_LIST),
      static_cast<int>(extension_names.size()), extension_names[0],
      extension_names[1], static_cast<int>(extension_names.size() - 2));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

}  // namespace

HidPinnedNotification::HidPinnedNotification() = default;

HidPinnedNotification::~HidPinnedNotification() = default;

// static
std::string HidPinnedNotification::GetNotificationId(Profile* profile) {
  return base::StrCat({"chrome://device_indicator/hid/", profile->UniqueId()});
}

std::unique_ptr<message_center::Notification>
HidPinnedNotification::CreateNotification(Profile* profile) {
  message_center::RichNotificationData data;
  data.buttons.emplace_back(GetContentSettingsLabel());
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
                hid_connection_tracker->ShowContentSettingsExceptions();
              },
              profile));
  auto notification_id = GetNotificationId(profile);
  auto* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/false);
  DCHECK(hid_connection_tracker);
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      GetTitleLabel(hid_connection_tracker->origins().size(),
                    hid_connection_tracker->total_connection_count()),
      GetMessageLabel(hid_connection_tracker), /*icon=*/ui::ImageModel(),
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
  // Set to low priority so it doesn't create a popup.
  notification->set_priority(message_center::LOW_PRIORITY);
  return notification;
}

void HidPinnedNotification::DisplayNotification(
    std::unique_ptr<message_center::Notification> notification) {
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

void HidPinnedNotification::ProfileAdded(Profile* profile) {
  DisplayNotification(CreateNotification(profile));
}

void HidPinnedNotification::ProfileRemoved(Profile* profile) {
  SystemNotificationHelper::GetInstance()->Close(GetNotificationId(profile));
}

void HidPinnedNotification::NotifyConnectionCountUpdated(Profile* profile) {
  DisplayNotification(CreateNotification(profile));
}
