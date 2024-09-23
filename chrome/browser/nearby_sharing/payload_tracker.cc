// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/payload_tracker.h"

#include "base/functional/callback.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/cross_device/logging/logging.h"

PayloadTracker::PayloadTracker(
    const ShareTarget& share_target,
    const base::flat_map<int64_t, AttachmentInfo>& attachment_info_map,
    base::RepeatingCallback<void(ShareTarget, TransferMetadata)>
        update_callback)
    : share_target_(share_target),
      update_callback_(std::move(update_callback)) {
  total_transfer_size_ = 0;

  for (const auto& file : share_target.file_attachments) {
    auto it = attachment_info_map.find(file.id());
    if (it == attachment_info_map.end() || !it->second.payload_id) {
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Failed to retrieve payload for file attachment id - "
          << file.id();
      continue;
    }

    payload_state_.emplace(*it->second.payload_id, State(file.size()));
    ++num_file_attachments_;
    total_transfer_size_ += file.size();
  }

  for (const auto& text : share_target.text_attachments) {
    auto it = attachment_info_map.find(text.id());
    if (it == attachment_info_map.end() || !it->second.payload_id) {
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Failed to retrieve payload for text attachment id - "
          << text.id();
      continue;
    }

    payload_state_.emplace(*it->second.payload_id, State(text.size()));
    ++num_text_attachments_;
    total_transfer_size_ += text.size();
  }

  for (const auto& wifi_credentials :
       share_target.wifi_credentials_attachments) {
    auto it = attachment_info_map.find(wifi_credentials.id());
    if (it == attachment_info_map.end() || !it->second.payload_id) {
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Failed to retrieve payload for Wi-Fi Credentials "
          << "attachment id - " << wifi_credentials.id();
      continue;
    }

    payload_state_.emplace(*it->second.payload_id, State(0));
    ++num_wifi_credentials_attachments_;
    total_transfer_size_ += wifi_credentials.size();
  }
}

PayloadTracker::~PayloadTracker() = default;

void PayloadTracker::OnStatusUpdate(PayloadTransferUpdatePtr update,
                                    std::optional<Medium> upgraded_medium) {
  auto it = payload_state_.find(update->payload_id);
  if (it == payload_state_.end())
    return;

  // For metrics.
  if (!first_update_timestamp_.has_value()) {
    first_update_timestamp_ = base::TimeTicks::Now();
    num_first_update_bytes_ = update->bytes_transferred;
  }
  if (upgraded_medium.has_value()) {
    last_upgraded_medium_ = upgraded_medium;
  }

  if (it->second.status != update->status) {
    it->second.status = update->status;

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Payload id " << update->payload_id
        << " had status change: " << update->status;
  }

  // The number of bytes transferred should never go down. That said, some
  // status updates like cancellation might send a value of 0. In that case, we
  // retain the last known value for use in metrics.
  if (update->bytes_transferred > it->second.amount_transferred) {
    it->second.amount_transferred = update->bytes_transferred;
  }

  OnTransferUpdate();
}

void PayloadTracker::OnTransferUpdate() {
  const double percent = CalculateProgressPercent();
  if (IsComplete()) {
    const bool is_transfer_complete =
        GetTotalTransferred() >= total_transfer_size_;
    if (is_transfer_complete) {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": All payloads are complete.";
      EmitFinalMetrics(nearby::connections::mojom::PayloadStatus::kSuccess);
      update_callback_.Run(share_target_,
                           TransferMetadataBuilder()
                               .set_status(TransferMetadata::Status::kComplete)
                               .set_progress(100)
                               .build());
      return;
    }

    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Payloads incomplete.";
    EmitFinalMetrics(nearby::connections::mojom::PayloadStatus::kFailure);
    update_callback_.Run(
        share_target_,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kIncompletePayloads)
            .set_progress(percent)
            .build());

    return;
  }

  if (IsCancelled()) {
    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Payloads cancelled.";
    EmitFinalMetrics(nearby::connections::mojom::PayloadStatus::kCanceled);
    update_callback_.Run(share_target_,
                         TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kCancelled)
                             .build());
    return;
  }

  if (HasFailed()) {
    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Payloads failed.";
    EmitFinalMetrics(nearby::connections::mojom::PayloadStatus::kFailure);
    update_callback_.Run(share_target_,
                         TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kFailed)
                             .build());
    return;
  }

  const int current_progress = static_cast<int>(percent * 100);
  base::Time current_time = base::Time::Now();

  if (current_progress == last_update_progress_ ||
      (current_time - last_update_timestamp_) < kMinProgressUpdateFrequency) {
    return;
  }

  last_update_progress_ = current_progress;
  last_update_timestamp_ = current_time;

  update_callback_.Run(share_target_,
                       TransferMetadataBuilder()
                           .set_status(TransferMetadata::Status::kInProgress)
                           .set_progress(percent)
                           .build());
}

