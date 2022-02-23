// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hps/hps_notify_notification_blocker.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/unified/hps_notify_controller.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

constexpr char kNotifierId[] = "hps-notify";

// Returns the right message for the number of titles.
std::u16string TitlesBlockedMessage(const std::vector<std::u16string>& titles) {
  switch (titles.size()) {
    case 0:
      return u"";
    case 1:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_1, titles[0]);
    case 2:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_2, titles[0],
          titles[1]);
    default:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_3_PLUS, titles[0],
          titles[1]);
  }
}

}  // namespace

HpsNotifyNotificationBlocker::HpsNotifyNotificationBlocker(
    message_center::MessageCenter* message_center,
    HpsNotifyController* controller)
    : NotificationBlocker(message_center),
      message_center_(message_center),
      controller_(controller) {
  controller_observation_.Observe(controller_);

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  message_center_observation_.Observe(message_center_);

  UpdateInfoNotificationIfNecessary();
}

HpsNotifyNotificationBlocker::~HpsNotifyNotificationBlocker() = default;

void HpsNotifyNotificationBlocker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  OnBlockingActiveChanged();

  // Listen to changes to the user's preferences.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      base::BindRepeating(
          &HpsNotifyNotificationBlocker::OnBlockingActiveChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

void HpsNotifyNotificationBlocker::OnBlockingActiveChanged() {
  NotifyBlockingStateChanged();
  UpdateInfoNotificationIfNecessary();
}

bool HpsNotifyNotificationBlocker::ShouldShowNotification(
    const message_center::Notification& notification) const {
  // Never show the info notification in the notification tray.
  return notification.id() != kInfoNotificationId;
}

bool HpsNotifyNotificationBlocker::ShouldShowNotificationAsPopup(
    const message_center::Notification& notification) const {
  // If we've populated our info popup, we're definitely hiding some other
  // notifications and need to inform the user.
  if (notification.id() == kInfoNotificationId)
    return true;

  // We don't suppress any popups while inactive.
  if (!BlockingActive())
    return true;

  // Always show important system notifications.
  // An exception: SMS content is personal information.
  return notification.notifier_id().type ==
             message_center::NotifierType::SYSTEM_COMPONENT &&
         !base::StartsWith(notification.id(), SmsObserver::kNotificationPrefix);
}

void HpsNotifyNotificationBlocker::OnSnoopingStatusChanged(bool /*snooper*/) {
  // Need to reevaluate blocking for (i.e. un/hide) all notifications when a
  // snooper appears. This also catches disabling the snooping feature all
  // together, since that is translated to a "no snooper" event by the
  // controller.
  OnBlockingActiveChanged();
}

void HpsNotifyNotificationBlocker::OnHpsNotifyControllerDestroyed() {
  controller_observation_.Reset();
}

void HpsNotifyNotificationBlocker::OnNotificationAdded(
    const std::string& notification_id) {
  if (notification_id != kInfoNotificationId)
    UpdateInfoNotificationIfNecessary();
}

void HpsNotifyNotificationBlocker::OnNotificationRemoved(
    const std::string& notification_id,
    bool /*by_user*/) {
  if (notification_id == kInfoNotificationId)
    info_popup_exists_ = false;
  else
    UpdateInfoNotificationIfNecessary();
}

void HpsNotifyNotificationBlocker::OnNotificationUpdated(
    const std::string& notification_id) {
  if (notification_id != kInfoNotificationId)
    UpdateInfoNotificationIfNecessary();
}

void HpsNotifyNotificationBlocker::OnBlockingStateChanged(
    message_center::NotificationBlocker* blocker) {
  if (blocker != this)
    UpdateInfoNotificationIfNecessary();
}

bool HpsNotifyNotificationBlocker::BlockingActive() const {
  // Never block if the feature is disabled.
  const PrefService* const pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service ||
      !pref_service->GetBoolean(
          prefs::kSnoopingProtectionNotificationSuppressionEnabled))
    return false;

  // Only block when there isn't a snooper detected.
  return controller_->SnooperPresent();
}

void HpsNotifyNotificationBlocker::UpdateInfoNotificationIfNecessary() {
  // Collect the IDs whose popups would be shown but for us.
  std::set<std::string> new_blocked_popups;
  if (BlockingActive()) {
    const message_center::NotificationList::PopupNotifications popup_list =
        message_center_->GetPopupNotificationsWithoutBlocker(*this);
    for (const auto* value : popup_list) {
      if (!ShouldShowNotificationAsPopup(*value))
        new_blocked_popups.insert(value->id());
    }
  }

  if (blocked_popups_ == new_blocked_popups)
    return;
  blocked_popups_ = new_blocked_popups;

  if (blocked_popups_.empty()) {
    // No-op if the user has already closed the popup.
    message_center_->RemoveNotification(kInfoNotificationId, /*by_user=*/false);
    info_popup_exists_ = false;
  } else if (info_popup_exists_) {
    message_center_->UpdateNotification(kInfoNotificationId,
                                        CreateInfoNotification());
    message_center_->ResetSinglePopup(kInfoNotificationId);
  } else {
    message_center_->AddNotification(CreateInfoNotification());
    info_popup_exists_ = true;
  }
}

std::unique_ptr<message_center::Notification>
HpsNotifyNotificationBlocker::CreateInfoNotification() const {
  // Create a list of popup titles in descending order of recentness and with no
  // duplicates.
  std::vector<std::u16string> titles;
  std::set<std::u16string> seen_titles;
  for (const message_center::Notification* notification :
       message_center_->GetPopupNotificationsWithoutBlocker(*this)) {
    const std::string& id = notification->id();
    if (blocked_popups_.find(id) == blocked_popups_.end())
      continue;

    // Websites have long URL titles; we use Web here for cleaner messages.
    // TODO(1294649): deduce the right title to use in this case and replace raw
    //                literal with a localized string.
    const std::u16string& title =
        notification->notifier_id().title.value_or(u"Web");
    if (seen_titles.find(title) != seen_titles.end())
      continue;

    titles.push_back(title);
    seen_titles.insert(title);
  }

  // TODO(1276903): find out how to make notification disappear after a few
  // seconds.
  // TODO(1276903): find out how to correctly show a system cog icon or similar.
  auto notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kInfoNotificationId,
      l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_TITLE),
      TitlesBlockedMessage(titles),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::NotificationDelegate>(),
      kSettingsIcon, message_center::SystemNotificationWarningLevel::NORMAL);

  return notification;
}

}  // namespace ash
