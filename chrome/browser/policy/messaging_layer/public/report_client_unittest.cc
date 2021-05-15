// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/public/report_client.h"

#include "base/base64.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/singleton.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::SizeIs;
using ::testing::WithArgs;

namespace reporting {
namespace {

class ReportClientTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
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
    chromeos::ProfileHelper::Get()->SetActiveUserIdForTesting(
        profile_->GetProfileUserName());
    mock_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Encryption is disabled by default.
    ASSERT_FALSE(EncryptionModuleInterface::is_enabled());
    if (is_encryption_enabled()) {
      // Enable encryption.
      scoped_feature_list_.InitFromCommandLine(
          "EncryptedReportingPipeline,EncryptedReporting", "");
      // Generate signing key pair.
      test::GenerateSigningKeyPair(signing_private_key_,
                                   signature_verification_public_key_);
      // Create decryption module.
      auto decryptor_result = test::Decryptor::Create();
      ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
      decryptor_ = std::move(decryptor_result.ValueOrDie());
      // Prepare the key.
      signed_encryption_key_ = GenerateAndSignKey();
    } else {
      scoped_feature_list_.InitFromCommandLine("EncryptedReportingPipeline",
                                               "EncryptedReporting");
    }

    // Provide a mock cloud policy client.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("FAKE_DM_TOKEN");
    test_reporting_ = std::make_unique<ReportingClient::TestEnvironment>(
        base::FilePath(location_.GetPath()),
        base::StringPiece(
            reinterpret_cast<const char*>(signature_verification_public_key_),
            kKeySize),
        client_.get());
  }

  void TearDown() override {
    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.reset();
    profile_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  SignedEncryptionInfo GenerateAndSignKey() {
    DCHECK(decryptor_) << "Decryptor not created";
    // Generate new pair of private key and public value.
    uint8_t private_key[kKeySize];
    Encryptor::PublicKeyId public_key_id;
    uint8_t public_value[kKeySize];
    test::GenerateEncryptionKeyPair(private_key, public_value);
    test::TestEvent<StatusOr<Encryptor::PublicKeyId>> prepare_key_pair;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(private_key), kKeySize),
        std::string(reinterpret_cast<const char*>(public_value), kKeySize),
        prepare_key_pair.cb());
    auto prepare_key_result = prepare_key_pair.result();
    DCHECK(prepare_key_result.ok());
    public_key_id = prepare_key_result.ValueOrDie();
    // Deliver public key to storage.
    SignedEncryptionInfo signed_encryption_key;
    signed_encryption_key.set_public_asymmetric_key(
        std::string(reinterpret_cast<const char*>(public_value), kKeySize));
    signed_encryption_key.set_public_key_id(public_key_id);
    // Sign public key.
    uint8_t value_to_sign[sizeof(Encryptor::PublicKeyId) + kKeySize];
    memcpy(value_to_sign, &public_key_id, sizeof(Encryptor::PublicKeyId));
    memcpy(value_to_sign + sizeof(Encryptor::PublicKeyId), public_value,
           kKeySize);
    uint8_t signature[kSignatureSize];
    test::SignMessage(
        signing_private_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature);
    signed_encryption_key.set_signature(
        std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
    // Double check signature.
    DCHECK(VerifySignature(
        signature_verification_public_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature));
    return signed_encryption_key;
  }

  std::unique_ptr<ReportQueue> CreateQueue(bool expect_key_roundtrip) {
    auto config_result = ReportQueueConfiguration::Create(
        dm_token_, destination_, policy_checker_callback_);
    EXPECT_TRUE(config_result.ok());

    test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> create_queue_event;
    if (expect_key_roundtrip) {
      EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
          .WillOnce(WithArgs<0, 2>(Invoke(
              [this](base::Value payload,
                     policy::CloudPolicyClient::ResponseCallback done_cb) {
                absl::optional<bool> const attach_encryption_settings =
                    payload.FindBoolKey("attachEncryptionSettings");
                ASSERT_TRUE(attach_encryption_settings.has_value());
                ASSERT_TRUE(attach_encryption_settings
                                .value());  // If set, must be true.
                ASSERT_TRUE(is_encryption_enabled());

                base::Value encryption_settings{base::Value::Type::DICTIONARY};
                std::string public_key;
                base::Base64Encode(
                    signed_encryption_key_.public_asymmetric_key(),
                    &public_key);
                encryption_settings.SetStringKey("publicKey", public_key);
                encryption_settings.SetIntKey(
                    "publicKeyId", signed_encryption_key_.public_key_id());
                std::string public_key_signature;
                base::Base64Encode(signed_encryption_key_.signature(),
                                   &public_key_signature);
                encryption_settings.SetStringKey("publicKeySignature",
                                                 public_key_signature);
                base::Value response{base::Value::Type::DICTIONARY};
                response.SetPath("encryptionSettings",
                                 std::move(encryption_settings));
                std::move(done_cb).Run(std::move(response));
              })))
          .RetiresOnSaturation();
    }
    ReportQueueProvider::CreateQueue(std::move(config_result.ValueOrDie()),
                                     create_queue_event.cb());
    auto result = create_queue_event.result();
    EXPECT_OK(result);
    auto report_queue = std::move(result.ValueOrDie());

    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();

    return report_queue;
  }

