// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/event_upload_size_controller.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "base/test/protobuf_matchers.h"
#include "chrome/browser/policy/messaging_layer/upload/testing_network_condition_service.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::Matches;
using ::testing::SizeIs;

namespace reporting {
namespace {

class EqualsProtoVector {
 public:
  static ::testing::Matcher<const std::vector<EncryptedRecord>&> Matcher(
      const google::protobuf::RepeatedPtrField<EncryptedRecord>&
          expected_records,
      std::optional<size_t> expected_count = std::nullopt) {
    return EqualsProtoVector(
        expected_records, expected_count.has_value()
                              ? expected_count.value()
                              : static_cast<size_t>(expected_records.size()));
  }

  using is_gtest_matcher = void;

  bool MatchAndExplain(const std::vector<EncryptedRecord>& records,
                       std::ostream* /*listener*/) const {
    if (records.size() != expected_count_) {
      return false;
    }
    for (size_t i = 0; i < records.size(); ++i) {
      if (!Matches(EqualsProto(expected_records_[i]))(records[i])) {
        return false;
      }
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const { *os << "All protos match"; }

  void DescribeNegationTo(std::ostream* os) const { *os << "Protos mismatch"; }

 private:
  EqualsProtoVector(const google::protobuf::RepeatedPtrField<EncryptedRecord>&
                        expected_records,
                    size_t expected_count)
      : expected_records_(expected_records), expected_count_(expected_count) {}

  const google::protobuf::RepeatedPtrField<EncryptedRecord> expected_records_;
  const size_t expected_count_;
};
}  // namespace

// Testing |EventUploadSizeController|. Since a big portion of the class is
// simply applying a formula, there is no reason to repeat the formula here.
// This test focuses on the dynamic elements (such as |AccountForRecord|).
class EventUploadSizeControllerTest : public ::testing::Test {
 protected:
  // Computes the maximum number of records before |IsMaximumUploadSizeReached|
  // returns true:
  //    |IsMaximumUploadSizeReached| return false if < max_num_of_records
  //    records are accounted for.
  //    Otherwise, |IsMaximumUploadSizeReached| returns true.
  // record_reserve is the space needed for each record. That is, the size of
  // the record if record_copy is not present, or the size of the record + max
  // file upload buffer size if record_copy is present.
  static uint64_t ComputeMaxNumOfRecords(
      const EventUploadSizeController& controller,
      const NetworkConditionService& network_condition_service,
      uint64_t record_reserve) {
    // This number may change from time to time if we adapt the formula in the
    // future.
    const uint64_t max_upload_size =
        controller.ComputeMaxUploadSize(network_condition_service);
    LOG(INFO) << "The computed max upload size is " << max_upload_size;

    return std::max<uint64_t>(
        1u, (max_upload_size + record_reserve - 1u) / record_reserve);
  }

  void SetUp() override {
    // Default is disabled
    ASSERT_FALSE(EventUploadSizeController::Enabler::Get());

    // Enable for testing purposes
    EventUploadSizeController::Enabler::Set(true);
    ASSERT_TRUE(EventUploadSizeController::Enabler::Get());
  }

  void TearDown() override { EventUploadSizeController::Enabler::Set(false); }

