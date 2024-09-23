// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_pinned_notification_renderer.h"

#include "ash/constants/ash_features.h"
#include "base/i18n/message_formatter.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/device_notifications/device_connection_tracker.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

std::u16string GetMessageLabel(DeviceConnectionTracker* connection_tracker,
                               int message_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::vector<std::u16string> extension_names;
  for (const auto& [origin, state] : connection_tracker->origins()) {
    CHECK_EQ(origin.scheme(), extensions::kExtensionScheme);
    extension_names.push_back(base::UTF8ToUTF16(state.name));
  }
  CHECK(!extension_names.empty());
  if (extension_names.size() == 1) {
    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(message_id), 1, extension_names[0]);
  } else if (extension_names.size() == 2) {
    return base::i18n::MessageFormatter::FormatWithNumberedArgs(
        l10n_util::GetStringUTF16(message_id), 2, extension_names[0],
        extension_names[1]);
  }
  return base::i18n::MessageFormatter::FormatWithNumberedArgs(
      l10n_util::GetStringUTF16(message_id),
      static_cast<int>(extension_names.size()), extension_names[0],
      extension_names[1], static_cast<int>(extension_names.size() - 2));
#else
  NOTREACHED();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
}

}  // namespace

DevicePinnedNotificationRenderer::DevicePinnedNotificationRenderer(
    DeviceSystemTrayIcon* device_system_tray_icon,
    const std::string& notification_id_prefix,
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const ash::NotificationCatalogName notification_catalog_name,
#endif
    const int message_id)
    : DeviceSystemTrayIconRenderer(device_system_tray_icon),
      notification_id_prefix_(notification_id_prefix),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      notification_catalog_name_(notification_catalog_name),
#endif
      message_id_(message_id) {}

DevicePinnedNotificationRenderer::~DevicePinnedNotificationRenderer() = default;

void DevicePinnedNotificationRenderer::AddProfile(Profile* profile) {
  DisplayNotification(CreateNotification(profile));
}

void DevicePinnedNotificationRenderer::RemoveProfile(Profile* profile) {
  SystemNotificationHelper::GetInstance()->Close(GetNotificationId(profile));
}

void DevicePinnedNotificationRenderer::NotifyConnectionUpdated(
    Profile* profile) {
  DisplayNotification(CreateNotification(profile));
}

void DevicePinnedNotificationRenderer::DisplayNotification(
    std::unique_ptr<message_center::Notification> notification) {
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

std::string DevicePinnedNotificationRenderer::GetNotificationId(
    Profile* profile) {
  return base::StrCat({notification_id_prefix_, profile->UniqueId()});
}

std::unique_ptr<message_center::Notification>
DevicePinnedNotificationRenderer::CreateNotification(Profile* profile) {
  message_center::RichNotificationData data;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The new pinned notification view uses a settings icon button.
  if (ash::features::AreOngoingProcessesEnabled()) {
    data.buttons.emplace_back(message_center::ButtonInfo(
        /*vector_icon=*/&vector_icons::kSettingsIcon,
        /*accessible_name=*/device_system_tray_icon_
            ->GetContentSettingsLabel()));
  } else {
    data.buttons.emplace_back(
        device_system_tray_icon_->GetContentSettingsLabel());
  }
#else
  data.buttons.emplace_back(
      device_system_tray_icon_->GetContentSettingsLabel());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  auto* device_connection_tracker =
      device_system_tray_icon_->GetConnectionTracker(profile->GetWeakPtr());
  DCHECK(device_connection_tracker);
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](DeviceConnectionTracker* connection_tracker,
                 std::optional<int> button_index) {
                // ConnectionTracker guarantees that RemoveProfile() is
                // called on Profile destruction so it is impossible to interact
                // with the notification after `connection_tracker` becomes a
                // dangling pointer.
                if (!button_index) {
                  return;
                }

                DCHECK_EQ(*button_index, 0);
                connection_tracker->ShowContentSettingsExceptions();
              },
              device_connection_tracker));
  auto notification_id = GetNotificationId(profile);
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      device_system_tray_icon_->GetTitleLabel(
          device_connection_tracker->origins().size(),
          device_connection_tracker->total_connection_count()),
      GetMessageLabel(device_connection_tracker, message_id_),
      /*icon=*/ui::ImageModel(),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id, notification_catalog_name_),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id),
#endif
      data, std::move(delegate));
  notification->set_vector_small_image(device_system_tray_icon_->GetIcon());
  notification->set_pinned(true);
  // Set to low priority so it doesn't create a popup.
  notification->set_priority(message_center::LOW_PRIORITY);
  return notification;
}
