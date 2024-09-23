// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <memory>
#include <string>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/request_handler_for_check_android_management.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kWellKnownConsumerName[] = "test@gmail.com";
constexpr char kFakeUserName[] = "test@example.com";
constexpr char kFakeGaiaId[] = "1234567890";

std::unique_ptr<KeyedService> CreateCertificateProviderService(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::CertificateProviderService>();
}

}  // namespace

namespace arc {

// Waits for the "arc.enabled" preference value from true to false.
class ArcPlayStoreDisabledWaiter : public ArcSessionManagerObserver {
 public:
  ArcPlayStoreDisabledWaiter() { ArcSessionManager::Get()->AddObserver(this); }

  ArcPlayStoreDisabledWaiter(const ArcPlayStoreDisabledWaiter&) = delete;
  ArcPlayStoreDisabledWaiter& operator=(const ArcPlayStoreDisabledWaiter&) =
      delete;

  ~ArcPlayStoreDisabledWaiter() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  void Wait() {
    base::RunLoop run_loop;
    base::AutoReset<raw_ptr<base::RunLoop>> reset(&run_loop_, &run_loop);
    run_loop.Run();
  }

 private:
  // ArcSessionManagerObserver override:
  void OnArcPlayStoreEnabledChanged(bool enabled) override {
    if (!enabled) {
      DCHECK(run_loop_);
      run_loop_->Quit();
    }
  }

  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

class ArcSessionManagerTest : public MixinBasedInProcessBrowserTest {
 protected:
  ArcSessionManagerTest() = default;

  ArcSessionManagerTest(const ArcSessionManagerTest&) = delete;
  ArcSessionManagerTest& operator=(const ArcSessionManagerTest&) = delete;

  ~ArcSessionManagerTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    // Init ArcSessionManager for testing.
    ArcServiceLauncher::Get()->ResetForTesting();
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcSessionManager::Get()->SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ExpandPropertyFilesForTesting(ArcSessionManager::Get());

    ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);

    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        fake_user_manager_->GetPrimaryUser(), profile_.get());

    // Seed account info properly.
    identity_test_env()->MakePrimaryAccountAvailable(
        kFakeUserName, signin::ConsentLevel::kSignin);

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);

    // TestingProfile is not interpreted as a primary profile. Inject factory so
    // that the instance of CertificateProviderService for the profile can be
    // created.
    chromeos::CertificateProviderServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(), base::BindRepeating(&CreateCertificateProviderService));

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
    // destructor of ash::FakeChromeUserManager.
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, kFakeGaiaId));
    fake_user_manager_->RemoveUserFromList(account_id);
    // Since ArcServiceLauncher is (re-)set up with profile() in
    // SetUpOnMainThread() it is necessary to Shutdown() before the profile()
    // is destroyed. ArcServiceLauncher::Shutdown() will be called again on
    // fixture destruction (because it is initialized with the original Profile
    // instance in fixture, once), but it should be no op.
    // TODO(hidehiko): Think about a way to test the code cleanly.
    ArcServiceLauncher::Get()->Shutdown();
    identity_test_environment_adaptor_.reset();
    profile_.reset();
    base::RunLoop().RunUntilIdle();
    ash::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
    fake_user_manager_.Reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void EnableArc() {
    session_manager::SessionManager::Get()
        ->HandleUserSessionStartUpTaskCompleted();

    PrefService* const prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kArcEnabled, true);
    base::RunLoop().RunUntilIdle();
  }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  Profile* profile() { return profile_.get(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_environment_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ConsumerAccount) {
  EnableArc();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      policy::kUnmanagedAuthToken, base::Time::Max());
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
      profile()->GetProfilePolicyConnector();
  connector->OverrideIsManagedForTesting(true);

  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ManagedAndroidAccount) {
  EnableArc();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      policy::kManagedAuthToken, base::Time::Max());
  ArcPlayStoreDisabledWaiter().Wait();
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
}

// Make sure that ARC is disabled upon entering locked fullscreen mode.
IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ArcDisabledInLockedFullscreen) {
  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());

  const int window_id = extensions::ExtensionTabUtil::GetWindowId(browser());
  const char kStateLockedFullscreen[] =
      "[%u, {\"state\": \"locked-fullscreen\"}]";

  auto function = base::MakeRefCounted<extensions::WindowsUpdateFunction>();
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder("Test")
          .SetID("pmgljoohajacndjcjlajcopidgnhphcl")
          .AddAPIPermission("lockWindowFullscreenPrivate")
          .Build());
  function->set_extension(extension.get());

  std::optional<base::Value> value =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), base::StringPrintf(kStateLockedFullscreen, window_id),
          browser()->profile());

  ASSERT_EQ(ArcSessionManager::State::STOPPED,
            ArcSessionManager::Get()->state());
}

}  // namespace arc
