// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"

#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace reporting {
namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  if (expected_serialized != actual_serialized) {
    LOG(ERROR) << "Provided proto did not match the expected proto"
               << "\n Serialized Expected Proto: " << expected_serialized
               << "\n Serialized Provided Proto: " << actual_serialized;
    return false;
  }
  return true;
}

// CloudPolicyClient and UploadClient are not usable outside of a managed
// environment, to sidestep this we override the functions that normally build
// and retrieve these clients and provide a MockCloudPolicyClient and a
// FakeUploadClient.
class TestEncryptedReportingUploadProvider
    : public EncryptedReportingUploadProvider {
 public:
  explicit TestEncryptedReportingUploadProvider(
      UploadClient::ReportSuccessfulUploadCallback report_successful_upload_cb,
      UploadClient::EncryptionKeyAttachedCallback encryption_key_attached_cb,
      policy::CloudPolicyClient* cloud_policy_client)
      : EncryptedReportingUploadProvider(
            report_successful_upload_cb,
            encryption_key_attached_cb,
            /*build_cloud_policy_client_cb=*/
            base::BindRepeating(
                [](policy::CloudPolicyClient* cloud_policy_client,
                   CloudPolicyClientResultCb callback) {
                  std::move(callback).Run(cloud_policy_client);
                },
                base::Unretained(cloud_policy_client)),
            /*upload_client_builder_cb=*/
            base::BindRepeating(
                [](policy::CloudPolicyClient* cloud_policy_client,
                   reporting::UploadClient::CreatedCallback
                       update_upload_client_cb) {
                  reporting::FakeUploadClient::Create(
                      cloud_policy_client, std::move(update_upload_client_cb));
                })) {}
};

class EncryptedReportingUploadProviderTest : public ::testing::Test {
 public:
  MOCK_METHOD(void,
              ReportSuccessfulUpload,
              (reporting::SequenceInformation, bool),
              ());
  MOCK_METHOD(void,
              EncryptionKeyCallback,
              (reporting::SignedEncryptionInfo),
              ());

 protected:
  void SetUp() override {
    cloud_policy_client_.SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
    service_provider_ = std::make_unique<TestEncryptedReportingUploadProvider>(
        base::BindRepeating(
            &EncryptedReportingUploadProviderTest::ReportSuccessfulUpload,
            base::Unretained(this)),
        base::BindRepeating(
            &EncryptedReportingUploadProviderTest::EncryptionKeyCallback,
            base::Unretained(this)),
        &cloud_policy_client_);

    record_.set_encrypted_wrapped_record("TEST_DATA");

    auto* sequence_information = record_.mutable_sequence_information();
    sequence_information->set_sequencing_id(42);
    sequence_information->set_generation_id(1701);
    sequence_information->set_priority(reporting::Priority::SLOW_BATCH);
  }

  Status CallRequestUploadEncryptedRecord(
      bool need_encryption_key,
      std::unique_ptr<std::vector<EncryptedRecord>> records) {
    test::TestEvent<Status> result;
    service_provider_->RequestUploadEncryptedRecords(
        need_encryption_key, std::move(records), result.cb());
    return result.result();
  }

  // Must be initialized before any other class member.
  base::test::TaskEnvironment task_environment_;

  policy::MockCloudPolicyClient cloud_policy_client_;
  reporting::EncryptedRecord record_;

  std::unique_ptr<TestEncryptedReportingUploadProvider> service_provider_;
};

// Disabled due to flakiness. See crbug.com/1308890.
TEST_F(EncryptedReportingUploadProviderTest,
       DISABLED_SuccessfullyUploadsRecord) {
  test::TestMultiEvent<SequenceInformation, bool /*force*/> uploaded_event;
  EXPECT_CALL(*this, ReportSuccessfulUpload(_, _))
      .WillOnce([&uploaded_event](SequenceInformation seq_info, bool force) {
        std::move(uploaded_event.cb()).Run(std::move(seq_info), force);
      });
  EXPECT_CALL(cloud_policy_client_,
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(::reporting::MakeUploadEncryptedReportAction());

  auto records = std::make_unique<std::vector<EncryptedRecord>>();
  records->push_back(record_);
  const auto status = CallRequestUploadEncryptedRecord(
      /*need_encryption_key=*/false, std::move(records));
  EXPECT_OK(status) << status;
  auto uploaded_result = uploaded_event.result();
  EXPECT_THAT(std::get<0>(uploaded_result),
              EqualsProto(record_.sequence_information()));
  EXPECT_FALSE(std::get<1>(uploaded_result));  // !force
}

}  // namespace
}  // namespace reporting
