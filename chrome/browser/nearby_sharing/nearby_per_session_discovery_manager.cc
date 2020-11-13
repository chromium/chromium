// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
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
    case TransferMetadata::Status::kRejected:
      return nearby_share::mojom::TransferStatus::kRejected;
    case TransferMetadata::Status::kTimedOut:
      return nearby_share::mojom::TransferStatus::kTimedOut;
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return nearby_share::mojom::TransferStatus::kUnsupportedAttachmentType;
    case TransferMetadata::Status::kMediaUnavailable:
      return nearby_share::mojom::TransferStatus::kMediaUnavailable;
    case TransferMetadata::Status::kNotEnoughSpace:
      return nearby_share::mojom::TransferStatus::kNotEnoughSpace;
    case TransferMetadata::Status::kFailed:
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return nearby_share::mojom::TransferStatus::kFailed;
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kCancelled:
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
      // Ignore all other transfer status updates.
      return base::nullopt;
  }
}

nearby_share::mojom::ShareType GetTextShareType(
    const TextAttachment* attachment) {
  switch (attachment->type()) {
    case TextAttachment::Type::kUrl:
      return nearby_share::mojom::ShareType::kUrl;
    case TextAttachment::Type::kAddress:
      return nearby_share::mojom::ShareType::kAddress;
    case TextAttachment::Type::kPhoneNumber:
      return nearby_share::mojom::ShareType::kPhone;
    default:
      return nearby_share::mojom::ShareType::kText;
  }
}

nearby_share::mojom::ShareType GetFileShareType(
    const FileAttachment* attachment) {
  switch (attachment->type()) {
    case FileAttachment::Type::kImage:
      return nearby_share::mojom::ShareType::kImageFile;
    case FileAttachment::Type::kVideo:
      return nearby_share::mojom::ShareType::kVideoFile;
    case FileAttachment::Type::kAudio:
      return nearby_share::mojom::ShareType::kAudioFile;
    default:
      break;
  }

  // Try matching on mime type if the attachment type is unrecognized.
  if (attachment->mime_type() == "application/pdf") {
    return nearby_share::mojom::ShareType::kPdfFile;
  } else if (attachment->mime_type() ==
             "application/vnd.google-apps.document") {
    return nearby_share::mojom::ShareType::kGoogleDocsFile;
  } else if (attachment->mime_type() ==
             "application/vnd.google-apps.spreadsheet") {
    return nearby_share::mojom::ShareType::kGoogleSheetsFile;
  } else if (attachment->mime_type() ==
             "application/vnd.google-apps.presentation") {
    return nearby_share::mojom::ShareType::kGoogleSlidesFile;
  } else {
    return nearby_share::mojom::ShareType::kUnknownFile;
  }
}

}  // namespace

NearbyPerSessionDiscoveryManager::NearbyPerSessionDiscoveryManager(
    NearbySharingService* nearby_sharing_service,
    std::vector<std::unique_ptr<Attachment>> attachments)
    : nearby_sharing_service_(nearby_sharing_service),
      attachments_(std::move(attachments)) {}

NearbyPerSessionDiscoveryManager::~NearbyPerSessionDiscoveryManager() {
  UnregisterSendSurface();
  base::UmaHistogramEnumeration(
      "Nearby.Share.Discovery.FurthestDiscoveryProgress", furthest_progress_);
  base::UmaHistogramCounts100(
      "Nearby.Share.Discovery.NumShareTargets.Discovered", num_discovered_);
  base::UmaHistogramCounts100("Nearby.Share.Discovery.NumShareTargets.Lost",
                              num_lost_);
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
  // Update metrics.
  UpdateFurthestDiscoveryProgressIfNecessary(
      DiscoveryProgress::kDiscoveredShareTargetNothingSent);
  if (!base::Contains(discovered_share_targets_, share_target.id)) {
    ++num_discovered_;
    if (num_discovered_ == 1) {
      base::UmaHistogramMediumTimes(
          "Nearby.Share.Discovery.Delay.FromStartDiscoveryToFirstDiscovery",
          base::TimeTicks::Now() - *discovery_start_time_);
    }
    base::UmaHistogramMediumTimes(
        "Nearby.Share.Discovery.Delay.FromStartDiscoveryToAnyDiscovery",
        base::TimeTicks::Now() - *discovery_start_time_);
  }

  base::InsertOrAssign(discovered_share_targets_, share_target.id,
                       share_target);
  share_target_listener_->OnShareTargetDiscovered(share_target);
}

void NearbyPerSessionDiscoveryManager::OnShareTargetLost(
    ShareTarget share_target) {
  if (base::Contains(discovered_share_targets_, share_target.id)) {
    ++num_lost_;
  }
  discovered_share_targets_.erase(share_target.id);
  share_target_listener_->OnShareTargetLost(share_target);
}

