// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/ui/fast_pair/fast_pair_presenter.h"

#include <string>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "ash/quick_pair/proto/fastpair.pb.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/ui/actions.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace quick_pair {

FastPairPresenter::FastPairPresenter() = default;
FastPairPresenter::~FastPairPresenter() = default;

void FastPairPresenter::ShowDiscovery(scoped_refptr<Device> device,
                                      DiscoveryCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindRepeating(&FastPairPresenter::OnDiscoveryMetadataRetrieved,
                          weak_pointer_factory_.GetWeakPtr(), std::move(device),
                          std::move(callback)));
}

void FastPairPresenter::OnDiscoveryMetadataRetrieved(
    scoped_refptr<Device> device,
    DiscoveryCallback callback,
    DeviceMetadata* device_metadata) {
  QP_LOG(VERBOSE) << __func__;
  if (!device_metadata) {
    return;
  }

  // Anti-spoofing keys were introduced in Fast Pair v2, so if this isn't
  // available then the device is v1.
  if (device_metadata->GetDetails()
          .anti_spoofing_key_pair()
          .public_key()
          .empty()) {
    device->SetAdditionalData(Device::AdditionalDataType::kFastPairVersion,
                              {1});
  }

  notification_controller_->ShowDiscoveryNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenter::OnDiscoveryClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenter::OnDiscoveryDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenter::OnDiscoveryClicked(DiscoveryCallback callback) {
  callback.Run(DiscoveryAction::kPairToDevice);
}

void FastPairPresenter::OnDiscoveryDismissed(DiscoveryCallback callback,
                                             bool user_dismissed) {
  callback.Run(user_dismissed ? DiscoveryAction::kDismissedByUser
                              : DiscoveryAction::kDismissed);
}

void FastPairPresenter::ShowPairing(scoped_refptr<Device> device) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenter::OnPairingMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device)));
}

void FastPairPresenter::OnPairingMetadataRetrieved(
    scoped_refptr<Device> device,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowPairingNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(), base::DoNothing(), base::DoNothing());
}

void FastPairPresenter::ShowPairingFailed(scoped_refptr<Device> device,
                                          PairingFailedCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenter::OnPairingFailedMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(callback)));
}

void FastPairPresenter::OnPairingFailedMetadataRetrieved(
    scoped_refptr<Device> device,
    PairingFailedCallback callback,
    DeviceMetadata* device_metadata) {
  if (!device_metadata) {
    return;
  }

  notification_controller_->ShowErrorNotification(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      device_metadata->image(),
      base::BindRepeating(&FastPairPresenter::OnNavigateToSettings,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenter::OnPairingFailedDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenter::OnNavigateToSettings(PairingFailedCallback callback) {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
  } else {
    QP_LOG(WARNING) << "Cannot open Bluetooth Settings since it's not possible "
                       "to opening WebUI settings";
  }

  callback.Run(PairingFailedAction::kNavigateToSettings);
}

void FastPairPresenter::OnPairingFailedDismissed(PairingFailedCallback callback,
                                                 bool user_dismissed) {
  callback.Run(user_dismissed ? PairingFailedAction::kDismissedByUser
                              : PairingFailedAction::kDismissed);
}

void FastPairPresenter::ShowAssociateAccount(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback) {
  const auto metadata_id = device->metadata_id;
  FastPairRepository::Get()->GetDeviceMetadata(
      metadata_id,
      base::BindOnce(&FastPairPresenter::OnAssociateAccountMetadataRetrieved,
                     weak_pointer_factory_.GetWeakPtr(), std::move(device),
                     std::move(callback)));
}

void FastPairPresenter::OnAssociateAccountMetadataRetrieved(
    scoped_refptr<Device> device,
    AssociateAccountCallback callback,
    DeviceMetadata* device_metadata) {
  QP_LOG(VERBOSE) << __func__ << ": " << device;
  if (!device_metadata) {
    return;
  }

  signin::IdentityManager* identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  if (!identity_manager) {
    QP_LOG(ERROR) << __func__
                  << ": IdentityManager is not available for Associate Account "
                     "notification.";
    return;
  }

  const std::string email =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;

  notification_controller_->ShowAssociateAccount(
      base::ASCIIToUTF16(device_metadata->GetDetails().name()),
      base::ASCIIToUTF16(email), device_metadata->image(),
      base::BindRepeating(&FastPairPresenter::OnAssociateAccountActionClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindRepeating(&FastPairPresenter::OnLearnMoreClicked,
                          weak_pointer_factory_.GetWeakPtr(), callback),
      base::BindOnce(&FastPairPresenter::OnAssociateAccountDismissed,
                     weak_pointer_factory_.GetWeakPtr(), callback));
}

void FastPairPresenter::OnAssociateAccountActionClicked(
    AssociateAccountCallback callback) {
  callback.Run(AssociateAccountAction::kAssoicateAccount);
}

void FastPairPresenter::OnLearnMoreClicked(AssociateAccountCallback callback) {
  // TODO (crbug/1256983): Implement support for "Learn More" Button in Show
  // Associate Account Notification
  callback.Run(AssociateAccountAction::kLearnMore);
}

void FastPairPresenter::OnAssociateAccountDismissed(
    AssociateAccountCallback callback,
    bool user_dismissed) {
  callback.Run(user_dismissed ? AssociateAccountAction::kDismissedByUser
                              : AssociateAccountAction::kDismissed);
}

void FastPairPresenter::ShowCompanionApp(scoped_refptr<Device> device,
                                         CompanionAppCallback callback) {}

void FastPairPresenter::RemoveNotifications(scoped_refptr<Device> device) {
  notification_controller_->RemoveNotifications();
}

}  // namespace quick_pair
}  // namespace ash
