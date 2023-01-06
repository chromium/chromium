// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/gesture_education/gesture_education_notification_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

// static
const char GestureEducationNotificationController::kNotificationId[] =
    "chrome://gesture_education";

GestureEducationNotificationController::
    GestureEducationNotificationController() {
  Shell::Get()->session_controller()->AddObserver(this);
  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
}

GestureEducationNotificationController::
    ~GestureEducationNotificationController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  tablet_mode_observation_.Reset();
}

void GestureEducationNotificationController::MaybeShowNotification() {
  bool is_user_session_blocked =
      Shell::Get()->session_controller()->IsUserSessionBlocked();
  if (features::IsHideShelfControlsInTabletModeEnabled() &&
      TabletModeController::Get()->InTabletMode() && !is_user_session_blocked &&
      (active_user_prefs_ && !active_user_prefs_->GetBoolean(
                                 prefs::kGestureEducationNotificationShown)) &&
      !ShelfConfig::Get()->ShelfControlsForcedShownForAccessibility()) {
    GenerateGestureEducationNotification();
    active_user_prefs_->SetBoolean(prefs::kGestureEducationNotificationShown,
                                   true);
  }
}

void GestureEducationNotificationController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  active_user_prefs_ = prefs;
  MaybeShowNotification();
}

void GestureEducationNotificationController::OnSessionStateChanged(
    session_manager::SessionState state) {
  MaybeShowNotification();
}

void GestureEducationNotificationController::OnTabletModeStarted() {
  MaybeShowNotification();
}

void GestureEducationNotificationController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void GestureEducationNotificationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  // Some tests fail when the notification shows up. Use |for_test|
  // as the default value so that it is true in test to suppress the
  // notification there.
  registry->RegisterBooleanPref(prefs::kGestureEducationNotificationShown,
                                for_test);
}

void GestureEducationNotificationController::
    GenerateGestureEducationNotification() {
  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          GetNotificationTitle(), GetNotificationMessage(),
          std::u16string() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, kNotificationId,
              NotificationCatalogName::kGestureEducation),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&GestureEducationNotificationController::
                                      HandleNotificationClick,
                                  weak_ptr_factory_.GetWeakPtr())),
          vector_icons::kSettingsIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void GestureEducationNotificationController::HandleNotificationClick() {
  Shell::Get()->system_tray_model()->client()->ShowGestureEducationHelp();
}

void GestureEducationNotificationController::ResetPrefForTest() {
  if (active_user_prefs_) {
    active_user_prefs_->SetBoolean(prefs::kGestureEducationNotificationShown,
                                   false);
  }
}

std::u16string GestureEducationNotificationController::GetNotificationMessage()
    const {
  return l10n_util::GetStringUTF16(IDS_GESTURE_NOTIFICATION_MESSAGE_LEARN_MORE);
}

std::u16string GestureEducationNotificationController::GetNotificationTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_GESTURE_NOTIFICATION_TITLE);
}

}  // namespace ash
