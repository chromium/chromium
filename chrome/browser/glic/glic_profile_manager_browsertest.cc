// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <string>
#include <type_traits>

#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic_features.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
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

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

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
    SigninWithPrimaryAccount(browser()->GetProfile());
    SetGlicCapability(browser()->GetProfile(), true);
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

// TODO(b/453696965): Broken in multi-instance.
IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       DISABLED_ProfileForLaunch_WithDetachedGlic) {
  auto* profile0 = browser()->GetProfile();
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
  CreateBrowser(profile1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // Simulate showing detached for Profile 0.
  // Profile 0 should now be selected for launch.
  service0->SetWindowDetached(true);
  EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_BasedOnActivationOrder) {
  auto* profile0 = browser()->GetProfile();
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

  bool is_wayland = false;
#if BUILDFLAG(IS_OZONE)
  is_wayland = ::ui::OzonePlatform::RunningOnWaylandForTest();
#endif
  if (!is_wayland) {
    // profile0 is the most recently used profile
#if BUILDFLAG(IS_CHROMEOS)
    session_manager::SessionManager::Get()->SwitchActiveSession(kAccountId0);
#endif  //  BUILDFLAG(IS_CHROMEOS)
    browser()->GetWindow()->Activate();
    ui_test_utils::WaitForBrowserSetLastActive(browser());
    EXPECT_EQ(profile0, profile_manager->GetProfileForLaunch());
  }
}

}  // namespace

class GlicProfileManagerDidSelectProfileTest
    : public GlicProfileManagerBrowserTest {
 public:
  GlicProfileManagerDidSelectProfileTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kGlicMultiInstance, mojom::features::kGlicMultiTab,
         features::kGlicMultitabUnderlines},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerDidSelectProfileTest,
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
  glic::GlicKeyedService::Get(profile)->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kNotStarted);
  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  EXPECT_CALL(*service,
              ShowUI(nullptr, mojom::InvocationSource::kProfilePicker));

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerDidSelectProfileTest,
                       DidSelectProfile_Consented) {
  // Create a profile that is eligible and has consented.
  Profile* profile =
#if BUILDFLAG(IS_CHROMEOS)
      CreateNewUserSessionAndProfile(kAccountId1, /*allow_glic=*/true);
#else
      CreateNewProfile(/*signin_and_allow_glic=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS)
  glic::GlicKeyedService::Get(profile)->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kCompleted);
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile));

  auto* service = GetMockGlicKeyedService(profile);

  EXPECT_CALL(*service, ShowUI(testing::IsNull(),
                               mojom::InvocationSource::kProfilePicker));

  GlicProfileManager::GetInstance()->DidSelectProfile(profile);
}

}  // namespace glic