  // Needed by NetworkConditionService.
  content::BrowserTaskEnvironment task_environment_;
};

// Although |EventUploadSizeController::AccountForRecord| and
// |EventUploadSizeController::IsMaximumUploadSizeReached| are private, the
// class is designed in a way that these two methods can be made public in the
// future without disruption. Hence, this test is kept here.
TEST_F(EventUploadSizeControllerTest, AccountForRecordAddUp) {
  TestingNetworkConditionService network_condition_service(&task_environment_);
  network_condition_service.SetUploadRate(10000);
  EventUploadSizeController event_upload_size_controller(
      network_condition_service,
      /*new_events_rate=*/1U,
      /*remaining_storage_capacity=*/std::numeric_limits<uint64_t>::max(),
      /*max_file_upload_buffer_size=*/1024UL);
  EncryptedRecord record;
  record.set_encrypted_wrapped_record(std::string(100, 'A'));
  const auto record_size = record.ByteSizeLong();
  const uint64_t max_num_of_records = ComputeMaxNumOfRecords(
      event_upload_size_controller, network_condition_service, record_size);

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

  // If disabled, |IsMaximumUploadSizeReached| returns false.
  EventUploadSizeController::Enabler::Set(false);
  ASSERT_FALSE(EventUploadSizeController::Enabler::Get());
  ASSERT_FALSE(event_upload_size_controller.IsMaximumUploadSizeReached());
}

TEST_F(EventUploadSizeControllerTest, AccountForFileUpload) {
  TestingNetworkConditionService network_condition_service(&task_environment_);
  network_condition_service.SetUploadRate(10000);
  EventUploadSizeController event_upload_size_controller(
      network_condition_service,
      /*new_events_rate=*/1U,
      /*remaining_storage_capacity=*/std::numeric_limits<uint64_t>::max(),
      /*max_file_upload_buffer_size=*/1024UL);
  // This number may change from time to time if we adapt the formula in the
  // future.
  const uint64_t max_upload_size =
      event_upload_size_controller.ComputeMaxUploadSize(
          network_condition_service);
  LOG(INFO) << "The computed max upload size is " << max_upload_size;

  // Add this single record, accounting for the max data buffer,
  // make sure |IsMaximumUploadSizeReached| gives the correct answer.
  ASSERT_FALSE(event_upload_size_controller.IsMaximumUploadSizeReached());
  event_upload_size_controller.AccountForData(/*size=*/max_upload_size);
  ASSERT_TRUE(event_upload_size_controller.IsMaximumUploadSizeReached())
      << "The maximum upload size is not reached when record with copy must "
         "have been accounted for.";

  // If disabled, |IsMaximumUploadSizeReached| returns false.
  EventUploadSizeController::Enabler::Set(false);
  ASSERT_FALSE(EventUploadSizeController::Enabler::Get());
  ASSERT_FALSE(event_upload_size_controller.IsMaximumUploadSizeReached());
}

// Tests BuildEncryptedRecords when all input are returned.
TEST_F(EventUploadSizeControllerTest, BuildEncryptedRecordsAll) {
  TestingNetworkConditionService network_condition_service(&task_environment_);

  google::protobuf::RepeatedPtrField<EncryptedRecord> encrypted_records;
  encrypted_records.Add()->set_encrypted_wrapped_record("abc");
  encrypted_records.Add()->set_encrypted_wrapped_record("de");
  encrypted_records.Add()->set_encrypted_wrapped_record("fghi");

  auto matcher = EqualsProtoVector::Matcher(encrypted_records);

  const auto results = EventUploadSizeController::BuildEncryptedRecords(
      std::move(encrypted_records),
      // This controller would let all records pass.
      ::reporting::EventUploadSizeController(
          network_condition_service, /*new_events_rate=*/1u,
          /*remaining_storage_capacity=*/std::numeric_limits<uint64_t>::max(),
          /*max_file_upload_buffer_size=*/0U));

  EXPECT_THAT(results, matcher);
}

// Tests BuildEncryptedRecords when only part of the input are returned.
TEST_F(EventUploadSizeControllerTest, BuildEncryptedRecordsPart) {
  static constexpr size_t kNumOfRecords = 50u;
  TestingNetworkConditionService network_condition_service(&task_environment_);
  network_condition_service.SetUploadRate(10);
  EventUploadSizeController event_upload_size_controller(
      network_condition_service, /*new_events_rate=*/2u,
      /*remaining_storage_capacity=*/std::numeric_limits<uint64_t>::max(),
      // The following is not taking effects because no record has record_copy.
      /*max_file_upload_buffer_size=*/std::numeric_limits<uint64_t>::max());

  google::protobuf::RepeatedPtrField<EncryptedRecord> encrypted_records;
  for (size_t i = 0; i < kNumOfRecords; ++i) {
    encrypted_records.Add()->set_encrypted_wrapped_record(
        std::string(100, 'a'));
  }

  const uint64_t expected_num_of_records = ComputeMaxNumOfRecords(
      event_upload_size_controller, network_condition_service,
      encrypted_records[0].ByteSizeLong());

  // Ensure we are dealing with a range of number of records that we would like
  // to test.
  ASSERT_GE(expected_num_of_records, 2u);
  ASSERT_LT(expected_num_of_records, kNumOfRecords);

  auto matcher =
      EqualsProtoVector::Matcher(encrypted_records, expected_num_of_records);

  const auto results = EventUploadSizeController::BuildEncryptedRecords(
      std::move(encrypted_records), std::move(event_upload_size_controller));

  EXPECT_THAT(results, matcher);
}

// Tests BuildEncryptedRecords when only part of the input are returned and
// record_copy is present (thus upload buffer is used).
TEST_F(EventUploadSizeControllerTest, BuildEncryptedRecordsPartWithRecordCopy) {
  static constexpr size_t kNumOfRecords = 50u;
  static constexpr size_t kMaxFileUploadBufferSize = 512u;

  TestingNetworkConditionService network_condition_service(&task_environment_);
  network_condition_service.SetUploadRate(10);
  EventUploadSizeController event_upload_size_controller(
      network_condition_service, /*new_events_rate=*/2u,
      /*remaining_storage_capacity=*/std::numeric_limits<uint64_t>::max(),
      /*max_file_upload_buffer_size=*/kMaxFileUploadBufferSize);

  google::protobuf::RepeatedPtrField<EncryptedRecord> encrypted_records;
  for (size_t i = 0; i < kNumOfRecords; ++i) {
    auto* record = encrypted_records.Add();
    record->set_encrypted_wrapped_record(std::string(100, 'a'));
    *record->mutable_record_copy() = Record();
  }

  const uint64_t expected_num_of_records = ComputeMaxNumOfRecords(
      event_upload_size_controller, network_condition_service,
      encrypted_records[0].ByteSizeLong() + kMaxFileUploadBufferSize);

  // Ensure we are dealing with a range of number of records that we would like
  // to test.
  ASSERT_GE(expected_num_of_records, 2u);
  ASSERT_LT(expected_num_of_records, kNumOfRecords);
  // Ensure that we are testing a different expected number of records from
  // when file upload buffer is not used. This is to prevent this test from
  // passing even if max file upload buffer size is not taken into account.
  ASSERT_NE(expected_num_of_records,
            ComputeMaxNumOfRecords(event_upload_size_controller,
                                   network_condition_service,
                                   encrypted_records[0].ByteSizeLong()));

  auto matcher =
      EqualsProtoVector::Matcher(encrypted_records, expected_num_of_records);

  const auto results = EventUploadSizeController::BuildEncryptedRecords(
      std::move(encrypted_records), std::move(event_upload_size_controller));

  EXPECT_THAT(results, matcher);
}
}  // namespace reporting
