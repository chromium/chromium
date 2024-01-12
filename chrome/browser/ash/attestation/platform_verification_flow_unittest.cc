// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/attestation/platform_verification_flow.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/attestation.pb.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArgs;

namespace ash {
namespace attestation {

namespace {

const char kTestID[] = "test_id";
const char kTestChallenge[] = "test_challenge";
const char kTestCertificate[] = "test_certificate";
const char kTestEmail[] = "test_email@chromium.org";

class FakeDelegate : public PlatformVerificationFlow::Delegate {
 public:
  FakeDelegate() : is_in_supported_mode_(true) {}

  FakeDelegate(const FakeDelegate&) = delete;
  FakeDelegate& operator=(const FakeDelegate&) = delete;

  ~FakeDelegate() override {}

  bool IsInSupportedMode() override { return is_in_supported_mode_; }

  void set_is_in_supported_mode(bool is_in_supported_mode) {
    is_in_supported_mode_ = is_in_supported_mode;
  }

 private:
  bool is_in_supported_mode_;
};

}  // namespace

class PlatformVerificationFlowTest : public ::testing::Test {
 public:
  PlatformVerificationFlowTest()
      : certificate_status_(ATTESTATION_SUCCESS),
        fake_certificate_index_(0),
        result_(PlatformVerificationFlow::INTERNAL_ERROR) {
    AttestationClient::InitializeFake();
  }
  ~PlatformVerificationFlowTest() override { AttestationClient::Shutdown(); }

  void SetUp() override {
    // Create a verifier for tests to call.
    verifier_ = new PlatformVerificationFlow(
        &mock_attestation_flow_, AttestationClient::Get(), &fake_delegate_);

    // Create callbacks for tests to use with verifier_.
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, true);

    // Configure the fake user.
    user_ = user_manager_.AddUser(AccountId::FromUserEmail(kTestEmail));
  }

  PlatformVerificationFlow::ChallengeCallback CreateChallengeCallback() {
    return base::BindOnce(&PlatformVerificationFlowTest::FakeChallengeCallback,
                          base::Unretained(this));
  }

  void ExpectAttestationFlow() {
    // When consent is not given or the feature is disabled, it is important
    // that there are no calls to the attestation service.  Thus, a test must
    // explicitly expect these calls or the mocks will fail the test.

    const AccountId account_id = AccountId::FromUserEmail(kTestEmail);
    // Configure the mock AttestationFlow to call FakeGetCertificate.
    EXPECT_CALL(mock_attestation_flow_,
                GetCertificate(PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                               account_id, kTestID, _, _, _, _, _))
        .WillRepeatedly(WithArgs<7>(
            Invoke(this, &PlatformVerificationFlowTest::FakeGetCertificate)));

    const std::string expected_key_name =
        std::string(kContentProtectionKeyPrefix) + std::string(kTestID);
    AttestationClient::Get()
        ->GetTestInterface()
        ->AllowlistSignSimpleChallengeKey(kTestEmail, expected_key_name);
  }

  void FakeGetCertificate(AttestationFlow::CertificateCallback callback) {
    std::string certificate =
        (fake_certificate_index_ < fake_certificate_list_.size()) ?
            fake_certificate_list_[fake_certificate_index_] : kTestCertificate;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), certificate_status_, certificate));
    ++fake_certificate_index_;
  }

  void FakeChallengeCallback(PlatformVerificationFlow::Result result,
                             const std::string& salt,
                             const std::string& signature,
                             const std::string& certificate) {
    result_ = result;
    challenge_salt_ = salt;
    challenge_signature_ = signature;
    certificate_ = certificate;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  StrictMock<MockAttestationFlow> mock_attestation_flow_;
  FakeDelegate fake_delegate_;
  ScopedCrosSettingsTestHelper settings_helper_;

  // Used to create a fake user.
  FakeChromeUserManager user_manager_;
  raw_ptr<user_manager::User> user_;

  scoped_refptr<PlatformVerificationFlow> verifier_;

  // Controls result of FakeGetCertificate.
  AttestationStatus certificate_status_;
  std::vector<std::string> fake_certificate_list_;
  size_t fake_certificate_index_;

  // Callback functions and data.
  PlatformVerificationFlow::Result result_;
  std::string challenge_salt_;
  std::string challenge_signature_;
  std::string certificate_;
};

TEST_F(PlatformVerificationFlowTest, Success) {
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestCertificate, certificate_);
  ::attestation::SignedData challenge_respoonse;
  challenge_respoonse.set_data(challenge_salt_);
  challenge_respoonse.set_signature(challenge_signature_);
  EXPECT_TRUE(
      AttestationClient::Get()
          ->GetTestInterface()
          ->VerifySimpleChallengeResponse(kTestChallenge, challenge_respoonse));
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabledByPolicy) {
  settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, false);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
}

TEST_F(PlatformVerificationFlowTest, NotVerifiedDueToUnspeciedFailure) {
  certificate_status_ = ATTESTATION_UNSPECIFIED_FAILURE;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, NotVerifiedDueToBadRequestFailure) {
  certificate_status_ = ATTESTATION_SERVER_BAD_REQUEST_FAILURE;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, ChallengeSigningError) {
  // Force the signing operation to fail.
  AttestationClient::Get()
      ->GetTestInterface()
      ->set_sign_simple_challenge_status(
          ::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, DBusFailure) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(::attestation::STATUS_DBUS_ERROR);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, AttestationServiceInternalError) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(
          ::attestation::STATUS_UNEXPECTED_DEVICE_ERROR);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, Timeout) {
  verifier_->set_timeout_delay(base::Seconds(0));
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::TIMEOUT, result_);
}

