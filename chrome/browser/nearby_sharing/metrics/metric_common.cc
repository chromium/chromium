// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/metrics/metric_common.h"

namespace nearby::share::metrics {

Platform GetPlatform(const ShareTarget& share_target) {
  return share_target.type;
}

DeviceRelationship GetDeviceRelationship(const ShareTarget& share_target) {
  if (share_target.for_self_share) {
    return DeviceRelationship::kSelf;
  } else if (share_target.is_known) {
    return DeviceRelationship::kContact;
  } else {
    return DeviceRelationship::kStranger;
  }
}

TransferResult GetTransferResult(TransferMetadata::Status status) {
  switch (status) {
    // These are non-terminal statuses.
    case TransferMetadata::Status::kUnknown:
    case TransferMetadata::Status::kConnecting:
    case TransferMetadata::Status::kAwaitingLocalConfirmation:
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
    case TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed:
    case TransferMetadata::Status::kMediaDownloading:
    case TransferMetadata::Status::kExternalProviderLaunched:
    case TransferMetadata::Status::kInProgress:
    // These are terminal, but not transfer related.
    case TransferMetadata::Status::kMissingTransferUpdateCallback:
    case TransferMetadata::Status::kMissingShareTarget:
    case TransferMetadata::Status::kMissingEndpointId:
    case TransferMetadata::Status::kFailedToCreateShareTarget:
    case TransferMetadata::Status::kDecodeAdvertisementFailed:
      return TransferResult::kUnknown;
    case TransferMetadata::Status::kComplete:
      return TransferResult::kComplete;
    case TransferMetadata::Status::kRejected:
      return TransferResult::kRejected;
    case TransferMetadata::Status::kCancelled:
      return TransferResult::kCancelled;
    case TransferMetadata::Status::kFailed:
      return TransferResult::kFailed;
    case TransferMetadata::Status::kTimedOut:
      return TransferResult::kTimedOut;
    case TransferMetadata::Status::kNotEnoughSpace:
      return TransferResult::kNotEnoughSpace;
    case TransferMetadata::Status::kUnsupportedAttachmentType:
      return TransferResult::kUnsupportedAttachmentType;
    case TransferMetadata::Status::kMissingPayloads:
      return TransferResult::kMissingPayloads;
    case TransferMetadata::Status::kIncompletePayloads:
      return TransferResult::kIncompletePayloads;
    case TransferMetadata::Status::kUnexpectedDisconnection:
      return TransferResult::kUnexpectedDisconnection;
    case TransferMetadata::Status::kFailedToInitiateOutgoingConnection:
      return TransferResult::kFailedToInitiateOutgoingConnection;
    case TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse:
      return TransferResult::kFailedToReadOutgoingConnectionResponse;
    case TransferMetadata::Status::kInvalidIntroductionFrame:
      return TransferResult::kInvalidIntroductionFrame;
    case TransferMetadata::Status::kPairedKeyVerificationFailed:
      return TransferResult::kPairedKeyVerificationFailed;
    case TransferMetadata::Status::kMediaUnavailable:
      return TransferResult::kMediaUnavailable;
  }
}

}  // namespace nearby::share::metrics
