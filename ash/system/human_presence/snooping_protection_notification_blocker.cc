// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/snooping_protection_notification_blocker.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/human_presence/human_presence_metrics.h"
#include "ash/system/human_presence/snooping_protection_controller.h"
#include "ash/system/human_presence/snooping_protection_notification_blocker_internal.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/sms_observer.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/status_area_widget.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/dbus/hps/hps_service.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

namespace metrics = ash::snooping_protection_metrics;

constexpr char kNotifierId[] = "hps-notify";

// Returns the capitalized version of an improper-noun notifier title, or the
// unchanged title if it is a proper noun.
std::u16string GetCapitalizedNotifierTitle(const std::u16string& title) {
  static base::NoDestructor<std::map<std::u16string, std::u16string>>
      kCapitalizedTitles(
          {{l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_APP_TITLE_LOWER),
            l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_APP_TITLE_UPPER)},
           {l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SYSTEM_TITLE_LOWER),
            l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SYSTEM_TITLE_UPPER)},
           {l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_LOWER),
            l10n_util::GetStringUTF16(
                IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_WEB_TITLE_UPPER)}});

  const auto it = kCapitalizedTitles->find(title);
  return it == kCapitalizedTitles->end() ? title : it->second;
}

}  // namespace

namespace hps_internal {

// Returns the popup message listing the correct notifier titles and the correct
// number of titles.
std::u16string GetTitlesBlockedMessage(
    const std::vector<std::u16string>& titles) {
  DCHECK(!titles.empty());

  const std::u16string& first_title = GetCapitalizedNotifierTitle(titles[0]);
  switch (titles.size()) {
    case 1:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_1, first_title);
    case 2:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_2, first_title,
          titles[1]);
    default:
      return l10n_util::GetStringFUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_MESSAGE_3_PLUS,
          first_title, titles[1]);
  }
}

}  // namespace hps_internal

SnoopingProtectionNotificationBlocker::SnoopingProtectionNotificationBlocker(
    message_center::MessageCenter* message_center,
    SnoopingProtectionController* controller)
    : NotificationBlocker(message_center),
      message_center_(message_center),
      controller_(controller) {
  controller_observation_.Observe(controller_.get());

  // Session controller is instantiated before us in the shell.
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  message_center_observation_.Observe(message_center_.get());

  UpdateInfoNotificationIfNecessary();
}

SnoopingProtectionNotificationBlocker::
    ~SnoopingProtectionNotificationBlocker() = default;

void SnoopingProtectionNotificationBlocker::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  OnBlockingActiveChanged();

  // Listen to changes to the user's preferences.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);
  pref_change_registrar_->Add(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled,
      base::BindRepeating(
          &SnoopingProtectionNotificationBlocker::OnBlockingPrefChanged,
          weak_ptr_factory_.GetWeakPtr()));
}

void SnoopingProtectionNotificationBlocker::OnBlockingActiveChanged() {
  NotifyBlockingStateChanged();
  UpdateInfoNotificationIfNecessary();
}

void SnoopingProtectionNotificationBlocker::OnBlockingPrefChanged() {
  DCHECK(pref_change_registrar_);
  DCHECK(pref_change_registrar_->prefs());
  const bool pref_enabled = pref_change_registrar_->prefs()->GetBoolean(
      prefs::kSnoopingProtectionNotificationSuppressionEnabled);
  base::UmaHistogramBoolean(
      metrics::kNotificationSuppressionEnabledHistogramName, pref_enabled);

  OnBlockingActiveChanged();
}

bool SnoopingProtectionNotificationBlocker::ShouldShowNotificationAsPopup(
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

void SnoopingProtectionNotificationBlocker::OnSnoopingStatusChanged(
    bool /*snooper*/) {
  // Need to reevaluate blocking for (i.e. un/hide) all notifications when a
  // snooper appears. This also catches disabling the snooping feature all
  // together, since that is translated to a "no snooper" event by the
  // controller.
  OnBlockingActiveChanged();
}

void SnoopingProtectionNotificationBlocker::
    OnSnoopingProtectionControllerDestroyed() {
  controller_observation_.Reset();
}

void SnoopingProtectionNotificationBlocker::OnNotificationAdded(
    const std::string& notification_id) {
  if (notification_id != kInfoNotificationId)
    UpdateInfoNotificationIfNecessary();
}

void SnoopingProtectionNotificationBlocker::OnNotificationRemoved(
    const std::string& notification_id,
    bool /*by_user*/) {
  if (notification_id == kInfoNotificationId)
    info_popup_exists_ = false;
  else
    UpdateInfoNotificationIfNecessary();
}

void SnoopingProtectionNotificationBlocker::OnNotificationUpdated(
    const std::string& notification_id) {
  if (notification_id != kInfoNotificationId)
    UpdateInfoNotificationIfNecessary();
}

void SnoopingProtectionNotificationBlocker::OnBlockingStateChanged(
    message_center::NotificationBlocker* blocker) {
  if (blocker != this)
    UpdateInfoNotificationIfNecessary();
}

void SnoopingProtectionNotificationBlocker::Close(bool by_user) {}

void SnoopingProtectionNotificationBlocker::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!button_index.has_value())
    return;
  switch (button_index.value()) {
    // Show notifications button
    case 0:
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->GetStatusAreaWidget()
          ->notification_center_tray()
          ->ShowBubble();
      break;
    // Show privacy settings button
    case 1:
      Shell::Get()->system_tray_model()->client()->ShowSmartPrivacySettings();
      break;
    default:
      NOTREACHED() << "Unknown button index value";
  }
}

bool SnoopingProtectionNotificationBlocker::BlockingActive() const {
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

void SnoopingProtectionNotificationBlocker::
    UpdateInfoNotificationIfNecessary() {
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
    message_center_->ResetSinglePopup(kInfoNotificationId);
    info_popup_exists_ = true;
  }
}

std::unique_ptr<message_center::Notification>
SnoopingProtectionNotificationBlocker::CreateInfoNotification() const {
  // Create a list of popup titles in descending order of recentness and with no
  // duplicates.
  std::vector<std::u16string> titles;
  std::set<std::u16string> seen_titles;
  for (const message_center::Notification* notification :
       message_center_->GetPopupNotificationsWithoutBlocker(*this)) {
    const std::string& id = notification->id();
    if (!base::Contains(blocked_popups_, id)) {
      continue;
    }

    // Use a human readable-title (e.g. "Web" vs "https://somesite.com:443").
    const std::u16string& title =
        hps_internal::GetNotifierTitle<apps::AppRegistryCacheWrapper>(
            notification->notifier_id(),
            Shell::Get()->session_controller()->GetActiveAccountId());
    if (base::Contains(seen_titles, title)) {
      continue;
    }

    titles.push_back(title);
    seen_titles.insert(title);
  }

  // TODO(1276903): find out how to make the correct notification count.
  message_center::RichNotificationData notification_data;

  notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SHOW_BUTTON_TEXT)));
  notification_data.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_SETTINGS_BUTTON_TEXT)));
  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kInfoNotificationId,
      l10n_util::GetStringUTF16(
          IDS_ASH_SMART_PRIVACY_SNOOPING_NOTIFICATION_TITLE),
      hps_internal::GetTitlesBlockedMessage(titles),
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kHPSNotify),
      notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetMutableWeakPtr()),
      kSystemTraySnoopingProtectionIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  return notification;
}

}  // namespace ash
