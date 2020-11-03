// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key_subtle.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "chrome/browser/chromeos/attestation/tpm_challenge_key_result.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/fake_user_private_token_kpm_service.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/mock_key_permissions_manager.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/attestation/fake_attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunOnceCallback;
using testing::_;
using testing::StrictMock;

namespace chromeos {
namespace attestation {
namespace {

constexpr char kTestUserEmail[] = "test@google.com";
constexpr char kTestUserDomain[] = "google.com";
constexpr char kTestUserGaiaId[] = "test_gaia_id";
constexpr char kEmptyKeyName[] = "";
constexpr char kNonDefaultKeyName[] = "key_name_123";

const char* GetDefaultKeyName(AttestationKeyType type) {
  switch (type) {
    case KEY_DEVICE:
      return kEnterpriseMachineKey;
    case KEY_USER:
      return kEnterpriseUserKey;
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

class CallbackObserver {
 public:
  TpmChallengeKeyCallback GetCallback() {
    return base::BindOnce(&CallbackObserver::Callback, base::Unretained(this));
  }

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
  base::Optional<TpmChallengeKeyResult> result_;
};

//================== TpmChallengeKeySubtleTest =================================

class TpmChallengeKeySubtleTest : public ::testing::Test {
 public:
  TpmChallengeKeySubtleTest();
  ~TpmChallengeKeySubtleTest();

 protected:
  void InitSigninProfile();
  void InitUnaffiliatedProfile();
  void InitAffiliatedProfile();
  void InitAfterProfileCreated();

  TestingProfile* CreateUserProfile(bool is_affiliated);
  TestingProfile* GetProfile();
  chromeos::ScopedCrosSettingsTestHelper* GetCrosSettingsHelper();
  chromeos::StubInstallAttributes* GetInstallAttributes();

  // Runs StartPrepareKeyStep and checks that the result is equal to
  // |public_key|.
  void RunOneStepAndExpect(AttestationKeyType key_type,
                           bool will_register_key,
                           const std::string& key_name,
                           const TpmChallengeKeyResult& public_key);
  // Runs StartPrepareKeyStep and checks that the result is success. Then runs
  // StartSignChallengeStep and checks that the result is equal to
  // |challenge_response|.
  void RunTwoStepsAndExpect(AttestationKeyType key_type,
                            bool will_register_key,
                            const std::string& key_name,
                            const TpmChallengeKeyResult& challenge_response);
  // Runs first two steps and checks that results are success. Then runs
  // StartRegisterKeyStep and checks that the result is equal to
  // |register_result|.
  void RunThreeStepsAndExpect(AttestationKeyType key_type,
                              bool will_register_key,
                              const std::string& key_name,
                              const TpmChallengeKeyResult& register_result);

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  StrictMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  cryptohome::MockAsyncMethodCaller* mock_async_method_caller_ = nullptr;
  chromeos::FakeCryptohomeClient cryptohome_client_;
  std::unique_ptr<platform_keys::MockKeyPermissionsManager>
      system_token_key_permissions_manager_;
  std::unique_ptr<platform_keys::MockKeyPermissionsManager>
      user_private_token_key_permissions_manager_;

  TestingProfileManager testing_profile_manager_;
  chromeos::FakeChromeUserManager fake_user_manager_;
  TestingProfile* testing_profile_ = nullptr;

  std::unique_ptr<TpmChallengeKeySubtleImpl> challenge_key_subtle_;
};

TpmChallengeKeySubtleTest::TpmChallengeKeySubtleTest()
    : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  ::chromeos::AttestationClient::InitializeFake();
  CHECK(testing_profile_manager_.SetUp());

  mock_async_method_caller_ =
      new StrictMock<cryptohome::MockAsyncMethodCaller>();
  // Ownership of mock_async_method_caller_ is transferred to
  // AsyncMethodCaller::InitializeForTesting.
  cryptohome::AsyncMethodCaller::InitializeForTesting(
      mock_async_method_caller_);

  challenge_key_subtle_ =
      std::make_unique<TpmChallengeKeySubtleImpl>(&mock_attestation_flow_);

  cryptohome_client_.set_tpm_attestation_public_key(
      CryptohomeClient::TpmAttestationDataResult{/*success=*/true,
                                                 GetPublicKey()});
}

TpmChallengeKeySubtleTest::~TpmChallengeKeySubtleTest() {
  cryptohome::AsyncMethodCaller::Shutdown();
  ::chromeos::AttestationClient::Shutdown();
}

void TpmChallengeKeySubtleTest::InitSigninProfile() {
  testing_profile_ =
      testing_profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
  InitAfterProfileCreated();
}

void TpmChallengeKeySubtleTest::InitUnaffiliatedProfile() {
  testing_profile_ = CreateUserProfile(/*is_affiliated=*/false);
  InitAfterProfileCreated();
}

void TpmChallengeKeySubtleTest::InitAffiliatedProfile() {
  testing_profile_ = CreateUserProfile(/*is_affiliated=*/true);
  InitAfterProfileCreated();
  GetProfile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAttestationEnabled, std::make_unique<base::Value>(true));
}

void TpmChallengeKeySubtleTest::InitAfterProfileCreated() {
  GetInstallAttributes()->SetCloudManaged("google.com", "device_id");

  GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      true);

