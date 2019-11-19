// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/attestation/tpm_challenge_key.h"

#include <mutex>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/constants/attestation_constants.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::WithArgs;

namespace utils = extension_function_test_utils;

namespace chromeos {
namespace attestation {

namespace {

const char kUserEmail[] = "test@google.com";
const char kChallenge[] = "challenge";
const char kResponse[] = "response";
const char kKeyNameForSpkac[] = "attest-ent-machine-123456";

void RegisterKeyCallbackTrue(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const cryptohome::AsyncMethodCaller::Callback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, true, cryptohome::MOUNT_ERROR_NONE));
}

void RegisterKeyCallbackFalse(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const cryptohome::AsyncMethodCaller::Callback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, false, cryptohome::MOUNT_ERROR_NONE));
}

void SignChallengeCallbackTrue(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    chromeos::attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const std::string& key_name_for_spkac,
    const cryptohome::AsyncMethodCaller::DataCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, true, "response"));
}

void SignChallengeCallbackFalse(
    chromeos::attestation::AttestationKeyType key_type,
    const cryptohome::Identification& user_id,
    const std::string& key_name,
    const std::string& domain,
    const std::string& device_id,
    chromeos::attestation::AttestationChallengeOptions options,
    const std::string& challenge,
    const std::string& key_name_for_spkac,
    const cryptohome::AsyncMethodCaller::DataCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, false, ""));
}

void GetCertificateCallbackTrue(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(callback, chromeos::attestation::ATTESTATION_SUCCESS,
                          "certificate"));
}

void GetCertificateCallbackUnspecifiedFailure(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(
          callback, chromeos::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
          ""));
}

void GetCertificateCallbackBadRequestFailure(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const std::string& key_name,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(
          callback,
          chromeos::attestation::ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

class TpmChallengeKeyTestBase : public BrowserWithTestWindowTest {
 public:
  enum class ProfileType { kUserProfile, kSigninProfile };

 protected:
  TpmChallengeKeyTestBase(ProfileType profile_type,
                          chromeos::attestation::AttestationKeyType key_type)
      : profile_type_(profile_type),
        fake_user_manager_(new chromeos::FakeChromeUserManager()),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)),
        key_type_(key_type) {
    mock_async_method_caller_ =
        new NiceMock<cryptohome::MockAsyncMethodCaller>();
    // Ownership of mock_async_method_caller_ is transferred to
    // AsyncMethodCaller::InitializeForTesting.
    cryptohome::AsyncMethodCaller::InitializeForTesting(
        mock_async_method_caller_);

    challenge_key_impl_ =
        std::make_unique<TpmChallengeKeyImpl>(&mock_attestation_flow_);

    // Set up the default behavior of mocks.
    ON_CALL(*mock_async_method_caller_, TpmAttestationRegisterKey)
        .WillByDefault(Invoke(RegisterKeyCallbackTrue));
    ON_CALL(*mock_async_method_caller_, TpmAttestationSignEnterpriseChallenge)
        .WillByDefault(Invoke(SignChallengeCallbackTrue));
    ON_CALL(mock_attestation_flow_, GetCertificate)
        .WillByDefault(Invoke(GetCertificateCallbackTrue));

    GetInstallAttributes()->SetCloudManaged("google.com", "device_id");

    GetCrosSettingsHelper()->ReplaceDeviceSettingsProviderWithStub();
    GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                        true);
  }

  ~TpmChallengeKeyTestBase() { cryptohome::AsyncMethodCaller::Shutdown(); }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    if (profile_type_ == ProfileType::kUserProfile) {
      prefs_ = GetProfile()->GetPrefs();
      SetAuthenticatedUser();
    }
  }

  void TearDown() override { BrowserWithTestWindowTest::TearDown(); }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  TestingProfile* CreateProfile() override {
    switch (profile_type_) {
      case ProfileType::kUserProfile:
        fake_user_manager_->AddUserWithAffiliation(
            AccountId::FromUserEmail(kUserEmail), true);
        return profile_manager()->CreateTestingProfile(kUserEmail);

      case ProfileType::kSigninProfile:
        return profile_manager()->CreateTestingProfile(chrome::kInitialProfile);
    }
    NOTREACHED() << "Invalid profile type: " << static_cast<int>(profile_type_);
  }

  // Derived classes can override this method to set the required authenticated
  // user in the IdentityManager class.
  virtual void SetAuthenticatedUser() {
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(GetProfile());
    signin::MakePrimaryAccountAvailable(identity_manager, kUserEmail);
  }

  void RunFunc(const std::string& challenge,
               bool register_key,
               const std::string& key_name_for_spkac,
               TpmChallengeKeyResult* res) {
    auto callback = [](base::OnceClosure done_closure,
                       TpmChallengeKeyResult* res,
                       const TpmChallengeKeyResult& tpm_result) {
      *res = tpm_result;
      std::move(done_closure).Run();
    };

    base::RunLoop loop;
    challenge_key_impl_->BuildResponse(
        key_type_, GetProfile(), base::Bind(callback, loop.QuitClosure(), res),
        challenge, register_key, key_name_for_spkac);
    loop.Run();
  }

  chromeos::FakeCryptohomeClient cryptohome_client_;
  cryptohome::MockAsyncMethodCaller* mock_async_method_caller_ = nullptr;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  ProfileType profile_type_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  PrefService* prefs_ = nullptr;
  std::unique_ptr<TpmChallengeKey> challenge_key_impl_;
  chromeos::attestation::AttestationKeyType key_type_;
};

