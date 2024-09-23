// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/policy/messaging_layer/upload/upload_provider.h"

#include <list>
#include <memory>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::SizeIs;
using ::testing::WithArgs;

namespace reporting {
namespace {

// CloudPolicyClient and UploadClient are not usable outside of a managed
// environment, to sidestep this we override the functions that normally build
// and retrieve these clients and provide a MockCloudPolicyClient and a
// FakeUploadClient.
class TestEncryptedReportingUploadProvider
    : public EncryptedReportingUploadProvider {
 public:
  TestEncryptedReportingUploadProvider(
      ReportSuccessfulUploadCallback report_successful_upload_cb,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      UpdateConfigInMissiveCallback update_config_in_missive_cb)
      : EncryptedReportingUploadProvider(
            report_successful_upload_cb,
            encryption_key_attached_cb,
            update_config_in_missive_cb,
            /*upload_client_builder_cb=*/
            base::BindRepeating(&FakeUploadClient::Create)) {}
};

class EncryptedReportingUploadProviderTest : public ::testing::Test {
 public:
  MOCK_METHOD(void, ReportSuccessfulUpload, (SequenceInformation, bool), ());
  MOCK_METHOD(void, EncryptionKeyCallback, (SignedEncryptionInfo), ());
  MOCK_METHOD(void,
              UpdateConfigInMissiveCallback,
              (ListOfBlockedDestinations),
              ());

 protected:
  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
    test_env_ = std::make_unique<ReportingServerConnector::TestEnvironment>();
    service_provider_ = std::make_unique<TestEncryptedReportingUploadProvider>(
        base::BindRepeating(
            &EncryptedReportingUploadProviderTest::ReportSuccessfulUpload,
            base::Unretained(this)),
        base::BindRepeating(
            &EncryptedReportingUploadProviderTest::EncryptionKeyCallback,
            base::Unretained(this)),
        base::BindRepeating(&EncryptedReportingUploadProviderTest::
                                UpdateConfigInMissiveCallback,
                            base::Unretained(this)));

    record_.set_encrypted_wrapped_record("TEST_DATA");

    auto* sequence_information = record_.mutable_sequence_information();
    sequence_information->set_sequencing_id(42);
    sequence_information->set_generation_id(1701);
    sequence_information->set_priority(Priority::SLOW_BATCH);
  }

  void TearDown() override {
    test_env_.reset();
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  StatusOr<std::list<int64_t>> CallRequestUploadEncryptedRecord(
      bool need_encryption_key,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation) {
    test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
    service_provider_->RequestUploadEncryptedRecords(
        need_encryption_key, std::move(records), std::move(scoped_reservation),
        enqueued_event.cb());
    return enqueued_event.result();
  }

  // Must be initialized before any other class member.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<ReportingServerConnector::TestEnvironment> test_env_;
  EncryptedRecord record_;

  scoped_refptr<ResourceManager> memory_resource_;

  std::unique_ptr<TestEncryptedReportingUploadProvider> service_provider_;
};

// TODO(b/289375752): Test is flaky across platforms.
TEST_F(EncryptedReportingUploadProviderTest,
       DISABLED_SuccessfullyUploadsRecord) {
  test::TestMultiEvent<SequenceInformation, bool /*force*/> uploaded_event;
  EXPECT_CALL(*this, ReportSuccessfulUpload(_, _))
      .WillOnce([&uploaded_event](SequenceInformation seq_info, bool force) {
        std::move(uploaded_event.cb()).Run(std::move(seq_info), force);
      });

  std::vector<EncryptedRecord> records;
  records.emplace_back(record_);
  ScopedReservation record_reservation(records.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());
  const auto enqueued_result = CallRequestUploadEncryptedRecord(
      /*need_encryption_key=*/false, std::move(records),
      std::move(record_reservation));

  EXPECT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(),
              ElementsAre(record_.sequence_information().sequencing_id()));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());

  test_env_->SimulateResponseForRequest(0);

  auto uploaded_result = uploaded_event.result();
  EXPECT_THAT(std::get<0>(uploaded_result),
              EqualsProto(record_.sequence_information()));
  EXPECT_FALSE(std::get<1>(uploaded_result));  // !force
}

}  // namespace
}  // namespace reporting
