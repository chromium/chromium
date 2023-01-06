// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/update/update_notification_controller.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/cpp/update_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/session/shutdown_confirmation_dialog.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

const char kNotifierId[] = "ash.update";

const char kNotificationId[] = "chrome://update";

bool CheckForSlowBoot(const base::FilePath& slow_boot_file_path) {
  if (base::PathExists(slow_boot_file_path)) {
    return true;
  }
  return false;
}

std::u16string GetDomainManager(
    RelaunchNotificationState::PolicySource policy_source) {
  EnterpriseDomainModel* enterprise_domain =
      Shell::Get()->system_tray_model()->enterprise_domain();
  std::string domain_manager;
  switch (policy_source) {
    case RelaunchNotificationState::kDevice:
      domain_manager = enterprise_domain->enterprise_domain_manager();
      break;
    case RelaunchNotificationState::kUser:
      domain_manager = enterprise_domain->account_domain_manager();
      break;
  }
  return base::UTF8ToUTF16(domain_manager);
}

}  // namespace

UpdateNotificationController::UpdateNotificationController()
    : model_(Shell::Get()->system_tray_model()->update_model()),
      slow_boot_file_path_("/mnt/stateful_partition/etc/slow_boot_required") {
  model_->AddObserver(this);
  OnUpdateAvailable();
}

UpdateNotificationController::~UpdateNotificationController() {
  model_->RemoveObserver(this);
}

void UpdateNotificationController::GenerateUpdateNotification(
    absl::optional<bool> slow_boot_file_path_exists) {
  if (!ShouldShowUpdate()) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
    return;
  }

  if (slow_boot_file_path_exists != absl::nullopt) {
    slow_boot_file_path_exists_ = slow_boot_file_path_exists.value();
  }

  std::unique_ptr<Notification> notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, GetTitle(),
      GetMessage(), std::u16string() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId, NotificationCatalogName::kUpdate),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &UpdateNotificationController::HandleNotificationClick,
              weak_ptr_factory_.GetWeakPtr())),
      GetIcon(), GetWarningLevel());
  notification->set_pinned(true);

  if (model_->relaunch_notification_state().requirement_type ==
      RelaunchNotificationState::kRequired)
    notification->SetSystemPriority();

  if (ShouldShowDeferredUpdate()) {
    notification->set_buttons({
        message_center::ButtonInfo(l10n_util::GetStringUTF16(
            IDS_UPDATE_NOTIFICATION_APPLY_UPDATE_BUTTON)),
        message_center::ButtonInfo(l10n_util::GetStringUTF16(
            IDS_UPDATE_NOTIFICATION_AUTOMATIC_UPDATE_BUTTON)),
    });
  } else if (model_->update_required()) {
    std::vector<message_center::ButtonInfo> notification_actions;
    if (model_->rollback()) {
      notification_actions.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_RESTART_BUTTON)));
    } else if (model_->factory_reset_required()) {
      notification_actions.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_RESET_TO_UPDATE)));
    } else {
      notification_actions.push_back(message_center::ButtonInfo(
          l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_RESTART_BUTTON)));
    }
    notification->set_buttons(notification_actions);
  }

  MessageCenter::Get()->AddNotification(std::move(notification));
}

void UpdateNotificationController::OnUpdateAvailable() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CheckForSlowBoot, slow_boot_file_path_),
      base::BindOnce(&UpdateNotificationController::GenerateUpdateNotification,
                     weak_ptr_factory_.GetWeakPtr()));
}

bool UpdateNotificationController::ShouldShowUpdate() const {
  return model_->update_required() ||
         model_->update_over_cellular_available() || ShouldShowDeferredUpdate();
}

bool UpdateNotificationController::ShouldShowDeferredUpdate() const {
  return model_->update_deferred() == DeferredUpdateState::kShowNotification;
}

std::u16string UpdateNotificationController::GetTitle() const {
  switch (model_->relaunch_notification_state().requirement_type) {
    case RelaunchNotificationState::kRecommendedAndOverdue:
      return model_->rollback() ? l10n_util::GetStringUTF16(
                                      IDS_ROLLBACK_OVERDUE_NOTIFICATION_TITLE)
                                : l10n_util::GetStringUTF16(
                                      IDS_RELAUNCH_RECOMMENDED_OVERDUE_TITLE);

    case RelaunchNotificationState::kRecommendedNotOverdue:
      return model_->rollback()
                 ? l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_TITLE)
                 : l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE);

    case RelaunchNotificationState::kRequired: {
      const base::TimeDelta& rounded_offset =
          model_->relaunch_notification_state()
              .rounded_time_until_reboot_required;
      if (rounded_offset.InDays() >= 2) {
        return l10n_util::GetPluralStringFUTF16(
            model_->rollback() ? IDS_ROLLBACK_REQUIRED_TITLE_DAYS
                               : IDS_RELAUNCH_REQUIRED_TITLE_DAYS,
            rounded_offset.InDays());
      }
      if (rounded_offset.InHours() >= 1) {
        return l10n_util::GetPluralStringFUTF16(
            model_->rollback() ? IDS_ROLLBACK_REQUIRED_TITLE_HOURS
                               : IDS_RELAUNCH_REQUIRED_TITLE_HOURS,
            rounded_offset.InHours());
      }
      if (rounded_offset.InMinutes() >= 1) {
        return l10n_util::GetPluralStringFUTF16(
            model_->rollback() ? IDS_ROLLBACK_REQUIRED_TITLE_MINUTES
                               : IDS_RELAUNCH_REQUIRED_TITLE_MINUTES,
            rounded_offset.InMinutes());
      }
      return l10n_util::GetPluralStringFUTF16(
          model_->rollback() ? IDS_ROLLBACK_REQUIRED_TITLE_SECONDS
                             : IDS_RELAUNCH_REQUIRED_TITLE_SECONDS,
          rounded_offset.InSeconds());
    }
    case RelaunchNotificationState::kNone:
      return model_->rollback()
                 ? l10n_util::GetStringUTF16(IDS_ROLLBACK_NOTIFICATION_TITLE)
                 : l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_TITLE);
  }
}

