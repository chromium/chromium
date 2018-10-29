// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/attestation/fake_certificate.h"
#include "chrome/browser/chromeos/attestation/platform_verification_flow.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/attestation/attestation.pb.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;
using testing::WithArgs;

namespace chromeos {
namespace attestation {

namespace {

const char kTestID[] = "test_id";
const char kTestChallenge[] = "test_challenge";
const char kTestSignedData[] = "test_challenge_with_salt";
const char kTestSignature[] = "test_signature";
const char kTestCertificate[] = "test_certificate";
const char kTestEmail[] = "test_email@chromium.org";
const char kTestURL[] = "http://mytestdomain/test";

class FakeDelegate : public PlatformVerificationFlow::Delegate {
 public:
  FakeDelegate()
      : url_(kTestURL),
        is_permitted_by_user_(true),
        is_in_supported_mode_(true) {
    // Configure a user for the mock user manager.
    mock_user_manager_.SetActiveUser(AccountId::FromUserEmail(kTestEmail));
  }
  ~FakeDelegate() override {}

  const GURL& GetURL(content::WebContents* web_contents) override {
    return url_;
  }

  user_manager::User* GetUser(content::WebContents* web_contents) override {
    return mock_user_manager_.GetActiveUser();
  }

  bool IsPermittedByUser(content::WebContents* web_contents) override {
    return is_permitted_by_user_;
  }

  bool IsInSupportedMode(content::WebContents* web_contents) override {
    return is_in_supported_mode_;
  }

  void set_url(const GURL& url) {
    url_ = url;
  }

  void set_is_permitted_by_user(bool is_permitted_by_user) {
    is_permitted_by_user_ = is_permitted_by_user;
  }

  void set_is_in_supported_mode(bool is_in_supported_mode) {
    is_in_supported_mode_ = is_in_supported_mode;
  }

 private:
  MockUserManager mock_user_manager_;
  GURL url_;
  bool is_permitted_by_user_;
  bool is_in_supported_mode_;

  DISALLOW_COPY_AND_ASSIGN(FakeDelegate);
};

}  // namespace

class PlatformVerificationFlowTest : public ::testing::Test {
 public:
  PlatformVerificationFlowTest()
      : certificate_status_(ATTESTATION_SUCCESS),
        fake_certificate_index_(0),
        sign_challenge_success_(true),
        result_(PlatformVerificationFlow::INTERNAL_ERROR) {}

  void SetUp() {
    // Create a verifier for tests to call.
    verifier_ = new PlatformVerificationFlow(&mock_attestation_flow_,
                                             &mock_async_caller_,
                                             &fake_cryptohome_client_,
                                             &fake_delegate_);

    // Create callbacks for tests to use with verifier_.
    callback_ = base::Bind(&PlatformVerificationFlowTest::FakeChallengeCallback,
                           base::Unretained(this));

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, true);
  }

  void ExpectAttestationFlow() {
    // When consent is not given or the feature is disabled, it is important
    // that there are no calls to the attestation service.  Thus, a test must
    // explicitly expect these calls or the mocks will fail the test.

    const AccountId account_id = AccountId::FromUserEmail(kTestEmail);
    // Configure the mock AttestationFlow to call FakeGetCertificate.
    EXPECT_CALL(mock_attestation_flow_,
                GetCertificate(PROFILE_CONTENT_PROTECTION_CERTIFICATE,
                               account_id, kTestID, _, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &PlatformVerificationFlowTest::FakeGetCertificate)));

    // Configure the mock AsyncMethodCaller to call FakeSignChallenge.
    std::string expected_key_name = std::string(kContentProtectionKeyPrefix) +
                                    std::string(kTestID);
    EXPECT_CALL(mock_async_caller_,
                TpmAttestationSignSimpleChallenge(
                    KEY_USER, cryptohome::Identification(account_id),
                    expected_key_name, kTestChallenge, _))
        .WillRepeatedly(WithArgs<4>(
            Invoke(this, &PlatformVerificationFlowTest::FakeSignChallenge)));
  }

