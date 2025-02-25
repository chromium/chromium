// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <memory>
#include <string>

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
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/test/regular_logged_in_browser_test_mixin.h"
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
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "chromeos/ash/experiences/arc/session/arc_session_runner.h"
#include "chromeos/ash/experiences/arc/test/arc_util_test_support.h"
#include "chromeos/ash/experiences/arc/test/fake_arc_session.h"
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
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kWellKnownConsumerName[] = "test@gmail.com";
constexpr char kFakeUserName[] = "test@example.com";
constexpr GaiaId::Literal kFakeGaiaId("1234567890");

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
  explicit ArcSessionManagerTest(std::string_view user_email = kFakeUserName)
      : account_id_(AccountId::FromUserEmailGaiaId(user_email, kFakeGaiaId)) {}

  ArcSessionManagerTest(const ArcSessionManagerTest&) = delete;
  ArcSessionManagerTest& operator=(const ArcSessionManagerTest&) = delete;

  ~ArcSessionManagerTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Init ArcSessionManager for testing.
    ArcSessionManager::SetUiEnabledForTesting(false);
    ArcSessionManager::EnableCheckAndroidManagementForTesting(true);
    ArcServiceLauncher::SetArcSessionRunnerForTesting(
        std::make_unique<ArcSessionRunner>(
            base::BindRepeating(FakeArcSession::Create)));
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    MixinBasedInProcessBrowserTest::SetUpBrowserContextKeyedServices(context);

    // Inject only for a user Profile.
    if (ash::IsSigninBrowserContext(context)) {
      return;
    }
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    chromeos::CertificateProviderServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(&CreateCertificateProviderService));
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    // Seed account info properly.
    identity_test_env()->MakePrimaryAccountAvailable(
        account_id_.GetUserEmail(), signin::ConsentLevel::kSignin);

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);
    profile()->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, true);
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  void EnableArc() {
    PrefService* const prefs = profile()->GetPrefs();
    prefs->SetBoolean(prefs::kArcEnabled, true);
    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return browser()->profile(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_environment_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }

 private:
  const AccountId account_id_;
  ash::RegularLoggedInBrowserTestMixin logged_in_mixin_{&mixin_host_,
                                                        account_id_};
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ConsumerAccount) {
  EnableArc();
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      policy::kUnmanagedAuthToken, base::Time::Max());
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

class ArcSessionManagerWellKnownConsumerNameTest
    : public ArcSessionManagerTest {
 public:
  ArcSessionManagerWellKnownConsumerNameTest()
      : ArcSessionManagerTest(kWellKnownConsumerName) {}
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerWellKnownConsumerNameTest,
                       WellKnownConsumerAccount) {
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
