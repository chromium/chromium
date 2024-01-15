// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"

// static
TransferMetadataBuilder TransferMetadataBuilder::Clone(
    const TransferMetadata& metadata) {
  TransferMetadataBuilder builder;
  builder.is_original_ = metadata.is_original();
  builder.progress_ = metadata.progress();
  builder.status_ = metadata.status();
  builder.token_ = metadata.token();
  return builder;
}

TransferMetadataBuilder::TransferMetadataBuilder() = default;

TransferMetadataBuilder::TransferMetadataBuilder(TransferMetadataBuilder&&) =
    default;

TransferMetadataBuilder& TransferMetadataBuilder::operator=(
    TransferMetadataBuilder&&) = default;

TransferMetadataBuilder::~TransferMetadataBuilder() = default;

TransferMetadataBuilder& TransferMetadataBuilder::set_is_original(
    bool is_original) {
  is_original_ = is_original;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_progress(
    double progress) {
  progress_ = progress;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_status(
    TransferMetadata::Status status) {
  status_ = status;
  return *this;
}

TransferMetadataBuilder& TransferMetadataBuilder::set_token(
    std::optional<std::string> token) {
  token_ = std::move(token);
  return *this;
}

TransferMetadata TransferMetadataBuilder::build() const {
  return TransferMetadata(status_, progress_, token_, is_original_,
                          TransferMetadata::IsFinalStatus(status_));
}
