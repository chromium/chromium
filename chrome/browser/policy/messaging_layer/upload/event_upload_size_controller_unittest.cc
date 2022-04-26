// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"
#include "chrome/browser/policy/messaging_layer/upload/testing_network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "content/public/test/browser_task_environment.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

// Testing |EventUploadSizeController|. Since a big portion of the class is
// simply applying a formula, there is no reason to repeat the formula here.
// This test focuses on the dynamic elements (such as |AccountForRecord|).
class EventUploadSizeControllerTest : public ::testing::Test {
 protected:
  // Needed by NetworkConditionService.
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EventUploadSizeControllerTest, AccountForRecordAddUp) {
  TestingNetworkConditionService network_condition_service(&task_environment_);
  network_condition_service.SetUploadRate(10000);
  EventUploadSizeController event_upload_size_controller(
      network_condition_service, /*enabled=*/true);
  // This number may change from time to time if we adapt the formula in the
  // future.
  const uint64_t max_upload_size =
      EventUploadSizeController::ComputeMaxUploadSize(
          network_condition_service);
  LOG(INFO) << "The computed max upload size is " << max_upload_size;

  EncryptedRecord record;
  record.set_encrypted_wrapped_record(std::string(100, 'A'));
  const auto record_size = record.ByteSizeLong();
  // Maximum number of records of size of "record" before
  // |IsMaximumUploadSizeReached| returns true:
  //    |IsMaximumUploadSizeReached| return false if < max_num_of_records
  //    |records are accounted for.
  //    Otherwise, |IsMaximumUploadSizeReached| returns true.
  const uint64_t max_num_of_records =
      max_upload_size / record_size + (max_upload_size % record_size != 0);
  // A sanity check. Because the formula may change from time to time, the
  // following is satisfied for the thoroughness of this test.
  ASSERT_GE(max_num_of_records, 2U)
      << "The test is tuned to only allow less than 2 records before the "
         "maximum upload size is reached. However, the parameter must be set "
         "so that the maximum upload size is reached after no less than 2 "
         "records is uploaded for the throughness of this test.";
  // Add each record at a time, make sure |IsMaximumUploadSizeReached| gives the
  // correct answer.
  for (uint64_t i = 0; i < max_num_of_records; ++i) {
    ASSERT_FALSE(event_upload_size_controller.IsMaximumUploadSizeReached())
        << "The maximum upload size is reached when " << i
        << " records of size " << record_size
        << " have been accounted for, less than " << max_num_of_records
        << " records.";
    event_upload_size_controller.AccountForRecord(record);
  }
  ASSERT_TRUE(event_upload_size_controller.IsMaximumUploadSizeReached())
      << "The maximum upload size is not reached when " << max_num_of_records
      << " records of size " << record_size << " have been accounted for.";
}

}  // namespace reporting
