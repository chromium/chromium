// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_EVENT_UPLOAD_SIZE_CONTROLLER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_EVENT_UPLOAD_SIZE_CONTROLLER_H_

#include <atomic>
#include <cstddef>
#include <vector>

#include "base/gtest_prod_util.h"
#include "chrome/browser/policy/messaging_layer/upload/network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"

namespace reporting {

// Control how large upload size is and whether it is OK to continue uploading
// after a specified set of records has been uploaded.
class EventUploadSizeController {
 public:
  // Manage the global setting of whether upload size adjustment should be
  // enabled. If disabled, |IsMaximumUploadSizeReached| would always return
  // false.
  class Enabler {
   public:
    Enabler() = delete;
    // Sets whether the upload size adjustment should be enabled.
    static void Set(bool enabled);
    // Gets whether the upload size adjustment should be enabled.
    static bool Get();

   private:
    static std::atomic<bool> enabled_;
  };

  EventUploadSizeController(
      const NetworkConditionService& network_condition_service,
      uint64_t new_events_rate,
      uint64_t remaining_storage_capacity,
      uint64_t max_file_upload_buffer_size);

  // Build the vector of encrypted records based on the records in the upload
  // request. Event upload size is adjusted.
  [[nodiscard]] static std::vector<EncryptedRecord> BuildEncryptedRecords(
      google::protobuf::RepeatedPtrField<EncryptedRecord> encrypted_records,
      EventUploadSizeController&& controller);

 private:
  friend class EventUploadSizeControllerTest;
  FRIEND_TEST_ALL_PREFIXES(EventUploadSizeControllerTest,
                           AccountForRecordAddUp);
  FRIEND_TEST_ALL_PREFIXES(EventUploadSizeControllerTest, AccountForFileUpload);

  // The maximum time in seconds during which a single connection should
  // remain open, a constant set heuristically.
  static constexpr uint64_t kTimeCeiling = 60;
  // Size of overheads in each upload in bytes. Heuristically set by a human.
  static constexpr uint64_t kOverhead = 32;

  // Estimates upload rate (bytes/sec).
  static uint64_t GetUploadRate(
      const NetworkConditionService& network_condition_service);

  // Estimates the rate at which new events are coming in (bytes/sec). This
  // estimate is informed by missive via dbus.
  uint64_t GetNewEventsRate() const;
  // Estimates the remaining storage capacity (bytes) here. This estimate is
  // informed by missive via dbus.
  uint64_t GetRemainingStorageCapacity() const;
  // Computes the maximum upload size.
  [[nodiscard]] uint64_t ComputeMaxUploadSize(
      const NetworkConditionService& network_condition_service) const;
  // Has the set maximum upload size been reached? Always returns false if
  // adjustment based on network condition is not enabled.
  bool IsMaximumUploadSizeReached() const;
  // Bumps up by the size of the record to be uploaded.
  void AccountForRecord(const EncryptedRecord& record);
  // Bumpls up by the size of the data chunk to be uploaded;
  void AccountForData(size_t size);

  // The rate (bytes per seconds) at which new events are accepted by missive.
  const uint64_t new_events_rate_;
  // How much local storage is left as informed by missive.
  const uint64_t remaining_storage_capacity_;
  // maximum file upl
  const uint64_t max_file_upload_buffer_size_;
  // maximum upload size.
  const uint64_t max_upload_size_;
  // Already uploaded size.
  uint64_t uploaded_size_ = 0;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_EVENT_UPLOAD_SIZE_CONTROLLER_H_
