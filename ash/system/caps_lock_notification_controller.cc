// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/caps_lock_notification_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kCapsLockNotificationId[] = "capslock";
const char kNotifierCapsLock[] = "ash.caps-lock";

std::unique_ptr<Notification> CreateNotification() {
  const int string_id =
      CapsLockNotificationController::IsSearchKeyMappedToCapsLock()
          ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH
          : IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  std::unique_ptr<Notification> notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kCapsLockNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED),
      l10n_util::GetStringUTF16(string_id),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCapsLock),
      message_center::RichNotificationData(), nullptr,
      kNotificationCapslockIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_pinned(true);
  return notification;
}

}  // namespace

CapsLockNotificationController::CapsLockNotificationController() {
  Shell::Get()->ime_controller()->AddObserver(this);
}

CapsLockNotificationController::~CapsLockNotificationController() {
  Shell::Get()->ime_controller()->RemoveObserver(this);
}

// static
bool CapsLockNotificationController::IsSearchKeyMappedToCapsLock() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Null early in mash startup.
  if (!prefs)
    return false;

  // Don't bother to observe for the pref changing because the system tray
  // menu is rebuilt every time it is opened and the user has to close the
  // menu to open settings to change the pref. It's not worth the complexity
  // to worry about sync changing the pref while the menu or notification is
  // visible.
  return prefs->GetInteger(prefs::kLanguageRemapSearchKeyTo) ==
         static_cast<int>(ui::chromeos::ModifierKey::kCapsLockKey);
}

// static
void CapsLockNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterIntegerPref(
        prefs::kLanguageRemapSearchKeyTo,
        static_cast<int>(ui::chromeos::ModifierKey::kSearchKey));
    return;
  }
}

void CapsLockNotificationController::OnCapsLockChanged(bool enabled) {
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      enabled ? AccessibilityAlert::CAPS_ON : AccessibilityAlert::CAPS_OFF);

  if (enabled) {
    Shell::Get()->metrics()->RecordUserMetricsAction(
        UMA_STATUS_AREA_CAPS_LOCK_POPUP);

    MessageCenter::Get()->AddNotification(CreateNotification());
  } else if (MessageCenter::Get()->FindVisibleNotificationById(
                 kCapsLockNotificationId)) {
    MessageCenter::Get()->RemoveNotification(kCapsLockNotificationId, false);
  }
}

}  // namespace ash
