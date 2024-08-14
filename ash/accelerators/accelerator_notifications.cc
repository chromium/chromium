// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_notifications.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_lookup.h"
#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/strings/string_split.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

using gfx::VectorIcon;
using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotificationDelegate;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace {

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

constexpr char kNotifierAccelerator[] = "ash.accelerator-controller";
constexpr char kSpokenFeedbackToggleAccelNotificationId[] =
    "chrome://settings/accessibility/spokenfeedback";

// Ensures that there are no word breaks at the "+"s in the shortcut texts such
// as "Ctrl+Shift+Space".
void EnsureNoWordBreaks(std::u16string* shortcut_text) {
  std::vector<std::u16string> keys = base::SplitString(
      *shortcut_text, u"+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (keys.size() < 2U)
    return;

  // The plus sign surrounded by the word joiner to guarantee an non-breaking
  // shortcut.
  const std::u16string non_breaking_plus = u"\u2060+\u2060";
  shortcut_text->clear();
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    *shortcut_text += keys[i];
    *shortcut_text += non_breaking_plus;
  }

  *shortcut_text += keys.back();
}

// Gets the notification message after it formats it in such a way that there
// are no line breaks in the middle of the shortcut texts.
std::u16string GetNotificationText(int message_id, int new_shortcut_id) {
  std::u16string new_shortcut = l10n_util::GetStringUTF16(new_shortcut_id);
  EnsureNoWordBreaks(&new_shortcut);

  return l10n_util::GetStringFUTF16(message_id, new_shortcut);
}

std::unique_ptr<Notification> CreateNotification(
    const std::string& notification_id,
    const NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const VectorIcon& icon,
    scoped_refptr<NotificationDelegate> click_handler = nullptr,
    const RichNotificationData& rich_data = RichNotificationData()) {
  return CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string() /* display source */, GURL(),
      NotifierId(NotifierType::SYSTEM_COMPONENT, kNotifierAccelerator,
                 catalog_name),
      rich_data, click_handler, icon, SystemNotificationWarningLevel::NORMAL);
}

void CreateAndShowStickyNotification(
    const std::string& notification_id,
    const NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const VectorIcon& icon) {
  std::unique_ptr<Notification> notification =
      CreateNotification(notification_id, catalog_name, title, message, icon);

  notification->set_priority(message_center::SYSTEM_PRIORITY);
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void CreateAndShowNotification(
    const std::string& notification_id,
    const NotificationCatalogName& catalog_name,
    const std::u16string& title,
    const std::u16string& message,
    const VectorIcon& icon,
    scoped_refptr<NotificationDelegate> click_handler = nullptr,
    const RichNotificationData& rich_data = RichNotificationData()) {
  std::unique_ptr<Notification> notification =
      CreateNotification(notification_id, catalog_name, title, message, icon,
                         click_handler, rich_data);
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void NotifyAccessibilityFeatureDisabledByAdmin(
    int feature_name_id,
    bool feature_state,
    const std::string& notification_id) {
  const std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_TITLE);
  const std::u16string organization_manager =
      base::UTF8ToUTF16(Shell::Get()
                            ->system_tray_model()
                            ->enterprise_domain()
                            ->enterprise_domain_manager());
  const std::u16string activation_string = l10n_util::GetStringUTF16(
      feature_state ? IDS_ASH_ACCESSIBILITY_FEATURE_ACTIVATED
                    : IDS_ASH_ACCESSIBILITY_FEATURE_DEACTIVATED);

  const std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_MSG, organization_manager,
      activation_string, l10n_util::GetStringUTF16(feature_name_id));

  CreateAndShowStickyNotification(
      notification_id, NotificationCatalogName::kAccessibilityFeatureDisabled,
      title, message, chromeos::kEnterpriseIcon);
}

// Shows a notification with the given title and message and the accessibility
// icon, without any click handler.
void ShowAccessibilityNotification(
    int title_id,
    int message_id,
    const std::u16string& accelerator,
    const std::string& notification_id,
    const NotificationCatalogName& catalog_name) {
  // Show a notification that times out.
  CreateAndShowNotification(notification_id, catalog_name,
                            l10n_util::GetStringUTF16(title_id),
                            l10n_util::GetStringFUTF16(message_id, accelerator),
                            kNotificationAccessibilityIcon);
}

void RemoveNotification(const std::string& notification_id) {
  MessageCenter::Get()->RemoveNotification(notification_id,
                                           /*by_user=*/false);
}

}  // namespace

// Shortcut help URL.
const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

// Accessibility notification ids.
const char kDockedMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/dockedmagnifier";
const char kFullscreenMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/fullscreenmagnifier";
const char kHighContrastToggleAccelNotificationId[] =
    "chrome://settings/accessibility/highcontrast";

// A nudge/tutorial will not be shown if it already been shown 3 times, or if 24
// hours have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

// We only display notifications for active user sessions (signed-in/guest with
// desktop ready). Also do not show notifications in signin or lock screen.
bool IsActiveUserSession() {
  const auto* session_controller = Shell::Get()->session_controller();
  return !session_controller->IsUserSessionBlocked();
}

