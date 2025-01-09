// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include <list>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "google_apis/gaia/core_account_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace reporting {
namespace {

using ::policy::MockCloudPolicyClient;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ContainerEq;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Property;
using testing::SizeIs;
using ::testing::StrictMock;
using ::testing::WithArgs;

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class UploadClientTest : public ::testing::TestWithParam<
                             ::testing::tuple</*need_encryption_key*/ bool,
                                              /*force_confirm*/ bool>> {
 public:
  UploadClientTest() = default;

 protected:
  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Set up fake primary profile.
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef")));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
    const user_manager::User* user =
        fake_user_manager->AddPublicAccountUser(account_id);
    fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.reset();
    profile_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<ResourceManager> memory_resource_;
};

using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;
using TestConfigFileAttached = MockFunction<void(ConfigFile)>;

TEST_P(UploadClientTest, CreateUploadClientAndUploadRecords) {
  static constexpr int64_t kExpectedCallTimes = 10;
  static constexpr int64_t kGenerationId = 1234;
#if BUILDFLAG(IS_CHROMEOS)
  static constexpr char kGenerationGuid[] =
      "c947e7e9-b87d-4592-9fe7-407792544e53";
#endif  // BUILDFLAG(IS_CHROMEOS)

  base::Value::Dict data;
  data.Set("TEST_KEY", "TEST_VALUE");

  std::string json_data;
  ASSERT_TRUE(base::JSONWriter::Write(data, &json_data));

  WrappedRecord wrapped_record;
  Record* record = wrapped_record.mutable_record();
  record->set_data(json_data);
  record->set_destination(Destination::UPLOAD_EVENTS);

  std::list<int64_t> expected_cached_seq_ids;
  ScopedReservation total_reservation(0uL, memory_resource_);
  std::string serialized_record;
  wrapped_record.SerializeToString(&serialized_record);
  std::vector<EncryptedRecord> records;
  for (int64_t i = 0; i < kExpectedCallTimes; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(serialized_record);
    SequenceInformation* sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_sequencing_id(i);
    sequence_information->set_generation_id(kGenerationId);
#if BUILDFLAG(IS_CHROMEOS)
    sequence_information->set_generation_guid(kGenerationGuid);
#endif  // BUILDFLAG(IS_CHROMEOS)
    sequence_information->set_priority(Priority::IMMEDIATE);
    ScopedReservation record_reservation(encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    EXPECT_TRUE(record_reservation.reserved());
    total_reservation.HandOver(record_reservation);
    records.push_back(encrypted_record);
    expected_cached_seq_ids.push_back(i);
  }

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(
      encryption_key_attached,
      Call(AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                          Not(IsEmpty())),
                 Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
                 Property(&SignedEncryptionInfo::signature, Not(IsEmpty())))))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(
      config_file_attached,
      Call(AllOf(Property(&ConfigFile::blocked_event_configs, Not(IsEmpty())),
                 Property(&ConfigFile::version, Gt(4444)),
                 Property(&ConfigFile::config_file_signature, Not(IsEmpty())))))
      .Times(0);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  auto test_env = std::make_unique<ReportingServerConnector::TestEnvironment>();

  static constexpr char matched_record_template[] =
#if BUILDFLAG(IS_CHROMEOS)
      R"JSON(
{
  "sequenceInformation": {
    "generationId": "1234",
    "generationGuid": "c947e7e9-b87d-4592-9fe7-407792544e53",
    "priority": 1,
    "sequencingId": "%d"
  }
}
)JSON"
#else   // BUILDFLAG(IS_CHROMEOS)
      R"JSON(
{
  "sequenceInformation": {
    "generationId": "1234",
    "priority": 1,
    "sequencingId": "%d"
  }
}
)JSON"
#endif  // BUILDFLAG(IS_CHROMEOS)
      ;

  test::TestMultiEvent<SequenceInformation, bool> upload_success_event;

  // Save last record seq info for verification.
  const SequenceInformation last_record_seq_info =
      records.back().sequence_information();

  test::TestEvent<StatusOr<std::unique_ptr<UploadClient>>> e;
  UploadClient::Create(e.cb());
  StatusOr<std::unique_ptr<UploadClient>> upload_client_result = e.result();
  ASSERT_TRUE(upload_client_result.has_value()) << upload_client_result.error();

  auto upload_client = std::move(upload_client_result.value());
  // config_file_version is set to 0 for testing. The default value is -1 and we
  // want to override it.
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  upload_client->EnqueueUpload(
      need_encryption_key(), /*config_file_version=*/0, std::move(records),
      std::move(total_reservation), enqueued_event.cb(),
      upload_success_event.repeating_cb(), encryption_key_attached_cb,
      std::move(config_file_attached_cb));
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env->url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env->request_body(0);
  EXPECT_THAT(request_body, AllOf(IsDataUploadRequestValid(),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 0)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 1)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 2)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 3)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 4)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 5)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 6)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 7)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 8)),
                                  DoesRequestContainRecord(base::StringPrintf(
                                      matched_record_template, 9))));

  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm())
                      .Build();
  ASSERT_OK(response) << response.error();
  test_env->SimulateCustomResponseForRequest(0, std::move(*response));

  auto upload_success_result = upload_success_event.result();
  EXPECT_THAT(std::get<0>(upload_success_result),
              EqualsProto(last_record_seq_info));
  EXPECT_THAT(std::get<1>(upload_success_result), Eq(force_confirm()));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    UploadClientTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));

}  // namespace
}  // namespace reporting
