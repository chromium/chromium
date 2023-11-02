// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/memory/ref_counted.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/interface.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ReportSuccessfulUploadCallback =
    ::reporting::UploadClient::ReportSuccessfulUploadCallback;
using EncryptionKeyAttachedCallback =
    ::reporting::UploadClient::EncryptionKeyAttachedCallback;

using UploadProvider = ::reporting::EncryptedReportingUploadProvider;

using ::testing::_;
using ::testing::Eq;

namespace ash {
namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  if (!message.SerializeToString(&expected_serialized)) {
    *result_listener << "Expected proto fails to serialize";
    return false;
  }
  if (!arg.SerializeToString(&actual_serialized)) {
    *result_listener << "Actual proto fails to serialize";
    return false;
  }
  if (expected_serialized != actual_serialized) {
    *result_listener << "Provided proto did not match the expected proto"
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
class TestEncryptedReportingServiceProvider
    : public EncryptedReportingServiceProvider {
 public:
  TestEncryptedReportingServiceProvider(
      ReportSuccessfulUploadCallback report_successful_upload_cb,
      EncryptionKeyAttachedCallback encrypted_key_cb)
      : EncryptedReportingServiceProvider(std::make_unique<UploadProvider>(
            report_successful_upload_cb,
            encrypted_key_cb,
            /*upload_client_builder_cb=*/
            base::BindRepeating(&::reporting::FakeUploadClient::Create))) {}

  TestEncryptedReportingServiceProvider(
      const TestEncryptedReportingServiceProvider& other) = delete;
  TestEncryptedReportingServiceProvider& operator=(
      const TestEncryptedReportingServiceProvider& other) = delete;
};

class EncryptedReportingServiceProviderTest : public ::testing::Test {
 public:
  MOCK_METHOD(void,
              ReportSuccessfulUpload,
              (::reporting::SequenceInformation, bool),
              ());
  MOCK_METHOD(void,
              EncryptionKeyCallback,
              (::reporting::SignedEncryptionInfo),
              ());

 protected:
  void SetUp() override {
    chromeos::MissiveClient::InitializeFake();
    mock_client_.SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());

    auto successful_upload_cb = base::BindRepeating(
        &EncryptedReportingServiceProviderTest::ReportSuccessfulUpload,
        base::Unretained(this));
    auto encryption_key_cb = base::BindRepeating(
        &EncryptedReportingServiceProviderTest::EncryptionKeyCallback,
        base::Unretained(this));
    service_provider_ = std::make_unique<TestEncryptedReportingServiceProvider>(
        successful_upload_cb, encryption_key_cb);

    record_.set_encrypted_wrapped_record("TEST_DATA");

    auto* sequence_information = record_.mutable_sequence_information();
    sequence_information->set_sequencing_id(42);
    sequence_information->set_generation_id(1701);
    sequence_information->set_priority(::reporting::Priority::SLOW_BATCH);
  }

  void TearDown() override {
    // Destruct test helper before the client shut down.
    test_helper_.TearDown();
    chromeos::MissiveClient::Shutdown();
  }

  void SetupForRequestUploadEncryptedRecord() {
    test_helper_.SetUp(
        chromeos::kChromeReportingServiceName,
        dbus::ObjectPath(chromeos::kChromeReportingServicePath),
        chromeos::kChromeReportingServiceInterface,
        chromeos::kChromeReportingServiceUploadEncryptedRecordMethod,
        service_provider_.get());
    // There are multiple Tasks that are started by calling the Upload request.
    // We need to wait for them to complete, or we will get race conditions on
    // exit and some test runs will be flakey.
    task_environment_.RunUntilIdle();
  }

  void CallRequestUploadEncryptedRecord(
      const ::reporting::UploadEncryptedRecordRequest& request,
      ::reporting::UploadEncryptedRecordResponse* encrypted_record_response) {
    dbus::MethodCall method_call(
        chromeos::kChromeReportingServiceInterface,
        chromeos::kChromeReportingServiceUploadEncryptedRecordMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(request);

    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);

    ASSERT_TRUE(response);
    dbus::MessageReader reader(response.get());
    ASSERT_TRUE(reader.PopArrayOfBytesAsProto(encrypted_record_response));
  }

  // Must be initialized before any other class member.
  content::BrowserTaskEnvironment task_environment_;

  policy::MockCloudPolicyClient mock_client_;
  ::reporting::ReportingServerConnector::TestEnvironment test_env_{
      &mock_client_};
  ::reporting::EncryptedRecord record_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestEncryptedReportingServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

TEST_F(EncryptedReportingServiceProviderTest, SuccessfullyUploadsRecord) {
  SetupForRequestUploadEncryptedRecord();
  EXPECT_CALL(*this, ReportSuccessfulUpload(
                         EqualsProto(record_.sequence_information()), _))
      .Times(1);
  EXPECT_CALL(mock_client_, UploadEncryptedReport(
                                ::reporting::IsDataUploadRequestValid(), _, _))
      .WillOnce(::reporting::MakeUploadEncryptedReportAction());

  ::reporting::UploadEncryptedRecordRequest request;
  request.add_encrypted_record()->CheckTypeAndMergeFrom(record_);

  ::reporting::UploadEncryptedRecordResponse response;
  CallRequestUploadEncryptedRecord(request, &response);

  EXPECT_THAT(response.status().code(), Eq(::reporting::error::OK));
}

TEST_F(EncryptedReportingServiceProviderTest,
       NoRecordUploadWhenUploaderDisabled) {
  SetupForRequestUploadEncryptedRecord();
  EXPECT_CALL(mock_client_, UploadEncryptedReport(_, _, _)).Times(0);

  ::reporting::UploadEncryptedRecordRequest request;
  request.add_encrypted_record()->CheckTypeAndMergeFrom(record_);

  // Disable uploader.
  scoped_feature_list_.InitFromCommandLine("", "ProvideUploader");

  ::reporting::UploadEncryptedRecordResponse response;
  CallRequestUploadEncryptedRecord(request, &response);
}

}  // namespace
}  // namespace ash
