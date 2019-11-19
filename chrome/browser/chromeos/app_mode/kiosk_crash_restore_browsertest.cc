// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/ownership/mock_owner_key_util.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

const char kTestKioskApp[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";

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
    OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util_);
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
        CryptohomeClient::GetStubSanitizedUsername(cryptohome_id));

    fake_cws_->Init(embedded_test_server());
    fake_cws_->SetUpdateCrx(test_app_id_, test_app_id_ + ".crx", "1.0.0");
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

  const std::string GetTestAppUserId() const {
    return policy::GenerateDeviceLocalAccountUserId(
        test_app_id_, policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }

  const std::string& test_app_id() const { return test_app_id_; }

 private:
  void SetUpExistingKioskApp() {
    // Create policy data that contains the test app as an existing kiosk app.
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_.payload().mutable_device_local_accounts();

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(test_app_id_);
    account->set_type(
        em::DeviceLocalAccountInfoProto_AccountType_ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(test_app_id_);
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
    base::WriteFile(local_state_file, local_state_json.data(),
                    local_state_json.size());
  }

  std::string test_app_id_ = kTestKioskApp;

  policy::DevicePolicyBuilder device_policy_;
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util_;
  std::unique_ptr<FakeCWS> fake_cws_;

  DISALLOW_COPY_AND_ASSIGN(KioskCrashRestoreTest);
};

IN_PROC_BROWSER_TEST_F(KioskCrashRestoreTest, AppNotInstalled) {
  // If app is not installed when restoring from crash, the kiosk launch is
  // expected to fail, as in that case the crash occured during the app
  // initialization - before the app was actually launched.
  EXPECT_EQ(KioskAppLaunchError::UNABLE_TO_LAUNCH, KioskAppLaunchError::Get());
}

}  // namespace chromeos