class TpmChallengeMachineKeyTest : public TpmChallengeKeyTestBase {
 protected:
  explicit TpmChallengeMachineKeyTest(
      ProfileType profile_type = ProfileType::kUserProfile)
      : TpmChallengeKeyTestBase(profile_type,
                                chromeos::attestation::KEY_DEVICE) {}
};

TEST_F(TpmChallengeMachineKeyTest, NonEnterpriseDevice) {
  GetInstallAttributes()->SetConsumerOwned();

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kNonEnterpriseDeviceErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, DevicePolicyDisabled) {
  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDevicePolicyDisabledErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDbusErrorMsg, res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate)
      .WillRepeatedly(Invoke(GetCertificateCallbackUnspecifiedFailure));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kGetCertificateFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, SignChallengeFailed) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationSignEnterpriseChallenge)
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kSignChallengeFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationDeviceCertificate("attest-ent-machine",
                                                        std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate).Times(0);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

TEST_F(TpmChallengeMachineKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kResetRequiredErrorMsg,
            res.GetErrorMessage());
}

// Test that we get proper error message in case we don't have TPM.
TEST_F(TpmChallengeMachineKeyTest, AttestationUnsupported) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);
  cryptohome_client_.set_tpm_is_enabled(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kAttestationUnsupportedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDbusErrorMsg, res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationRegisterKey)
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, kKeyNameForSpkac, &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kKeyRegistrationFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeMachineKeyTest, KeyNotRegisteredSuccess) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationRegisterKey).Times(0);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

TEST_F(TpmChallengeMachineKeyTest, KeyRegisteredSuccess) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                  _, _, _, _, _))
      .Times(1);
  // TpmAttestationRegisterKey must be called exactly once.
  EXPECT_CALL(*mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_DEVICE,
                                        _ /* Unused by the API. */,
                                        kKeyNameForSpkac, _))
      .Times(1);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(
      *mock_async_method_caller_,
      TpmAttestationSignEnterpriseChallenge(
          chromeos::attestation::KEY_DEVICE, _, "attest-ent-machine",
          "google.com", "device_id", _, "challenge", kKeyNameForSpkac, _))
      .Times(1);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, kKeyNameForSpkac, &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

// Tests the API with all profiles types as determined by the test parameter.
class TpmChallengeMachineKeyAllProfilesTest
    : public TpmChallengeMachineKeyTest,
      public ::testing::WithParamInterface<
          TpmChallengeKeyTestBase::ProfileType> {
 protected:
  TpmChallengeMachineKeyAllProfilesTest()
      : TpmChallengeMachineKeyTest(GetParam()) {}
};

TEST_P(TpmChallengeMachineKeyAllProfilesTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                  _, _, _, _, _))
      .Times(1);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(*mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(
                  chromeos::attestation::KEY_DEVICE, _, "attest-ent-machine",
                  "google.com", "device_id", _, "challenge", _, _))
      .Times(1);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

INSTANTIATE_TEST_SUITE_P(
    AllProfiles,
    TpmChallengeMachineKeyAllProfilesTest,
    ::testing::Values(TpmChallengeKeyTestBase::ProfileType::kUserProfile,
                      TpmChallengeKeyTestBase::ProfileType::kSigninProfile));

class TpmChallengeUserKeyTest : public TpmChallengeKeyTestBase {
 protected:
  explicit TpmChallengeUserKeyTest(
      ProfileType profile_type = ProfileType::kUserProfile)
      : TpmChallengeKeyTestBase(profile_type, chromeos::attestation::KEY_USER) {
  }

  void SetUp() override {
    TpmChallengeKeyTestBase::SetUp();

    if (profile_type_ == ProfileType::kUserProfile) {
      GetProfile()->GetTestingPrefService()->SetManagedPref(
          prefs::kAttestationEnabled, std::make_unique<base::Value>(true));
    }
  }
};

