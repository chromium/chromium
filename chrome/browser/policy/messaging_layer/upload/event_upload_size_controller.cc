// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"

#include <algorithm>
#include <limits>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "event_upload_size_controller.h"
#include "file_upload_impl.h"

namespace reporting {

EventUploadSizeController::EventUploadSizeController(
    const NetworkConditionService& network_condition_service,
    uint64_t new_events_rate,
    uint64_t remaining_storage_capacity,
    uint64_t max_file_upload_buffer_size)
    : new_events_rate_(new_events_rate > 0 ? new_events_rate : 1),
      remaining_storage_capacity_(remaining_storage_capacity),
      max_file_upload_buffer_size_(max_file_upload_buffer_size),
      max_upload_size_(ComputeMaxUploadSize(network_condition_service)) {
  base::UmaHistogramCounts10000(
      "Browser.ERP.EventUploadSizeAdjustment.NewEventsRate", new_events_rate_);
  base::UmaHistogramCustomCounts(
      "Browser.ERP.EventUploadSizeAdjustment.RemainingStorageCapacity",
      /*sample=*/remaining_storage_capacity_,
      /*min=*/1,
      /*exclusive_max=*/80'000'000,
      /*buckets=*/50);
  base::UmaHistogramCounts10M(
      "Browser.ERP.EventUploadSizeAdjustment.MaxUploadSize", max_upload_size_);
}

bool EventUploadSizeController::IsMaximumUploadSizeReached() const {
  return Enabler::Get() && uploaded_size_ >= max_upload_size_;
}

void EventUploadSizeController::AccountForRecord(
    const EncryptedRecord& record) {
  AccountForData(record.ByteSizeLong());
}

void EventUploadSizeController::AccountForData(size_t size) {
  uploaded_size_ += size;
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
std::vector<EncryptedRecord> EventUploadSizeController::BuildEncryptedRecords(
    google::protobuf::RepeatedPtrField<EncryptedRecord> encrypted_records,
    EventUploadSizeController&& controller) {
  std::vector<EncryptedRecord> records;
  for (auto& record : encrypted_records) {
    // Check if we have uploaded enough records after adding each record
    controller.AccountForRecord(record);
    if (record.has_record_copy()) {
      // Add potential maximum upload size.
      // Given that the records featuring upload are rare, this should not
      // significantly impact the capacity. Each event usually triggers the
      // buffer-size upload, so we reserve that much.
      controller.AccountForData(controller.max_file_upload_buffer_size_);
    }
    records.push_back(std::move(record));
    if (controller.IsMaximumUploadSizeReached()) {
      break;
    }
  }
  return records;
}

// Enabler implementation ------------------------------

// static
std::atomic<bool> EventUploadSizeController::Enabler::enabled_ = false;

// static
void EventUploadSizeController::Enabler::Set(bool enabled) {
  enabled_ = enabled;
}

// static
bool EventUploadSizeController::Enabler::Get() {
  return enabled_;
}
}  // namespace reporting
