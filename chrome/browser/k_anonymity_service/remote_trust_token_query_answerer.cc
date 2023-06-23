// Copyright 2022 The Chromium Authors
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
  DCHECK(!pending_has_trust_tokens_request_);
  pending_has_trust_tokens_request_ = std::move(callback);
  if (!cached_answerer_ || !cached_answerer_.is_connected()) {
    UpdateCachedAnswerer();
  }
  cached_answerer_->HasTrustTokens(
      issuer,
      base::BindOnce(&RemoteTrustTokenQueryAnswerer::OnHasTrustTokensCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RemoteTrustTokenQueryAnswerer::OnHasTrustTokensCompleted(
    network::mojom::HasTrustTokensResultPtr result) {
  DCHECK(pending_has_trust_tokens_request_);
  HasTrustTokensCallback request_callback =
      std::move(pending_has_trust_tokens_request_.value());
  pending_has_trust_tokens_request_.reset();
  std::move(request_callback).Run(std::move(result));
}

void RemoteTrustTokenQueryAnswerer::HasRedemptionRecord(
    const url::Origin& issuer,
    HasRedemptionRecordCallback callback) {
  DCHECK(!pending_has_redemption_record_request_);
  pending_has_redemption_record_request_ = std::move(callback);
  if (!cached_answerer_ || !cached_answerer_.is_connected()) {
    UpdateCachedAnswerer();
  }
  // TODO(behamilton): If the network service crashes while this request
  // has been queued the callback will never be called.
  cached_answerer_->HasRedemptionRecord(
      issuer,
      base::BindOnce(
          &RemoteTrustTokenQueryAnswerer::OnHasRedemptionRecordCompleted,
          weak_ptr_factory_.GetWeakPtr()));
}

void RemoteTrustTokenQueryAnswerer::OnHasRedemptionRecordCompleted(
    network::mojom::HasRedemptionRecordResultPtr result) {
  DCHECK(pending_has_redemption_record_request_);
  HasRedemptionRecordCallback request_callback =
      std::move(pending_has_redemption_record_request_.value());
  pending_has_redemption_record_request_.reset();
  std::move(request_callback).Run(std::move(result));
}

void RemoteTrustTokenQueryAnswerer::OnDisconnect() {
  if (pending_has_trust_tokens_request_) {
    OnHasTrustTokensCompleted(nullptr);
  }
  if (pending_has_redemption_record_request_) {
    OnHasRedemptionRecordCompleted(nullptr);
  }
}

void RemoteTrustTokenQueryAnswerer::UpdateCachedAnswerer() {
  cached_answerer_.reset();
  profile_->GetDefaultStoragePartition()->CreateTrustTokenQueryAnswerer(
      cached_answerer_.BindNewPipeAndPassReceiver(), top_frame_origin_);
  cached_answerer_.set_disconnect_handler(
      base::BindOnce(&RemoteTrustTokenQueryAnswerer::OnDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
}
