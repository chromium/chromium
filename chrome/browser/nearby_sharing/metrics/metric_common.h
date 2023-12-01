// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_
#define CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_

#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"

namespace nearby::share::metrics {

// For now, we just forward the mojom interface until we can differentiate
// between Windows and ChromeOS.
using Platform = nearby_share::mojom::ShareTargetType;

enum DeviceRelationship {
  kSelf = 0,
  kContact = 1,
  kStranger = 2,
};

// This has significant overlap with `TransferMetadata::Status`, but it only
// captures terminal states.
enum TransferResult {
  kUnknown = 0,
  kComplete = 1,
  kFailed = 2,
  kCancelled = 3,
  kRejected = 4,
  kTimedOut = 5,
  kNotEnoughSpace = 6,
  kUnsupportedAttachmentType = 7,
  kMissingPayloads = 8,
  kIncompletePayloads = 9,
  kUnexpectedDisconnection = 10,
  kFailedToInitiateOutgoingConnection = 11,
  kFailedToReadOutgoingConnectionResponse = 12,
  kInvalidIntroductionFrame = 13,
  kPairedKeyVerificationFailed = 14,
  kMediaUnavailable = 15,
};

Platform GetPlatform(const ShareTarget& share_target);
DeviceRelationship GetDeviceRelationship(const ShareTarget& share_target);
TransferResult GetTransferResult(TransferMetadata::Status status);

}  // namespace nearby::share::metrics

#endif  // CHROME_BROWSER_NEARBY_SHARING_METRICS_METRIC_COMMON_H_
