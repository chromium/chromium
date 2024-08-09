// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/caps_lock_notification_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/metrics/user_metrics.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/pref_names.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

using message_center::MessageCenter;
using message_center::Notification;
using SystemTrayButtonSize = ash::UnifiedSystemTrayModel::SystemTrayButtonSize;

namespace ash {

namespace {

const char kCapsLockNotificationId[] = "capslock";
const char kNotifierCapsLock[] = "ash.caps-lock";

int GetMessageStringId() {
  if (!features::IsInputDeviceSettingsSplitEnabled()) {
    return CapsLockNotificationController::IsSearchKeyMappedToCapsLock()
               ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH
               : IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  }

  const ash::mojom::Keyboard* keyboard =
      Shell::Get()
          ->input_device_settings_controller()
          ->GetGeneralizedKeyboard();

  if (keyboard != nullptr) {
    // If the keyboard has function key, return function key message id.
    if (base::ranges::find(keyboard->modifier_keys,
                           ui::mojom::ModifierKey::kFunction) !=
        keyboard->modifier_keys.end()) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      return IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_FN;
#else
      return IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_FN_SEARCH;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
    }

    // If the search key is remapped to kCapsLock, return search key message id.
    auto iter = keyboard->settings->modifier_remappings.find(
        ui::mojom::ModifierKey::kMeta);
    if (iter != keyboard->settings->modifier_remappings.end() &&
        iter->second == ui::mojom::ModifierKey::kCapsLock) {
      return IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH;
    }
  }

  // Otherwise return alt search key message id.
  return IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
}

std::unique_ptr<Notification> CreateNotification() {
  const gfx::VectorIcon& capslock_icon =
      Shell::Get()->keyboard_capability()->IsModifierSplitEnabled()
          ? kModifierSplitNotificationCapslockIcon
          : kNotificationCapslockIcon;
  std::unique_ptr<Notification> notification = ash::CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kCapsLockNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED),
      l10n_util::GetStringUTF16(GetMessageStringId()),
      std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCapsLock,
                                 NotificationCatalogName::kCapsLock),
      message_center::RichNotificationData(), nullptr, capslock_icon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_pinned(true);

  SystemTrayButtonSize primary_tray_button_size =
      Shell::GetPrimaryRootWindowController()
          ->GetStatusAreaWidget()
          ->unified_system_tray()
          ->model()
          ->GetSystemTrayButtonSize();

  // Set the priority to low to prevent the notification showing as a popup in
  // medium or large size tray button because we already show an icon in tray
  // for this in the feature.
  if (primary_tray_button_size != SystemTrayButtonSize::kSmall) {
    notification->set_priority(message_center::LOW_PRIORITY);
  }

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
         static_cast<int>(ui::mojom::ModifierKey::kCapsLock);
}

void CapsLockNotificationController::OnCapsLockChanged(bool enabled) {
  // Send an a11y alert.
  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      enabled ? AccessibilityAlert::CAPS_ON : AccessibilityAlert::CAPS_OFF);

  if (enabled) {
    base::RecordAction(base::UserMetricsAction("StatusArea_CapsLock_Popup"));
    MessageCenter::Get()->AddNotification(CreateNotification());
  } else {
    MessageCenter::Get()->RemoveNotification(kCapsLockNotificationId, false);
  }
}

}  // namespace ash
