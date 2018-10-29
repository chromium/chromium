// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/mock_async_method_caller.h"
#include "chromeos/dbus/attestation_constants.h"
#include "chromeos/dbus/dbus_method_call_status.h"
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
      base::BindOnce(callback, chromeos::attestation::ATTESTATION_SUCCESS,
                     "certificate"));
}

void GetCertificateCallbackFalse(
    chromeos::attestation::AttestationCertificateProfile certificate_profile,
    const AccountId& account_id,
    const std::string& request_origin,
    bool force_new_key,
    const chromeos::attestation::AttestationFlow::CertificateCallback&
        callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(callback,
                     chromeos::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
                     ""));
}

class EPKChallengeKeyTestBase : public BrowserWithTestWindowTest {
 protected:
  EPKChallengeKeyTestBase()
      : settings_helper_(false),
        extension_(ExtensionBuilder("Test").Build()),
        fake_user_manager_(new chromeos::FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)) {
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

    // Set the user preferences.
    prefs_ = browser()->profile()->GetPrefs();
    base::ListValue whitelist;
    whitelist.AppendString(extension_->id());
    prefs_->Set(prefs::kAttestationExtensionWhitelist, whitelist);

    SetAuthenticatedUser();
  }

  // This will be called by BrowserWithTestWindowTest::SetUp();
  TestingProfile* CreateProfile() override {
    fake_user_manager_->AddUserWithAffiliation(
        AccountId::FromUserEmail(kUserEmail), true);
    return profile_manager()->CreateTestingProfile(kUserEmail);
  }

  // Derived classes can override this method to set the required authenticated
  // user in the SigninManager class.
  virtual void SetAuthenticatedUser() {
    SigninManagerFactory::GetForProfile(browser()->profile())
        ->SetAuthenticatedAccountInfo("12345", kUserEmail);
  }

  // Like extension_function_test_utils::RunFunctionAndReturnError but with an
  // explicit ListValue.
  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        std::unique_ptr<base::ListValue> args,
                                        Browser* browser) {
    utils::RunFunction(function, std::move(args), browser,
                       extensions::api_test_utils::NONE);
    EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
    return function->GetError();
  }

  // Like extension_function_test_utils::RunFunctionAndReturnSingleResult but
  // with an explicit ListValue.
  base::Value* RunFunctionAndReturnSingleResult(
      UIThreadExtensionFunction* function,
      std::unique_ptr<base::ListValue> args,
      Browser* browser) {
    scoped_refptr<ExtensionFunction> function_owner(function);
    // Without a callback the function will not generate a result.
    function->set_has_callback(true);
    utils::RunFunction(function, std::move(args), browser,
                       extensions::api_test_utils::NONE);
    EXPECT_TRUE(function->GetError().empty()) << "Unexpected error: "
                                              << function->GetError();
    const base::Value* single_result = NULL;
    if (function->GetResultList() != NULL &&
        function->GetResultList()->Get(0, &single_result)) {
      return single_result->DeepCopy();
    }
    return NULL;
  }

  chromeos::FakeCryptohomeClient cryptohome_client_;
  NiceMock<cryptohome::MockAsyncMethodCaller> mock_async_method_caller_;
  NiceMock<chromeos::attestation::MockAttestationFlow> mock_attestation_flow_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  scoped_refptr<const extensions::Extension> extension_;
  chromeos::StubInstallAttributes stub_install_attributes_;
  // fake_user_manager_ is owned by user_manager_enabler_.
  chromeos::FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  PrefService* prefs_ = nullptr;
};

class EPKChallengeMachineKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeMachineKeyTest()
      : impl_(&cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(new EnterprisePlatformKeysChallengeMachineKeyFunction(&impl_)) {
    func_->set_extension(extension_.get());
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(
        EPKPChallengeMachineKey::kGetCertificateFailedError, error_code);
  }

  std::unique_ptr<base::ListValue> CreateArgs() {
    return CreateArgsInternal(nullptr);
  }

  std::unique_ptr<base::ListValue> CreateArgsNoRegister() {
    return CreateArgsInternal(std::make_unique<bool>(false));
  }

  std::unique_ptr<base::ListValue> CreateArgsRegister() {
    return CreateArgsInternal(std::make_unique<bool>(true));
  }

  std::unique_ptr<base::ListValue> CreateArgsInternal(
      std::unique_ptr<bool> register_key) {
    std::unique_ptr<base::ListValue> args(new base::ListValue);
    args->Append(base::Value::CreateWithCopiedBuffer("challenge", 9));
    if (register_key) {
      args->AppendBoolean(*register_key);
    }
    return args;
  }

  EPKPChallengeMachineKey impl_;
  scoped_refptr<EnterprisePlatformKeysChallengeMachineKeyFunction> func_;
  base::ListValue args_;
};