bool PayloadTracker::IsComplete() const {
  for (const auto& state : payload_state_) {
    if (state.second.status !=
        nearby::connections::mojom::PayloadStatus::kSuccess) {
      return false;
    }
  }
  return true;
}

bool PayloadTracker::IsCancelled() const {
  for (const auto& state : payload_state_) {
    if (state.second.status ==
        nearby::connections::mojom::PayloadStatus::kCanceled) {
      return true;
    }
  }
  return false;
}

bool PayloadTracker::HasFailed() const {
  for (const auto& state : payload_state_) {
    if (state.second.status ==
        nearby::connections::mojom::PayloadStatus::kFailure) {
      return true;
    }
  }
  return false;
}

uint64_t PayloadTracker::GetTotalTransferred() const {
  uint64_t total_transferred = 0;
  for (const auto& state : payload_state_)
    total_transferred += state.second.amount_transferred;

  return total_transferred;
}

double PayloadTracker::CalculateProgressPercent() const {
  if (!total_transfer_size_) {
    CD_LOG(WARNING, Feature::NS) << __func__ << ": Total attachment size is 0";
    return 100.0;
  }

  return (100.0 * GetTotalTransferred()) / total_transfer_size_;
}

void PayloadTracker::EmitFinalMetrics(
    nearby::connections::mojom::PayloadStatus status) const {
  DCHECK_NE(status, nearby::connections::mojom::PayloadStatus::kInProgress);
  RecordNearbySharePayloadFinalStatusMetric(status, last_upgraded_medium_);
  RecordNearbySharePayloadMediumMetric(
      last_upgraded_medium_, share_target_.type, GetTotalTransferred());
  RecordNearbySharePayloadSizeMetric(share_target_.is_incoming,
                                     share_target_.type, last_upgraded_medium_,
                                     status, total_transfer_size_);
  RecordNearbySharePayloadNumAttachmentsMetric(
      num_text_attachments_, num_file_attachments_,
      num_wifi_credentials_attachments_);

  // Because we only start tracking after receiving the first status update,
  // subtract off that first transfer size.
  uint64_t transferred_bytes_with_offset =
      GetTotalTransferred() - num_first_update_bytes_;
  if (first_update_timestamp_ && transferred_bytes_with_offset > 0) {
    RecordNearbySharePayloadTransferRateMetric(
        share_target_.is_incoming, share_target_.type, last_upgraded_medium_,
        status, transferred_bytes_with_offset,
        base::TimeTicks::Now() - *first_update_timestamp_);
  }

  for (const auto& file_attachment : share_target_.file_attachments) {
    RecordNearbySharePayloadFileAttachmentTypeMetric(
        file_attachment.type(), share_target_.is_incoming,
        share_target_.is_known, share_target_.for_self_share, status);
  }

  for (const auto& text_attachment : share_target_.text_attachments) {
    RecordNearbySharePayloadTextAttachmentTypeMetric(
        text_attachment.type(), share_target_.is_incoming,
        share_target_.is_known, share_target_.for_self_share, status);
  }

  for (size_t i = 0; i < share_target_.wifi_credentials_attachments.size();
       ++i) {
    RecordNearbySharePayloadWifiCredentialsAttachmentTypeMetric(
        share_target_.is_incoming, share_target_.is_known,
        share_target_.for_self_share, status);
  }
}
