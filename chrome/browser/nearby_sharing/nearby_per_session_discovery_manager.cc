// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom-forward.h"
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
  if (!transfer_update_listener_.is_bound()) {
    // This can happen when registering the send surface and an existing
    // transfer is happening or recently happened.
    NS_LOG(VERBOSE) << __func__
                    << ": transfer_update_listener_ is not is_bound(), cannot "
                       "forward transfer updates";
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Nearby per-session discovery manager: "
                  << "Transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(
                         transfer_metadata.status());

  base::Optional<nearby_share::mojom::TransferStatus> status =
      GetTransferStatus(transfer_metadata);

  if (!status) {
    NS_LOG(VERBOSE) << __func__ << ": Nearby per-session discovery manager: "
                    << " skipping status update, no mojo mapping defined yet.";
    return;
  }

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
  // Starting discovery again closes any previous discovery session.
  share_target_listener_.reset();
  share_target_listener_.Bind(std::move(listener));
  // NOTE: Previously we set a disconnect handler here that called
  // UnregisterSendSurface, but this causes transfer updates to stop flowing to
  // to the UI. Instead, we rely on the destructor's call to
  // UnregisterSendSurface which will trigger when the share sheet goes away.

  if (nearby_sharing_service_->RegisterSendSurface(
          this, this, NearbySharingService::SendSurfaceState::kForeground) !=
      NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << "Failed to register send surface";
    share_target_listener_.reset();
    std::move(callback).Run(/*success=*/false);
    return;
  }
  // Once this object is registered as send surface, we stay registered until
  // UnregisterSendSurface is called so that the transfer update listeners can
  // get updates even if Discovery is stopped.
  registered_as_send_surface_ = true;
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
  transfer_update_listener_.reset_on_disconnect();

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

void NearbyPerSessionDiscoveryManager::GetSendPreview(
    GetSendPreviewCallback callback) {
  nearby_share::mojom::SendPreviewPtr send_preview =
      nearby_share::mojom::SendPreview::New();
  send_preview->file_count = 0;
  send_preview->share_type = nearby_share::mojom::ShareType::kText;
  if (attachments_.empty()) {
    // Return with an empty text attachment.
    std::move(callback).Run(std::move(send_preview));
    return;
  }

  // We have at least 1 attachment, use that one for the default description.
  auto& attachment = attachments_[0];
  send_preview->description = attachment->GetDescription();

  // For text we are all done, but for files we have to handle the two cases of
  // sharing a single file or sharing multiple files.
  if (attachment->family() == Attachment::Family::kFile) {
    send_preview->file_count = attachments_.size();
    if (attachments_.size() > 1) {
      // For multiple files we don't capture the types.
      send_preview->share_type = nearby_share::mojom::ShareType::kMultipleFiles;
    } else {
      FileAttachment* file_attachment =
          static_cast<FileAttachment*>(attachment.get());
      switch (file_attachment->type()) {
        case FileAttachment::Type::kImage:
          send_preview->share_type = nearby_share::mojom::ShareType::kImageFile;
          break;
        case FileAttachment::Type::kVideo:
          send_preview->share_type = nearby_share::mojom::ShareType::kVideoFile;
          break;
        case FileAttachment::Type::kAudio:
          send_preview->share_type = nearby_share::mojom::ShareType::kAudioFile;
          break;
        default:
          send_preview->share_type =
              nearby_share::mojom::ShareType::kUnknownFile;
          break;
      }
    }
  }

  std::move(callback).Run(std::move(send_preview));
}

void NearbyPerSessionDiscoveryManager::UnregisterSendSurface() {
  if (registered_as_send_surface_) {
    if (nearby_sharing_service_->UnregisterSendSurface(this, this) !=
        NearbySharingService::StatusCodes::kOk) {
      NS_LOG(WARNING) << "Failed to unregister send surface";
    }
    registered_as_send_surface_ = false;
  }

  share_target_listener_.reset();
}
