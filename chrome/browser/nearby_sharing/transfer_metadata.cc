// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

// static
bool TransferMetadata::IsFinalStatus(Status status) {
  switch (status) {
    case Status::kAwaitingRemoteAcceptanceFailed:
    case Status::kComplete:
    case Status::kFailed:
    case Status::kRejected:
    case Status::kCancelled:
    case Status::kTimedOut:
    case Status::kMediaUnavailable:
    case Status::kNotEnoughSpace:
    case Status::kUnsupportedAttachmentType:
    case Status::kExternalProviderLaunched:
      return true;
    default:
      return false;
  }
}

// static
std::string TransferMetadata::StatusToString(Status status) {
  switch (status) {
    case Status::kConnecting:
      return "kConnecting";
    case Status::kUnknown:
      return "kUnknown";
    case Status::kAwaitingLocalConfirmation:
      return "kAwaitingLocalConfirmation";
    case Status::kAwaitingRemoteAcceptance:
      return "kAwaitingRemoteAcceptance";
    case Status::kAwaitingRemoteAcceptanceFailed:
      return "kAwaitingRemoteAcceptanceFailed";
    case Status::kInProgress:
      return "kInProgress";
    case Status::kComplete:
      return "kComplete";
    case Status::kFailed:
      return "kFailed";
    case Status::kRejected:
      return "kReject";
    case Status::kCancelled:
      return "kCancelled";
    case Status::kTimedOut:
      return "kTimedOut";
    case Status::kMediaUnavailable:
      return "kMediaUnavailable";
    case Status::kMediaDownloading:
      return "kMediaDownloading";
    case Status::kNotEnoughSpace:
      return "kNotEnoughSpace";
    case Status::kUnsupportedAttachmentType:
      return "kUnsupportedAttachmentType";
    case Status::kExternalProviderLaunched:
      return "kExternalProviderLaunched";
  }
}

// static
nearby_share::mojom::TransferStatus TransferMetadata::StatusToMojo(
    Status status) {
  switch (status) {
    case Status::kUnknown:
      return nearby_share::mojom::TransferStatus::kUnknown;
    case Status::kConnecting:
      return nearby_share::mojom::TransferStatus::kConnecting;
    case Status::kAwaitingLocalConfirmation:
      return nearby_share::mojom::TransferStatus::kAwaitingLocalConfirmation;
    case Status::kAwaitingRemoteAcceptance:
      return nearby_share::mojom::TransferStatus::kAwaitingRemoteAcceptance;
    case Status::kAwaitingRemoteAcceptanceFailed:
      return nearby_share::mojom::TransferStatus::
          kAwaitingRemoteAcceptanceFailed;
    case Status::kInProgress:
      return nearby_share::mojom::TransferStatus::kInProgress;
    case Status::kComplete:
      return nearby_share::mojom::TransferStatus::kComplete;
    case Status::kFailed:
      return nearby_share::mojom::TransferStatus::kFailed;
    case Status::kRejected:
      return nearby_share::mojom::TransferStatus::kRejected;
    case Status::kCancelled:
      return nearby_share::mojom::TransferStatus::kCancelled;
    case Status::kTimedOut:
      return nearby_share::mojom::TransferStatus::kTimedOut;
    case Status::kMediaUnavailable:
      return nearby_share::mojom::TransferStatus::kMediaUnavailable;
    case Status::kNotEnoughSpace:
      return nearby_share::mojom::TransferStatus::kNotEnoughSpace;
    case Status::kUnsupportedAttachmentType:
      return nearby_share::mojom::TransferStatus::kUnsupportedAttachmentType;
    case Status::kMediaDownloading:
    case Status::kExternalProviderLaunched:
      // These statuses are not used anywhere.
      NOTREACHED();
      return nearby_share::mojom::TransferStatus::kUnknown;
  }
  NOTREACHED();
}

nearby_share::mojom::TransferMetadataPtr TransferMetadata::ToMojo() const {
  auto mojo_transfer_metadata = nearby_share::mojom::TransferMetadata::New();
  mojo_transfer_metadata->status = StatusToMojo(status());
  mojo_transfer_metadata->progress = progress();
  mojo_transfer_metadata->token = token();
  mojo_transfer_metadata->is_original = is_original();
  mojo_transfer_metadata->is_final_status = is_final_status();
  return mojo_transfer_metadata;
}

TransferMetadata::TransferMetadata(Status status,
                                   float progress,
                                   base::Optional<std::string> token,
                                   bool is_original,
                                   bool is_final_status)
    : status_(status),
      progress_(progress),
      token_(std::move(token)),
      is_original_(is_original),
      is_final_status_(is_final_status) {}

TransferMetadata::~TransferMetadata() = default;

TransferMetadata::TransferMetadata(const TransferMetadata&) = default;

TransferMetadata& TransferMetadata::operator=(const TransferMetadata&) =
    default;
