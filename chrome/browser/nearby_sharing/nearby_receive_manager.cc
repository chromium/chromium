// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_receive_manager.h"

#include "base/functional/callback_helpers.h"
#include "components/cross_device/logging/logging.h"

NearbyReceiveManager::NearbyReceiveManager(
    NearbySharingService* nearby_sharing_service)
    : nearby_sharing_service_(nearby_sharing_service) {
  DCHECK(nearby_sharing_service_);
  nearby_sharing_service_->AddObserver(this);
}

NearbyReceiveManager::~NearbyReceiveManager() {
  UnregisterForegroundReceiveSurface(base::DoNothing());
  observers_set_.Clear();
  nearby_sharing_service_->RemoveObserver(this);
}

void NearbyReceiveManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  TransferMetadata::Status status = transfer_metadata.status();
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Nearby receive manager: "
      << "Transfer update for share target with ID " << share_target.id << ": "
      << TransferMetadata::StatusToString(transfer_metadata.status());

  NotifyOnTransferUpdate(share_target, transfer_metadata);
  if (TransferMetadata::Status::kAwaitingLocalConfirmation == status) {
    share_targets_map_.insert_or_assign(share_target.id, share_target);
  } else if (transfer_metadata.is_final_status()) {
    share_targets_map_.erase(share_target.id);
  }
}

void NearbyReceiveManager::AddReceiveObserver(
    ::mojo::PendingRemote<nearby_share::mojom::ReceiveObserver> observer) {
  observers_set_.Add(std::move(observer));
}

void NearbyReceiveManager::IsInHighVisibility(
    IsInHighVisibilityCallback callback) {
  std::move(callback).Run(nearby_sharing_service_->IsInHighVisibility());
}

void NearbyReceiveManager::RegisterForegroundReceiveSurface(
    RegisterForegroundReceiveSurfaceCallback callback) {
  const NearbySharingService::StatusCodes result =
      nearby_sharing_service_->RegisterReceiveSurface(
          this, NearbySharingService::ReceiveSurfaceState::kForeground);
  switch (result) {
    case NearbySharingService::StatusCodes::kOk:
      std::move(callback).Run(
          nearby_share::mojom::RegisterReceiveSurfaceResult::kSuccess);
      return;
    case NearbySharingService::StatusCodes::kError:
    case NearbySharingService::StatusCodes::kOutOfOrderApiCall:
    case NearbySharingService::StatusCodes::kStatusAlreadyStopped:
      std::move(callback).Run(
          nearby_share::mojom::RegisterReceiveSurfaceResult::kFailure);
      return;
    case NearbySharingService::StatusCodes::kTransferAlreadyInProgress:
      std::move(callback).Run(
          nearby_share::mojom::RegisterReceiveSurfaceResult::
              kTransferInProgress);
      return;
    case NearbySharingService::StatusCodes::kNoAvailableConnectionMedium:
      std::move(callback).Run(
          nearby_share::mojom::RegisterReceiveSurfaceResult::
              kNoConnectionMedium);
      return;
  }
}

void NearbyReceiveManager::UnregisterForegroundReceiveSurface(
    UnregisterForegroundReceiveSurfaceCallback callback) {
  bool success = NearbySharingService::StatusCodes::kOk ==
                 nearby_sharing_service_->UnregisterReceiveSurface(this);
  std::move(callback).Run(success);
}

void NearbyReceiveManager::Accept(const base::UnguessableToken& share_target_id,
                                  AcceptCallback callback) {
  auto iter = share_targets_map_.find(share_target_id);
  if (iter == share_targets_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Unknown share target accepted: id=" << share_target_id;
    std::move(callback).Run(false);
    return;
  }
  nearby_sharing_service_->Accept(
      iter->second, base::BindOnce(
                        [](AcceptCallback accept_callback,
                           NearbySharingService::StatusCodes status_code) {
                          bool success =
                              NearbySharingService::StatusCodes::kOk ==
                              status_code;
                          std::move(accept_callback).Run(success);
                        },
                        std::move(callback)));
}

void NearbyReceiveManager::Reject(const base::UnguessableToken& share_target_id,
                                  RejectCallback callback) {
  auto iter = share_targets_map_.find(share_target_id);
  if (iter == share_targets_map_.end()) {
    CD_LOG(ERROR, Feature::NS)
        << "Unknown share target rejected: id=" << share_target_id;
    std::move(callback).Run(false);
    return;
  }
  nearby_sharing_service_->Reject(
      iter->second, base::BindOnce(
                        [](RejectCallback reject_callback,
                           NearbySharingService::StatusCodes status_code) {
                          bool success =
                              NearbySharingService::StatusCodes::kOk ==
                              status_code;
                          std::move(reject_callback).Run(success);
                        },
                        std::move(callback)));
}

void NearbyReceiveManager::RecordFastInitiationNotificationUsage(bool success) {
  nearby_sharing_service_->RecordFastInitiationNotificationUsage(success);
}

void NearbyReceiveManager::OnHighVisibilityChanged(bool in_high_visibility) {
  for (auto& remote : observers_set_) {
    remote->OnHighVisibilityChanged(in_high_visibility);
  }
}

void NearbyReceiveManager::OnNearbyProcessStopped() {
  for (auto& remote : observers_set_) {
    remote->OnNearbyProcessStopped();
  }
}

void NearbyReceiveManager::OnStartAdvertisingFailure() {
  for (auto& remote : observers_set_) {
    remote->OnStartAdvertisingFailure();
  }
}

void NearbyReceiveManager::NotifyOnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& metadata) {
  for (auto& remote : observers_set_) {
    remote->OnTransferUpdate(share_target, metadata.ToMojo());
  }
}
