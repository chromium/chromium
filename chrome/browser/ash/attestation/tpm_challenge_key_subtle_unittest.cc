// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/tpm_challenge_key_subtle.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/mock_machine_certificate_uploader.h"
#include "chrome/browser/ash/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/fake_user_private_token_kpm_service.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/key_permissions/mock_key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/attestation/fake_attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/components/kiosk/kiosk_test_utils.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::Invoke;
using testing::StrictMock;

namespace ash {
namespace attestation {
namespace {

constexpr char kTestUserEmail[] = "test@google.com";
constexpr char kTestUserGaiaId[] = "test_gaia_id";
constexpr char kEmptyKeyName[] = "";
constexpr char kNonDefaultKeyName[] = "key_name_123";
constexpr char kFakeCertificate[] = "fake_cert";
constexpr char kUnsupportedVAFlowTypeKey[] = "unsupported VA flow type";

std::string GetDefaultKeyName(::attestation::VerifiedAccessFlow type,
                              const std::string& username = std::string()) {
  switch (type) {
    case ::attestation::ENTERPRISE_MACHINE:
      return kEnterpriseMachineKey;
    case ::attestation::ENTERPRISE_USER:
      return kEnterpriseUserKey;
    default:
      return kUnsupportedVAFlowTypeKey;
  }
}

std::string GetChallenge() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'c',  'h',
                                 'a', 'l', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

std::string GetChallengeResponse(bool include_spkac) {
  return AttestationClient::Get()
      ->GetTestInterface()
      ->GetEnterpriseChallengeFakeSignature(GetChallenge(), include_spkac);
}

std::string GetPublicKey() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'p',  'u',
                                 'b', 'k', 0xfd, 0xfe, 0xff};
  return std::string(reinterpret_cast<const char*>(kBuffer), sizeof(kBuffer));
}

std::vector<uint8_t> GetPublicKeyBin() {
  constexpr uint8_t kBuffer[] = {0x0, 0x1, 0x2,  'p',  'u',
                                 'b', 'k', 0xfd, 0xfe, 0xff};
  return std::vector<uint8_t>(std::begin(kBuffer), std::end(kBuffer));
}

class CallbackObserver {
 public:
  TpmChallengeKeyCallback GetCallback() {
    return base::BindOnce(&CallbackObserver::Callback, base::Unretained(this));
  }

  bool IsResultReceived() const { return result_.has_value(); }

  const TpmChallengeKeyResult& GetResult() const {
    CHECK(result_.has_value()) << "Callback was never called";
    return result_.value();
  }

  void WaitForCallback() { loop_.Run(); }

 private:
  void Callback(const TpmChallengeKeyResult& result) {
    CHECK(!result_.has_value()) << "Callback was called more than once";
    result_ = result;
    loop_.Quit();
  }

  base::RunLoop loop_;
  std::optional<TpmChallengeKeyResult> result_;
};

template <typename T>
struct CallbackHolder {
  void SaveCallback(T new_callback) {
    CHECK(callback.is_null());
    callback = std::move(new_callback);
  }

  T callback;
};

//================= MockableFakeAttestationFlow ================================

class MockableFakeAttestationFlow : public MockAttestationFlow {
 public:
  MockableFakeAttestationFlow() {
    ON_CALL(*this, GetCertificate(_, _, _, _, _, _, _, _))
        .WillByDefault(
            Invoke(this, &MockableFakeAttestationFlow::GetCertificateInternal));
  }
  ~MockableFakeAttestationFlow() override = default;
  void set_status(AttestationStatus status) { status_ = status; }