  bool is_encryption_enabled() const { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;
  // BrowserTaskEnvironment must be instantiated before other classes that posts
  // tasks.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;

  base::ScopedTempDir location_;

  uint8_t signature_verification_public_key_[kKeySize];
  uint8_t signing_private_key_[kSignKeySize];
  scoped_refptr<test::Decryptor> decryptor_;
  SignedEncryptionInfo signed_encryption_key_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  const std::string dm_token_ = "TOKEN";
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  ReportQueueConfiguration::PolicyCheckCallback policy_checker_callback_ =
      base::BindRepeating([]() { return Status::StatusOK(); });
};

// Tests that a ReportQueue can be created using the ReportingClient.
TEST_P(ReportClientTest, CreatesReportQueue) {
  auto report_queue = CreateQueue(is_encryption_enabled());
  ASSERT_THAT(report_queue.get(), Ne(nullptr));
}

// Ensures that created ReportQueues are actually different.
TEST_P(ReportClientTest, CreatesTwoDifferentReportQueues) {
  // Create first queue.
  auto report_queue_1 = CreateQueue(is_encryption_enabled());
  ASSERT_THAT(report_queue_1.get(), Ne(nullptr));

  // Create second queue. It will reuse the same ReportClient, so even if
  // encryption is enabled, there will be no roundtrip to server to get the key.
  auto report_queue_2 = CreateQueue(/*expect_key_roundtrip=*/false);
  ASSERT_THAT(report_queue_2.get(), Ne(nullptr));

  EXPECT_NE(report_queue_1.get(), report_queue_2.get());
}

// Creates queue, enqueues messages and verifies they are uploaded.
TEST_P(ReportClientTest, EnqueueMessageAndUpload) {
  // Create queue.
  auto report_queue = CreateQueue(is_encryption_enabled());

  // Enqueue event.
  test::TestEvent<Status> enqueue_record_event;
  report_queue->Enqueue("Record", FAST_BATCH, enqueue_record_event.cb());
  const auto enqueue_record_result = enqueue_record_event.result();
  EXPECT_OK(enqueue_record_result) << enqueue_record_result;

  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([this](base::Value payload,
                        policy::CloudPolicyClient::ResponseCallback done_cb) {
            base::Value* const records = payload.FindListKey("encryptedRecord");
            ASSERT_THAT(records, Ne(nullptr));
            base::Value::ListView records_list = records->GetList();
            ASSERT_THAT(records_list, SizeIs(1));
            base::Value& record = records_list[0];
            if (is_encryption_enabled()) {
              const base::Value* const enctyption_info =
                  record.FindDictKey("encryptionInfo");
              ASSERT_THAT(enctyption_info, Ne(nullptr));
              const std::string* const encryption_key =
                  enctyption_info->FindStringKey("encryptionKey");
              ASSERT_THAT(encryption_key, Ne(nullptr));
              const std::string* const public_key_id =
                  enctyption_info->FindStringKey("publicKeyId");
              ASSERT_THAT(public_key_id, Ne(nullptr));
              int64_t key_id;
              ASSERT_TRUE(base::StringToInt64(*public_key_id, &key_id));
              EXPECT_THAT(key_id, Eq(signed_encryption_key_.public_key_id()));
            } else {
              ASSERT_THAT(record.FindKey("encryptionInfo"), Eq(nullptr));
            }
            base::Value* const seq_info =
                record.FindDictKey("sequenceInformation");
            ASSERT_THAT(seq_info, Ne(nullptr));
            base::Value response{base::Value::Type::DICTIONARY};
            response.SetPath("lastSucceedUploadedRecord", std::move(*seq_info));
            std::move(done_cb).Run(std::move(response));
          })));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

INSTANTIATE_TEST_SUITE_P(ReportClientTestSuite,
                         ReportClientTest,
                         ::testing::Bool() /* true - encryption enabled */);

}  // namespace
}  // namespace reporting
