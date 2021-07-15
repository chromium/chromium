// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/ash/dbus/encrypted_reporting_service_provider.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/upload/fake_upload_client.h"
#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/interface.pb.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ReportSuccessfulUploadCallback =
    reporting::UploadClient::ReportSuccessfulUploadCallback;
using EncryptionKeyAttachedCallback =
    reporting::UploadClient::EncryptionKeyAttachedCallback;

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace ash {
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

// Helper function composes JSON represented as base::Value from Sequencing
// information in request.
base::Value ValueFromSucceededSequencingInfo(
    const absl::optional<base::Value> request,
    bool force_confirm_flag) {
  EXPECT_TRUE(request.has_value());
  EXPECT_TRUE(request.value().is_dict());
  base::Value response(base::Value::Type::DICTIONARY);

  // Retrieve and process data
  const base::Value* const encrypted_record_list =
      request.value().FindListKey("encryptedRecord");
  EXPECT_NE(encrypted_record_list, nullptr);
  EXPECT_FALSE(encrypted_record_list->GetList().empty());

  // Retrieve and process sequencing information
  const base::Value* unsigned_seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequencingInformation");
  EXPECT_NE(unsigned_seq_info, nullptr);
  const base::Value* seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequenceInformation");
  EXPECT_TRUE(seq_info != nullptr);
  response.SetPath("lastSucceedUploadedRecord", seq_info->Clone());

  // If forceConfirm confirm is expected, set it.
  if (force_confirm_flag) {
    response.SetPath("forceConfirm", base::Value(true));
  }

  // If attach_encryption_settings it true, process that.
  const auto attach_encryption_settings =
      request.value().FindBoolKey("attachEncryptionSettings");
  if (attach_encryption_settings.has_value() &&
      attach_encryption_settings.value()) {
    base::Value encryption_settings{base::Value::Type::DICTIONARY};
    std::string public_key;
    base::Base64Encode("PUBLIC KEY", &public_key);
    encryption_settings.SetStringKey("publicKey", public_key);
    encryption_settings.SetIntKey("publicKeyId", 12345);
    std::string public_key_signature;
    base::Base64Encode("PUBLIC KEY SIG", &public_key_signature);
    encryption_settings.SetStringKey("publicKeySignature",
                                     public_key_signature);
    response.SetPath("encryptionSettings", std::move(encryption_settings));
  }

  return response;
}

// CloudPolicyClient and UploadClient are not usable outside of a managed
// environment, to sidestep this we override the functions that normally build
// and retrieve these clients and provide a MockCloudPolicyClient and a
// FakeUploadClient.
class TestEncryptedReportingServiceProvider
    : public EncryptedReportingServiceProvider {
 public:
  TestEncryptedReportingServiceProvider(
      policy::CloudPolicyClient* cloud_policy_client,
      ReportSuccessfulUploadCallback report_successful_upload_cb,
      EncryptionKeyAttachedCallback encrypted_key_cb)
      : EncryptedReportingServiceProvider(
            /*build_cloud_policy_client_cb=*/base::BindRepeating(
                [](policy::CloudPolicyClient* cloud_policy_client,
                   base::OnceCallback<void(
                       reporting::StatusOr<policy::CloudPolicyClient*>)>
                       callback) {
                  std::move(callback).Run(cloud_policy_client);
                },
                base::Unretained(cloud_policy_client)),
            /*upload_client_builder_cb=*/base::BindRepeating(
                [](ReportSuccessfulUploadCallback report_successful_upload_cb,
                   EncryptionKeyAttachedCallback encrypted_key_cb,
                   scoped_refptr<
                       reporting::StorageModuleInterface> /*storage_module*/,
                   policy::CloudPolicyClient* client,
                   reporting::UploadClient::CreatedCallback
                       update_upload_client_cb) {
                  reporting::FakeUploadClient::Create(
                      client, report_successful_upload_cb, encrypted_key_cb,
                      std::move(update_upload_client_cb));
                },
                report_successful_upload_cb,
                encrypted_key_cb)) {}
};

class EncryptedReportingServiceProviderTest : public ::testing::Test {
 public:
  MOCK_METHOD(void,
              ReportSuccessfulUpload,
              (reporting::SequencingInformation, bool),
              ());
  MOCK_METHOD(void,
              EncryptionKeyCallback,
              (reporting::SignedEncryptionInfo),
              ());

 protected:
  void SetUp() override {
    MissiveClient::InitializeFake();
    cloud_policy_client_.SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
    auto successful_upload_cb = base::BindRepeating(
        &EncryptedReportingServiceProviderTest::ReportSuccessfulUpload,
        base::Unretained(this));
    auto encryption_key_cb = base::BindRepeating(
        &EncryptedReportingServiceProviderTest::EncryptionKeyCallback,
        base::Unretained(this));
    service_provider_ = std::make_unique<TestEncryptedReportingServiceProvider>(
        &cloud_policy_client_, std::move(successful_upload_cb),
        std::move(encryption_key_cb));

    record_.set_encrypted_wrapped_record("TEST_DATA");

    auto* sequencing_information = record_.mutable_sequencing_information();
    sequencing_information->set_sequencing_id(42);
    sequencing_information->set_generation_id(1701);
    sequencing_information->set_priority(reporting::Priority::SLOW_BATCH);
  }

  void TearDown() override {
    // Destruct test helper before the client shut down.
    test_helper_.TearDown();
    MissiveClient::Shutdown();
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
      const reporting::UploadEncryptedRecordRequest& request,
      reporting::UploadEncryptedRecordResponse* encrypted_record_response) {
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

  policy::MockCloudPolicyClient cloud_policy_client_;
  reporting::EncryptedRecord record_;

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestEncryptedReportingServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

TEST_F(EncryptedReportingServiceProviderTest, SuccessfullyUploadsRecord) {
  SetupForRequestUploadEncryptedRecord();
  EXPECT_CALL(*this, ReportSuccessfulUpload(
                         EqualsProto(record_.sequencing_information()), _))
      .Times(1);
  EXPECT_CALL(cloud_policy_client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([](base::Value request,
                    policy::CloudPolicyClient::ResponseCallback response_cb) {
            std::move(response_cb)
                .Run(ValueFromSucceededSequencingInfo(std::move(request),
                                                      false));
          })));

  reporting::UploadEncryptedRecordRequest request;
  request.add_encrypted_record()->CheckTypeAndMergeFrom(record_);

  reporting::UploadEncryptedRecordResponse response;
  CallRequestUploadEncryptedRecord(request, &response);

  EXPECT_EQ(response.status().code(), reporting::error::OK);
}

}  // namespace
}  // namespace ash
