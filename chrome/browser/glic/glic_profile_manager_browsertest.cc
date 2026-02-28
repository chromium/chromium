// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <string>
#include <type_traits>

#include "base/memory/memory_pressure_level.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/account_id/account_id_literal.h"  // nogncheck
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {
namespace {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  MockGlicKeyedService(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager,
                         glic_profile_manager,
                         contextual_cueing_service,
                         actor_keyed_service) {}
  MOCK_METHOD(void, CloseFloatingPanel, (), (override));
  MOCK_METHOD(void,
              OpenFreDialogInNewTab,
              (BrowserWindowInterface*, mojom::InvocationSource),
              (override));
  MOCK_METHOD(void,
              ToggleUI,
              (BrowserWindowInterface*,
               bool,
               mojom::InvocationSource,
               std::optional<std::string>,
               bool),
              (override));

  bool IsWindowDetached() const override { return detached_; }
  void SetWindowDetached() { detached_ = true; }

  bool IsWindowShowing() const override { return showing_; }
  void SetWindowShowing() { showing_ = true; }

 private:
  bool detached_ = false;
  bool showing_ = false;
};

class GlicProfileManagerBrowserTest : public InProcessBrowserTest {
 public:
  GlicProfileManagerBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDestroyProfileOnBrowserClose);

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &GlicProfileManagerBrowserTest::SetTestingFactory,
                base::Unretained(this)));

    // Manually set up these states with `SigninWithPrimaryAccount` and
    // `SetGlicCapability`.
    glic_test_environment_.SetForceSigninAndModelExecutionCapability(false);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Enable GLIC for the default profile.
    SigninWithPrimaryAccount(browser()->profile());
    SetGlicCapability(browser()->profile(), true);
  }

#if BUILDFLAG(IS_CHROMEOS)
  void SetUpLocalStatePrefService(PrefService* local_state) override {
    InProcessBrowserTest::SetUpLocalStatePrefService(local_state);

    user_manager::TestHelper::RegisterPersistedUser(*local_state, kAccountId0);
    user_manager::TestHelper::RegisterPersistedUser(*local_state, kAccountId1);
    user_manager::TestHelper::RegisterPersistedUser(*local_state, kAccountId2);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // Log-in with the first user.
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        cryptohome::Identification(kAccountId0).id());
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    kAccountId0.GetUserEmail());
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  MockGlicKeyedService* GetMockGlicKeyedService(Profile* profile) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    return static_cast<MockGlicKeyedService*>(service);
  }

  // In ChromeOS, each regular profile is associated with a user session.
#if BUILDFLAG(IS_CHROMEOS)
  Profile* CreateNewUserSessionAndProfile(const AccountId& account_id,
                                          bool allow_glic) {
    auto userhash = user_manager::TestHelper::GetFakeUsernameHash(account_id);
    session_manager::SessionManager::Get()->CreateSession(
        account_id, userhash,
        /*new_user=*/false,
        /*has_active_session=*/false);

    Profile* new_profile = nullptr;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;

      base::FilePath user_data_dir =
          base::PathService::CheckedGet(chrome::DIR_USER_DATA);
      base::FilePath profile_dir = user_data_dir.AppendASCII(
          ash::BrowserContextHelper::GetUserBrowserContextDirName(userhash));
      new_profile =
          g_browser_process->profile_manager()->GetProfile(profile_dir);
    }
    CHECK(new_profile);
    CHECK_EQ(account_id, *ash::AnnotatedAccountId::Get(new_profile));

    // Session is automatically switched to the new user when its corresponding
    // profile is created and initialized.
    CHECK_EQ(account_id, session_manager::SessionManager::Get()
                             ->GetActiveSession()
                             ->account_id());

    SigninWithPrimaryAccount(new_profile);
    SetGlicCapability(new_profile, allow_glic);
    return new_profile;
  }
