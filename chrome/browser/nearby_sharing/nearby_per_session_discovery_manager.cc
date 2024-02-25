// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom-forward.h"
#include "components/cross_device/logging/logging.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {
std::optional<nearby_share::mojom::TransferStatus> GetTransferStatus(
    const TransferMetadata& transfer_metadata) {
  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
      return nearby_share::mojom::TransferStatus::kAwaitingLocalConfirmation;
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      return nearby_share::mojom::TransferStatus::kAwaitingRemoteAcceptance;
    case TransferMetadata::Status::kComplete:
      return nearby_share::mojom::TransferStatus::kComplete;
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
      return nearby_share::mojom::TransferStatus::kFailed;
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
      return nearby_share::mojom::TransferStatus::
          kAwaitingRemoteAcceptanceFailed;
    case TransferMetadata::Status::kUnknown:
      return nearby_share::mojom::TransferStatus::kUnknown;
    case TransferMetadata::Status::kConnecting:
      return nearby_share::mojom::TransferStatus::kConnecting;
    case TransferMetadata::Status::kCancelled:
      return nearby_share::mojom::TransferStatus::kCancelled;
    case TransferMetadata::Status::kDecodeAdvertisementFailed:
      return nearby_share::mojom::TransferStatus::kDecodeAdvertisementFailed;
    case TransferMetadata::Status::kMissingTransferUpdateCallback:
      return nearby_share::mojom::TransferStatus::
          kMissingTransferUpdateCallback;
    case TransferMetadata::Status::kMissingShareTarget:
      return nearby_share::mojom::TransferStatus::kMissingShareTarget;
    case TransferMetadata::Status::kMissingEndpointId:
      return nearby_share::mojom::TransferStatus::kMissingEndpointId;
    case TransferMetadata::Status::kMissingPayloads:
      return nearby_share::mojom::TransferStatus::kMissingPayloads;
    case TransferMetadata::Status::kPairedKeyVerificationFailed:
      return nearby_share::mojom::TransferStatus::kPairedKeyVerificationFailed;
    case TransferMetadata::Status::kInvalidIntroductionFrame:
      return nearby_share::mojom::TransferStatus::kInvalidIntroductionFrame;
    case TransferMetadata::Status::kIncompletePayloads:
      return nearby_share::mojom::TransferStatus::kIncompletePayloads;
    case TransferMetadata::Status::kFailedToCreateShareTarget:
      return nearby_share::mojom::TransferStatus::kFailedToCreateShareTarget;
    case TransferMetadata::Status::kFailedToInitiateOutgoingConnection:
      return nearby_share::mojom::TransferStatus::
          kFailedToInitiateOutgoingConnection;
    case TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse:
      return nearby_share::mojom::TransferStatus::
          kFailedToReadOutgoingConnectionResponse;
    case TransferMetadata::Status::kUnexpectedDisconnection:
      return nearby_share::mojom::TransferStatus::kUnexpectedDisconnection;
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
      // Ignore all other transfer status updates.
      return std::nullopt;
  }
}

std::string GetDeviceIdForLogs(const ShareTarget& share_target) {
  return share_target.device_id
             ? base::HexEncode(share_target.device_id.value())
             : "[null]";
}

}  // namespace

NearbyPerSessionDiscoveryManager::NearbyPerSessionDiscoveryManager(
    NearbySharingService* nearby_sharing_service,
    std::vector<std::unique_ptr<Attachment>> attachments)
    : nearby_sharing_service_(nearby_sharing_service),
      attachments_(std::move(attachments)) {
  nearby_sharing_service_->AddObserver(this);
}

NearbyPerSessionDiscoveryManager::~NearbyPerSessionDiscoveryManager() {
  StopDiscovery(base::DoNothing());
  observers_set_.Clear();
  nearby_sharing_service_->RemoveObserver(this);
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
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": transfer_update_listener_ is not is_bound(), cannot "
           "forward transfer updates";
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Nearby per-session discovery manager: "
      << "Transfer update for share target with ID " << share_target.id << ": "
      << TransferMetadata::StatusToString(transfer_metadata.status());

  std::optional<nearby_share::mojom::TransferStatus> status =
      GetTransferStatus(transfer_metadata);

  if (!status) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Nearby per-session discovery manager: "
        << " skipping status update, no mojo mapping defined yet.";
    return;
  }

  transfer_update_listener_->OnTransferUpdate(*status,
                                              transfer_metadata.token());
}

void NearbyPerSessionDiscoveryManager::OnShareTargetDiscovered(
    ShareTarget share_target) {
  CD_LOG(VERBOSE, Feature::NS)
      << "NearbyPerSessionDiscoveryManager::" << __func__
      << ": id=" << share_target.id
      << ", device_id=" << GetDeviceIdForLogs(share_target);
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

  // Dedup by the more stable device ID if possible.
  if (share_target.device_id) {
    auto it =
        base::ranges::find(discovered_share_targets_, share_target.device_id,
                           [](const auto& id_share_target_pair) {
                             return id_share_target_pair.second.device_id;
                           });

    if (it != discovered_share_targets_.end()) {
      CD_LOG(VERBOSE, Feature::NS)
          << "NearbyPerSessionDiscoveryManager::" << __func__
          << ": Removing previously discovered share target with "
          << "identical device_id=" << GetDeviceIdForLogs(share_target);
      OnShareTargetLost(it->second);
    }
  }

  discovered_share_targets_.insert_or_assign(share_target.id, share_target);
  share_target_listener_->OnShareTargetDiscovered(share_target);
}

