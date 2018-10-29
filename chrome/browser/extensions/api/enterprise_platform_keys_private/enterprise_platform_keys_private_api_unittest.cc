// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys_private/enterprise_platform_keys_private_api.h"

#include <string>

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
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/signin_manager_factory.h"
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
#include "chromeos/dbus/attestation_constants.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
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

namespace extensions {
namespace {

// Certificate errors as reported to the calling extension.
const int kDBusError = 1;
const int kUserRejected = 2;
const int kGetCertificateFailed = 3;
const int kResetRequired = 4;

const char kUserEmail[] = "test@google.com";

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
    const cryptohome::AsyncMethodCaller::DataCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, false, ""));
}

void GetCertificateCallbackTrue(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
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
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindRepeating(
          callback,
          chromeos::attestation::ATTESTATION_SERVER_BAD_REQUEST_FAILURE, ""));
}

class EPKPChallengeKeyTestBase : public BrowserWithTestWindowTest {
 public:
  enum class ProfileType { USER_PROFILE, SIGNIN_PROFILE };

 protected:
  explicit EPKPChallengeKeyTestBase(ProfileType profile_type)
      : settings_helper_(false),
        profile_type_(profile_type),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
    // Create the extension.
    extension_ = CreateExtension();

    // Set up the default behavior of mocks.
    ON_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
        .WillByDefault(Invoke(RegisterKeyCallbackTrue));
    ON_CALL(mock_async_method_caller_,
            TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
        .WillByDefault(Invoke(SignChallengeCallbackTrue));
    ON_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
        .WillByDefault(Invoke(GetCertificateCallbackTrue));

    stub_install_attributes_.SetCloudManaged("google.com", "device_id");

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, true);
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    if (profile_type_ == ProfileType::USER_PROFILE) {
      // Set the user preferences.
      prefs_ = browser()->profile()->GetPrefs();
      base::ListValue whitelist;
      whitelist.AppendString(extension_->id());
      prefs_->Set(prefs::kAttestationExtensionWhitelist, whitelist);

      SetAuthenticatedUser();
    }
  }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  TestingProfile* CreateProfile() override {
    switch (profile_type_) {
      case ProfileType::USER_PROFILE:
        fake_user_manager_->AddUserWithAffiliation(
            AccountId::FromUserEmail(kUserEmail), true);
        return profile_manager()->CreateTestingProfile(kUserEmail);

      case ProfileType::SIGNIN_PROFILE:
        return profile_manager()->CreateTestingProfile(chrome::kInitialProfile);
    }
  }

  // Derived classes can override this method to set the required authenticated
  // user in the SigninManager class.
  virtual void SetAuthenticatedUser() {
    SigninManagerFactory::GetForProfile(browser()->profile())->
        SetAuthenticatedAccountInfo("12345", kUserEmail);
  }

  chromeos::FakeCryptohomeClient cryptohome_client_;
  NiceMock<cryptohome::MockAsyncMethodCaller> mock_async_method_caller_;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<const Extension> extension_;
  chromeos::StubInstallAttributes stub_install_attributes_;
  ProfileType profile_type_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  PrefService* prefs_ = nullptr;

 private:
  scoped_refptr<const Extension> CreateExtension() {
    switch (profile_type_) {
      case ProfileType::USER_PROFILE:
        return ExtensionBuilder("Test").Build();

      case ProfileType::SIGNIN_PROFILE:
        return ExtensionBuilder("Test", ExtensionBuilder::Type::PLATFORM_APP)
            .SetLocation(Manifest::Location::EXTERNAL_POLICY)
            .Build();
    }
  }
};

class EPKPChallengeMachineKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kArgs[];

  explicit EPKPChallengeMachineKeyTest(
      ProfileType profile_type = ProfileType::USER_PROFILE)
      : EPKPChallengeKeyTestBase(profile_type),
        impl_(&cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(new EnterprisePlatformKeysPrivateChallengeMachineKeyFunction(
            &impl_)) {
    func_->set_extension(extension_.get());
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(
        EPKPChallengeMachineKey::kGetCertificateFailedError,
        error_code);
  }

  EPKPChallengeMachineKey impl_;
  scoped_refptr<EnterprisePlatformKeysPrivateChallengeMachineKeyFunction> func_;
};

// Base 64 encoding of 'challenge'.
const char EPKPChallengeMachineKeyTest::kArgs[] = "[\"Y2hhbGxlbmdl\"]";

TEST_F(EPKPChallengeMachineKeyTest, ChallengeBadBase64) {
  EXPECT_EQ(EPKPChallengeKeyBase::kChallengeBadBase64Error,
            utils::RunFunctionAndReturnError(
                func_.get(), "[\"****\"]", browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, NonEnterpriseDevice) {
  stub_install_attributes_.SetConsumerOwned();

  EXPECT_EQ(EPKPChallengeMachineKey::kNonEnterpriseDeviceError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackUnspecifiedFailure));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationDeviceCertificate("attest-ent-machine",
                                                        std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), kArgs, browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKPChallengeMachineKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  EXPECT_EQ(GetCertificateError(kResetRequired),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeMachineKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

// Tests the API with all profiles types as determined by the test parameter.
class EPKPChallengeMachineKeyAllProfilesTest
    : public EPKPChallengeMachineKeyTest,
      public ::testing::WithParamInterface<
          EPKPChallengeKeyTestBase::ProfileType> {
 protected:
  EPKPChallengeMachineKeyAllProfilesTest()
      : EPKPChallengeMachineKeyTest(GetParam()) {}
};

TEST_P(EPKPChallengeMachineKeyAllProfilesTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                  _, _, _, _))
      .Times(1);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(
      mock_async_method_caller_,
      TpmAttestationSignEnterpriseChallenge(
          chromeos::attestation::KEY_DEVICE, cryptohome::Identification(),
          "attest-ent-machine", "google.com", "device_id", _, "challenge", _))
      .Times(1);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kArgs, browser(), extensions::api_test_utils::NONE));

  std::string response;
  value->GetAsString(&response);
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */, response);
}

