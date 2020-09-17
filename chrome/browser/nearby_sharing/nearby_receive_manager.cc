// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_receive_manager.h"

#include "chrome/browser/nearby_sharing/logging/logging.h"

NearbyReceiveManager::NearbyReceiveManager(
    NearbySharingService* nearby_sharing_service)
    : nearby_sharing_service_(nearby_sharing_service) {
  DCHECK(nearby_sharing_service_);
}

NearbyReceiveManager::~NearbyReceiveManager() {
  ExitHighVisibility(base::DoNothing());
  observers_set_.Clear();
}

void NearbyReceiveManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  TransferMetadata::Status status = transfer_metadata.status();
  NS_LOG(VERBOSE) << __func__ << ": Nearby receive manager: "
                  << "Transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(
                         transfer_metadata.status());

  if (TransferMetadata::Status::kAwaitingLocalConfirmation == status) {
    share_targets_map_.insert_or_assign(share_target.id, share_target);
    NotifyOnIncomingShare(share_target, transfer_metadata.token());
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
  std::move(callback).Run(in_high_visibility_);
}

void NearbyReceiveManager::EnterHighVisibility(
    EnterHighVisibilityCallback callback) {
  bool success =
      NearbySharingService::StatusCodes::kOk ==
      nearby_sharing_service_->RegisterReceiveSurface(
          this, NearbySharingService::ReceiveSurfaceState::kForeground);
  // We are in high-visibility only if the call was successful.
  SetInHighVisibility(success);
  std::move(callback).Run(success);
}

void NearbyReceiveManager::ExitHighVisibility(
    ExitHighVisibilityCallback callback) {
  bool success = NearbySharingService::StatusCodes::kOk ==
                 nearby_sharing_service_->UnregisterReceiveSurface(this);
  // We have only exited high visibility if the call was successful.
  SetInHighVisibility(success ? false : this->in_high_visibility_);
  std::move(callback).Run(success);
}

void NearbyReceiveManager::Accept(const base::UnguessableToken& share_target_id,
                                  AcceptCallback callback) {
  auto iter = share_targets_map_.find(share_target_id);
  if (iter == share_targets_map_.end()) {
    NS_LOG(ERROR) << "Unknown share target accepted: id=" << share_target_id;
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
    NS_LOG(ERROR) << "Unknown share target rejected: id=" << share_target_id;
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

void NearbyReceiveManager::SetInHighVisibility(bool in_high_visibility) {
  if (in_high_visibility_ != in_high_visibility) {
    in_high_visibility_ = in_high_visibility;
    NotifyOnHighVisibilityChanged(in_high_visibility_);
  }
}

void NearbyReceiveManager::NotifyOnHighVisibilityChanged(
    bool in_high_visibility) {
  for (auto& remote : observers_set_) {
    remote->OnHighVisibilityChanged(in_high_visibility);
  }
}

void NearbyReceiveManager::NotifyOnIncomingShare(
    const ShareTarget& share_target,
    const base::Optional<std::string>& connection_token) {
  for (auto& remote : observers_set_) {
    remote->OnIncomingShare(share_target, connection_token);
  }
}