std::u16string UpdateNotificationController::GetMessage() const {
  if (ShouldShowDeferredUpdate()) {
    return l10n_util::GetStringUTF16(
        IDS_UPDATE_NOTIFICATION_MESSAGE_DEFERRED_UPDATE);
  }

  const std::u16string system_app_name =
      l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_SYSTEM_APP_NAME);
  if (model_->factory_reset_required() && !model_->rollback()) {
    return l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_POWERWASH,
                                      ui::GetChromeOSDeviceName(),
                                      system_app_name);
  }

  absl::optional<int> body_message_id = absl::nullopt;
  switch (model_->relaunch_notification_state().requirement_type) {
    case RelaunchNotificationState::kRecommendedNotOverdue:
      body_message_id = model_->rollback()
                            ? IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK
                            : IDS_RELAUNCH_RECOMMENDED_BODY;
      break;
    case RelaunchNotificationState::kRecommendedAndOverdue:
      body_message_id = model_->rollback()
                            ? IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK_OVERDUE
                            : IDS_RELAUNCH_RECOMMENDED_OVERDUE_BODY;
      break;
    case RelaunchNotificationState::kRequired:
      body_message_id = model_->rollback() ? IDS_ROLLBACK_REQUIRED_BODY
                                           : IDS_RELAUNCH_REQUIRED_BODY;
      break;
    case RelaunchNotificationState::kNone:
      if (model_->rollback()) {
        body_message_id = IDS_UPDATE_NOTIFICATION_MESSAGE_ROLLBACK;
      }
      break;
  }

  std::u16string update_text;
  std::u16string domain_manager =
      GetDomainManager(model_->relaunch_notification_state().policy_source);
  if (body_message_id.has_value() && !domain_manager.empty()) {
    update_text = l10n_util::GetStringFUTF16(*body_message_id, domain_manager,
                                             ui::GetChromeOSDeviceName());
  } else {
    update_text = l10n_util::GetStringFUTF16(
        IDS_UPDATE_NOTIFICATION_MESSAGE_LEARN_MORE, system_app_name);
  }

  if (slow_boot_file_path_exists_) {
    return l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE_SLOW_BOOT,
                                      update_text);
  }
  return update_text;
}

const gfx::VectorIcon& UpdateNotificationController::GetIcon() const {
  if (model_->rollback())
    return kSystemMenuRollbackIcon;
  if (model_->relaunch_notification_state().requirement_type ==
      RelaunchNotificationState::kNone)
    return kSystemMenuUpdateIcon;
  return vector_icons::kBusinessIcon;
}

message_center::SystemNotificationWarningLevel
UpdateNotificationController::GetWarningLevel() const {
  if (model_->rollback() &&
      (model_->relaunch_notification_state().requirement_type ==
           RelaunchNotificationState::kRequired ||
       model_->relaunch_notification_state().requirement_type ==
           RelaunchNotificationState::kRecommendedAndOverdue)) {
    return message_center::SystemNotificationWarningLevel::CRITICAL_WARNING;
  }
  if (model_->rollback() ||
      model_->relaunch_notification_state().requirement_type ==
          RelaunchNotificationState::kRequired) {
    return message_center::SystemNotificationWarningLevel::WARNING;
  }
  return message_center::SystemNotificationWarningLevel::NORMAL;
}

void UpdateNotificationController::RestartForUpdate() {
  confirmation_dialog_ = nullptr;
  // System updates require restarting the device.
  Shell::Get()->session_controller()->RequestRestartForUpdate();
}

void UpdateNotificationController::RestartCancelled() {
  confirmation_dialog_ = nullptr;
  // Put the notification back.
  GenerateUpdateNotification(absl::nullopt);
}

void UpdateNotificationController::HandleNotificationClick(
    absl::optional<int> button_index) {
  DCHECK(ShouldShowUpdate());

  if (!button_index) {
    // Notification message body clicked, which says "learn more".
    Shell::Get()->system_tray_model()->client()->ShowAboutChromeOS();
    return;
  }

  if (button_index.value() == 0) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);

    if (ShouldShowDeferredUpdate()) {
      // When the "update" button is clicked, apply the deferred update.
      ash::UpdateEngineClient::Get()->ApplyDeferredUpdate(
          /*shutdown_after_update=*/false, base::DoNothing());
    } else if (model_->update_required()) {
      // Restart
      if (slow_boot_file_path_exists_) {
        // An active dialog exists already.
        if (confirmation_dialog_)
          return;

        confirmation_dialog_ = new ShutdownConfirmationDialog(
            IDS_DIALOG_TITLE_SLOW_BOOT, IDS_DIALOG_MESSAGE_SLOW_BOOT,
            base::BindOnce(&UpdateNotificationController::RestartForUpdate,
                           weak_ptr_factory_.GetWeakPtr()),
            base::BindOnce(&UpdateNotificationController::RestartCancelled,
                           weak_ptr_factory_.GetWeakPtr()));
      } else {
        RestartForUpdate();
      }
    } else {
      // Shows the about chrome OS page and checks for update after the page is
      // loaded.
      Shell::Get()->system_tray_model()->client()->ShowAboutChromeOS();
    }
  } else {
    // When the "automatic update" button is clicked, take user to the ChromeOS
    // additional details page that has the automatic update toggle.
    Shell::Get()->system_tray_model()->client()->ShowAboutChromeOSDetails();
  }
}

}  // namespace ash