 private:
  void GetCertificateInternal(
      AttestationCertificateProfile /*certificate_profile*/,
      const AccountId& account_id,
      const std::string& /*request_origin*/,
      bool /*force_new_key*/,
      ::attestation::KeyType /*key_crypto_type*/,
      const std::string& key_name,
      const std::optional<AttestationFlow::CertProfileSpecificData>&
          profile_specific_data,
      CertificateCallback callback) {
    std::string certificate;
    if (status_ == ATTESTATION_SUCCESS) {
      certificate = certificate_;
      AttestationClient::Get()
          ->GetTestInterface()
          ->GetMutableKeyInfoReply(cryptohome::Identification(account_id).id(),
                                   key_name)
          ->set_public_key(public_key_);
    }
    std::move(callback).Run(status_, certificate);
  }
  AttestationStatus status_ = ATTESTATION_SUCCESS;
  const std::string certificate_ = kFakeCertificate;
  const std::string public_key_ = GetPublicKey();
};

//==================== TpmChallengeKeySubtleTestBase ===========================

// Describes which kind of profile will be used with TpmChallengeKeySubtle.
enum class TestProfileChoice {
  // Pass nullptr as Profile.
  kNoProfile,
  // Pass the sign-in Profile.
  kSigninProfile,
  // Pass the Profile of an unmanaged user.
  kUnmanagedProfile,
  // Pass the Profile of a managed, unaffiliated user.
  kUnaffiliatedProfile,
  // Pass the Profile of an affiliated user.
  kAffiliatedProfile,
  // Pass the Profile of signed-in kiosk user.
  kKioskProfile
};

class TpmChallengeKeySubtleTestBase : public ::testing::Test {
 public:
  explicit TpmChallengeKeySubtleTestBase(TestProfileChoice test_profile_choice);
  ~TpmChallengeKeySubtleTestBase() override;

 protected:
  // ::testing::Test:
  void SetUp() override;

  TestingProfile* CreateUserProfile(bool is_managed, bool is_affiliated);
  virtual TestingProfile* CreateKioskProfile();
  TestingProfile* GetProfile();
  ScopedCrosSettingsTestHelper* GetCrosSettingsHelper();
  StubInstallAttributes* GetInstallAttributes();
  std::string GetTestingProfileDomainName();

  // Runs StartPrepareKeyStep and checks that the result is equal to
  // |public_key|.
  void RunOneStepAndExpect(::attestation::VerifiedAccessFlow flow_type,
                           bool will_register_key,
                           const std::string& key_name,
                           const TpmChallengeKeyResult& public_key);
  // Runs StartPrepareKeyStep and checks that the result is success. Then runs
  // StartSignChallengeStep and checks that the result is equal to
  // |challenge_response|.
  void RunTwoStepsAndExpect(::attestation::VerifiedAccessFlow flow_type,
                            bool will_register_key,
                            const std::string& key_name,
                            const TpmChallengeKeyResult& challenge_response);
  // Runs first two steps and checks that results are success. Then runs
  // StartRegisterKeyStep and checks that the result is equal to
  // |register_result|.
  void RunThreeStepsAndExpect(::attestation::VerifiedAccessFlow flow_type,
                              bool will_register_key,
                              const std::string& key_name,
                              const TpmChallengeKeyResult& register_result);

  virtual ::attestation::KeyType KeyCryptoType();

 protected:
  const TestProfileChoice test_profile_choice_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<MockableFakeAttestationFlow> mock_attestation_flow_;
  std::unique_ptr<platform_keys::MockKeyPermissionsManager>
      system_token_key_permissions_manager_;
  std::unique_ptr<platform_keys::MockKeyPermissionsManager>
      user_private_token_key_permissions_manager_;
  MockMachineCertificateUploader mock_cert_uploader_;

  std::unique_ptr<TpmChallengeKeySubtle> challenge_key_subtle_;

  TestingProfileManager testing_profile_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  // A sign-in Profile is always created in SetUp().
  raw_ptr<TestingProfile> signin_profile_ = nullptr;
  // The profile that will be passed to TpmChallengeKeySubtle - can be nullptr.
  raw_ptr<TestingProfile> testing_profile_ = nullptr;
};

TpmChallengeKeySubtleTestBase::TpmChallengeKeySubtleTestBase(
    TestProfileChoice test_profile_choice)
    : test_profile_choice_(test_profile_choice),
      testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  ::chromeos::TpmManagerClient::InitializeFake();
  AttestationClient::InitializeFake();
  CHECK(testing_profile_manager_.SetUp());

  challenge_key_subtle_ = std::make_unique<TpmChallengeKeySubtleImpl>(
      &mock_attestation_flow_, &mock_cert_uploader_);

  // By default make it reply that the certificate is already uploaded.
  ON_CALL(mock_cert_uploader_, WaitForUploadComplete)
      .WillByDefault(base::test::RunOnceCallbackRepeatedly<0>(true));
}

TpmChallengeKeySubtleTestBase::~TpmChallengeKeySubtleTestBase() {
  AttestationClient::Shutdown();
  ::chromeos::TpmManagerClient::Shutdown();
}

