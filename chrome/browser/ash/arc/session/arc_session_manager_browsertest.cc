// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/wm/window_pin_util.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
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
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kWellKnownConsumerName[] = "test@gmail.com";
constexpr char kFakeUserName[] = "test@example.com";
constexpr char kMuteAudioWithSuccessHistogram[] = "Arc.MuteAudioSuccess";
constexpr char kUnmuteAudioWithSuccessHistogram[] = "Arc.UnmuteAudioSuccess";
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

class ArcSessionManagerLockedFullscreenTest : public ArcSessionManagerTest {
 protected:
  ArcSessionManagerLockedFullscreenTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kBocaOnTaskMuteArcAudio);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerLockedFullscreenTest,
                       ArcDisabledInLockedFullscreen) {
  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());

  // ARC should be disabled in locked fullscreen.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  ASSERT_EQ(ArcSessionManager::State::STOPPED,
            ArcSessionManager::Get()->state());

  // ARC should not remain disabled once we exit this mode.
  UnpinWindow(browser()->window()->GetNativeWindow());
  EXPECT_NE(ArcSessionManager::State::STOPPED,
            ArcSessionManager::Get()->state());
}

// TODO - crbug.com/401589420: Move audio tests to the
// //c/b/ash/arc/locked_fullscreen folder.
class ArcSessionManagerLockedFullscreenWithMuteAudioTest
    : public ArcSessionManagerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ArcSessionManagerLockedFullscreenWithMuteAudioTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kBocaOnTaskMuteArcAudio);
  }

  bool IsMuteArcVMAudioSuccess() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ArcSessionManagerLockedFullscreenWithMuteAudioTest,
                       AttemptArcVMMuteAudioInLockedFullscreen) {
  base::HistogramTester histogram_tester;
  EnableArc();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());

  ash::FakeConciergeClient* const concierge_client =
      ash::FakeConciergeClient::Get();
  vm_tools::concierge::SuccessFailureResponse mute_vm_audio_response;
  mute_vm_audio_response.set_success(IsMuteArcVMAudioSuccess());
  concierge_client->set_mute_vm_audio_response(mute_vm_audio_response);

  // ARC should remain enabled when entering fullscreen mode. This is because
  // we attempt to mute ARC VM audio instead.
  PinWindow(browser()->window()->GetNativeWindow(), /*trusted=*/true);
  content::RunAllTasksUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
  EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 1);
  histogram_tester.ExpectUniqueSample(kMuteAudioWithSuccessHistogram,
                                      IsMuteArcVMAudioSuccess(), 1);

  // ARC should remain enabled once we exit locked fullscreen mode.
  UnpinWindow(browser()->window()->GetNativeWindow());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
  EXPECT_EQ(concierge_client->mute_vm_audio_call_count(), 2);
  histogram_tester.ExpectUniqueSample(kUnmuteAudioWithSuccessHistogram,
                                      IsMuteArcVMAudioSuccess(), 1);
}

INSTANTIATE_TEST_SUITE_P(ArcSessionManagerLockedFullscreenWithMuteAudioTests,
                         ArcSessionManagerLockedFullscreenWithMuteAudioTest,
                         ::testing::Bool());

}  // namespace arc
