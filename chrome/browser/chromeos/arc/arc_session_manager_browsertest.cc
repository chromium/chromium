// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kRefreshToken[] = "fake-refresh-token";
// Set managed auth token for Android managed accounts.
constexpr char kManagedAuthToken[] = "managed-auth-token";
// Set unmanaged auth token for other Android unmanaged accounts.
constexpr char kUnmanagedAuthToken[] = "unmanaged-auth-token";
constexpr char kWellKnownConsumerName[] = "test@gmail.com";
constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeGaiaId[] = "1234567890";

}  // namespace

namespace arc {

// Waits for the "arc.enabled" preference value from true to false.
class ArcPlayStoreDisabledWaiter : public ArcSessionManager::Observer {
 public:
  ArcPlayStoreDisabledWaiter() { ArcSessionManager::Get()->AddObserver(this); }

  ~ArcPlayStoreDisabledWaiter() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  void Wait() {
    base::RunLoop run_loop;
    base::AutoReset<base::RunLoop*> reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

 private:
  // ArcSessionManager::Observer override:
  void OnArcPlayStoreEnabledChanged(bool enabled) override {
    if (!enabled) {
      DCHECK(run_loop_);
      run_loop_->Quit();
    }
  }

  base::RunLoop* run_loop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreDisabledWaiter);
};

class ArcSessionManagerTest : public InProcessBrowserTest {
 protected:
  ArcSessionManagerTest() {}

  // InProcessBrowserTest:
  ~ArcSessionManagerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Start test device management server.
    test_server_.reset(new policy::LocalPolicyTestServer());
    ASSERT_TRUE(test_server_->Start());

    // Specify device management server URL.
    std::string url = test_server_->GetServiceURL().spec();
    base::CommandLine* const command_line =
        base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    url);
  }

  void SetUpOnMainThread() override {
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);
    profile_builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    profile_builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    profile_ = profile_builder.Build();

    // Seed account info properly.
    const std::string account_id_str =
        AccountTrackerServiceFactory::GetForProfile(profile_.get())
            ->SeedAccountInfo(kFakeGaiaId, kFakeUserName);
    token_service_ = static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
    FakeSigninManagerForTesting* fake_signin_manager =
        static_cast<FakeSigninManagerForTesting*>(
            SigninManagerFactory::GetForProfile(profile()));
    fake_signin_manager->SignIn(account_id_str);
    token_service_->UpdateCredentials(account_id_str, kRefreshToken);

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);

    // Set up ARC for test profile.
    // Currently, ArcSessionManager is singleton and set up with the original
    // Profile instance. This re-initializes the ArcServiceLauncher by
    // overwriting Profile with profile().
    // TODO(hidehiko): This way several ArcService instances created with
    // the original Profile instance on Browser creatuion are kept in the
    // ArcServiceManager. For proper overwriting, those should be removed.
    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());
  }

  void TearDownOnMainThread() override {
    // Explicitly removing the user is required; otherwise ProfileHelper keeps
    // a dangling pointer to the User.
    // TODO(nya): Consider removing all users from ProfileHelper in the
    // destructor of FakeChromeUserManager.
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    GetFakeUserManager()->RemoveUserFromList(account_id);
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    // TODO(hidehiko): Think about a way to test the code cleanly.
    ArcServiceLauncher::Get()->Shutdown();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    user_manager_enabler_.reset();
    test_server_.reset();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void EnableArc() {
    PrefService* const prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kArcEnabled, true);
    base::RunLoop().RunUntilIdle();
  }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  Profile* profile() { return profile_.get(); }

  FakeProfileOAuth2TokenService* token_service() { return token_service_; }

 private:
  std::unique_ptr<policy::LocalPolicyTestServer> test_server_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  FakeProfileOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTest);
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ConsumerAccount) {
  EnableArc();
  token_service()->IssueTokenForAllPendingRequests(kUnmanagedAuthToken,
                                                   base::Time::Max());
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, WellKnownConsumerAccount) {
  set_profile_name(kWellKnownConsumerName);
  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ManagedChromeAccount) {
  policy::ProfilePolicyConnector* const connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile());
  connector->OverrideIsManagedForTesting(true);

  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ManagedAndroidAccount) {
  EnableArc();
  token_service()->IssueTokenForAllPendingRequests(kManagedAuthToken,
                                                   base::Time::Max());
  ArcPlayStoreDisabledWaiter().Wait();
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

}  // namespace arc