void TpmChallengeKeySubtleTestBase::SetUp() {
  signin_profile_ =
      testing_profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
  switch (test_profile_choice_) {
    case TestProfileChoice::kNoProfile:
      testing_profile_ = nullptr;
      break;
    case TestProfileChoice::kSigninProfile:
      testing_profile_ = signin_profile_;
      break;
    case TestProfileChoice::kUnmanagedProfile:
      testing_profile_ =
          CreateUserProfile(/*is_managed=*/false, /*is_affiliated=*/false);
      break;
    case TestProfileChoice::kUnaffiliatedProfile:
      testing_profile_ =
          CreateUserProfile(/*is_managed=*/true, /*is_affiliated=*/false);
      break;
    case TestProfileChoice::kAffiliatedProfile:
      testing_profile_ =
          CreateUserProfile(/*is_managed=*/true, /*is_affiliated=*/true);
      break;
    case TestProfileChoice::kKioskProfile:
      testing_profile_ = CreateKioskProfile();
      break;
  }

  GetInstallAttributes()->SetCloudManaged(GetTestingProfileDomainName(),
                                          "device_id");

  system_token_key_permissions_manager_ =
      std::make_unique<platform_keys::MockKeyPermissionsManager>();
  platform_keys::KeyPermissionsManagerImpl::
      SetSystemTokenKeyPermissionsManagerForTesting(
          system_token_key_permissions_manager_.get());

  if (!testing_profile_) {
    return;
  }
  user_private_token_key_permissions_manager_ =
      std::make_unique<platform_keys::MockKeyPermissionsManager>();
  platform_keys::UserPrivateTokenKeyPermissionsManagerServiceFactory::
      GetInstance()
          ->SetTestingFactory(
              testing_profile_,
              base::BindRepeating(
                  &platform_keys::
                      BuildFakeUserPrivateTokenKeyPermissionsManagerService,
                  user_private_token_key_permissions_manager_.get()));
}

TestingProfile* TpmChallengeKeySubtleTestBase::CreateUserProfile(
    bool is_managed,
    bool is_affiliated) {
  TestingProfile* testing_profile =
      testing_profile_manager_.CreateTestingProfile(kTestUserEmail);
  CHECK(testing_profile);

  testing_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
      is_managed);

  auto test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  fake_user_manager_->AddUserWithAffiliation(test_account, is_affiliated);

  return testing_profile;
}

TestingProfile* TpmChallengeKeySubtleTestBase::CreateKioskProfile() {
  return nullptr;
}

TestingProfile* TpmChallengeKeySubtleTestBase::GetProfile() {
  return testing_profile_;
}

ScopedCrosSettingsTestHelper*
TpmChallengeKeySubtleTestBase::GetCrosSettingsHelper() {
  return signin_profile_->ScopedCrosSettingsTestHelper();
}

StubInstallAttributes* TpmChallengeKeySubtleTestBase::GetInstallAttributes() {
  return GetCrosSettingsHelper()->InstallAttributes();
}

std::string TpmChallengeKeySubtleTestBase::GetTestingProfileDomainName() {
  if (!testing_profile_ || testing_profile_ == signin_profile_) {
    // Fallback to the constant domain if there is no `testing_profile_` or
    // a default `signin_profile_` only.
    return "fake.domain";
  }
  return gaia::ExtractDomainName(testing_profile_->GetProfileUserName());
}

::attestation::KeyType TpmChallengeKeySubtleTestBase::KeyCryptoType() {
  return ::attestation::KEY_TYPE_RSA;
}

void TpmChallengeKeySubtleTestBase::RunOneStepAndExpect(
    ::attestation::VerifiedAccessFlow flow_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& public_key) {
  CallbackObserver callback_observer;
  challenge_key_subtle_->StartPrepareKeyStep(
      flow_type, will_register_key, KeyCryptoType(), key_name, GetProfile(),
      callback_observer.GetCallback(), /*signals=*/std::nullopt);
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), public_key);
}

void TpmChallengeKeySubtleTestBase::RunTwoStepsAndExpect(
    ::attestation::VerifiedAccessFlow flow_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& challenge_response) {
  RunOneStepAndExpect(flow_type, will_register_key, key_name,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartSignChallengeStep(
      GetChallenge(), callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), challenge_response);
}

void TpmChallengeKeySubtleTestBase::RunThreeStepsAndExpect(
    ::attestation::VerifiedAccessFlow flow_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& register_result) {
  RunTwoStepsAndExpect(flow_type, will_register_key, key_name,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(will_register_key)));

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartRegisterKeyStep(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), register_result);
}