void MaybeShowDeprecatedAcceleratorNotification(const char* notification_id,
                                                int message_id,
                                                int new_shortcut_id,
                                                ui::Accelerator replacement,
                                                AcceleratorAction action_id,
                                                const char* pref_name) {
  const std::vector<AcceleratorDetails> available_accelerators =
      Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
          action_id);

  if (!base::Contains(available_accelerators, replacement,
                      &AcceleratorDetails::accelerator)) {
    // No current accelerators for the action or the replacement accelerator
    // is not available.
    return;
  }

  if (!IsActiveUserSession()) {
    return;
  }

  CHECK(ash::Shell::HasInstance() && Shell::Get()->session_controller());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(prefs);

  const int shown_count =
      prefs->GetDict(prefs::kDeprecatedAcceleratorNotificationsShownCounts)
          .FindInt(pref_name)
          .value_or(0);
  std::optional<base::Time> last_shown_time = base::ValueToTime(
      prefs->GetDict(prefs::kDeprecatedAcceleratorNotificationsLastShown)
          .Find(pref_name));

  // Do not show the nudge more than three times, or if it has already been
  // shown in the past 24 hours.
  const base::Time now = base::Time::Now();
  if ((shown_count >= kNudgeMaxShownCount) ||
      (last_shown_time.has_value() &&
       (now - last_shown_time.value()) < kNudgeTimeBetweenShown)) {
    return;
  }

  ScopedDictPrefUpdate count_update(
      prefs, prefs::kDeprecatedAcceleratorNotificationsShownCounts);
  ScopedDictPrefUpdate time_update(
      prefs, prefs::kDeprecatedAcceleratorNotificationsLastShown);
  count_update->Set(pref_name, shown_count + 1);
  time_update->Set(pref_name, base::TimeToValue(now));

  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE);
  const std::u16string message =
      GetNotificationText(message_id, new_shortcut_id);
  auto on_click_handler = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating([]() {
        if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
          Shell::Get()->shell_delegate()->OpenKeyboardShortcutHelpPage();
      }));

  CreateAndShowNotification(
      notification_id, NotificationCatalogName::kDeprecatedAccelerator, title,
      message, kNotificationKeyboardIcon, on_click_handler);
}

void ShowDockedMagnifierNotification() {
  std::vector<AcceleratorLookup::AcceleratorDetails> details =
      Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
          AcceleratorAction::kToggleDockedMagnifier);
  // This dialog is only shown when docked magnification was enabled from the
  // accelerator.
  CHECK(!details.empty());
  std::u16string accelerator =
      AcceleratorLookup::GetAcceleratorDetailsText(details[0]);
  ShowAccessibilityNotification(
      IDS_DOCKED_MAGNIFIER_ACCEL_TITLE, IDS_DOCKED_MAGNIFIER_ACCEL_MSG,
      accelerator, kDockedMagnifierToggleAccelNotificationId,
      NotificationCatalogName::kDockedMagnifierEnabled);
}

void ShowDockedMagnifierDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_DOCKED_MAGNIFIER_SHORTCUT_DISABLED, feature_state,
      kDockedMagnifierToggleAccelNotificationId);
}

void RemoveDockedMagnifierNotification() {
  RemoveNotification(kDockedMagnifierToggleAccelNotificationId);
}

void ShowFullscreenMagnifierNotification() {
  std::vector<AcceleratorLookup::AcceleratorDetails> details =
      Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
          AcceleratorAction::kToggleFullscreenMagnifier);
  // This dialog is only shown when fullscreen magnification was enabled from
  // the accelerator.
  CHECK(!details.empty());
  std::u16string accelerator =
      AcceleratorLookup::GetAcceleratorDetailsText(details[0]);
  ShowAccessibilityNotification(
      IDS_FULLSCREEN_MAGNIFIER_ACCEL_TITLE, IDS_FULLSCREEN_MAGNIFIER_ACCEL_MSG,
      accelerator, kFullscreenMagnifierToggleAccelNotificationId,
      NotificationCatalogName::kFullScreenMagnifierEnabled);
}

void ShowFullscreenMagnifierDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_FULLSCREEN_MAGNIFIER_SHORTCUT_DISABLED, feature_state,
      kFullscreenMagnifierToggleAccelNotificationId);
}

void RemoveFullscreenMagnifierNotification() {
  RemoveNotification(kFullscreenMagnifierToggleAccelNotificationId);
}

void ShowHighContrastNotification() {
  std::vector<AcceleratorLookup::AcceleratorDetails> details =
      Shell::Get()->accelerator_lookup()->GetAvailableAcceleratorsForAction(
          AcceleratorAction::kToggleHighContrast);
  // This dialog is only shown when high conrast was enabled from the
  // accelerator.
  CHECK(!details.empty());
  std::u16string accelerator =
      AcceleratorLookup::GetAcceleratorDetailsText(details[0]);
  ShowAccessibilityNotification(IDS_HIGH_CONTRAST_ACCEL_TITLE,
                                IDS_HIGH_CONTRAST_ACCEL_MSG, accelerator,
                                kHighContrastToggleAccelNotificationId,
                                NotificationCatalogName::kHighContrastEnabled);
}

void ShowHighContrastDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_HIGH_CONTRAST_SHORTCUT_DISABLED, feature_state,
      kHighContrastToggleAccelNotificationId);
}

void RemoveHighContrastNotification() {
  RemoveNotification(kHighContrastToggleAccelNotificationId);
}

void ShowSpokenFeedbackDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_SPOKEN_FEEDBACK_SHORTCUT_DISABLED, feature_state,
      kSpokenFeedbackToggleAccelNotificationId);
}

void RemoveSpokenFeedbackNotification() {
  RemoveNotification(kSpokenFeedbackToggleAccelNotificationId);
}

}  // namespace ash
