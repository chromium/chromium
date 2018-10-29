// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const char kNotifierMultiDevice[] = "ash.multi_device_setup";

}  // namespace

// static
const char MultiDeviceNotificationPresenter::kNotificationId[] =
    "cros_multi_device_setup_notification_id";

// static
std::string
MultiDeviceNotificationPresenter::GetNotificationDescriptionForLogging(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return "notification to prompt setup";
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return "notification of switch to new host";
    case Status::kExistingUserNewChromebookNotificationVisible:
      return "notification of new Chromebook added";
    case Status::kNoNotificationVisible:
      return "no notification";
  }
  NOTREACHED();
}

MultiDeviceNotificationPresenter::OpenUiDelegate::~OpenUiDelegate() = default;

void MultiDeviceNotificationPresenter::OpenUiDelegate::
    OpenMultiDeviceSetupUi() {
  Shell::Get()->system_tray_model()->client_ptr()->ShowMultiDeviceSetup();
}

void MultiDeviceNotificationPresenter::OpenUiDelegate::
    OpenConnectedDevicesSettings() {
  Shell::Get()
      ->system_tray_model()
      ->client_ptr()
      ->ShowConnectedDevicesSettings();
}

// static
MultiDeviceNotificationPresenter::NotificationType
MultiDeviceNotificationPresenter::GetMetricValueForNotification(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return kNotificationTypeNewUserPotentialHostExists;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return kNotificationTypeExistingUserHostSwitched;
    case Status::kExistingUserNewChromebookNotificationVisible:
      return kNotificationTypeExistingUserNewChromebookAdded;
    case Status::kNoNotificationVisible:
      NOTREACHED();
      return kNotificationTypeMax;
  }
}

MultiDeviceNotificationPresenter::MultiDeviceNotificationPresenter(
    message_center::MessageCenter* message_center,
    service_manager::Connector* connector)
    : message_center_(message_center),
      connector_(connector),
      binding_(this),
      open_ui_delegate_(std::make_unique<OpenUiDelegate>()),
      weak_ptr_factory_(this) {
  DCHECK(message_center_);
  DCHECK(connector_);

  Shell::Get()->session_controller()->AddObserver(this);

  // If the constructor is called after the session state has already been set
  // (e.g., if recovering from a crash), handle that now. If the user has not
  // yet logged in, this will be a no-op.
  ObserveMultiDeviceSetupIfPossible();
}

MultiDeviceNotificationPresenter::~MultiDeviceNotificationPresenter() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void MultiDeviceNotificationPresenter::OnPotentialHostExistsForNewUser() {
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE);
  ShowNotification(Status::kNewUserNotificationVisible, title, message);
}

void MultiDeviceNotificationPresenter::OnNoLongerNewUser() {
  if (notification_status_ != Status::kNewUserNotificationVisible)
    return;
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::OnConnectedHostSwitchedForExistingUser(
    const std::string& new_host_device_name) {
  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE);
  ShowNotification(Status::kExistingUserHostSwitchedNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::OnNewChromebookAddedForExistingUser(
    const std::string& new_host_device_name) {
  base::string16 title = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_TITLE,
      base::ASCIIToUTF16(new_host_device_name));
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_MESSAGE);
  ShowNotification(Status::kExistingUserNewChromebookNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::RemoveMultiDeviceSetupNotification() {
  notification_status_ = Status::kNoNotificationVisible;
  message_center_->RemoveNotification(kNotificationId,
                                      /* by_user */ false);
}

void MultiDeviceNotificationPresenter::OnUserSessionAdded(
    const AccountId& account_id) {
  ObserveMultiDeviceSetupIfPossible();
}

void MultiDeviceNotificationPresenter::OnSessionStateChanged(
    session_manager::SessionState state) {
  ObserveMultiDeviceSetupIfPossible();
}

void MultiDeviceNotificationPresenter::ObserveMultiDeviceSetupIfPossible() {
  // If already the delegate, there is nothing else to do.
  if (multidevice_setup_ptr_)
    return;

  const SessionController* session_controller =
      Shell::Get()->session_controller();

  // If no active user is logged in, there is nothing to do.
  if (session_controller->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  const mojom::UserSession* user_session =
      session_controller->GetPrimaryUserSession();

  // The primary user session may be unavailable (e.g., for test/guest users).
  if (!user_session)
    return;

  std::string service_user_id = user_session->user_info->service_user_id;

  // Cannot proceed if there is no service user ID.
  if (service_user_id.empty())
    return;

  connector_->BindInterface(
      service_manager::Identity(
          chromeos::multidevice_setup::mojom::kServiceName, service_user_id),
      &multidevice_setup_ptr_);

  // Add this object as the delegate of the MultiDeviceSetup Service.
  chromeos::multidevice_setup::mojom::AccountStatusChangeDelegatePtr
      delegate_ptr;
  binding_.Bind(mojo::MakeRequest(&delegate_ptr));
  multidevice_setup_ptr_->SetAccountStatusChangeDelegate(
      std::move(delegate_ptr));
}

void MultiDeviceNotificationPresenter::OnNotificationClicked() {
  DCHECK(notification_status_ != Status::kNoNotificationVisible);
  PA_LOG(INFO) << "User clicked "
               << GetNotificationDescriptionForLogging(notification_status_)
               << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationClicked",
                            GetMetricValueForNotification(notification_status_),
                            kNotificationTypeMax);
  switch (notification_status_) {
    case Status::kNewUserNotificationVisible:
      open_ui_delegate_->OpenMultiDeviceSetupUi();
      break;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      // Clicks on the 'host switched' and 'Chromebook added' notifications have
      // the same effect, i.e. opening the Settings subpage.
      FALLTHROUGH;
    case Status::kExistingUserNewChromebookNotificationVisible:
      open_ui_delegate_->OpenConnectedDevicesSettings();
      break;
    case Status::kNoNotificationVisible:
      NOTREACHED();
  }
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::ShowNotification(
    const Status notification_status,
    const base::string16& title,
    const base::string16& message) {
  PA_LOG(INFO) << "Showing "
               << GetNotificationDescriptionForLogging(notification_status)
               << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationShown",
                            GetMetricValueForNotification(notification_status),
                            kNotificationTypeMax);
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(kNotificationId,
                                        CreateNotification(title, message));
  } else {
    message_center_->AddNotification(CreateNotification(title, message));
  }
  notification_status_ = notification_status;
}

std::unique_ptr<message_center::Notification>
MultiDeviceNotificationPresenter::CreateNotification(
    const base::string16& title,
    const base::string16& message) {
  return message_center::Notification::CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId, title, message, base::string16() /* display_source */,
      GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          kNotifierMultiDevice),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &MultiDeviceNotificationPresenter::OnNotificationClicked,
          weak_ptr_factory_.GetWeakPtr())),
      ash::kNotificationMultiDeviceSetupIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

void MultiDeviceNotificationPresenter::FlushForTesting() {
  if (multidevice_setup_ptr_)
    multidevice_setup_ptr_.FlushForTesting();
}

}  // namespace ash