//==============================================================================

// Tests all Profile* options where TpmChallengeKeySubtle can be used with
// device-wide keys, including passing profile=nullptr.
class DeviceKeysAccessTpmChallengeKeySubtleTest
    : public TpmChallengeKeySubtleTestBase,
      public ::testing::WithParamInterface<TestProfileChoice> {
 public:
  DeviceKeysAccessTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(GetParam()) {}
  ~DeviceKeysAccessTpmChallengeKeySubtleTest() = default;
};

INSTANTIATE_TEST_SUITE_P(
    DeviceKeysAccessTpmChallengeKeySubtleTest,
    DeviceKeysAccessTpmChallengeKeySubtleTest,
    testing::Values(TestProfileChoice::kNoProfile,
                    TestProfileChoice::kSigninProfile,
                    TestProfileChoice::kAffiliatedProfile));

// Tests TpmChallengeKeySubtle with the sign-in Profile only.
class SigninProfileTpmChallengeKeySubtleTest
    : public TpmChallengeKeySubtleTestBase {
 public:
  SigninProfileTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kSigninProfile) {}
  ~SigninProfileTpmChallengeKeySubtleTest() override = default;
};

// Tests TpmChallengeKeySubtle with an affiliated user profile only.
class AffiliatedUserTpmChallengeKeySubtleTest
    : public TpmChallengeKeySubtleTestBase {
 public:
  AffiliatedUserTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kAffiliatedProfile) {}
  ~AffiliatedUserTpmChallengeKeySubtleTest() override = default;
};

// Tests TpmChallengeKeySubtle with an unmanaged user profile only.
class UnmanagedUserTpmChallengeKeySubtleTest
    : public TpmChallengeKeySubtleTestBase {
 public:
  UnmanagedUserTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kUnmanagedProfile) {}
  ~UnmanagedUserTpmChallengeKeySubtleTest() override = default;
};

// Tests TpmChallengeKeySubtle with a managed, unaffiliated user profile only.
class UnaffiliatedUserTpmChallengeKeySubtleTest
    : public TpmChallengeKeySubtleTestBase {
 public:
  UnaffiliatedUserTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kUnaffiliatedProfile) {
  }
  ~UnaffiliatedUserTpmChallengeKeySubtleTest() override = default;
};

// Tests TpmChallengeKeySubtle in a kiosk session.
class KioskTpmChallengeKeySubtleTest : public TpmChallengeKeySubtleTestBase {
 public:
  KioskTpmChallengeKeySubtleTest()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kKioskProfile) {}
  ~KioskTpmChallengeKeySubtleTest() override = default;

  TestingProfile* CreateKioskProfile() override {
    chromeos::SetUpFakeKioskSession();
    auto* user = fake_user_manager_->GetActiveUser();
    kiosk_user_email_ = user->GetAccountId().GetUserEmail();
    auto* testing_profile =
        testing_profile_manager_.CreateTestingProfile(kiosk_user_email_);
    testing_profile->GetProfilePolicyConnector()->OverrideIsManagedForTesting(
        true);
    fake_user_manager_->AddUserWithAffiliation(user->GetAccountId(),
                                               /* is_affiliated= */ true);
    return testing_profile;
  }

  const std::string& kiosk_user_email() const { return kiosk_user_email_; }

 private:
  std::string kiosk_user_email_;
};

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest,
       UnsupportedVerifiedAccessFlow) {
  RunOneStepAndExpect(
      ::attestation::CBCM, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kVerifiedAccessFlowUnsupportedError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest,
       DeviceKeyNonEnterpriseDevice) {
  GetInstallAttributes()->SetConsumerOwned();

  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kNonEnterpriseDeviceError));
}

TEST_F(UnaffiliatedUserTpmChallengeKeySubtleTest, DeviceKeyUserNotManaged) {
  RunOneStepAndExpect(::attestation::ENTERPRISE_MACHINE,
                      /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserNotManagedError));
}

TEST_F(SigninProfileTpmChallengeKeySubtleTest, UserKeyUserKeyNotAvailable) {
  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_USER, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kUserKeyNotAvailableError));
}

