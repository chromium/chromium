// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter_impl.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/cross_device/logging/logging.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/message_center/message_center.h"

namespace {

const char kDiscoveryLearnMoreLink[] =
    "https://support.google.com/chromebook?p=fast_pair_m101";
const char kAssociateAccountLearnMoreLink[] =
    "https://support.google.com/chromebook?p=bluetooth_pairing_m101";

bool ShouldShowUserEmail(ash::LoginStatus status) {
  switch (status) {
    case ash::LoginStatus::NOT_LOGGED_IN:
    case ash::LoginStatus::LOCKED:
    case ash::LoginStatus::KIOSK_APP:
    case ash::LoginStatus::GUEST:
    case ash::LoginStatus::PUBLIC:
      return false;
    case ash::LoginStatus::USER:
    case ash::LoginStatus::CHILD:
    default:
      return true;
  }
}

}  // namespace

namespace ash {
namespace quick_pair {

// static
FastPairPresenterImpl::Factory*
    FastPairPresenterImpl::Factory::g_test_factory_ = nullptr;

// static
std::unique_ptr<FastPairPresenter> FastPairPresenterImpl::Factory::Create(
    message_center::MessageCenter* message_center) {
  if (g_test_factory_) {
    return g_test_factory_->CreateInstance(message_center);
  }

  return base::WrapUnique(new FastPairPresenterImpl(message_center));
}

// static
void FastPairPresenterImpl::Factory::SetFactoryForTesting(
    Factory* g_test_factory) {
  g_test_factory_ = g_test_factory;
}

FastPairPresenterImpl::Factory::~Factory() = default;

FastPairPresenterImpl::FastPairPresenterImpl(
    message_center::MessageCenter* message_center)
    : notification_controller_(
          std::make_unique<FastPairNotificationController>(message_center)) {}

FastPairPresenterImpl::~FastPairPresenterImpl() = default;

void FastPairPresenterImpl::ShowDiscovery(scoped_refptr<Device> device,
                                          DiscoveryCallback callback) {
  DCHECK(device);
  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id, base::BindRepeating(
                       &FastPairPresenterImpl::OnDiscoveryMetadataRetrieved,
                       weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnDiscoveryMetadataRetrieved(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  device->set_version(device_metadata->InferFastPairVersion());

  if (device->protocol() == Protocol::kFastPairSubsequent) {
    ShowSubsequentDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  if (device->version().value() == DeviceFastPairVersion::kV1) {
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion1);
  } else {
    RecordFastPairDiscoveredVersion(FastPairVersion::kVersion2);
  }

  // If we are in guest-mode, or are missing the IdentifyManager needed to show
  // detailed user notification, show the guest notification. We don't have to
  // verify opt-in status in this case because Guests will be guaranteed to not
  // have opt-in status.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager ||
      !ShouldShowUserEmail(
          Shell::Get()->session_controller()->login_status())) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": in guest mode, showing guest notification";
    ShowGuestDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  // Check if the user is opted in to saving devices to their account. If the
  // user is not opted in, we will show the guest notification which does not
  // mention saving devices to the user account. This is flagged depending if
  // the Fast Pair Saved Devices is enabled and we are using a strict
  // interpretation of the opt-in status.
  if (features::IsFastPairSavedDevicesEnabled() &&
      features::IsFastPairSavedDevicesStrictOptInEnabled()) {
    FastPairRepository::Get()->CheckOptInStatus(base::BindOnce(
        &FastPairPresenterImpl::OnCheckOptInStatus,
        weak_pointer_factory_.GetWeakPtr(), device, callback, device_metadata));
    return;
  }

  // If we don't have SavedDevices flag enabled, then we can ignore the user's
  // opt in status and move forward to showing the User Discovery notification.
  ShowUserDiscoveryNotification(device, callback, device_metadata);
}

void FastPairPresenterImpl::OnCheckOptInStatus(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata,
    nearby::fastpair::OptInStatus status) {
  CD_LOG(INFO, Feature::FP) << __func__;

  if (status != nearby::fastpair::OptInStatus::STATUS_OPTED_IN) {
    ShowGuestDiscoveryNotification(device, callback, device_metadata);
    return;
  }

  ShowUserDiscoveryNotification(device, callback, device_metadata);
}

void FastPairPresenterImpl::ShowSubsequentDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    return;
  }

  // Since Subsequent Pairing scenario can only happen for a signed in user
  // when a device has already been saved to their account, this should never
  // be null. We cannot get to this scenario in Guest Mode.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  DCHECK(identity_manager);

