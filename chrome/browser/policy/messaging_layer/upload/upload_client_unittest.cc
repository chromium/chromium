// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/upload_client.h"

#include <tuple>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/test/test_network_connection_tracker.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace reporting {
namespace {

using ::policy::MockCloudPolicyClient;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::InvokeArgument;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Property;
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

// Helper function composes JSON represented as base::Value from Sequencing
// information in request.
base::Value ValueFromSucceededSequencingInfo(
    const base::Optional<base::Value> request,
    bool force_confirm_flag) {
  EXPECT_TRUE(request.has_value());
  EXPECT_TRUE(request.value().is_dict());
  base::Value response(base::Value::Type::DICTIONARY);

  // Retrieve and process data
  const base::Value* const encrypted_record_list =
      request.value().FindListKey("encryptedRecord");
  EXPECT_TRUE(encrypted_record_list != nullptr);
  EXPECT_FALSE(encrypted_record_list->GetList().empty());

  // Retrieve and process sequencing information
  const base::Value* unsigned_seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequencingInformation");
  EXPECT_TRUE(unsigned_seq_info != nullptr);
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

class UploadClientTest : public ::testing::TestWithParam<
                             ::testing::tuple</*need_encryption_key*/ bool,
                                              /*force_confirm*/ bool>> {
 public:
  UploadClientTest() = default;

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Set up fake primary profile.
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<ash::FakeChromeUserManager>>();
    profile_ = std::make_unique<TestingProfile>(
        base::FilePath(FILE_PATH_LITERAL("/home/chronos/u-0123456789abcdef")));
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
    const user_manager::User* user =
        mock_user_manager->AddPublicAccountUser(account_id);
    mock_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.reset();
    profile_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_envrionment_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;

TEST_P(UploadClientTest, CreateUploadClientAndUploadRecords) {
  static constexpr int64_t kExpectedCallTimes = 10;
  static constexpr int64_t kGenerationId = 1234;

  base::Value data{base::Value::Type::DICTIONARY};
  data.SetKey("TEST_KEY", base::Value("TEST_VALUE"));

  std::string json_data;
  ASSERT_TRUE(base::JSONWriter::Write(data, &json_data));

  WrappedRecord wrapped_record;
  Record* record = wrapped_record.mutable_record();
  record->set_data(json_data);
  record->set_destination(Destination::UPLOAD_EVENTS);

  std::string serialized_record;
  wrapped_record.SerializeToString(&serialized_record);
  std::unique_ptr<std::vector<EncryptedRecord>> records =
      std::make_unique<std::vector<EncryptedRecord>>();
  for (int64_t i = 0; i < kExpectedCallTimes; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(serialized_record);

    SequencingInformation* sequencing_information =
        encrypted_record.mutable_sequencing_information();
    sequencing_information->set_sequencing_id(static_cast<int64_t>(i));
    sequencing_information->set_generation_id(kGenerationId);
    sequencing_information->set_priority(Priority::IMMEDIATE);
    records->push_back(encrypted_record);
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

  auto client = std::make_unique<MockCloudPolicyClient>();
  client->SetDMToken(
      policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());

  const bool force_confirm_flag = force_confirm();
  EXPECT_CALL(*client, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_flag](
                     base::Value request,
                     policy::CloudPolicyClient::ResponseCallback response_cb) {
            std::move(response_cb)
                .Run(ValueFromSucceededSequencingInfo(std::move(request),
                                                      force_confirm_flag));
          })));

  test::TestMultiEvent<SequencingInformation, bool> upload_completion;
  UploadClient::ReportSuccessfulUploadCallback completion_cb =
      upload_completion.cb();

  // Save last record seq info for verification.
  const SequencingInformation last_record_seq_info =
      records->back().sequencing_information();

  test::TestEvent<StatusOr<std::unique_ptr<UploadClient>>> e;
  UploadClient::Create(client.get(), completion_cb, encryption_key_attached_cb,
                       e.cb());
  StatusOr<std::unique_ptr<UploadClient>> upload_client_result = e.result();
  ASSERT_OK(upload_client_result) << upload_client_result.status();

  auto upload_client = std::move(upload_client_result.ValueOrDie());
  auto enqueue_result =
      upload_client->EnqueueUpload(need_encryption_key(), std::move(records));
  EXPECT_TRUE(enqueue_result.ok());

  auto completion_result = upload_completion.result();
  EXPECT_THAT(std::get<0>(completion_result),
              EqualsProto(last_record_seq_info));
  EXPECT_THAT(std::get<1>(completion_result), Eq(force_confirm()));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    UploadClientTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));

}  // namespace
}  // namespace reporting
