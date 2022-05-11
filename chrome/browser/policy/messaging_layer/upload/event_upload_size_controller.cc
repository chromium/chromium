// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"

#include <algorithm>
#include <limits>

#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"

namespace reporting {

EventUploadSizeController::EventUploadSizeController(
    const NetworkConditionService& network_condition_service,
    bool enabled)
    : enabled_(enabled),
      max_upload_size_(ComputeMaxUploadSize(network_condition_service)) {}

bool EventUploadSizeController::IsMaximumUploadSizeReached() const {
  return enabled_ && uploaded_size_ >= max_upload_size_;
}

void EventUploadSizeController::AccountForRecord(
    const EncryptedRecord& record) {
  uploaded_size_ += record.ByteSizeLong();
}

uint64_t EventUploadSizeController::GetUploadRate(
    const NetworkConditionService& network_condition_service) {
  return std::min(network_condition_service.GetUploadRate(),
                  // This ensures ComputeMaxUploadSize won't overflow
                  std::numeric_limits<uint64_t>::max() / kTimeCeiling);
}

uint64_t EventUploadSizeController::GetNewEventRate() {
  return 1UL;
}

uint64_t EventUploadSizeController::GetRemainingStorageCapacity() {
  return std::numeric_limits<uint64_t>::max();
}

uint64_t EventUploadSizeController::ComputeMaxUploadSize(
    const NetworkConditionService& network_condition_service) {
  // Estimated acceptable time that a single connection can remain open.
  const uint64_t time_open =
      std::min(GetRemainingStorageCapacity() / GetNewEventRate(), kTimeCeiling);
  return GetUploadRate(network_condition_service) * time_open - kOverhead;
}
}  // namespace reporting