INSTANTIATE_TEST_CASE_P(
    AllProfiles,
    EPKPChallengeMachineKeyAllProfilesTest,
    ::testing::Values(EPKPChallengeKeyTestBase::ProfileType::USER_PROFILE,
                      EPKPChallengeKeyTestBase::ProfileType::SIGNIN_PROFILE));

class EPKPChallengeUserKeyTest : public EPKPChallengeKeyTestBase {
 protected:
  static const char kArgs[];

  explicit EPKPChallengeUserKeyTest(
      ProfileType profile_type = ProfileType::USER_PROFILE)
      : EPKPChallengeKeyTestBase(profile_type),
        impl_(&cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(
            new EnterprisePlatformKeysPrivateChallengeUserKeyFunction(&impl_)) {
    func_->set_extension(extension_.get());
  }

  void SetUp() override {
    EPKPChallengeKeyTestBase::SetUp();

    if (profile_type_ == ProfileType::USER_PROFILE) {
      // Set the user preferences.
      prefs_->SetBoolean(prefs::kAttestationEnabled, true);
    }
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(EPKPChallengeUserKey::kGetCertificateFailedError,
                              error_code);
  }

  EPKPChallengeUserKey impl_;
  scoped_refptr<EnterprisePlatformKeysPrivateChallengeUserKeyFunction> func_;
};

// Base 64 encoding of 'challenge'
const char EPKPChallengeUserKeyTest::kArgs[] = "[\"Y2hhbGxlbmdl\", true]";

TEST_F(EPKPChallengeUserKeyTest, ChallengeBadBase64) {
  EXPECT_EQ(EPKPChallengeKeyBase::kChallengeBadBase64Error,
            utils::RunFunctionAndReturnError(
                func_.get(), "[\"****\", true]", browser()));
}

TEST_F(EPKPChallengeUserKeyTest, UserPolicyDisabled) {
  prefs_->SetBoolean(prefs::kAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeUserKey::kUserPolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, GetCertificateFailedWithUnspecifiedFailure) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackUnspecifiedFailure));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, GetCertificateFailedWithBadRequestFailure) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackBadRequestFailure));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  EXPECT_EQ(EPKPChallengeUserKey::kKeyRegistrationFailedError,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationUserCertificate(
      cryptohome::CreateAccountIdentifierFromAccountId(
          AccountId::FromUserEmail(kUserEmail)),
      "attest-ent-user", std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), kArgs, browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKPChallengeUserKeyTest, KeyNotRegistered) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), "[\"Y2hhbGxlbmdl\", false]",
                                 browser(), extensions::api_test_utils::NONE));
}

TEST_F(EPKPChallengeUserKeyTest, PersonalDevice) {
  stub_install_attributes_.SetConsumerOwned();

  // Currently personal devices are not supported.
  EXPECT_EQ(GetCertificateError(kUserRejected),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_USER_CERTIFICATE,
                  _, _, _, _))
      .Times(1);
  const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(
                  chromeos::attestation::KEY_USER,
                  cryptohome::Identification(account_id), "attest-ent-user",
                  cryptohome::Identification(account_id).id(), "device_id", _,
                  "challenge", _))
      .Times(1);
  // RegisterKey must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_USER,
                                        cryptohome::Identification(account_id),
                                        "attest-ent-user", _))
      .Times(1);

  std::unique_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func_.get(), kArgs, browser(), extensions::api_test_utils::NONE));

  std::string response;
  value->GetAsString(&response);
  EXPECT_EQ("cmVzcG9uc2U=" /* Base64 encoding of 'response' */, response);
}

TEST_F(EPKPChallengeUserKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  EXPECT_EQ(GetCertificateError(kResetRequired),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

TEST_F(EPKPChallengeUserKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeUserKeySigninProfileTest : public EPKPChallengeUserKeyTest {
 protected:
  EPKPChallengeUserKeySigninProfileTest()
      : EPKPChallengeUserKeyTest(ProfileType::SIGNIN_PROFILE) {}
};

TEST_F(EPKPChallengeUserKeySigninProfileTest, UserKeyNotAvailable) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeUserKey::kUserKeyNotAvailable,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeMachineKeyUnmanagedUserTest
    : public EPKPChallengeMachineKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    SigninManagerFactory::GetForProfile(browser()->profile())
        ->SetAuthenticatedAccountInfo(account_id_.GetGaiaId(),
                                      account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    return profile_manager()->CreateTestingProfile(account_id_.GetUserEmail());
  }

  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test@chromium.com", "12345");
};

TEST_F(EPKPChallengeMachineKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

class EPKPChallengeUserKeyUnmanagedUserTest : public EPKPChallengeUserKeyTest {
 protected:
  void SetAuthenticatedUser() override {
    SigninManagerFactory::GetForProfile(browser()->profile())
        ->SetAuthenticatedAccountInfo(account_id_.GetGaiaId(),
                                      account_id_.GetUserEmail());
  }

  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUser(account_id_);
    return profile_manager()->CreateTestingProfile(account_id_.GetUserEmail());
  }

  const AccountId account_id_ =
      AccountId::FromUserEmailGaiaId("test@chromium.com", "12345");
};

TEST_F(EPKPChallengeUserKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            utils::RunFunctionAndReturnError(func_.get(), kArgs, browser()));
}

}  // namespace
}  // namespace extensions