  user_private_token_key_permissions_manager_ =
      std::make_unique<platform_keys::MockKeyPermissionsManager>();
  platform_keys::UserPrivateTokenKeyPermissionsManagerServiceFactory::
      GetInstance()
          ->SetTestingFactory(
              GetProfile(),
              base::BindRepeating(
                  &platform_keys::
                      BuildFakeUserPrivateTokenKeyPermissionsManagerService,
                  user_private_token_key_permissions_manager_.get()));

  system_token_key_permissions_manager_ =
      std::make_unique<platform_keys::MockKeyPermissionsManager>();
  platform_keys::KeyPermissionsManagerImpl::
      SetSystemTokenKeyPermissionsManagerForTesting(
          system_token_key_permissions_manager_.get());
}

TestingProfile* TpmChallengeKeySubtleTest::CreateUserProfile(
    bool is_affiliated) {
  TestingProfile* testing_profile =
      testing_profile_manager_.CreateTestingProfile(kTestUserEmail);
  CHECK(testing_profile);

  auto test_account =
      AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
  fake_user_manager_.AddUserWithAffiliation(test_account, is_affiliated);

  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      fake_user_manager_.GetPrimaryUser(), testing_profile);

  return testing_profile;
}

TestingProfile* TpmChallengeKeySubtleTest::GetProfile() {
  return testing_profile_;
}

chromeos::ScopedCrosSettingsTestHelper*
TpmChallengeKeySubtleTest::GetCrosSettingsHelper() {
  return GetProfile()->ScopedCrosSettingsTestHelper();
}

chromeos::StubInstallAttributes*
TpmChallengeKeySubtleTest::GetInstallAttributes() {
  return GetCrosSettingsHelper()->InstallAttributes();
}

void TpmChallengeKeySubtleTest::RunOneStepAndExpect(
    AttestationKeyType key_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& public_key) {
  CallbackObserver callback_observer;
  challenge_key_subtle_->StartPrepareKeyStep(key_type, will_register_key,
                                             key_name, GetProfile(),
                                             callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), public_key);
}

void TpmChallengeKeySubtleTest::RunTwoStepsAndExpect(
    AttestationKeyType key_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& challenge_response) {
  RunOneStepAndExpect(key_type, will_register_key, key_name,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartSignChallengeStep(
      GetChallenge(), callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), challenge_response);
}

void TpmChallengeKeySubtleTest::RunThreeStepsAndExpect(
    AttestationKeyType key_type,
    bool will_register_key,
    const std::string& key_name,
    const TpmChallengeKeyResult& register_result) {
  RunTwoStepsAndExpect(key_type, will_register_key, key_name,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(will_register_key)));

  CallbackObserver callback_observer;
  challenge_key_subtle_->StartRegisterKeyStep(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(), register_result);
}

//==============================================================================

TEST_F(TpmChallengeKeySubtleTest, DeviceKeyNonEnterpriseDevice) {
  InitSigninProfile();

  GetInstallAttributes()->SetConsumerOwned();

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kNonEnterpriseDeviceError));
}

TEST_F(TpmChallengeKeySubtleTest, DeviceKeyDeviceAttestationDisabled) {
  InitSigninProfile();

  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      false);

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kDevicePolicyDisabledError));
}

TEST_F(TpmChallengeKeySubtleTest, DeviceKeyUserNotManaged) {
  InitUnaffiliatedProfile();

  RunOneStepAndExpect(KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserNotManagedError));
}

TEST_F(TpmChallengeKeySubtleTest, UserKeyUserKeyNotAvailable) {
  InitSigninProfile();

  RunOneStepAndExpect(
      KEY_USER, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kUserKeyNotAvailableError));
}

TEST_F(TpmChallengeKeySubtleTest, UserKeyUserPolicyDisabled) {
  InitAffiliatedProfile();
  GetProfile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAttestationEnabled, std::make_unique<base::Value>(false));

  RunOneStepAndExpect(KEY_USER, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserPolicyDisabledError));
}

