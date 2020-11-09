// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_kerberos_credentials_updater.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/kerberos/kerberos_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kProfileEmail[] = "gaia_user@example.com";
constexpr char kPrincipal[] = "user@EXAMPLE.COM";
constexpr char kPassword[] = "m1sst1ped>_<";
constexpr char kConfig[] = "[libdefaults]";

std::unique_ptr<MockUserManager> CreateMockUserManager() {
  std::unique_ptr<MockUserManager> mock_user_manager =
      std::make_unique<testing::NiceMock<MockUserManager>>();
  mock_user_manager->AddUser(AccountId::FromUserEmail(kProfileEmail));
  return mock_user_manager;
}

}  // namespace

class SmbKerberosCredentialsUpdaterTest : public testing::Test {
 public:
  SmbKerberosCredentialsUpdaterTest()
      : scoped_user_manager_(CreateMockUserManager()),
        local_state_(TestingBrowserProcess::GetGlobal()) {
    // Enable Kerberos via policy.
    local_state_.Get()->SetManagedPref(prefs::kKerberosEnabled,
                                       std::make_unique<base::Value>(true));
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
    credentials_manager_.reset();
    KerberosClient::Shutdown();
  }

 protected:
  void AddAndAuthenticate(const std::string& principal_name) {
    base::RunLoop run_loop;
    credentials_manager_->AddAccountAndAuthenticate(
        principal_name, /*is_managed=*/true, kPassword,
        /*remember_password=*/true, kConfig, /*allow_existing=*/false,
        base::BindLambdaForTesting([&run_loop](kerberos::ErrorType result) {
          EXPECT_EQ(kerberos::ERROR_NONE, result);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager scoped_user_manager_;
  ScopedTestingLocalState local_state_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<KerberosCredentialsManager> credentials_manager_;
};

TEST_F(SmbKerberosCredentialsUpdaterTest, TestActiveAccountChanged) {
  // If active accounts changed callback is called, we will set this variable to
  // true.
  bool callback_called = false;
  std::string callback_account;

  smb_client::SmbKerberosCredentialsUpdater credentials_updater(
      credentials_manager_.get(),
      base::BindLambdaForTesting([&callback_called, &callback_account](
                                     const std::string& account_identifier) {
        callback_called = true;
        callback_account = account_identifier;
      }));

  EXPECT_FALSE(callback_called);

  AddAndAuthenticate(kPrincipal);

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(callback_account, kPrincipal);

  callback_called = false;

  // Try to notify the change now without changing/adding user.
  credentials_manager_->SetActiveAccount(kPrincipal);

  EXPECT_FALSE(callback_called);
}

}  // namespace chromeos