// Checks that a user should be affiliated with a device
TEST_F(UnaffiliatedUserTpmChallengeKeySubtleTest, UserKeyUserNotAffiliated) {
  RunOneStepAndExpect(::attestation::ENTERPRISE_USER,
                      /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserNotManagedError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, DoesKeyExistDbusFailed) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
      ->set_status(::attestation::STATUS_DBUS_ERROR);

  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(TpmChallengeKeyResultCode::kDbusError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, GetCertificateFailed) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;

  mock_attestation_flow_.set_status(ATTESTATION_UNSPECIFIED_FAILURE);
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _, _, _, _));

  RunOneStepAndExpect(
      flow_type, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kGetCertificateFailedError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, KeyExists) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;

  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply(/*username=*/"", kEnterpriseMachineKey)
      ->set_public_key(GetPublicKey());

  RunOneStepAndExpect(flow_type, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, AttestationNotPrepared) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);

  RunOneStepAndExpect(::attestation::ENTERPRISE_MACHINE,
                      /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kResetRequiredError));
}

// Test that we get a proper error message in case we don't have a TPM.
TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, AttestationUnsupported) {
  AttestationClient::Get()->GetTestInterface()->ConfigureEnrollmentPreparations(
      false);
  chromeos::TpmManagerClient::Get()
      ->GetTestInterface()
      ->mutable_nonsensitive_status_reply()
      ->set_is_enabled(false);

  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kAttestationUnsupportedError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest,
       AttestationPreparedDbusFailed) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(::attestation::STATUS_DBUS_ERROR);

  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(TpmChallengeKeyResultCode::kDbusError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest,
       AttestationPreparedServiceInternalError) {
  AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(
          ::attestation::STATUS_NOT_AVAILABLE);

  RunOneStepAndExpect(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/false,
      kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kAttestationServiceInternalError));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest,
       DeviceKeyNotRegisteredSuccess) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;
  const std::string key_name = GetDefaultKeyName(flow_type);

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_key_label(key_name);
  expected_request.set_domain(std::string());
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(true);
  expected_request.set_flow_type(::attestation::ENTERPRISE_MACHINE);
  expected_request.set_include_certificate(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(flow_type, /*will_register_key=*/false, kEmptyKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, DeviceKeyRegisteredSuccess) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;
  const char* const key_name = kNonDefaultKeyName;

  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(_, _, _, _, ::attestation::KEY_TYPE_RSA, key_name, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_key_label(GetDefaultKeyName(flow_type));
  expected_request.set_key_name_for_spkac(key_name);
  expected_request.set_domain(std::string());
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(true);
  expected_request.set_flow_type(::attestation::ENTERPRISE_MACHINE);
  expected_request.set_include_certificate(false);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  AttestationClient::Get()->GetTestInterface()->AllowlistRegisterKey(
      /*username=*/"", key_name);

  EXPECT_CALL(
      *system_token_key_permissions_manager_,
      AllowKeyForUsage(/*callback=*/_, platform_keys::KeyUsage::kCorporate,
                       GetPublicKeyBin()))
      .WillOnce(RunOnceCallback<0>(chromeos::platform_keys::Status::kSuccess));

  RunThreeStepsAndExpect(flow_type, /*will_register_key=*/true, key_name,
                         TpmChallengeKeyResult::MakeSuccess());
}

class TpmChallengeKeySubtleTestECC : public TpmChallengeKeySubtleTestBase {
 public:
  TpmChallengeKeySubtleTestECC()
      : TpmChallengeKeySubtleTestBase(TestProfileChoice::kAffiliatedProfile) {}
  ~TpmChallengeKeySubtleTestECC() override = default;

  // TpmChallengeKeySubtleTestBase
  ::attestation::KeyType KeyCryptoType() override {
    return ::attestation::KEY_TYPE_ECC;
  }
};

TEST_F(TpmChallengeKeySubtleTestECC, DeviceKeyRegisteredSuccessECC) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;
  const char* const key_name = kNonDefaultKeyName;

  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(_, _, _, _, ::attestation::KEY_TYPE_ECC, key_name, _, _));

  RunOneStepAndExpect(flow_type, /*will_register_key=*/true, key_name,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest,
       UserKeyRegisteredSuccessDefaultNameRsa) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const std::string key_name = std::string(kEnterpriseUserKey);

  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(_, _, _, _, ::attestation::KEY_TYPE_RSA, key_name, _, _));

  RunOneStepAndExpect(flow_type, /*will_register_key=*/true, "",
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

TEST_F(TpmChallengeKeySubtleTestECC, UserKeyRegisteredSuccessDefaultNameECC) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const std::string key_name = std::string(kEnterpriseUserKey) + "-ecdsa";

  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(_, _, _, _, ::attestation::KEY_TYPE_ECC, key_name, _, _));

  RunOneStepAndExpect(flow_type, /*will_register_key=*/true, "",
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, UserKeyNotRegisteredSuccess) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const std::string key_name = GetDefaultKeyName(flow_type);

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(GetDefaultKeyName(flow_type));
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(false);
  expected_request.set_flow_type(::attestation::ENTERPRISE_USER);
  expected_request.set_include_certificate(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(flow_type, /*will_register_key=*/false, kEmptyKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, UserKeyRegisteredSuccess) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const char* const key_name = kNonDefaultKeyName;

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(kNonDefaultKeyName);
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(false);
  expected_request.set_flow_type(::attestation::ENTERPRISE_USER);
  expected_request.set_include_certificate(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  AttestationClient::Get()->GetTestInterface()->AllowlistRegisterKey(
      kTestUserEmail, key_name);

  EXPECT_CALL(
      *user_private_token_key_permissions_manager_,
      AllowKeyForUsage(/*callback=*/_, platform_keys::KeyUsage::kCorporate,
                       GetPublicKeyBin()))
      .WillOnce(RunOnceCallback<0>(chromeos::platform_keys::Status::kSuccess));

  RunThreeStepsAndExpect(flow_type, /*will_register_key=*/true, key_name,
                         TpmChallengeKeyResult::MakeSuccess());
}

TEST_P(DeviceKeysAccessTpmChallengeKeySubtleTest, SignChallengeFailed) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_MACHINE;

  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(_, _, _, _, _, GetDefaultKeyName(flow_type), _, _));

  // The signing operations fails because we don't allowlist any key.
  RunTwoStepsAndExpect(
      flow_type, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kSignChallengeFailedError));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, RestorePreparedKeyState) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const char* const key_name = kNonDefaultKeyName;

  // Inject mocks into the next result of CreateForPreparedKey.
  TpmChallengeKeySubtleFactory::SetForTesting(
      std::make_unique<TpmChallengeKeySubtleImpl>(&mock_attestation_flow_,
                                                  &mock_cert_uploader_));

  challenge_key_subtle_ = TpmChallengeKeySubtleFactory::CreateForPreparedKey(
      flow_type, /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
      key_name, GetPublicKey(), GetProfile());

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(kNonDefaultKeyName);
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(false);
  expected_request.set_flow_type(::attestation::ENTERPRISE_USER);
  expected_request.set_include_certificate(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  {
    CallbackObserver callback_observer;
    challenge_key_subtle_->StartSignChallengeStep(
        GetChallenge(), callback_observer.GetCallback());
    callback_observer.WaitForCallback();

    EXPECT_EQ(callback_observer.GetResult(),
              TpmChallengeKeyResult::MakeChallengeResponse(
                  GetChallengeResponse(/*include_spkac=*/true)));
  }

  AttestationClient::Get()->GetTestInterface()->AllowlistRegisterKey(
      kTestUserEmail, key_name);

  EXPECT_CALL(
      *user_private_token_key_permissions_manager_,
      AllowKeyForUsage(/*callback=*/_, platform_keys::KeyUsage::kCorporate,
                       GetPublicKeyBin()))
      .WillOnce(RunOnceCallback<0>(chromeos::platform_keys::Status::kSuccess));

  {
    CallbackObserver callback_observer;
    challenge_key_subtle_->StartRegisterKeyStep(
        callback_observer.GetCallback());
    callback_observer.WaitForCallback();

    EXPECT_EQ(callback_observer.GetResult(),
              TpmChallengeKeyResult::MakeSuccess());
  }
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, KeyRegistrationFailed) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const char* const key_name = kNonDefaultKeyName;

  // Inject mocks into the next result of CreateForPreparedKey.
  TpmChallengeKeySubtleFactory::SetForTesting(
      std::make_unique<TpmChallengeKeySubtleImpl>(&mock_attestation_flow_,
                                                  &mock_cert_uploader_));

  challenge_key_subtle_ = TpmChallengeKeySubtleFactory::CreateForPreparedKey(
      flow_type, /*will_register_key=*/true, ::attestation::KEY_TYPE_RSA,
      key_name, GetPublicKey(), GetProfile());

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartRegisterKeyStep(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakeError(
                TpmChallengeKeyResultCode::kKeyRegistrationFailedError));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, GetPublicKeyFailed) {
  const char* const key_name = kNonDefaultKeyName;

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  // Force the attestation client to report absence even after successful
  // attestation flow.
  AttestationClient::Get()
      ->GetTestInterface()
      ->GetMutableKeyInfoReply(/*username=*/"", key_name)
      ->set_status(::attestation::STATUS_INVALID_PARAMETER);

  RunOneStepAndExpect(::attestation::ENTERPRISE_MACHINE,
                      /*will_register_key=*/true, key_name,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kGetPublicKeyFailedError));
}

TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, WaitForCertificateUploaded) {
  const char* const key_name = kNonDefaultKeyName;

  using CallbackHolderT =
      CallbackHolder<MachineCertificateUploader::UploadCallback>;
  CallbackHolderT callback_holder;
  EXPECT_CALL(mock_cert_uploader_, WaitForUploadComplete)
      .WillOnce(
          testing::Invoke(&callback_holder, &CallbackHolderT::SaveCallback));

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartPrepareKeyStep(
      ::attestation::ENTERPRISE_MACHINE, /*will_register_key=*/true,
      ::attestation::KEY_TYPE_RSA, key_name, GetProfile(),
      callback_observer.GetCallback(),
      /*signals=*/std::nullopt);

  // |challenge_key_subtle_| should wait until the certificate is uploaded.
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_FALSE(callback_observer.IsResultReceived());

  // Emulate callback from the certificate uploader, |challenge_key_subtle_|
  // should be able to continue now.
  std::move(callback_holder.callback).Run(true);
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

// Check that the class works when MachineCertificateUploader is not provided
// (e.g. if device is managed by Active Directory).
TEST_F(AffiliatedUserTpmChallengeKeySubtleTest, NoCertificateUploaderSuccess) {
  const char* const key_name = kNonDefaultKeyName;

  challenge_key_subtle_ = std::make_unique<TpmChallengeKeySubtleImpl>(
      &mock_attestation_flow_, /*machine_certificate_uploader=*/nullptr);

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  RunOneStepAndExpect(::attestation::ENTERPRISE_USER,
                      /*will_register_key=*/true, key_name,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

// Checks that the include_customer_id field is true in kiosk sessions.
TEST_F(KioskTpmChallengeKeySubtleTest, IncludesCustomerId) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::ENTERPRISE_USER;
  const std::string key_name = GetDefaultKeyName(flow_type);

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, key_name, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kiosk_user_email());
  expected_request.set_key_label(GetDefaultKeyName(flow_type));
  expected_request.set_domain(kiosk_user_email());
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(true);
  expected_request.set_flow_type(::attestation::ENTERPRISE_USER);
  expected_request.set_include_certificate(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(flow_type, /*will_register_key=*/false, kEmptyKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

// TODO(b/277707201): remove once user email from login screen can be passed to
// TpmChallengeKeySubtle.
TEST_F(SigninProfileTpmChallengeKeySubtleTest,
       DeviceTrustConnectorProfileRequired) {
  RunOneStepAndExpect(
      ::attestation::DEVICE_TRUST_CONNECTOR, /*will_register_key=*/false,
      kNonDefaultKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kUserKeyNotAvailableError));
}

TEST_F(UnmanagedUserTpmChallengeKeySubtleTest,
       DeviceTrustConnectorUserNotManaged) {
  RunOneStepAndExpect(::attestation::DEVICE_TRUST_CONNECTOR,
                      /*will_register_key=*/false, kNonDefaultKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserNotManagedError));
}

TEST_F(UnaffiliatedUserTpmChallengeKeySubtleTest, DeviceTrustConnectorSuccess) {
  const ::attestation::VerifiedAccessFlow flow_type =
      ::attestation::DEVICE_TRUST_CONNECTOR;
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, _, kNonDefaultKeyName, _, _));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_key_label(kNonDefaultKeyName);
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  expected_request.set_include_customer_id(false);
  expected_request.set_flow_type(::attestation::DEVICE_TRUST_CONNECTOR);
  expected_request.set_include_certificate(true);
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(flow_type, /*will_register_key=*/false,
                       kNonDefaultKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

}  // namespace
}  // namespace attestation
}  // namespace ash
