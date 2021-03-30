// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace em = enterprise_management;
constexpr em::DeviceLocalAccountInfoProto_AccountType kWebKioskAccountType =
    em::DeviceLocalAccountInfoProto_AccountType_ACCOUNT_TYPE_WEB_KIOSK_APP;
constexpr em::DeviceLocalAccountInfoProto_AccountType kKioskAccountType =
    em::DeviceLocalAccountInfoProto_AccountType_ACCOUNT_TYPE_KIOSK_APP;

namespace ash {

namespace {

const char kTestKioskApp[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";
const char kTestWebAppId[] = "id";
const char kTestWebAppUrl[] = "https://example.com/";

}  // namespace

class KioskCrashRestoreTest : public InProcessBrowserTest {
 public:
  KioskCrashRestoreTest()
      : owner_key_util_(new ownership::MockOwnerKeyUtil()),
        fake_cws_(new FakeCWS) {}

  // InProcessBrowserTest
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  bool SetUpUserDataDirectory() override {
    SetUpExistingKioskApp();
    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Override device policy.
    OwnerSettingsServiceAshFactory::GetInstance()->SetOwnerKeyUtilForTesting(
        owner_key_util_);
    owner_key_util_->SetPublicKeyFromPrivateKey(
        *device_policy_.GetSigningKey());

    // SessionManagerClient will be destroyed in ChromeBrowserMain.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::FakeSessionManagerClient::Get()->set_device_policy(
        device_policy_.GetBlob());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    const AccountId account_id = AccountId::FromUserEmail(GetTestAppUserId());
    const cryptohome::AccountIdentifier cryptohome_id =
        cryptohome::CreateAccountIdentifierFromAccountId(account_id);

    command_line->AppendSwitchASCII(switches::kLoginUser,
                                    cryptohome_id.account_id());
    command_line->AppendSwitchASCII(
        switches::kLoginProfile,
        chromeos::UserDataAuthClient::GetStubSanitizedUsername(cryptohome_id));

    fake_cws_->Init(embedded_test_server());
    fake_cws_->SetUpdateCrx(kTestKioskApp, std::string(kTestKioskApp) + ".crx",
                            "1.0.0");
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  virtual const std::string GetTestAppUserId() const = 0;

 private:
  void SetUpExistingKioskApp() {
    // Create policy data that contains the test app as an existing kiosk app.
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_.payload().mutable_device_local_accounts();

    {
      em::DeviceLocalAccountInfoProto* const account =
          device_local_accounts->add_account();
      account->set_account_id(kTestKioskApp);
      account->set_type(kKioskAccountType);
      account->mutable_kiosk_app()->set_app_id(kTestKioskApp);
    }
    {
      em::DeviceLocalAccountInfoProto* const account =
          device_local_accounts->add_account();
      account->set_account_id(kTestWebAppId);
      account->set_type(kWebKioskAccountType);
      account->mutable_web_kiosk_app()->set_url(kTestWebAppUrl);
    }
    device_policy_.Build();

    // Prepare the policy data to store in device policy cache.
    em::PolicyData policy_data;
    CHECK(device_policy_.payload().SerializeToString(
        policy_data.mutable_policy_value()));
    const std::string policy_data_string = policy_data.SerializeAsString();
    std::string encoded;
    base::Base64Encode(policy_data_string, &encoded);

    // Store policy data and existing device local accounts in local state.
    const std::string local_state_json =
        extensions::DictionaryBuilder()
            .Set(prefs::kDeviceSettingsCache, encoded)
            .Set("PublicAccounts",
                 extensions::ListBuilder().Append(GetTestAppUserId()).Build())
            .ToJSON();

    base::FilePath local_state_file;
    CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &local_state_file));
    local_state_file = local_state_file.Append(chrome::kLocalStateFilename);
    base::WriteFile(local_state_file, local_state_json);
  }

  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  std::unique_ptr<FakeCWS> fake_cws_;

  DISALLOW_COPY_AND_ASSIGN(KioskCrashRestoreTest);
};

class ChromeKioskCrashRestoreTest : public KioskCrashRestoreTest {
  const std::string GetTestAppUserId() const override {
    return policy::GenerateDeviceLocalAccountUserId(
        kTestKioskApp, policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }
};

IN_PROC_BROWSER_TEST_F(ChromeKioskCrashRestoreTest, ChromeAppNotInstalled) {
  // If app is not installed when restoring from crash, the kiosk launch is
  // expected to fail, as in that case the crash occured during the app
  // initialization - before the app was actually launched.
  EXPECT_EQ(KioskAppLaunchError::Error::kUnableToLaunch,
            KioskAppLaunchError::Get());
}

class WebKioskCrashRestoreTest : public KioskCrashRestoreTest {
  const std::string GetTestAppUserId() const override {
    return policy::GenerateDeviceLocalAccountUserId(
        kTestWebAppId, policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP);
  }
};

IN_PROC_BROWSER_TEST_F(WebKioskCrashRestoreTest, WebKioskLaunches) {
  // If app is not installed when restoring from crash, the kiosk launch is
  // expected to fail, as in that case the crash occured during the app
  // initialization - before the app was actually launched.
  EXPECT_EQ(KioskAppLaunchError::Error::kNone, KioskAppLaunchError::Get());
  KioskSessionInitializedWaiter().Wait();
}

}  // namespace ash
