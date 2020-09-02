// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

base::Optional<nearby_share::mojom::TransferStatus> GetTransferStatus(
    const TransferMetadata& transfer_metadata) {
  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
      return nearby_share::mojom::TransferStatus::kAwaitingLocalConfirmation;
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      return nearby_share::mojom::TransferStatus::kAwaitingRemoteAcceptance;
    case TransferMetadata::Status::kComplete:
    case TransferMetadata::Status::kInProgress:
      return nearby_share::mojom::TransferStatus::kInProgress;
    default:
      break;
  }

  // TODO(crbug.com/1123934): Show error if transfer_metadata.is_final_status().

  // Ignore all other transfer status updates.
  return base::nullopt;
}

}  // namespace

NearbyPerSessionDiscoveryManager::NearbyPerSessionDiscoveryManager(
    NearbySharingService* nearby_sharing_service,
    std::vector<std::unique_ptr<Attachment>> attachments)
    : nearby_sharing_service_(nearby_sharing_service),
      attachments_(std::move(attachments)) {}

NearbyPerSessionDiscoveryManager::~NearbyPerSessionDiscoveryManager() {
  UnregisterSendSurface();
}

void NearbyPerSessionDiscoveryManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  DCHECK(transfer_update_listener_.is_bound());
  base::Optional<nearby_share::mojom::TransferStatus> status =
      GetTransferStatus(transfer_metadata);
  if (!status)
    return;

  transfer_update_listener_->OnTransferUpdate(*status,
                                              transfer_metadata.token());
}

void NearbyPerSessionDiscoveryManager::OnShareTargetDiscovered(
    ShareTarget share_target) {
  base::InsertOrAssign(discovered_share_targets_, share_target.id,
                       share_target);
  share_target_listener_->OnShareTargetDiscovered(share_target);
}

void NearbyPerSessionDiscoveryManager::OnShareTargetLost(
    ShareTarget share_target) {
  discovered_share_targets_.erase(share_target.id);
  share_target_listener_->OnShareTargetLost(share_target);
}

void NearbyPerSessionDiscoveryManager::StartDiscovery(
    mojo::PendingRemote<nearby_share::mojom::ShareTargetListener> listener,
    StartDiscoveryCallback callback) {
  share_target_listener_.Bind(std::move(listener));
  // |share_target_listener_| owns the callbacks and is guaranteed to be
  // destroyed before |this|, therefore making base::Unretained() safe to use.
  share_target_listener_.set_disconnect_handler(
      base::BindOnce(&NearbyPerSessionDiscoveryManager::UnregisterSendSurface,
                     base::Unretained(this)));

  if (nearby_sharing_service_->RegisterSendSurface(
          this, this, NearbySharingService::SendSurfaceState::kForeground) !=
      NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << "Failed to register send surface";
    share_target_listener_.reset();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::move(callback).Run(/*success=*/true);
}

void NearbyPerSessionDiscoveryManager::SelectShareTarget(
    const base::UnguessableToken& share_target_id,
    SelectShareTargetCallback callback) {
  DCHECK(share_target_listener_.is_bound());
  DCHECK(!transfer_update_listener_.is_bound());

  auto iter = discovered_share_targets_.find(share_target_id);
  if (iter == discovered_share_targets_.end()) {
    NS_LOG(VERBOSE) << "Unknown share target selected: id=" << share_target_id;
    std::move(callback).Run(
        nearby_share::mojom::SelectShareTargetResult::kInvalidShareTarget,
        mojo::NullReceiver(), mojo::NullRemote());
    return;
  }

  // Bind update listener before calling the sharing service to get all updates.
  mojo::PendingReceiver<nearby_share::mojom::TransferUpdateListener> receiver =
      transfer_update_listener_.BindNewPipeAndPassReceiver();

  NearbySharingService::StatusCodes status =
      nearby_sharing_service_->SendAttachments(iter->second,
                                               std::move(attachments_));

  // If the send call succeeded, we expect OnTransferUpdate() to be called next.
  if (status == NearbySharingService::StatusCodes::kOk) {
    mojo::PendingRemote<nearby_share::mojom::ConfirmationManager> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<NearbyConfirmationManager>(
                                    nearby_sharing_service_, iter->second),
                                remote.InitWithNewPipeAndPassReceiver());

    std::move(callback).Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            std::move(receiver), std::move(remote));
    return;
  }

  NS_LOG(VERBOSE) << "Failed to select share target";
  transfer_update_listener_.reset();
  std::move(callback).Run(nearby_share::mojom::SelectShareTargetResult::kError,
                          mojo::NullReceiver(), mojo::NullRemote());
}

void NearbyPerSessionDiscoveryManager::UnregisterSendSurface() {
  if (!share_target_listener_.is_bound())
    return;

  share_target_listener_.reset();

  if (nearby_sharing_service_->UnregisterSendSurface(this, this) !=
      NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << "Failed to unregister send surface";
  }
}
