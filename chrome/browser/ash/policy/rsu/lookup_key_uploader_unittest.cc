// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/rsu/lookup_key_uploader.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/mock_enrollment_certificate_uploader.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using CertificateStatus =
    ash::attestation::EnrollmentCertificateUploader::Status;
using ash::attestation::MockEnrollmentCertificateUploader;
using testing::_;

namespace policy {

namespace {
const char kValidRsuDeviceId[] = "123";
const char kValidRsuDeviceIdEncoded[] =
    "MTIz";  // base::Base64Encode(kValidRsuDeviceId, kValidRsuDeviceencoded)

class FakeCryptohomeMiscClientWithRsuReplyWaiter
    : public ash::FakeCryptohomeMiscClient {
 public:
  static FakeCryptohomeMiscClientWithRsuReplyWaiter* Get() {
    return static_cast<FakeCryptohomeMiscClientWithRsuReplyWaiter*>(
        ash::FakeCryptohomeMiscClient::Get());
  }

  void GetRsuDeviceId(const ::user_data_auth::GetRsuDeviceIdRequest& request,
                      GetRsuDeviceIdCallback callback) override {
    ash::FakeCryptohomeMiscClient::GetRsuDeviceId(
        request,
        base::BindOnce(
            [](GetRsuDeviceIdCallback callback,
               base::OnceClosure reply_consumed,
               std::optional<::user_data_auth::GetRsuDeviceIdReply> reply) {
              std::move(callback).Run(std::move(reply));
              std::move(reply_consumed).Run();
            },
            std::move(callback), reply_consumed_.GetCallback()));
  }

  bool WaitForRsuDeviceIdReply() { return reply_consumed_.WaitAndClear(); }

 private:
  base::test::TestFuture<void> reply_consumed_;
};
}  // namespace

class LookupKeyUploaderTest : public ash::DeviceSettingsTestBase {
 public:
  LookupKeyUploaderTest(const LookupKeyUploaderTest&) = delete;
  LookupKeyUploaderTest& operator=(const LookupKeyUploaderTest&) = delete;

 protected:
  LookupKeyUploaderTest() = default;

  void SetUp() override {
    ash::DeviceSettingsTestBase::SetUp();
    // Replace the default fake so tests can wait until the uploader consumes
    // its posted RSU reply. The base fixture shuts this instance down.
    ash::CryptohomeMiscClient::Shutdown();
    new FakeCryptohomeMiscClientWithRsuReplyWaiter();
    pref_service_.registry()->RegisterStringPref(
        ash::prefs::kLastRsuDeviceIdUploaded, std::string());
    lookup_key_uploader_ = std::make_unique<LookupKeyUploader>(
        nullptr, &pref_service_, &certificate_uploader_);
    lookup_key_uploader_->SetClock(&clock_);
    // Drive the uploader through the mock store's public observer API. TearDown
    // removes this test-only observation before either object is destroyed.
    policy_store_.AddObserver(lookup_key_uploader_.get());
    // We initialize clock to imitate real time.
    clock_.Advance(base::Days(50));
  }

  void TearDown() override {
    policy_store_.RemoveObserver(lookup_key_uploader_.get());
    ash::DeviceSettingsTestBase::TearDown();
  }

  void ExpectSavedIdToBe(const std::string& key) {
    EXPECT_EQ(pref_service_.GetString(ash::prefs::kLastRsuDeviceIdUploaded),
              key);
  }
  bool NeedsUpload() { return lookup_key_uploader_->needs_upload_; }

  void SetCryptohomeReplyTo(const std::string& rsu_device_id) {
    FakeCryptohomeMiscClientWithRsuReplyWaiter::Get()->set_rsu_device_id(
        rsu_device_id);
  }

  void AdvanceTime() { clock_.Advance(lookup_key_uploader_->kRetryFrequency); }
  void NotifyStoreLoadedAndWaitForRsuDeviceIdReply() {
    policy_store_.NotifyStoreLoaded();
    ASSERT_TRUE(FakeCryptohomeMiscClientWithRsuReplyWaiter::Get()
                    ->WaitForRsuDeviceIdReply())
        << "Timed out waiting for the RSU device ID reply";
  }

  TestingPrefServiceSimple pref_service_;
  base::SimpleTestClock clock_;
  MockEnrollmentCertificateUploader certificate_uploader_;
  std::unique_ptr<LookupKeyUploader> lookup_key_uploader_;
  MockCloudPolicyStore policy_store_{dm_protocol::GetChromeUserPolicyType()};
};

TEST_F(LookupKeyUploaderTest, Uploads) {
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
      .WillOnce(
          [](base::OnceCallback<void(CertificateStatus status)> callback) {
            std::move(callback).Run(CertificateStatus::kSuccess);
          });
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  ExpectSavedIdToBe(kValidRsuDeviceIdEncoded);
}

TEST_F(LookupKeyUploaderTest, ReuploadsOnFail) {
  SetCryptohomeReplyTo("");
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_)).Times(0);
  EXPECT_TRUE(NeedsUpload());
}

TEST_F(LookupKeyUploaderTest, DoesntUploadTwice) {
  pref_service_.SetString(ash::prefs::kLastRsuDeviceIdUploaded,
                          kValidRsuDeviceIdEncoded);
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_)).Times(0);
  ExpectSavedIdToBe(kValidRsuDeviceIdEncoded);
  EXPECT_FALSE(NeedsUpload());
}

TEST_F(LookupKeyUploaderTest, DoesNotUploadVeryFrequently) {
  SetCryptohomeReplyTo("");
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  EXPECT_TRUE(NeedsUpload());  // Will ask for restart.

  // Next upload should not be executed -- because of the frequency limit.
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  policy_store_.NotifyStoreLoaded();
  ExpectSavedIdToBe("");
  EXPECT_TRUE(NeedsUpload());  // Will ask for restart.

  AdvanceTime();

  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
      .WillOnce(
          [](base::OnceCallback<void(CertificateStatus status)> callback) {
            std::move(callback).Run(CertificateStatus::kSuccess);
          });
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  ExpectSavedIdToBe(kValidRsuDeviceIdEncoded);
  EXPECT_FALSE(NeedsUpload());
}

TEST_F(LookupKeyUploaderTest, UploadsEvenWhenSubmittedBeforeIfForcedByPolicy) {
  EXPECT_CALL(certificate_uploader_, ObtainAndUploadCertificate(_))
      .Times(2)
      .WillRepeatedly(
          [](base::OnceCallback<void(CertificateStatus status)> callback) {
            std::move(callback).Run(CertificateStatus::kSuccess);
          });
  SetCryptohomeReplyTo(kValidRsuDeviceId);
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
  ExpectSavedIdToBe(kValidRsuDeviceIdEncoded);
  EXPECT_FALSE(NeedsUpload());

  // We set the policy for obtaining RSU lookup key.
  auto policy_data = std::make_unique<enterprise_management::PolicyData>();
  policy_data->mutable_client_action_required()
      ->set_enrollment_certificate_needed(true);
  policy_store_.set_policy_data_for_testing(std::move(policy_data));

  // We expect the ObtainAndUploadCertificate to called twice.
  AdvanceTime();
  NotifyStoreLoadedAndWaitForRsuDeviceIdReply();
}

}  // namespace policy