TEST_F(EPKChallengeMachineKeyTest, NonEnterpriseDevice) {
  stub_install_attributes_.SetConsumerOwned();

  EXPECT_EQ(EPKPChallengeMachineKey::kNonEnterpriseDeviceError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackFalse));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  EXPECT_EQ(
      EPKPChallengeMachineKey::kKeyRegistrationFailedError,
      RunFunctionAndReturnError(func_.get(), CreateArgsRegister(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationDeviceCertificate("attest-ent-machine",
                                                        std::string());

  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _)).Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgs(), browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKChallengeMachineKeyTest, KeyNotRegisteredByDefault) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgs(), browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKChallengeMachineKeyTest, KeyNotRegistered) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgsNoRegister(), browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKChallengeMachineKeyTest, Success) {
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

  std::unique_ptr<base::Value> value(
      RunFunctionAndReturnSingleResult(func_.get(), CreateArgs(), browser()));

  ASSERT_TRUE(value->is_blob());
  EXPECT_EQ("response",
            std::string(value->GetBlob().begin(), value->GetBlob().end()));
}

TEST_F(EPKChallengeMachineKeyTest, KeyRegisteredSuccess) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(mock_attestation_flow_,
              GetCertificate(
                  chromeos::attestation::PROFILE_ENTERPRISE_MACHINE_CERTIFICATE,
                  _, _, _, _))
      .Times(1);
  // TpmAttestationRegisterKey must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_DEVICE,
                                        _ /* Unused by the API. */,
                                        "attest-ent-machine", _))
      .Times(1);
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(
      mock_async_method_caller_,
      TpmAttestationSignEnterpriseChallenge(
          chromeos::attestation::KEY_DEVICE, cryptohome::Identification(),
          "attest-ent-machine", "google.com", "device_id", _, "challenge", _))
      .Times(1);

  std::unique_ptr<base::Value> value(RunFunctionAndReturnSingleResult(
      func_.get(), CreateArgsRegister(), browser()));

  ASSERT_TRUE(value->is_blob());
  EXPECT_EQ("response",
            std::string(value->GetBlob().begin(), value->GetBlob().end()));
}

