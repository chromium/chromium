// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/remote_trust_token_query_answerer.h"

#include "content/public/browser/storage_partition.h"

RemoteTrustTokenQueryAnswerer::RemoteTrustTokenQueryAnswerer(
    url::Origin top_frame_origin,
    Profile* profile)
    : top_frame_origin_(std::move(top_frame_origin)), profile_(profile) {}

RemoteTrustTokenQueryAnswerer::~RemoteTrustTokenQueryAnswerer() = default;

void RemoteTrustTokenQueryAnswerer::HasTrustTokens(
    const url::Origin& issuer,
    HasTrustTokensCallback callback) {
  if (!cached_answerer_ || !cached_answerer_.is_connected()) {
    UpdateCachedAnswerer();
  }
  // TODO(behamilton): If the network service crashes while this request
  // has been queued the callback will never be called.
  return cached_answerer_->HasTrustTokens(issuer, std::move(callback));
}

void RemoteTrustTokenQueryAnswerer::HasRedemptionRecord(
    const url::Origin& issuer,
    HasRedemptionRecordCallback callback) {
  if (!cached_answerer_ || !cached_answerer_.is_connected()) {
    UpdateCachedAnswerer();
  }
  // TODO(behamilton): If the network service crashes while this request
  // has been queued the callback will never be called.
  return cached_answerer_->HasRedemptionRecord(issuer, std::move(callback));
}

void RemoteTrustTokenQueryAnswerer::UpdateCachedAnswerer() {
  cached_answerer_.reset();
  profile_->GetDefaultStoragePartition()->CreateTrustTokenQueryAnswerer(
      cached_answerer_.BindNewPipeAndPassReceiver(), top_frame_origin_);
}
