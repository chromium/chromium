// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"

namespace reporting {

EventUploadSizeController::EventUploadSizeController(
    const NetworkConditionService& network_condition_service,
    uint64_t new_events_rate,
    uint64_t remaining_storage_capacity,
    bool enabled)
    : enabled_(enabled),
      new_events_rate_(new_events_rate > 0 ? new_events_rate : 1),
      remaining_storage_capacity_(remaining_storage_capacity),
      max_upload_size_(ComputeMaxUploadSize(network_condition_service)) {}

bool EventUploadSizeController::IsMaximumUploadSizeReached() const {
  return enabled_ && uploaded_size_ >= max_upload_size_;
}

void EventUploadSizeController::AccountForRecord(
    const EncryptedRecord& record) {
  uploaded_size_ += record.ByteSizeLong();
}

// static
uint64_t EventUploadSizeController::GetUploadRate(
    const NetworkConditionService& network_condition_service) {
  return std::min(network_condition_service.GetUploadRate(),
                  // This ensures ComputeMaxUploadSize won't overflow
                  std::numeric_limits<uint64_t>::max() / kTimeCeiling);
}

uint64_t EventUploadSizeController::GetNewEventsRate() const {
  return new_events_rate_;
}

uint64_t EventUploadSizeController::GetRemainingStorageCapacity() const {
  return remaining_storage_capacity_;
}

uint64_t EventUploadSizeController::ComputeMaxUploadSize(
    const NetworkConditionService& network_condition_service) const {
  // Estimated acceptable time that a single connection can remain open.
  const uint64_t time_open = std::min(
      GetRemainingStorageCapacity() / GetNewEventsRate(), kTimeCeiling);
  // Must always at least upload some decent amount.
  return std::max<uint64_t>(
      2048U, GetUploadRate(network_condition_service) * time_open - kOverhead);
}

// static
std::vector<reporting::EncryptedRecord>
EventUploadSizeController::BuildEncryptedRecords(
    const google::protobuf::RepeatedPtrField<EncryptedRecord>&
        encrypted_records,
    EventUploadSizeController&& controller) {
  std::vector<reporting::EncryptedRecord> records;
  for (auto& record : encrypted_records) {
    // Check if we have uploaded enough records after adding each record
    controller.AccountForRecord(record);
    records.push_back(std::move(record));
    if (controller.IsMaximumUploadSizeReached()) {
      break;
    }
  }
  return records;
}
}  // namespace reporting