#else
  Profile* CreateNewProfile(bool signin_and_allow_glic) {
    auto* profile_manager = g_browser_process->profile_manager();
    auto new_path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, new_path);
    Profile* new_profile = profile_manager->GetProfile(new_path);

    if (signin_and_allow_glic) {
      SigninWithPrimaryAccount(new_profile);
      SetGlicCapability(new_profile, true);
    }
    return new_profile;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  void SetTestingFactory(content::BrowserContext* context) {
    GlicKeyedServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(
                     &GlicProfileManagerBrowserTest::CreateMockGlicKeyedService,
                     base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMockGlicKeyedService(
      content::BrowserContext* context) {
    auto* identitity_manager = IdentityManagerFactory::GetForProfile(
        Profile::FromBrowserContext(context));
    auto* actor_keyed_service =
        actor::ActorKeyedServiceFactory::GetActorKeyedService(context);
    return std::make_unique<MockGlicKeyedService>(
        context, identitity_manager, g_browser_process->profile_manager(),
        GlicProfileManager::GetInstance(),
        /*contextual_cueing_service=*/nullptr, actor_keyed_service);
  }

#if BUILDFLAG(IS_CHROMEOS)
  static constexpr auto kAccountId0 =
      AccountIdLiteral::FromUserEmailGaiaId("user0@example.com",
                                            GaiaId::Literal("12345"));
  static constexpr auto kAccountId1 =
      AccountIdLiteral::FromUserEmailGaiaId("user1@example.com",
                                            GaiaId::Literal("67890"));
  static constexpr auto kAccountId2 =
      AccountIdLiteral::FromUserEmailGaiaId("user2@example.com",
                                            GaiaId::Literal("abcde"));
#endif  // BUILDFLAG(IS_CHROMEOS)

  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       SetActiveGlic_SameProfile) {
  auto* service0 = GetMockGlicKeyedService(browser()->profile());
  GlicProfileManager::GetInstance()->SetActiveGlic(service0);
  // Opening glic twice for the same profile shouldn't cause it to close.
  EXPECT_CALL(*service0, CloseFloatingPanel()).Times(0);
  GlicProfileManager::GetInstance()->SetActiveGlic(service0);
}

// TODO(crbug.com/448406730): Re-enable after testing the logic of close panel
// being now handled by EmbedderDelegate.
IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       DISABLED_SetActiveGlic_DifferentProfiles) {
  auto* service0 = GetMockGlicKeyedService(browser()->profile());

  auto* profile1 =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/true);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  auto* service1 = GetMockGlicKeyedService(profile1);

  auto* profile_manager = GlicProfileManager::GetInstance();
  profile_manager->SetActiveGlic(service0);

  // Tell the mock glic to pretend that the window is open (otherwise, we won't
  // attempt to close it).
  service0->SetWindowShowing();

  // Opening glic from a second profile should make the profile manager close
  // the first one.
  EXPECT_CALL(*service0, CloseFloatingPanel());
  profile_manager->SetActiveGlic(service1);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_WithDetachedGlic) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }

  auto* profile0 = browser()->profile();
  auto* service0 = GetMockGlicKeyedService(profile0);

  // Setup Profile 1
  auto* profile1 =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/true);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  CHECK(profile1);

  auto* profile_manager = GlicProfileManager::GetInstance();
  // Profile 0 is the last used Glic and Profile 1 is the last used window.
  // Profile 1 should be selected for launch.
  profile_manager->SetActiveGlic(service0);
  CreateBrowser(profile1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // Simulate showing detached for Profile 0.
  // Profile 0 should now be selected for launch.
  service0->SetWindowDetached();
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_BasedOnActivationOrder) {
  auto* profile0 = browser()->profile();
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile0));

  // Setup Profile 1
  auto* profile1 =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/true);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile1));

  // Setup Profile 2 (not glic compliant)
  auto* profile2 =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId2, /*allow_glic=*/false);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS)
  ASSERT_FALSE(GlicEnabling::IsEnabledAndConsentForProfile(profile2));

  auto* profile_manager = GlicProfileManager::GetInstance();
  // profile0 is the most recently used profile
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());

  // profile1 is the most recently used profile
#if BUILDFLAG(IS_CHROMEOS)
  session_manager::SessionManager::Get()->SwitchActiveSession(kAccountId1);
#endif  //  BUILDFLAG(IS_CHROMEOS)
  auto* browser1 = CreateBrowser(profile1);
  ui_test_utils::WaitForBrowserSetLastActive(browser1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // profile2 is the most recently used profile but it isn't
  // compliant, so still using profile1
#if BUILDFLAG(IS_CHROMEOS)
  session_manager::SessionManager::Get()->SwitchActiveSession(kAccountId2);
#endif  //  BUILDFLAG(IS_CHROMEOS)
  auto* browser2 = CreateBrowser(profile2);
  ui_test_utils::WaitForBrowserSetLastActive(browser2);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

#if !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
  // profile0 is the most recently used profile
#if BUILDFLAG(IS_CHROMEOS)
  session_manager::SessionManager::Get()->SwitchActiveSession(kAccountId0);
#endif  //  BUILDFLAG(IS_CHROMEOS)
  browser()->window()->Activate();
  ui_test_utils::WaitForBrowserSetLastActive(browser());
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
#endif  // !BUILDFLAG(SUPPORTS_OZONE_WAYLAND)
}

class GlicProfileManagerPreloadingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  explicit GlicProfileManagerPreloadingTest(const std::string& delay_ms) {
    if (IsPrewarmingEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kGlicWarming,
                                 {{features::kGlicWarmingDelayMs.name,
                                   delay_ms},
                                  {features::kGlicWarmingJitterMs.name, "0"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kGlicWarming});
    }