// Checks that a user should be affiliated with a device
TEST_F(TpmChallengeKeySubtleTest, UserKeyUserNotAffiliated) {
  InitUnaffiliatedProfile();
  GetProfile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAttestationEnabled, std::make_unique<base::Value>(true));

  RunOneStepAndExpect(KEY_USER, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kUserNotManagedError));
}

TEST_F(TpmChallengeKeySubtleTest, UserKeyDeviceAttestationDisabled) {
  InitAffiliatedProfile();
  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      false);

  RunOneStepAndExpect(
      KEY_USER, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kDevicePolicyDisabledError));
}

TEST_F(TpmChallengeKeySubtleTest, DoesKeyExistDbusFailed) {
  InitSigninProfile();

  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(TpmChallengeKeyResultCode::kDbusError));
}

TEST_F(TpmChallengeKeySubtleTest, GetCertificateFailed) {
  InitSigninProfile();
  AttestationKeyType key_type = KEY_DEVICE;

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, GetDefaultKeyName(key_type), _))
      .WillOnce(RunOnceCallback<5>(
          chromeos::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
          /*pem_certificate_chain=*/""));

  RunOneStepAndExpect(
      key_type, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kGetCertificateFailedError));
}

TEST_F(TpmChallengeKeySubtleTest, KeyExists) {
  InitSigninProfile();
  AttestationKeyType key_type = KEY_DEVICE;

  cryptohome_client_.SetTpmAttestationDeviceCertificate("attest-ent-machine",
                                                        std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, GetDefaultKeyName(key_type), _))
      .Times(0);

  RunOneStepAndExpect(key_type, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakePublicKey(GetPublicKey()));
}

TEST_F(TpmChallengeKeySubtleTest, AttestationNotPrepared) {
  InitSigninProfile();

  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(false);

  RunOneStepAndExpect(KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kResetRequiredError));
}

// Test that we get a proper error message in case we don't have a TPM.
TEST_F(TpmChallengeKeySubtleTest, AttestationUnsupported) {
  InitSigninProfile();
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparations(false);
  cryptohome_client_.set_tpm_is_enabled(false);

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kAttestationUnsupportedError));
}

TEST_F(TpmChallengeKeySubtleTest, AttestationPreparedDbusFailed) {
  InitSigninProfile();

  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(::attestation::STATUS_DBUS_ERROR);

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(TpmChallengeKeyResultCode::kDbusError));
}

TEST_F(TpmChallengeKeySubtleTest, AttestationPreparedServiceInternalError) {
  InitSigninProfile();

  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->ConfigureEnrollmentPreparationsStatus(
          ::attestation::STATUS_NOT_AVAILABLE);

  RunOneStepAndExpect(
      KEY_DEVICE, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kAttestationServiceInternalError));
}

