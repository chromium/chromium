// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_connection_tracker.h"

#include <string>

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

std::u16string GetDeviceConnectedNotificationMessage(
    Profile* profile,
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    const auto* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    DCHECK(extension_registry);
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    return l10n_util::GetStringFUTF16(
        IDS_WEBHID_DEVICE_CONNECTED_BY_EXTENSION_NOTIFICATION_MESSAGE,
        base::UTF8ToUTF16(extension->name()));
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED();
  return u"";
}

std::string GetDeviceOpenedNotificationId(Profile* profile,
                                          const url::Origin& origin) {
  return base::StrCat(
      {"webhid.opened.", profile->UniqueId(), ".", origin.host()});
}

}  // namespace

HidConnectionTracker::HidConnectionTracker(Profile* profile)
    : profile_(profile) {}

HidConnectionTracker::~HidConnectionTracker() {
  CleanUp();
}

void HidConnectionTracker::IncrementConnectionCount() {
  ++connection_count_;
  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }

  if (connection_count_ == 1) {
    hid_system_tray_icon->AddProfile(profile_);
  } else {
    hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
  }
}

void HidConnectionTracker::DecrementConnectionCount() {
  --connection_count_;
  auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
  if (!hid_system_tray_icon) {
    return;
  }

  if (connection_count_ == 0) {
    hid_system_tray_icon->RemoveProfile(profile_);
  } else {
    hid_system_tray_icon->NotifyConnectionCountUpdated(profile_);
  }
}

void HidConnectionTracker::NotifyDeviceConnected(const url::Origin& origin) {
  auto delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              [](HidConnectionTracker* hid_connection_tracker,
                 const url::Origin& origin, absl::optional<int> button_index) {
                // |hid_connection_tracker| will always be valid here because an
                // active notification prevents the Profile (which owns the
                // HidConnectionTracker as a KeyedService) from being destroyed.
                hid_connection_tracker->ShowSiteSettings(origin);
              },
              this, origin));

  auto notification_id = GetDeviceOpenedNotificationId(profile_, origin);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(
          IDS_WEBHID_DEVICE_CONNECTED_BY_EXTENSION_NOTIFICATION_TITLE),
      GetDeviceConnectedNotificationMessage(profile_, origin),
      ui::ImageModel::FromVectorIcon(vector_icons::kVideogameAssetIcon,
                                     ui::kColorIcon, 64),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), /*origin_url=*/{},
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id,
                                 ash::NotificationCatalogName::kWebHid),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::RichNotificationData(), std::move(delegate));
  NotificationDisplayService::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification,
      /*metadata=*/nullptr);
}

void HidConnectionTracker::ShowHidContentSettingsExceptions() {
  chrome::ShowContentSettingsExceptionsForProfile(
      profile_, ContentSettingsType::HID_CHOOSER_DATA);
}

void HidConnectionTracker::ShowSiteSettings(const url::Origin& origin) {
  chrome::ShowSiteSettings(profile_, origin.GetURL());
}

void HidConnectionTracker::CleanUp() {
  if (connection_count_ > 0) {
    connection_count_ = 0;
    auto* hid_system_tray_icon = g_browser_process->hid_system_tray_icon();
    if (hid_system_tray_icon)
      hid_system_tray_icon->RemoveProfile(profile_);
  }
}