TEST_F(PlatformVerificationFlowTest, ExpiredCert) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(3);
  ASSERT_TRUE(
      GetFakeCertificatePEM(base::Days(-1), &fake_certificate_list_[0]));
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &fake_certificate_list_[1]));
  // This is the opportunistic renewal certificate. Send it back expired to test
  // that it does not pass through the certificate expiry check again.
  ASSERT_TRUE(
      GetFakeCertificatePEM(base::Days(-1), &fake_certificate_list_[2]));
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[1], certificate_);
  // Once for the expired certificate, once for the almost expired certificate,
  // and once for the opportunistic renewal attempt.
  EXPECT_EQ(3ul, fake_certificate_index_);
}

TEST_F(PlatformVerificationFlowTest, ExpiredIntermediateCert) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(2);
  std::string leaf_cert;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(60), &leaf_cert));
  std::string intermediate_cert;
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(-1), &intermediate_cert));
  fake_certificate_list_[0] = leaf_cert + intermediate_cert;
  ASSERT_TRUE(
      GetFakeCertificatePEM(base::Days(90), &fake_certificate_list_[1]));
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[1], certificate_);
  // Once for the expired intermediate, once for the renewal.
  EXPECT_EQ(2ul, fake_certificate_index_);
}

TEST_F(PlatformVerificationFlowTest, AsyncRenewalMultipleHits) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(4);
  ASSERT_TRUE(GetFakeCertificatePEM(base::Days(1), &fake_certificate_list_[0]));
  std::fill(fake_certificate_list_.begin() + 1, fake_certificate_list_.end(),
            fake_certificate_list_[0]);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[0], certificate_);
  // Once per challenge and only one renewal.
  EXPECT_EQ(4ul, fake_certificate_index_);
}

TEST_F(PlatformVerificationFlowTest, CertificateNotPEM) {
  ExpectAttestationFlow();
  fake_certificate_list_.push_back("invalid_pem");
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[0], certificate_);
}

TEST_F(PlatformVerificationFlowTest, CertificateNotX509) {
  ExpectAttestationFlow();
  std::string not_x509 =
      "-----BEGIN CERTIFICATE-----\n"
      "Vm0wd2QyUXlWa1pOVldoVFYwZDRWVll3WkRSV1JteFZVMjA1VjFadGVEQmFWVll3WVd4YWMx"
      "TnNiRlZXYkhCUVdWZHplRll5VGtWUwpiSEJPVWpKb1RWZFhkR0ZUTWs1eVRsWmtZUXBTYlZK"
      "d1ZXcEtiMDFzWkZkV2JVWlVZbFpHTTFSc1dsZFZaM0JwVTBWS2RsWkdZM2hpCk1rbDRWMnhX"
      "VkdGc1NsaFpiRnBIVGtaYVNFNVZkRmRhTTBKd1ZteGFkMVpXWkZobFIzUnBDazFXY0VoV01X"
      "aHpZV3hLV1ZWc1ZscGkKUm5Cb1dsZDRXbVZWTlZkYVIyaFdWMFZLVlZacVFsZFRNVnBYV2ta"
      "b2JGSXpVbGREYlVwWFYydG9WMDF1VW5aWmExcExZMnMxVjFScwpjRmdLVTBWS1dWWnRjRWRq"
      "TWs1elYyNVNVRll5YUZkV01GWkxWbXhhVlZGc1pGUk5Wa3BJVmpKNGIyRnNTbGxWYkVKRVlr"
      "VndWbFZ0CmVHOVdNVWw2WVVkb1dGWnNjRXhXTUZwWFpGWk9jd3BhUjJkTFdWUkNkMDVzV2to"
      "TlZGSmFWbTFTUjFSV1ZsZFdNa3BKVVd4a1YwMUcKV2t4V01uaGhWMGRXU0dSRk9WTk5WWEJa"
      "Vm1wR2IySXhXblJTV0hCV1lrWktSVmxZY0VkbGJGbDVDbU5GVGxkTlZtdzJWbGMxWVZkdApS"
      "WGhqUlhSaFZucEdTRlZ0TVZOU2QzQmhVbTFPVEZkWGVGWmtNbEY0VjJ0V1UySkhVbFpVVjNS"
      "M1pXeFdXR1ZHWkZWaVJYQmFWa2QwCk5GSkdjRFlLVFVSc1JGcDZNRGxEWnowOUNnPT0K\n"
      "-----END CERTIFICATE-----\n";
  fake_certificate_list_.push_back(not_x509);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[0], certificate_);
}

TEST_F(PlatformVerificationFlowTest, UnsupportedMode) {
  fake_delegate_.set_is_in_supported_mode(false);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, AttestationNotPrepared) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);
  verifier_->ChallengePlatformKey(user_, kTestID, kTestChallenge,
                                  CreateChallengeCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

}  // namespace attestation
}  // namespace ash