    // We prevent any premature preloading by disabling it.
    GlicProfileManager::SetPrewarmingEnabledForTesting(false);
    GlicProfileManager::ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
  }

  GlicProfileManagerPreloadingTest() : GlicProfileManagerPreloadingTest("0") {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GlicProfileManager::ForceProfileForLaunchForTesting(browser()->profile());
  }

  void TearDown() override {
    GlicProfileManager::SetPrewarmingEnabledForTesting(true);
    GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
    InProcessBrowserTest::TearDown();
  }

  bool IsPrewarmingEnabled() const { return GetParam(); }

  void ResetPrewarming() {
    GlicProfileManager::SetPrewarmingEnabledForTesting(true);
  }

  GlicPrewarmingChecksResult WaitForShouldPreload() {
    base::test::TestFuture<GlicPrewarmingChecksResult> future;
    GlicProfileManager::GetInstance()->ShouldPreloadForProfile(
        browser()->profile(), future.GetCallback());
    return future.Get();
  }

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    GlicProfileManager::ForceConnectionTypeForTesting(connection_type);
  }

  bool IsWarmed() {
    auto* service =
        GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
    if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      auto& coordinator = static_cast<GlicInstanceCoordinatorImpl&>(
          service->window_controller());
      return coordinator.HasWarmedInstanceForTesting();
    } else {
      return service->GetSingleInstanceWindowController().IsWarmed();
    }
  }

 private:
  GlicTestEnvironment glic_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Success) {
  ResetPrewarming();
  const bool should_preload = IsPrewarmingEnabled();
  EXPECT_EQ(WaitForShouldPreload(),
            should_preload ? GlicPrewarmingChecksResult::kSuccess
                           : GlicPrewarmingChecksResult::kWarmingDisabled);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_NotSupportedProfile) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
  SetGlicCapability(browser()->profile(), false);
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kProfileNotEligible);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  browser()->profile()->NotifyWillBeDestroyed();
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kBrowserShuttingDown);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_MemoryPressure) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  base::RunLoop run_loop;
  base::MemoryPressureListener::SimulatePressureNotificationAsync(
      base::MEMORY_PRESSURE_LEVEL_MODERATE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kUnderMemoryPressure);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Cellular) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(WaitForShouldPreload(),
            GlicPrewarmingChecksResult::kCellularConnection);
}

// See *Deferred* below. Checks that we don't defer preloading when there's no
// delay.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_DoNotDefer) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  service->TryPreload();
  // Since we have no delay, running until idle should mean that we do warm
  // (provided warming is enabled).
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerPreloadingTest,
                         ::testing::Bool());

class GlicProfileManagerDeferredPreloadingTest
    : public GlicProfileManagerPreloadingTest {
 public:
  // This sets the delay to 500 ms.
  GlicProfileManagerDeferredPreloadingTest()
      : GlicProfileManagerPreloadingTest(/*delay_ms=*/"500") {}
  ~GlicProfileManagerDeferredPreloadingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is really a keyed service test, but it is convenient to locate it here
// for now. It just checks that if we have a preload delay, that we won't
// preload immediately.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerDeferredPreloadingTest,
                       ShouldPreloadForProfile_Defer) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  service->TryPreload();
  // Since we shouldn't preload until after the delay, we shouldn't be warmed
  // after running until idle.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsWarmed());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerDeferredPreloadingTest,
                       ShouldPreloadForProfile_DeferWithProfileDeletion) {
  if (!IsPrewarmingEnabled()) {
    GTEST_SKIP() << "This test only applies if prewarming is enabled.";
  }
  ResetPrewarming();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  base::RunLoop run_loop;
  service->AddPreloadCallback(run_loop.QuitClosure());
  service->TryPreload();
  service->reset_profile_for_test();
  run_loop.Run();
  EXPECT_FALSE(IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerDeferredPreloadingTest,
                         ::testing::Bool());

}  // namespace

class GlicProfileManagerDidSelectProfileTest
    : public GlicProfileManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicProfileManagerDidSelectProfileTest() {
    if (IsTrustFREOnboardingEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kGlicTrustFirstOnboarding, features::kGlicMultiInstance,
           mojom::features::kGlicMultiTab, features::kGlicMultitabUnderlines},
          {});
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kGlicTrustFirstOnboarding);
    }
  }

  bool IsTrustFREOnboardingEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerDidSelectProfileTest,
                       DidSelectProfile_NoConsent) {
  // Create a profile that is eligible but has not consented.
  Profile* profile =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/false);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/false);
  SigninWithPrimaryAccount(profile);
#endif  // BUILDFLAG(IS_CHROMEOS)
  SetGlicCapability(profile, true);
  profile->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kNotStarted));
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  if (IsTrustFREOnboardingEnabled()) {
    EXPECT_CALL(*service, ToggleUI(testing::NotNull(), true,
                                   mojom::InvocationSource::kProfilePicker,
                                   testing::Eq(std::nullopt), testing::_));
  } else {
    EXPECT_CALL(*service,
                OpenFreDialogInNewTab(testing::NotNull(),
                                      mojom::InvocationSource::kProfilePicker));
  }

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerDidSelectProfileTest,
                       DidSelectProfile_Consented) {
  // Create a profile that is eligible and has consented.
  Profile* profile =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/true);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  profile->GetPrefs()->SetInteger(
      glic::prefs::kGlicCompletedFre,
      static_cast<int>(glic::prefs::FreStatus::kCompleted));
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  EXPECT_CALL(*service, ToggleUI(testing::IsNull(), true,
                                 mojom::InvocationSource::kProfilePicker,
                                 testing::Eq(std::nullopt), testing::_));

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerDidSelectProfileTest,
                         testing::Bool());
}  // namespace glic
