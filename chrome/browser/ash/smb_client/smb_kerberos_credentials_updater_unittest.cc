// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_kerberos_credentials_updater.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/kerberos/kerberos_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunClosure;

namespace ash::smb_client {

namespace {

constexpr char kProfileEmail[] = "gaia_user@example.com";
constexpr char kPrincipal[] = "user@EXAMPLE.COM";
constexpr char kOtherPrincipal[] = "icebear_cloud@EXAMPLE.COM";
constexpr char kPassword[] = "m1sst1ped>_<";
constexpr char kConfig[] = "[libdefaults]";

}  // namespace

class SmbKerberosCredentialsUpdaterTest : public testing::Test {
 public:
  SmbKerberosCredentialsUpdaterTest()
      : scoped_user_manager_(std::make_unique<FakeChromeUserManager>()),
        local_state_(TestingBrowserProcess::GetGlobal()) {
    // Enable Kerberos via policy.
    SetPref(prefs::kKerberosEnabled, base::Value(true));

    auto* user_manager =
        static_cast<FakeChromeUserManager*>(user_manager::UserManager::Get());
    user_manager->AddUser(AccountId::FromUserEmail(kProfileEmail));

    // Initialize User, Profile and KerberosCredentialsManager.
    KerberosClient::InitializeFake();
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName(kProfileEmail);
    profile_ = profile_builder.Build();
    credentials_manager_ = std::make_unique<KerberosCredentialsManager>(
        local_state_.Get(), profile_.get());
  }

  ~SmbKerberosCredentialsUpdaterTest() override {
    // Remove KerberosCredentialsManager instance, before shutting down
    // KerberosClient.
    credentials_updater_.reset();
    credentials_manager_.reset();
    KerberosClient::Shutdown();
  }

 protected:
  void AddAccountAndAuthenticate(
      const std::string& principal_name,
      KerberosCredentialsManager::ResultCallback callback) {
    credentials_manager_->AddAccountAndAuthenticate(
        principal_name, /*is_managed=*/false, kPassword,
        /*remember_password=*/true, kConfig, /*allow_existing=*/false,
        std::move(callback));
  }

  void SetPref(const char* name, base::Value value) {
    local_state_.Get()->SetManagedPref(
        name, std::make_unique<base::Value>(std::move(value)));
  }

  void CreateCredentialsUpdater(
      SmbKerberosCredentialsUpdater::ActiveAccountChangedCallback callback) {
    credentials_updater_ = std::make_unique<SmbKerberosCredentialsUpdater>(
        credentials_manager_.get(), std::move(callback));
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<KerberosCredentialsManager> credentials_manager_;
  std::unique_ptr<SmbKerberosCredentialsUpdater> credentials_updater_;
};

TEST_F(SmbKerberosCredentialsUpdaterTest, FirstAccountAdded) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(const std::string& account_identifier)>
      account_changed_callback;
  base::MockOnceCallback<void(kerberos::ErrorType)> result_callback;

  CreateCredentialsUpdater(account_changed_callback.Get());
  EXPECT_CALL(account_changed_callback, Run(kPrincipal)).Times(1);
  EXPECT_CALL(result_callback, Run(kerberos::ErrorType::ERROR_NONE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  AddAccountAndAuthenticate(kPrincipal, result_callback.Get());
  run_loop.Run();

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);

  // Set the active user to the same one.
  credentials_manager_->SetActiveAccount(kPrincipal);

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);
}

TEST_F(SmbKerberosCredentialsUpdaterTest, SecondAccountAdded) {
  base::RunLoop run_loop;
  base::RunLoop other_run_loop;
  base::MockRepeatingCallback<void(const std::string& account_identifier)>
      account_changed_callback;
  base::MockOnceCallback<void(kerberos::ErrorType)> result_callback;
  base::MockOnceCallback<void(kerberos::ErrorType)> other_result_callback;

  CreateCredentialsUpdater(account_changed_callback.Get());
  EXPECT_CALL(account_changed_callback, Run(kPrincipal)).Times(2);
  EXPECT_CALL(account_changed_callback, Run(kOtherPrincipal)).Times(1);
  EXPECT_CALL(result_callback, Run(kerberos::ErrorType::ERROR_NONE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(other_result_callback, Run(kerberos::ErrorType::ERROR_NONE))
      .WillOnce(RunClosure(other_run_loop.QuitClosure()));

  AddAccountAndAuthenticate(kPrincipal, result_callback.Get());
  run_loop.Run();

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);

  AddAccountAndAuthenticate(kOtherPrincipal, other_result_callback.Get());
  other_run_loop.Run();

  EXPECT_EQ(credentials_updater_->active_account_name(), kOtherPrincipal);

  // Change the active user back to the first one.
  credentials_manager_->SetActiveAccount(kPrincipal);

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);
}

TEST_F(SmbKerberosCredentialsUpdaterTest, KerberosGetsDisabled) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(const std::string& account_identifier)>
      account_changed_callback;
  base::MockOnceCallback<void(kerberos::ErrorType)> result_callback;

  EXPECT_CALL(account_changed_callback, Run("")).Times(1);
  EXPECT_CALL(result_callback, Run(kerberos::ErrorType::ERROR_NONE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Starting with one account added.
  AddAccountAndAuthenticate(kPrincipal, result_callback.Get());
  run_loop.Run();

  CreateCredentialsUpdater(account_changed_callback.Get());

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);

  // Disable Kerberos via policy.
  SetPref(prefs::kKerberosEnabled, base::Value(false));

  EXPECT_TRUE(credentials_updater_->active_account_name().empty());
}

TEST_F(SmbKerberosCredentialsUpdaterTest, KerberosGetsEnabled) {
  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(const std::string& account_identifier)>
      account_changed_callback;
  base::MockOnceCallback<void(kerberos::ErrorType)> result_callback;

  EXPECT_CALL(account_changed_callback, Run(kPrincipal)).Times(1);
  EXPECT_CALL(result_callback, Run(kerberos::ErrorType::ERROR_NONE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  // Starting with Kerberos disabled via policy.
  SetPref(prefs::kKerberosEnabled, base::Value(false));

  CreateCredentialsUpdater(account_changed_callback.Get());

  EXPECT_TRUE(credentials_updater_->active_account_name().empty());

  // Enable Kerberos via policy, then add an account.
  SetPref(prefs::kKerberosEnabled, base::Value(true));
  AddAccountAndAuthenticate(kPrincipal, result_callback.Get());
  run_loop.Run();

  EXPECT_EQ(credentials_updater_->active_account_name(), kPrincipal);
}

}  // namespace ash::smb_client
