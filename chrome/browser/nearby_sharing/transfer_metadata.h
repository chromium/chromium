// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_

#include <optional>
#include <string>

#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "url/gurl.h"

// Metadata about an ongoing transfer. Wraps transient data like status and
// progress. This is used to refresh the UI with error messages and show
// notifications so additions should be explicitly handled on the frontend.
class TransferMetadata {
 public:
  enum class Status {
    kUnknown,
    kConnecting,
    kAwaitingLocalConfirmation,
    kAwaitingRemoteAcceptance,
    kAwaitingRemoteAcceptanceFailed,
    kInProgress,
    kComplete,
    kFailed,
    kRejected,
    kCancelled,
    kTimedOut,
    kMediaUnavailable,
    kMediaDownloading,
    kNotEnoughSpace,
    kUnsupportedAttachmentType,
    kExternalProviderLaunched,
    kDecodeAdvertisementFailed,
    kMissingTransferUpdateCallback,
    kMissingShareTarget,
    kMissingEndpointId,
    kMissingPayloads,
    kPairedKeyVerificationFailed,
    kInvalidIntroductionFrame,
    kIncompletePayloads,
    kFailedToCreateShareTarget,
    kFailedToInitiateOutgoingConnection,
    kFailedToReadOutgoingConnectionResponse,
    kUnexpectedDisconnection,
    kMaxValue = kUnexpectedDisconnection
  };

  enum class Result {
    kIndeterminate,
    kSuccess,
    kFailure,
    kMaxValue = kFailure
  };

  static bool IsFinalStatus(Status status);

  static Result ToResult(Status status);

  static std::string StatusToString(TransferMetadata::Status status);

  static nearby_share::mojom::TransferStatus StatusToMojo(Status status);

  TransferMetadata(Status status,
                   float progress,
                   std::optional<std::string> token,
                   bool is_original,
                   bool is_final_status);
  ~TransferMetadata();
  TransferMetadata(const TransferMetadata&);
  TransferMetadata& operator=(const TransferMetadata&);

  Status status() const { return status_; }

  // Returns transfer progress as percentage.
  float progress() const { return progress_; }

  // Represents the UKey2 token from Nearby Connection. std::nullopt if no
  // UKey2 comparison is needed for this transfer.
  const std::optional<std::string>& token() const { return token_; }

  // True if this |TransferMetadata| has not been seen.
  bool is_original() const { return is_original_; }

  // True if this |TransferMetadata| is the last status for this transfer.
  bool is_final_status() const { return is_final_status_; }

  nearby_share::mojom::TransferMetadataPtr ToMojo() const;

 private:
  Status status_;
  float progress_;
  std::optional<std::string> token_;
  bool is_original_;
  bool is_final_status_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_H_