TEST_F(TpmChallengeUserKeyTest, UserPolicyDisabled) {
  GetProfile()->GetTestingPrefService()->SetManagedPref(
      prefs::kAttestationEnabled, std::make_unique<base::Value>(false));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kUserPolicyDisabledErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, DevicePolicyDisabled) {
  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDevicePolicyDisabledErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDbusErrorMsg, res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, GetCertificateFailedWithUnspecifiedFailure) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate)
      .WillRepeatedly(Invoke(GetCertificateCallbackUnspecifiedFailure));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kGetCertificateFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, GetCertificateFailedWithBadRequestFailure) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate)
      .WillRepeatedly(Invoke(GetCertificateCallbackBadRequestFailure));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kGetCertificateFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, SignChallengeFailed) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationSignEnterpriseChallenge)
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kSignChallengeFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationRegisterKey)
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kKeyRegistrationFailedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationUserCertificate(
      cryptohome::CreateAccountIdentifierFromAccountId(
          AccountId::FromUserEmail(kUserEmail)),
      "attest-ent-user", std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate).Times(0);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

TEST_F(TpmChallengeUserKeyTest, KeyNotRegisteredSuccess) {
  EXPECT_CALL(*mock_async_method_caller_, TpmAttestationRegisterKey).Times(0);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

TEST_F(TpmChallengeUserKeyTest, PersonalDevice) {
  GetInstallAttributes()->SetConsumerOwned();

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  // Currently personal devices are not supported.
  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kUserRejectedErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(chromeos::attestation::PROFILE_ENTERPRISE_USER_CERTIFICATE,
                     _, _, _, _, _))
      .Times(1);
  const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(*mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(
                  chromeos::attestation::KEY_USER,
                  cryptohome::Identification(account_id), "attest-ent-user",
                  cryptohome::Identification(account_id).id(), "device_id", _,
                  "challenge", _, _))
      .Times(1);
  // RegisterKey must be called exactly once.
  EXPECT_CALL(*mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_USER,
                                        cryptohome::Identification(account_id),
                                        "attest-ent-user", _))
      .Times(1);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_TRUE(res.IsSuccess());
  EXPECT_EQ(kResponse, res.data);
}

TEST_F(TpmChallengeUserKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kResetRequiredErrorMsg,
            res.GetErrorMessage());
}

TEST_F(TpmChallengeUserKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kDbusErrorMsg, res.GetErrorMessage());
}

class TpmChallengeUserKeySigninProfileTest : public TpmChallengeUserKeyTest {
 protected:
  TpmChallengeUserKeySigninProfileTest()
      : TpmChallengeUserKeyTest(ProfileType::kSigninProfile) {}
};

TEST_F(TpmChallengeUserKeySigninProfileTest, UserKeyNotAvailable) {
  GetCrosSettingsHelper()->SetBoolean(chromeos::kDeviceAttestationEnabled,
                                      false);

  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kUserKeyNotAvailableErrorMsg,
            res.GetErrorMessage());
}

class TpmChallengeMachineKeyUnmanagedUserTest
    : public TpmChallengeMachineKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(GetProfile()),
        account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    return profile_manager()->CreateTestingProfile(account_id_.GetUserEmail());
  }

  const std::string email = "test@chromium.com";
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(email,
                                     signin::GetTestGaiaIdForEmail(email));
};

TEST_F(TpmChallengeMachineKeyUnmanagedUserTest, UserNotManaged) {
  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/false, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kUserNotManagedErrorMsg,
            res.GetErrorMessage());
}

class TpmChallengeUserKeyUnmanagedUserTest : public TpmChallengeUserKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    signin::MakePrimaryAccountAvailable(
        IdentityManagerFactory::GetForProfile(GetProfile()),
        account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    return profile_manager()->CreateTestingProfile(account_id_.GetUserEmail());
  }

  const std::string email = "test@chromium.com";
  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId(email,
                                     signin::GetTestGaiaIdForEmail(email));
};

TEST_F(TpmChallengeUserKeyUnmanagedUserTest, UserNotManaged) {
  TpmChallengeKeyResult res;
  RunFunc(kChallenge, /*register_key=*/true, "", &res);

  EXPECT_FALSE(res.IsSuccess());
  EXPECT_EQ("", res.data);
  EXPECT_EQ(TpmChallengeKeyResult::kUserNotManagedErrorMsg,
            res.GetErrorMessage());
}

}  // namespace
}  // namespace attestation
}  // namespace chromeos
