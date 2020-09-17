// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/payload_tracker.h"

#include "base/callback.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"

PayloadTracker::PayloadTracker(
    const ShareTarget& share_target,
    const base::flat_map<int64_t, AttachmentInfo>& attachment_info_map,
    base::RepeatingCallback<void(ShareTarget, TransferMetadata)>
        update_callback)
    : share_target_(share_target),
      update_callback_(std::move(update_callback)) {
  total_download_size_ = 0;

  for (const auto& file : share_target.file_attachments) {
    auto it = attachment_info_map.find(file.id());
    if (it == attachment_info_map.end() || !it->second.payload_id) {
      NS_LOG(WARNING)
          << __func__
          << ": Failed to retrieve payload for file attachment id - "
          << file.id();
      continue;
    }

    payload_state_.emplace(*it->second.payload_id, State(file.size()));
    total_download_size_ += file.size();
  }

  for (const auto& text : share_target.text_attachments) {
    auto it = attachment_info_map.find(text.id());
    if (it == attachment_info_map.end() || !it->second.payload_id) {
      NS_LOG(WARNING)
          << __func__
          << ": Failed to retrieve payload for text  attachment id - "
          << text.id();
      continue;
    }

    payload_state_.emplace(*it->second.payload_id, State(text.size()));
    total_download_size_ += text.size();
  }
}

PayloadTracker::~PayloadTracker() = default;

void PayloadTracker::OnStatusUpdate(PayloadTransferUpdatePtr update) {
  auto it = payload_state_.find(update->payload_id);
  if (it == payload_state_.end())
    return;

  it->second.amount_downloaded = update->bytes_transferred;
  if (it->second.status != update->status) {
    it->second.status = update->status;

    NS_LOG(VERBOSE) << __func__ << ": Payload id " << update->payload_id
                    << " had status change: " << update->status;
  }
  OnTransferUpdate();
}

void PayloadTracker::OnTransferUpdate() {
  if (IsComplete()) {
    NS_LOG(VERBOSE) << __func__ << ": All payloads are complete.";
    update_callback_.Run(share_target_,
                         TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kComplete)
                             .set_progress(100)
                             .build());
    return;
  }

  if (IsCancelled()) {
    NS_LOG(VERBOSE) << __func__ << ": Payloads cancelled.";
    update_callback_.Run(share_target_,
                         TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kCancelled)
                             .build());
    return;
  }

  if (HasFailed()) {
    NS_LOG(VERBOSE) << __func__ << ": Payloads failed.";
    update_callback_.Run(share_target_,
                         TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kFailed)
                             .build());
    return;
  }

  double percent = CalculateProgressPercent();
  int current_progress = static_cast<int>(percent * 100);
  base::Time current_time = base::Time::Now();

  if (current_progress == last_update_progress_ ||
      (current_time - last_update_timestamp_) < kMinProgressUpdateFrequency) {
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Payloads are in progress at " << percent
                  << " percent.";
  last_update_progress_ = current_progress;
  last_update_timestamp_ = current_time;

  update_callback_.Run(share_target_,
                       TransferMetadataBuilder()
                           .set_status(TransferMetadata::Status::kInProgress)
                           .set_progress(percent)
                           .build());
}

bool PayloadTracker::IsComplete() {
  for (const auto& state : payload_state_) {
    if (state.second.status !=
        location::nearby::connections::mojom::PayloadStatus::kSuccess) {
      return false;
    }
  }
  return true;
}

bool PayloadTracker::IsCancelled() {
  for (const auto& state : payload_state_) {
    if (state.second.status ==
        location::nearby::connections::mojom::PayloadStatus::kCanceled) {
      return true;
    }
  }
  return false;
}

bool PayloadTracker::HasFailed() {
  for (const auto& state : payload_state_) {
    if (state.second.status ==
        location::nearby::connections::mojom::PayloadStatus::kFailure) {
      return true;
    }
  }
  return false;
}

double PayloadTracker::CalculateProgressPercent() {
  if (!total_download_size_) {
    NS_LOG(WARNING) << __func__ << ": Total attachment size is 0";
    return 100.0;
  }

  uint64_t total_downloaded = 0;
  for (const auto& state : payload_state_)
    total_downloaded += state.second.amount_downloaded;

  return (100.0 * total_downloaded) / total_download_size_;
}