  const std::string& email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  notification_controller_->ShowSubsequentDiscoveryNotification(
      base::UTF8ToUTF16(device->display_name().value()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowGuestDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  notification_controller_->ShowGuestDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowUserDiscoveryNotification(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  // Since we check this in |OnInitialDiscoveryMetadataRetrieved| to determine
  // if we should show the Guest notification, this should never be null.
  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  DCHECK(identity_manager);

  const std::string& email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  notification_controller_->ShowUserDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenterImpl::OnDiscoveryLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnDiscoveryClicked(DiscoveryCallback callback) {
  callback.Run(DiscoveryAction::kPairToDevice);
}

void FastPairPresenterImpl::OnDiscoveryDismissed(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    FastPairNotificationDismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case FastPairNotificationDismissReason::kDismissedByUser:
      callback.Run(DiscoveryAction::kDismissedByUser);
      break;
    case FastPairNotificationDismissReason::kDismissedByOs:
      callback.Run(DiscoveryAction::kDismissedByOs);
      break;
    case FastPairNotificationDismissReason::kDismissedByTimeout:
      callback.Run(DiscoveryAction::kDismissedByTimeout);
      break;
    default:
      NOTREACHED();
  }
}

void FastPairPresenterImpl::OnDiscoveryLearnMoreClicked(
    DiscoveryCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kDiscoveryLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  callback.Run(DiscoveryAction::kLearnMore);
}

void FastPairPresenterImpl::ShowPairing(scoped_refptr<Device> device) {
  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device));
}

void FastPairPresenterImpl::OnPairingMetadataRetrieved(
    scoped_refptr<Device> device,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowPairingNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(), base::DoNothing());
}

void FastPairPresenterImpl::ShowPairingFailed(scoped_refptr<Device> device,
                                              PairingFailedCallback callback) {
  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnPairingFailedMetadataRetrieved(
    scoped_refptr<Device> device,
    PairingFailedCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowErrorNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnNavigateToSettings,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnPairingFailedDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnNavigateToSettings(
    PairingFailedCallback callback) {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
    RecordNavigateToSettingsResult(/*success=*/true);
  } else {
    CD_LOG(WARNING, Feature::FP)
        << "Cannot open Bluetooth Settings since it's not possible "
           "to opening WebUI settings";
    RecordNavigateToSettingsResult(/*success=*/false);
  }

  callback.Run(PairingFailedAction::kNavigateToSettings);
}

void FastPairPresenterImpl::OnPairingFailedDismissed(
    PairingFailedCallback callback,
    FastPairNotificationDismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case FastPairNotificationDismissReason::kDismissedByUser:
      callback.Run(PairingFailedAction::kDismissedByUser);
      break;
    case FastPairNotificationDismissReason::kDismissedByOs:
      callback.Run(PairingFailedAction::kDismissed);
      break;
    case FastPairNotificationDismissReason::kDismissedByTimeout:
      // Fast Pair Error Notifications do not have a timeout, so this is never
      // expected to be hit.
      NOTREACHED();
    default:
      NOTREACHED();
  }
}

void FastPairPresenterImpl::ShowAssociateAccount(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback) {
  RecordRetroactiveSuccessFunnelFlow(
      FastPairRetroactiveSuccessFunnelEvent::kNotificationDisplayed);
  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnAssociateAccountMetadataRetrieved(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": " << device;
  if (!device_metadata) {
    return;
  }

  device->set_version(device_metadata->InferFastPairVersion());

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager) {
    CD_LOG(ERROR, Feature::FP)
        << __func__
        << ": IdentityManager is not available for Associate Account "
           "notification.";
    return;
  }

  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  std::u16string device_name;
  // If the name of the device has been set by the user, use that name,
  // otherwise use the OEM default name.
  if (device->display_name().has_value()) {
    device_name = base::UTF8ToUTF16(device->display_name().value());
  } else {
    device_name = base::ASCIIToUTF16(device_metadata->GetDetails().name());
  }

  notification_controller_->ShowAssociateAccount(
      device_name, base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountActionClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(
          &FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked,
          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnAssociateAccountDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnAssociateAccountActionClicked(
    AssociateAccountCallback callback) {
  callback.Run(AssociateAccountAction::kAssociateAccount);
}

void FastPairPresenterImpl::OnAssociateAccountLearnMoreClicked(
    AssociateAccountCallback callback) {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kAssociateAccountLearnMoreLink),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
  callback.Run(AssociateAccountAction::kLearnMore);
}

void FastPairPresenterImpl::OnAssociateAccountDismissed(
    AssociateAccountCallback callback,
    FastPairNotificationDismissReason dismiss_reason) {
  switch (dismiss_reason) {
    case FastPairNotificationDismissReason::kDismissedByUser:
      callback.Run(AssociateAccountAction::kDismissedByUser);
      break;
    case FastPairNotificationDismissReason::kDismissedByOs:
      callback.Run(AssociateAccountAction::kDismissedByOs);
      break;
    case FastPairNotificationDismissReason::kDismissedByTimeout:
      callback.Run(AssociateAccountAction::kDismissedByTimeout);
      break;
    default:
      NOTREACHED();
  }
}

void FastPairPresenterImpl::ShowInstallCompanionApp(
    scoped_refptr<Device> device,
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  toast_collision_avoidance_timer_.Start(
      FROM_HERE, ash::ToastData::kDefaultToastDuration,
      base::BindOnce(&FastPairPresenterImpl::ShowInstallCompanionAppDelayed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowInstallCompanionAppDelayed(
    scoped_refptr<Device> device,
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairPresenterImpl::OnInstallCompanionAppMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnInstallCompanionAppMetadataRetrieved(
    scoped_refptr<Device> device,
    CompanionAppCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  if (!device_metadata) {
    return;
  }

  std::u16string device_name;
  // If the name of the device has been set by the user, use that name,
  // otherwise use the OEM default name.
  if (device->display_name().has_value()) {
    device_name = base::UTF8ToUTF16(device->display_name().value());
  } else {
    device_name = base::ASCIIToUTF16(device_metadata->GetDetails().name());
  }

  notification_controller_->ShowApplicationAvailableNotification(
      device_name, device_metadata->image(),
      base::BindRepeating(&FastPairPresenterImpl::OnCompanionAppInstallClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnCompanionAppDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::ShowLaunchCompanionApp(
    scoped_refptr<Device> device,
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  toast_collision_avoidance_timer_.Start(
      FROM_HERE, ash::ToastData::kDefaultToastDuration,
      base::BindOnce(&FastPairPresenterImpl::ShowLaunchCompanionAppDelayed,
                     weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::ShowLaunchCompanionAppDelayed(
    scoped_refptr<Device> device,
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  const auto metadata_id = device->metadata_id();
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(
          &FastPairPresenterImpl::OnLaunchCompanionAppMetadataRetrieved,
          weak_pointer_factory_.GetWeakPtr(), device, callback));
}

void FastPairPresenterImpl::OnLaunchCompanionAppMetadataRetrieved(
    scoped_refptr<Device> device,
    CompanionAppCallback callback,
    DeviceMetadata* device_metadata,
    bool has_retryable_error) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  if (!device_metadata) {
    return;
  }

  std::u16string device_name;
  // If the name of the device has been set by the user, use that name,
  // otherwise use the OEM default name.
  if (device->display_name().has_value()) {
    device_name = base::UTF8ToUTF16(device->display_name().value());
  } else {
    device_name = base::ASCIIToUTF16(device_metadata->GetDetails().name());
  }

  notification_controller_->ShowApplicationInstalledNotification(
      // temporarily hardcoded text in place of companion app name
      device_name, device_metadata->image(), u"the web companion",
      base::BindRepeating(&FastPairPresenterImpl::OnCompanionAppSetupClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenterImpl::OnCompanionAppDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenterImpl::OnCompanionAppInstallClicked(
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  callback.Run(CompanionAppAction::kDownloadAndLaunchApp);
}

void FastPairPresenterImpl::OnCompanionAppSetupClicked(
    CompanionAppCallback callback) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  callback.Run(CompanionAppAction::kLaunchApp);
}

void FastPairPresenterImpl::OnCompanionAppDismissed(
    CompanionAppCallback callback,
    FastPairNotificationDismissReason dismiss_reason) {
  CHECK(features::IsFastPairPwaCompanionEnabled());

  switch (dismiss_reason) {
    case FastPairNotificationDismissReason::kDismissedByUser:
      callback.Run(CompanionAppAction::kDismissedByUser);
      break;
    case FastPairNotificationDismissReason::kDismissedByOs:
      [[fallthrough]];
    case FastPairNotificationDismissReason::kDismissedByTimeout:
      callback.Run(CompanionAppAction::kDismissed);
      break;
    default:
      NOTREACHED();
  }
}

void FastPairPresenterImpl::ShowPasskey(std::u16string device_name,
                                        uint32_t passkey) {
  notification_controller_->ShowPasskey(device_name, passkey);
}

void FastPairPresenterImpl::RemoveNotifications() {
  notification_controller_->RemoveNotifications();
}

void FastPairPresenterImpl::ExtendNotification() {
  notification_controller_->ExtendNotification();
}

}  // namespace quick_pair
}  // namespace ash