TEST_F(EPKChallengeMachineKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  EXPECT_EQ(GetCertificateError(kResetRequired),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeMachineKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

class EPKChallengeUserKeyTest : public EPKChallengeKeyTestBase {
 protected:
  EPKChallengeUserKeyTest()
      : impl_(&cryptohome_client_,
              &mock_async_method_caller_,
              &mock_attestation_flow_,
              &stub_install_attributes_),
        func_(new EnterprisePlatformKeysChallengeUserKeyFunction(&impl_)) {
    func_->set_extension(extension_.get());
  }

  void SetUp() override {
    EPKChallengeKeyTestBase::SetUp();

    // Set the user preferences.
    prefs_->SetBoolean(prefs::kAttestationEnabled, true);
  }

  // Returns an error string for the given code.
  std::string GetCertificateError(int error_code) {
    return base::StringPrintf(EPKPChallengeUserKey::kGetCertificateFailedError,
                              error_code);
  }

  std::unique_ptr<base::ListValue> CreateArgs() {
    return CreateArgsInternal(true);
  }

  std::unique_ptr<base::ListValue> CreateArgsNoRegister() {
    return CreateArgsInternal(false);
  }

  std::unique_ptr<base::ListValue> CreateArgsInternal(bool register_key) {
    std::unique_ptr<base::ListValue> args(new base::ListValue);
    args->Append(base::Value::CreateWithCopiedBuffer("challenge", 9));
    args->AppendBoolean(register_key);
    return args;
  }

  EPKPChallengeUserKey impl_;
  scoped_refptr<EnterprisePlatformKeysChallengeUserKeyFunction> func_;
};

TEST_F(EPKChallengeUserKeyTest, UserPolicyDisabled) {
  prefs_->SetBoolean(prefs::kAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeUserKey::kUserPolicyDisabledError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, ExtensionNotWhitelisted) {
  base::ListValue empty_whitelist;
  prefs_->Set(prefs::kAttestationExtensionWhitelist, empty_whitelist);

  EXPECT_EQ(EPKPChallengeKeyBase::kExtensionNotWhitelistedError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, DevicePolicyDisabled) {
  settings_helper_.SetBoolean(chromeos::kDeviceAttestationEnabled, false);

  EXPECT_EQ(EPKPChallengeKeyBase::kDevicePolicyDisabledError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, DoesKeyExistDbusFailed) {
  cryptohome_client_.set_tpm_attestation_does_key_exist_should_succeed(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, GetCertificateFailed) {
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _))
      .WillRepeatedly(Invoke(GetCertificateCallbackFalse));

  EXPECT_EQ(GetCertificateError(kGetCertificateFailed),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, SignChallengeFailed) {
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationSignEnterpriseChallenge(_, _, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(SignChallengeCallbackFalse));

  EXPECT_EQ(EPKPChallengeKeyBase::kSignChallengeFailedError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, KeyRegistrationFailed) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .WillRepeatedly(Invoke(RegisterKeyCallbackFalse));

  EXPECT_EQ(EPKPChallengeUserKey::kKeyRegistrationFailedError,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, KeyExists) {
  cryptohome_client_.SetTpmAttestationUserCertificate(
      cryptohome::CreateAccountIdentifierFromAccountId(
          AccountId::FromUserEmail(kUserEmail)),
      "attest-ent-user", std::string());
  // GetCertificate must not be called if the key exists.
  EXPECT_CALL(mock_attestation_flow_, GetCertificate(_, _, _, _, _)).Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgs(), browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKChallengeUserKeyTest, KeyNotRegistered) {
  EXPECT_CALL(mock_async_method_caller_, TpmAttestationRegisterKey(_, _, _, _))
      .Times(0);

  EXPECT_TRUE(utils::RunFunction(func_.get(), CreateArgsNoRegister(), browser(),
                                 extensions::api_test_utils::NONE));
}

TEST_F(EPKChallengeUserKeyTest, PersonalDevice) {
  stub_install_attributes_.SetConsumerOwned();

  // Currently personal devices are not supported.
  EXPECT_EQ(GetCertificateError(kUserRejected),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, Success) {
  // GetCertificate must be called exactly once.
  EXPECT_CALL(
      mock_attestation_flow_,
      GetCertificate(chromeos::attestation::PROFILE_ENTERPRISE_USER_CERTIFICATE,
                     _, _, _, _))
      .Times(1);
  const cryptohome::Identification cryptohome_id(
      AccountId::FromUserEmail(kUserEmail));
  // SignEnterpriseChallenge must be called exactly once.
  EXPECT_CALL(
      mock_async_method_caller_,
      TpmAttestationSignEnterpriseChallenge(
          chromeos::attestation::KEY_USER, cryptohome_id, "attest-ent-user",
          kUserEmail, "device_id", _, "challenge", _))
      .Times(1);
  // RegisterKey must be called exactly once.
  EXPECT_CALL(mock_async_method_caller_,
              TpmAttestationRegisterKey(chromeos::attestation::KEY_USER,
                                        cryptohome_id, "attest-ent-user", _))
      .Times(1);

  std::unique_ptr<base::Value> value(
      RunFunctionAndReturnSingleResult(func_.get(), CreateArgs(), browser()));

  ASSERT_TRUE(value->is_blob());
  EXPECT_EQ("response",
            std::string(value->GetBlob().begin(), value->GetBlob().end()));
}

TEST_F(EPKChallengeUserKeyTest, AttestationNotPrepared) {
  cryptohome_client_.set_tpm_attestation_is_prepared(false);

  EXPECT_EQ(GetCertificateError(kResetRequired),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

TEST_F(EPKChallengeUserKeyTest, AttestationPreparedDbusFailed) {
  cryptohome_client_.SetServiceIsAvailable(false);

  EXPECT_EQ(GetCertificateError(kDBusError),
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

class EPKChallengeMachineKeyUnmanagedUserTest
    : public EPKChallengeMachineKeyTest {
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

TEST_F(EPKChallengeMachineKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

class EPKChallengeUserKeyUnmanagedUserTest : public EPKChallengeUserKeyTest {
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

TEST_F(EPKChallengeUserKeyUnmanagedUserTest, UserNotManaged) {
  EXPECT_EQ(EPKPChallengeKeyBase::kUserNotManaged,
            RunFunctionAndReturnError(func_.get(), CreateArgs(), browser()));
}

}  // namespace
}  // namespace extensions
