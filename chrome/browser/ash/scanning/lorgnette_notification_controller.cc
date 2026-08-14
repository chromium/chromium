// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_notification_controller.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/device_event_log/device_event_log.h"
#include "components/user_manager/user.h"
#include "third_party/cros_system_api/constants/lorgnette_dlc.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
const char kNotifierId[] = "scanning.dlc";
const char kNotificationId[] = "scanning_dlc_notification";

std::unique_ptr<message_center::Notification> NewNotification(
    const std::string& notification_id,
    int title,
    int message,
    ui::ColorId color_id,
    const gfx::VectorIcon& image) {
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.accent_color_id = color_id;
  rich_notification_data.vector_small_image = &image;
  return std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      l10n_util::GetStringUTF16(title), l10n_util::GetStringUTF16(message),
      ui::ImageModel(),  // icon
      l10n_util::GetStringUTF16(IDS_SCANNING_DLC_NOTIFICATION_DISPLAY_SOURCE),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierId,
                                 NotificationCatalogName::kDocumentScanning),
      rich_notification_data, new message_center::NotificationDelegate());
}
}  // namespace

LorgnetteNotificationController::LorgnetteNotificationController(
    const user_manager::User& user)
    : dlc_observer_(this),
      supported_dlc_ids_(std::set<std::string>(
          {lorgnette::kSaneBackendsPfuDlcId, lorgnette::kSaneBackendsCanonDlcId,
           lorgnette::kSaneBackendsBrother5DlcId})) {
  notification_id_ =
      CreateUserScopedNotificationId(kNotificationId, user.username_hash());
  profile_id_ = user.GetAccountId().GetUserEmail();
  dlc_observer_.Observe(DlcserviceClient::Get());

  for (const auto& id : supported_dlc_ids_) {
    current_state_per_dlc_[id] = DlcState::kIdle;
  }
}

LorgnetteNotificationController::~LorgnetteNotificationController() = default;

void LorgnetteNotificationController::OnDlcStateChanged(
    const dlcservice::DlcState& dlc_state) {
  if (supported_dlc_ids_.find(dlc_state.id()) == supported_dlc_ids_.end()) {
    return;
  }

  switch (dlc_state.state()) {
    case dlcservice::DlcState::INSTALLED:
      // Only set state to kInstalledSuccessfully if previous start was
      // kInstalling to send a notification only if the DLC is downloading and
      // not just mounting.
      if (current_state_per_dlc_[dlc_state.id()] == DlcState::kInstalling) {
        current_state_per_dlc_[dlc_state.id()] =
            DlcState::kInstalledSuccessfully;
      } else {
        current_state_per_dlc_[dlc_state.id()] = DlcState::kIdle;
      }
      PRINTER_LOG(EVENT) << "Scanning DLC ID: " << dlc_state.id()
                         << " installed successfully";
      break;
    case dlcservice::DlcState::INSTALLING:
      current_state_per_dlc_[dlc_state.id()] = DlcState::kInstalling;
      break;
    case dlcservice::DlcState::NOT_INSTALLED:
      current_state_per_dlc_[dlc_state.id()] = DlcState::kInstallError;
      PRINTER_LOG(ERROR) << "Scanning DLC ID: " << dlc_state.id()
                         << " exited with error: "
                         << dlc_state.last_error_code();
      break;
    default:
      NOTREACHED();
  }
  std::unique_ptr<message_center::Notification> notification =
      CreateNotification(dlc_state.id());
  DisplayNotification(std::move(notification), dlc_state.id());
}

std::unique_ptr<message_center::Notification>
LorgnetteNotificationController::CreateNotification(const std::string& dlc_id) {
  switch (current_state_per_dlc_[dlc_id]) {
    case DlcState::kInstalledSuccessfully:
      return NewNotification(notification_id_,
                             IDS_SCANNING_DLC_NOTIFICATION_INSTALLED_TITLE,
                             IDS_EMPTY_STRING, cros_tokens::kCrosSysPrimary,
                             ash::kNotificationPrintingIcon);
    case DlcState::kInstalling:
      return NewNotification(notification_id_,
                             IDS_SCANNING_DLC_NOTIFICATION_INSTALLING_TITLE,
                             IDS_EMPTY_STRING, cros_tokens::kCrosSysPrimary,
                             ash::kNotificationPrintingIcon);
    case DlcState::kInstallError:
      return NewNotification(
          notification_id_, IDS_SCANNING_DLC_NOTIFICATION_INSTALL_FAILED_TITLE,
          IDS_SCANNING_DLC_NOTIFICATION_INSTALL_FAILED_MESSAGE,
          cros_tokens::kCrosSysError, ash::kNotificationPrintingWarningIcon);
    case DlcState::kIdle:
      return nullptr;
  }
  NOTREACHED();
}

void LorgnetteNotificationController::DisplayNotification(
    std::unique_ptr<message_center::Notification> notification,
    const std::string& dlc_id) {
  if (current_state_per_dlc_[dlc_id] == DlcState::kIdle) {
    message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                             /*by_user=*/false);
  } else {
    notification->set_profile_id(profile_id_);
    message_center::MessageCenter::Get()->AddNotification(
        std::move(notification));
  }
}

std::optional<LorgnetteNotificationController::DlcState>
LorgnetteNotificationController::current_state_for_testing(
    const std::string& dlc_id) {
  auto itr = current_state_per_dlc_.find(dlc_id);
  if (itr == current_state_per_dlc_.end()) {
    return std::nullopt;
  }

  return itr->second;
}

}  // namespace ash