TEST_F(TpmChallengeKeySubtleTest, DeviceKeyNotRegisteredSuccess) {
  InitSigninProfile();
  AttestationKeyType key_type = KEY_DEVICE;
  const char* key_name = GetDefaultKeyName(key_type);

  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, key_name, _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_key_label(key_name);
  expected_request.set_domain(kTestUserDomain);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(key_type, /*will_register_key=*/false, kEmptyKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

TEST_F(TpmChallengeKeySubtleTest, DeviceKeyRegisteredSuccess) {
  InitSigninProfile();
  AttestationKeyType key_type = KEY_DEVICE;
  const char* key_name = kNonDefaultKeyName;

  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, key_name, _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_key_label(GetDefaultKeyName(key_type));
  expected_request.set_key_name_for_spkac(key_name);
  expected_request.set_domain(kTestUserDomain);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  AttestationClient::Get()->GetTestInterface()->AllowlistRegisterKey(
      /*username=*/"", key_name);

  EXPECT_CALL(
      *system_token_key_permissions_manager_,
      AllowKeyForUsage(/*callback=*/_, platform_keys::KeyUsage::kCorporate,
                       GetPublicKey()))
      .WillOnce(RunOnceCallback<0>(platform_keys::Status::kSuccess));

  RunThreeStepsAndExpect(key_type, /*will_register_key=*/true, key_name,
                         TpmChallengeKeyResult::MakeSuccess());
}

TEST_F(TpmChallengeKeySubtleTest, UserKeyNotRegisteredSuccess) {
  InitAffiliatedProfile();

  AttestationKeyType key_type = KEY_USER;
  const char* key_name = GetDefaultKeyName(key_type);

  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, key_name, _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(GetDefaultKeyName(key_type));
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  RunTwoStepsAndExpect(key_type, /*will_register_key=*/false, kEmptyKeyName,
                       TpmChallengeKeyResult::MakeChallengeResponse(
                           GetChallengeResponse(/*include_spkac=*/false)));
}

TEST_F(TpmChallengeKeySubtleTest, UserKeyRegisteredSuccess) {
  InitAffiliatedProfile();

  AttestationKeyType key_type = KEY_USER;
  const char* key_name = kNonDefaultKeyName;

  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, key_name, _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(kNonDefaultKeyName);
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  AttestationClient::Get()->GetTestInterface()->AllowlistRegisterKey(
      kTestUserEmail, key_name);

  EXPECT_CALL(
      *user_private_token_key_permissions_manager_,
      AllowKeyForUsage(/*callback=*/_, platform_keys::KeyUsage::kCorporate,
                       GetPublicKey()))
      .WillOnce(RunOnceCallback<0>(platform_keys::Status::kSuccess));

  RunThreeStepsAndExpect(key_type, /*will_register_key=*/true, key_name,
                         TpmChallengeKeyResult::MakeSuccess());
}

TEST_F(TpmChallengeKeySubtleTest, SignChallengeFailed) {
  InitSigninProfile();
  AttestationKeyType key_type = KEY_DEVICE;

  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(_, _, _, _, GetDefaultKeyName(key_type), _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  // The signing operations fails because we don't allowlist any key.
  RunTwoStepsAndExpect(
      key_type, /*will_register_key=*/false, kEmptyKeyName,
      TpmChallengeKeyResult::MakeError(
          TpmChallengeKeyResultCode::kSignChallengeFailedError));
}

TEST_F(TpmChallengeKeySubtleTest, RestorePreparedKeyState) {
  InitAffiliatedProfile();
  AttestationKeyType key_type = KEY_USER;
  const char* key_name = kNonDefaultKeyName;

  std::unique_ptr<TpmChallengeKeySubtle> challenge_key_subtle =
      TpmChallengeKeySubtleFactory::CreateForPreparedKey(
          key_type, /*will_register_key=*/true, key_name, GetPublicKey(),
          GetProfile());

  ::attestation::SignEnterpriseChallengeRequest expected_request;
  expected_request.set_username(kTestUserEmail);
  expected_request.set_key_label(kNonDefaultKeyName);
  expected_request.set_domain(kTestUserEmail);
  expected_request.set_device_id(GetInstallAttributes()->GetDeviceId());
  AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignEnterpriseChallengeKey(expected_request);

  {
    CallbackObserver callback_observer;
    challenge_key_subtle->StartSignChallengeStep(
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
                       GetPublicKey()))
      .WillOnce(RunOnceCallback<0>(platform_keys::Status::kSuccess));

  {
    CallbackObserver callback_observer;
    challenge_key_subtle->StartRegisterKeyStep(callback_observer.GetCallback());
    callback_observer.WaitForCallback();

    EXPECT_EQ(callback_observer.GetResult(),
              TpmChallengeKeyResult::MakeSuccess());
  }
}

TEST_F(TpmChallengeKeySubtleTest, KeyRegistrationFailed) {
  InitAffiliatedProfile();
  AttestationKeyType key_type = KEY_USER;
  const char* key_name = kNonDefaultKeyName;

  std::unique_ptr<TpmChallengeKeySubtle> challenge_key_subtle =
      TpmChallengeKeySubtleFactory::CreateForPreparedKey(
          key_type, /*will_register_key=*/true, key_name, GetPublicKey(),
          GetProfile());

  CallbackObserver callback_observer;
  challenge_key_subtle->StartRegisterKeyStep(callback_observer.GetCallback());
  callback_observer.WaitForCallback();

  EXPECT_EQ(callback_observer.GetResult(),
            TpmChallengeKeyResult::MakeError(
                TpmChallengeKeyResultCode::kKeyRegistrationFailedError));
}

TEST_F(TpmChallengeKeySubtleTest, GetPublicKeyFailed) {
  InitAffiliatedProfile();
  const char* key_name = kNonDefaultKeyName;

  cryptohome_client_.set_tpm_attestation_public_key(base::nullopt);

  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, key_name, _))
      .WillOnce(
          RunOnceCallback<5>(chromeos::attestation::ATTESTATION_SUCCESS,
                             /*pem_certificate_chain=*/"fake_certificate"));

  RunOneStepAndExpect(KEY_DEVICE, /*will_register_key=*/true, key_name,
                      TpmChallengeKeyResult::MakeError(
                          TpmChallengeKeyResultCode::kGetPublicKeyFailedError));
}

}  // namespace
}  // namespace attestation
}  // namespace chromeos
