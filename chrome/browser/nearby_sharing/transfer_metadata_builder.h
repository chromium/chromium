// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_
#define CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_

#include <optional>
#include <string>

#include "chrome/browser/nearby_sharing/transfer_metadata.h"

class TransferMetadataBuilder {
 public:
  static TransferMetadataBuilder Clone(const TransferMetadata& metadata);

  TransferMetadataBuilder();
  TransferMetadataBuilder(TransferMetadataBuilder&&);
  TransferMetadataBuilder& operator=(TransferMetadataBuilder&&);
  ~TransferMetadataBuilder();

  TransferMetadataBuilder& set_is_original(bool is_original);

  TransferMetadataBuilder& set_progress(double progress);

  TransferMetadataBuilder& set_status(TransferMetadata::Status status);

  TransferMetadataBuilder& set_token(std::optional<std::string> token);

  TransferMetadata build() const;

 private:
  bool is_original_ = false;
  double progress_ = 0;
  TransferMetadata::Status status_ = TransferMetadata::Status::kInProgress;
  std::optional<std::string> token_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_TRANSFER_METADATA_BUILDER_H_