void NearbyPerSessionDiscoveryManager::StartDiscovery(
    mojo::PendingRemote<nearby_share::mojom::ShareTargetListener> listener,
    StartDiscoveryCallback callback) {
  discovery_start_time_ = base::TimeTicks::Now();

  // Starting discovery again closes any previous discovery session.
  share_target_listener_.reset();
  share_target_listener_.Bind(std::move(listener));
  // NOTE: Previously we set a disconnect handler here that called
  // UnregisterSendSurface, but this causes transfer updates to stop flowing to
  // to the UI. Instead, we rely on the destructor's call to
  // UnregisterSendSurface which will trigger when the share sheet goes away.

  NearbySharingService::StatusCodes status =
      nearby_sharing_service_->RegisterSendSurface(
          this, this, NearbySharingService::SendSurfaceState::kForeground);
  base::UmaHistogramEnumeration("Nearby.Share.Discovery.StartDiscovery",
                                status);
  if (status != NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << __func__ << ": Failed to register send surface";
    UpdateFurthestDiscoveryProgressIfNecessary(
        DiscoveryProgress::kFailedToStartDiscovery);
    share_target_listener_.reset();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  UpdateFurthestDiscoveryProgressIfNecessary(
      DiscoveryProgress::kStartedDiscoveryNothingFound);

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
  bool look_up_share_target_success = iter != discovered_share_targets_.end();
  base::UmaHistogramBoolean("Nearby.Share.Discovery.LookUpSelectedShareTarget",
                            look_up_share_target_success);
  if (!look_up_share_target_success) {
    NS_LOG(VERBOSE) << __func__ << ": Unknown share target selected: id="
                    << share_target_id;
    UpdateFurthestDiscoveryProgressIfNecessary(
        DiscoveryProgress::kFailedToLookUpSelectedShareTarget);
    std::move(callback).Run(
        nearby_share::mojom::SelectShareTargetResult::kInvalidShareTarget,
        mojo::NullReceiver(), mojo::NullRemote());
    return;
  }

  // Bind update listener before calling the sharing service to get all updates.
  mojo::PendingReceiver<nearby_share::mojom::TransferUpdateListener> receiver =
      transfer_update_listener_.BindNewPipeAndPassReceiver();
  transfer_update_listener_.reset_on_disconnect();

  base::UmaHistogramCounts100(
      "Nearby.Share.Discovery.NumShareTargets.PresentWhenSendStarts",
      discovered_share_targets_.size());
  base::UmaHistogramMediumTimes(
      "Nearby.Share.Discovery.Delay.FromStartDiscoveryToStartSend",
      base::TimeTicks::Now() - *discovery_start_time_);

  NearbySharingService::StatusCodes status =
      nearby_sharing_service_->SendAttachments(iter->second,
                                               std::move(attachments_));
  base::UmaHistogramEnumeration("Nearby.Share.Discovery.StartSend", status);

  // If the send call succeeded, we expect OnTransferUpdate() to be called next.
  if (status == NearbySharingService::StatusCodes::kOk) {
    UpdateFurthestDiscoveryProgressIfNecessary(DiscoveryProgress::kStartedSend);
    mojo::PendingRemote<nearby_share::mojom::ConfirmationManager> remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<NearbyConfirmationManager>(
                                    nearby_sharing_service_, iter->second),
                                remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(nearby_share::mojom::SelectShareTargetResult::kOk,
                            std::move(receiver), std::move(remote));
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Failed to start send to share target";
  UpdateFurthestDiscoveryProgressIfNecessary(
      DiscoveryProgress::kFailedToStartSend);
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

  // TODO(crbug.com/1144942) Add virtual GetShareType to Attachment to eliminate
  // these casts.
  switch (attachment->family()) {
    case Attachment::Family::kText:
      send_preview->share_type =
          GetTextShareType(static_cast<TextAttachment*>(attachment.get()));
      break;
    case Attachment::Family::kFile:
      send_preview->file_count = attachments_.size();
      // For multiple files we don't capture the types.
      send_preview->share_type =
          attachments_.size() > 1
              ? nearby_share::mojom::ShareType::kMultipleFiles
              : GetFileShareType(
                    static_cast<FileAttachment*>(attachment.get()));
      break;
  }

  std::move(callback).Run(std::move(send_preview));
}

void NearbyPerSessionDiscoveryManager::UnregisterSendSurface() {
  if (registered_as_send_surface_) {
    NearbySharingService::StatusCodes status =
        nearby_sharing_service_->UnregisterSendSurface(this, this);
    base::UmaHistogramEnumeration(
        "Nearby.Share.Discovery.UnregisterSendSurface", status);
    if (status != NearbySharingService::StatusCodes::kOk) {
      NS_LOG(WARNING) << __func__ << ": Failed to unregister send surface";
    }
    registered_as_send_surface_ = false;
  }

  share_target_listener_.reset();
}

void NearbyPerSessionDiscoveryManager::
    UpdateFurthestDiscoveryProgressIfNecessary(DiscoveryProgress progress) {
  if (static_cast<int>(progress) > static_cast<int>(furthest_progress_))
    furthest_progress_ = progress;
}