void NearbyPerSessionDiscoveryManager::AddDiscoveryObserver(
    ::mojo::PendingRemote<nearby_share::mojom::DiscoveryObserver> observer) {
  observers_set_.Add(std::move(observer));
}

void NearbyPerSessionDiscoveryManager::OnShareTargetLost(
    ShareTarget share_target) {
  CD_LOG(VERBOSE, Feature::NS)
      << "NearbyPerSessionDiscoveryManager::" << __func__
      << ": id=" << share_target.id
      << ", device_id=" << GetDeviceIdForLogs(share_target);

  // It is possible that we already removed a ShareTarget from the map when
  // deduping by ShareTarget device_id.
  if (!base::Contains(discovered_share_targets_, share_target.id)) {
    CD_LOG(VERBOSE, Feature::NS)
        << "NearbyPerSessionDiscoveryManager::" << __func__
        << ": Share target id=" << share_target.id
        << " already removed. Taking no action.";
    return;
  }

  ++num_lost_;
  discovered_share_targets_.erase(share_target.id);
  share_target_listener_->OnShareTargetLost(share_target);
}

void NearbyPerSessionDiscoveryManager::StartDiscovery(
    mojo::PendingRemote<nearby_share::mojom::ShareTargetListener> listener,
    StartDiscoveryCallback callback) {
  if (nearby_sharing_service_->IsTransferring() ||
      nearby_sharing_service_->IsScanning() ||
      nearby_sharing_service_->IsConnecting()) {
    // Is there is currently a file transfer ongoing, return early with the
    // corresponding error code.
    std::move(callback).Run(nearby_share::mojom::StartDiscoveryResult::
                                kErrorInProgressTransferring);
    return;
  }

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
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to register send surface";
    UpdateFurthestDiscoveryProgressIfNecessary(
        DiscoveryProgress::kFailedToStartDiscovery);
    share_target_listener_.reset();

    nearby_share::mojom::StartDiscoveryResult errorStatus;
    switch (status) {
      case NearbySharingService::StatusCodes::kNoAvailableConnectionMedium:
        errorStatus =
            nearby_share::mojom::StartDiscoveryResult::kNoConnectionMedium;
        break;
      default:
        errorStatus = nearby_share::mojom::StartDiscoveryResult::kErrorGeneric;
    }
    std::move(callback).Run(errorStatus);

    return;
  }

  UpdateFurthestDiscoveryProgressIfNecessary(
      DiscoveryProgress::kStartedDiscoveryNothingFound);

  // Once this object is registered as send surface, we stay registered until
  // UnregisterSendSurface is called so that the transfer update listeners can
  // get updates even if Discovery is stopped.
  registered_as_send_surface_ = true;
  std::move(callback).Run(nearby_share::mojom::StartDiscoveryResult::kSuccess);
}

void NearbyPerSessionDiscoveryManager::StopDiscovery(
    base::OnceClosure callback) {
  if (registered_as_send_surface_) {
    NearbySharingService::StatusCodes status =
        nearby_sharing_service_->UnregisterSendSurface(this, this);
    base::UmaHistogramEnumeration(
        "Nearby.Share.Discovery.UnregisterSendSurface", status);
    if (status != NearbySharingService::StatusCodes::kOk) {
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Failed to unregister send surface";
    }
    registered_as_send_surface_ = false;
  }

  share_target_listener_.reset();
  std::move(callback).Run();
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
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Unknown share target selected: id=" << share_target_id;
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

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Failed to start send to share target";
  UpdateFurthestDiscoveryProgressIfNecessary(
      DiscoveryProgress::kFailedToStartSend);
  transfer_update_listener_.reset();
  std::move(callback).Run(nearby_share::mojom::SelectShareTargetResult::kError,
                          mojo::NullReceiver(), mojo::NullRemote());
}

void NearbyPerSessionDiscoveryManager::GetPayloadPreview(
    GetPayloadPreviewCallback callback) {
  // TODO(crbug.com/1158627): Extract this which is very similar to logic in
  // nearby share mojo traits.
  nearby_share::mojom::PayloadPreviewPtr payload_preview =
      nearby_share::mojom::PayloadPreview::New();
  payload_preview->file_count = 0;
  payload_preview->share_type = nearby_share::mojom::ShareType::kText;
  if (attachments_.empty()) {
    // Return with an empty text attachment.
    std::move(callback).Run(std::move(payload_preview));
    return;
  }

  // We have at least 1 attachment, use that one for the default description.
  auto& attachment = attachments_[0];
  payload_preview->description = attachment->GetDescription();

  if (attachment->family() == Attachment::Family::kFile)
    payload_preview->file_count = attachments_.size();

  if (payload_preview->file_count > 1) {
    payload_preview->share_type =
        nearby_share::mojom::ShareType::kMultipleFiles;
  } else {
    payload_preview->share_type = attachment->GetShareType();
  }

  std::move(callback).Run(std::move(payload_preview));
}

void NearbyPerSessionDiscoveryManager::OnNearbyProcessStopped() {
  for (auto& remote : observers_set_) {
    remote->OnNearbyProcessStopped();
  }
}

void NearbyPerSessionDiscoveryManager::OnStartDiscoveryResult(bool success) {
  for (auto& remote : observers_set_) {
    remote->OnStartDiscoveryResult(success);
  }
}

void NearbyPerSessionDiscoveryManager::
    UpdateFurthestDiscoveryProgressIfNecessary(DiscoveryProgress progress) {
  if (static_cast<int>(progress) > static_cast<int>(furthest_progress_))
    furthest_progress_ = progress;
}