  void FakeGetCertificate(
      const AttestationFlow::CertificateCallback& callback) {
    std::string certificate =
        (fake_certificate_index_ < fake_certificate_list_.size()) ?
            fake_certificate_list_[fake_certificate_index_] : kTestCertificate;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, certificate_status_, certificate));
    ++fake_certificate_index_;
  }

  void FakeSignChallenge(
      const cryptohome::AsyncMethodCaller::DataCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, sign_challenge_success_,
                                  CreateFakeResponseProto()));
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

  std::string CreateFakeResponseProto() {
    SignedData pb;
    pb.set_data(kTestSignedData);
    pb.set_signature(kTestSignature);
    std::string serial;
    CHECK(pb.SerializeToString(&serial));
    return serial;
  }

 protected:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  StrictMock<MockAttestationFlow> mock_attestation_flow_;
  cryptohome::MockAsyncMethodCaller mock_async_caller_;
  chromeos::FakeCryptohomeClient fake_cryptohome_client_;
  FakeDelegate fake_delegate_;
  ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<PlatformVerificationFlow> verifier_;

  // Controls result of FakeGetCertificate.
  AttestationStatus certificate_status_;
  std::vector<std::string> fake_certificate_list_;
  size_t fake_certificate_index_;

  // Controls result of FakeSignChallenge.
  bool sign_challenge_success_;

  // Callback functions and data.
  PlatformVerificationFlow::ChallengeCallback callback_;
  PlatformVerificationFlow::Result result_;
  std::string challenge_salt_;
  std::string challenge_signature_;
  std::string certificate_;
};

TEST_F(PlatformVerificationFlowTest, Success) {
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(kTestSignedData, challenge_salt_);
  EXPECT_EQ(kTestSignature, challenge_signature_);
  EXPECT_EQ(kTestCertificate, certificate_);
}

TEST_F(PlatformVerificationFlowTest, NotPermittedByUser) {
  fake_delegate_.set_is_permitted_by_user(false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::USER_REJECTED, result_);
}

TEST_F(PlatformVerificationFlowTest, FeatureDisabledByPolicy) {
  settings_helper_.SetBoolean(kAttestationForContentProtectionEnabled, false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::POLICY_REJECTED, result_);
}

TEST_F(PlatformVerificationFlowTest, NotVerifiedDueToUnspeciedFailure) {
  certificate_status_ = ATTESTATION_UNSPECIFIED_FAILURE;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, NotVerifiedDueToBadRequestFailure) {
  certificate_status_ = ATTESTATION_SERVER_BAD_REQUEST_FAILURE;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, ChallengeSigningError) {
  sign_challenge_success_ = false;
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, DBusFailure) {
  fake_cryptohome_client_.SetServiceIsAvailable(false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::INTERNAL_ERROR, result_);
}

TEST_F(PlatformVerificationFlowTest, Timeout) {
  verifier_->set_timeout_delay(base::TimeDelta::FromSeconds(0));
  ExpectAttestationFlow();
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::TIMEOUT, result_);
}

TEST_F(PlatformVerificationFlowTest, ExpiredCert) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(3);
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(-1),
                                    &fake_certificate_list_[0]));
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(1),
                                    &fake_certificate_list_[1]));
  // This is the opportunistic renewal certificate. Send it back expired to test
  // that it does not pass through the certificate expiry check again.
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(-1),
                                    &fake_certificate_list_[2]));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
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
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(60), &leaf_cert));
  std::string intermediate_cert;
  ASSERT_TRUE(
      GetFakeCertificatePEM(base::TimeDelta::FromDays(-1), &intermediate_cert));
  fake_certificate_list_[0] = leaf_cert + intermediate_cert;
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(90),
                                    &fake_certificate_list_[1]));
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[1], certificate_);
  // Once for the expired intermediate, once for the renewal.
  EXPECT_EQ(2ul, fake_certificate_index_);
}

TEST_F(PlatformVerificationFlowTest, AsyncRenewalMultipleHits) {
  ExpectAttestationFlow();
  fake_certificate_list_.resize(4);
  ASSERT_TRUE(GetFakeCertificatePEM(base::TimeDelta::FromDays(1),
                                    &fake_certificate_list_[0]));
  std::fill(fake_certificate_list_.begin() + 1, fake_certificate_list_.end(),
            fake_certificate_list_[0]);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[0], certificate_);
  // Once per challenge and only one renewal.
  EXPECT_EQ(4ul, fake_certificate_index_);
}

TEST_F(PlatformVerificationFlowTest, CertificateNotPEM) {
  ExpectAttestationFlow();
  fake_certificate_list_.push_back("invalid_pem");
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
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
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::SUCCESS, result_);
  EXPECT_EQ(fake_certificate_list_[0], certificate_);
}

TEST_F(PlatformVerificationFlowTest, UnsupportedMode) {
  fake_delegate_.set_is_in_supported_mode(false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

TEST_F(PlatformVerificationFlowTest, AttestationNotPrepared) {
  fake_cryptohome_client_.set_tpm_attestation_is_enrolled(false);
  fake_cryptohome_client_.set_tpm_attestation_is_prepared(false);
  verifier_->ChallengePlatformKey(NULL, kTestID, kTestChallenge, callback_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PlatformVerificationFlow::PLATFORM_NOT_VERIFIED, result_);
}

}  // namespace attestation
}  // namespace chromeos
