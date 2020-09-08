// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

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
