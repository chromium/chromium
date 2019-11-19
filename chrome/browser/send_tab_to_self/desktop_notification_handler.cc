// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/send_tab_to_self/desktop_notification_handler.h"

#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_metrics.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/strings/grit/ui_strings.h"

namespace send_tab_to_self {

namespace {

const char kDesktopNotificationSharedPrefix[] = "shared";

}  // namespace

DesktopNotificationHandler::DesktopNotificationHandler(Profile* profile)
    : profile_(profile) {}

DesktopNotificationHandler::~DesktopNotificationHandler() = default;

void DesktopNotificationHandler::DisplayNewEntries(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  for (const SendTabToSelfEntry* entry : new_entries) {
    const base::string16 device_info = l10n_util::GetStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_DEVICE_INFO,
        base::UTF8ToUTF16(entry->GetDeviceName()));
    const GURL& url = entry->GetURL();
    message_center::RichNotificationData optional_fields;
    // Set the notification to be persistent
    optional_fields.never_timeout = true;
    // Declare a notification
    message_center::Notification notification(
        message_center::NOTIFICATION_TYPE_SIMPLE, entry->GetGUID(),
        base::UTF8ToUTF16(entry->GetTitle()), device_info, gfx::Image(),
        base::UTF8ToUTF16(url.host()), url, message_center::NotifierId(url),
        optional_fields, /*delegate=*/nullptr);
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
        NotificationHandler::Type::SEND_TAB_TO_SELF, notification,
        /*metadata=*/nullptr);
    RecordNotificationHistogram(SendTabToSelfNotification::kShown);
  }
}

void DesktopNotificationHandler::DismissEntries(
    const std::vector<std::string>& guids) {
  for (const std::string& guid : guids) {
    NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
        NotificationHandler::Type::SEND_TAB_TO_SELF, guid);
  }
}

void DesktopNotificationHandler::OnClose(Profile* profile,
                                         const GURL& origin,
                                         const std::string& notification_id,
                                         bool by_user,
                                         base::OnceClosure completed_closure) {
  if (notification_id.find(kDesktopNotificationSharedPrefix)) {
    SendTabToSelfSyncServiceFactory::GetForProfile(profile)
        ->GetSendTabToSelfModel()
        ->DismissEntry(notification_id);
    RecordNotificationHistogram(SendTabToSelfNotification::kDismissed);
  }
  std::move(completed_closure).Run();
}

void DesktopNotificationHandler::OnClick(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply,
    base::OnceClosure completed_closure) {
  if (notification_id.find(kDesktopNotificationSharedPrefix)) {
    // Launch a new tab for the notification's |origin|,
    // and close the activated notification.
    NavigateParams params(profile, origin, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&params);
    NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
        NotificationHandler::Type::SEND_TAB_TO_SELF, notification_id);
    // Marks the the entry as opened in SendTabToSelfModel
    SendTabToSelfSyncServiceFactory::GetForProfile(profile)
        ->GetSendTabToSelfModel()
        ->MarkEntryOpened(notification_id);
    RecordNotificationHistogram(SendTabToSelfNotification::kOpened);
  }
  std::move(completed_closure).Run();
}

void DesktopNotificationHandler::DisplaySendingConfirmation(
    const SendTabToSelfEntry& entry,
    const std::string& target_device_name) {
  const base::string16 confirm_str = l10n_util::GetStringFUTF16(
      IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_SUCCESS,
      base::UTF8ToUTF16(target_device_name));
  const GURL& url = entry.GetURL();
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kDesktopNotificationSharedPrefix + entry.GetGUID(), confirm_str,
      base::UTF8ToUTF16(entry.GetTitle()), gfx::Image(),
      base::UTF8ToUTF16(url.host()), url, message_center::NotifierId(url),
      message_center::RichNotificationData(), /*delegate=*/nullptr);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SEND_TAB_TO_SELF, notification,
      /*metadata=*/nullptr);
}

void DesktopNotificationHandler::DisplayFailureMessage(const GURL& url) {
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kDesktopNotificationSharedPrefix + base::GenerateGUID(),
      l10n_util::GetStringUTF16(
          IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_TITLE),
      l10n_util::GetStringUTF16(
          IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_MESSAGE),
      gfx::Image(), base::UTF8ToUTF16(url.host()), url,
      message_center::NotifierId(url), message_center::RichNotificationData(),
      /*delegate=*/nullptr);
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::SEND_TAB_TO_SELF, notification,
      /*metadata=*/nullptr);
}

const Profile* DesktopNotificationHandler::GetProfile() const {
  return profile_;
}

}  // namespace send_tab_to_self
