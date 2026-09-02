// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/policy/profile_policy_connector_builder.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#endif

namespace glic {

namespace {

void WaitForWarmingDelay() {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();
}

}  // namespace

class GlicWarmingPoolBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicWarmingPoolBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlicWarming,
         {{"glic-warming-delay-ms", "100"}, {"glic-warming-jitter-ms", "0"}}},
        {features::kGlicWebContentsWarming,
         {{"glic-web-contents-warming-delay", "100ms"}}},
    };
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features, {});
  }

  GlicWebContentsWarmingPool& pool() {
    return coordinator().GetWebContentsWarmingPoolForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(b/496609005): Skip on ChromeOS due to profile ineligibility timeouts.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ColdWarming DISABLED_ColdWarming
#else
#define MAYBE_ColdWarming ColdWarming
#endif
IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest, MAYBE_ColdWarming) {
  EXPECT_TRUE(
      RunUntil([this]() { return pool().HasWarmedContainerForTesting(); },
               "Wait for cold warming"));
}

// TODO(b/496609005): Skip on ChromeOS due to profile ineligibility timeouts.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_BackfillWarming DISABLED_BackfillWarming
#else
#define MAYBE_BackfillWarming BackfillWarming
#endif
IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest, MAYBE_BackfillWarming) {
  // Wait for initial preload to complete.
  EXPECT_TRUE(
      RunUntil([this]() { return pool().HasWarmedContainerForTesting(); },
               "Wait for initial cold warming"));

  // Take the container, which should clear it and schedule a new delayed
  // preload.
  std::unique_ptr<WebUIContentsContainer> container = pool().TakeContainer();
  EXPECT_TRUE(container);
  EXPECT_FALSE(pool().HasWarmedContainerForTesting());

  // Wait for the delay timer to trigger a new preload.
  base::TimeTicks start = base::TimeTicks::Now();
  EXPECT_TRUE(
      RunUntil([this]() { return pool().HasWarmedContainerForTesting(); },
               "Wait for backfill warming"));
  base::TimeDelta elapsed = base::TimeTicks::Now() - start;

  // Verify that it waited approximately the configured 100ms.
  EXPECT_GE(elapsed,
            base::Milliseconds(90));  // Allow slight scheduling leeway.
}

class GlicManualWarmingPoolBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicManualWarmingPoolBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kGlicWebContentsWarming};
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(features::kDestroyProfileOnBrowserClose);
#endif
    scoped_feature_list_.InitWithFeatures(
        enabled_features,
        /*disabled_features=*/{features::kGlicWarming});
  }

  GlicWebContentsWarmingPool& pool() {
    return coordinator().GetWebContentsWarmingPoolForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Multiple profiles and profile deletion upon window close are only supported
// on desktop (Win/Mac/Linux).
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicManualWarmingPoolBrowserTest,
                       DestroyProfileWithWarmedContainerDoesNotCrash) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath new_profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();

  Profile* new_profile =
      &profiles::testing::CreateProfileSync(profile_manager, new_profile_path);
  SetFRECompletion(new_profile, prefs::FreStatus::kCompleted);

  BrowserWindowInterface* new_browser =
      PlatformBrowserTest::CreateBrowser(new_profile);

  GlicKeyedService* service = GlicKeyedService::Get(new_profile);
  ASSERT_TRUE(service);
  auto& warming_pool =
      static_cast<GlicInstanceCoordinatorImpl&>(service->instance_coordinator())
          .GetWebContentsWarmingPoolForTesting();
  ASSERT_TRUE(warming_pool.MaybeStartInitialWarming());
  ASSERT_TRUE(RunUntil(
      [&warming_pool]() -> bool {
        return warming_pool.HasWarmedContainerForTesting();
      },
      "Wait for initial cold warming"));
  auto* warmed_container = warming_pool.GetWarmedContainerForTesting();
  ASSERT_TRUE(warmed_container);
  content::WaitForLoadStop(warmed_container->web_contents());

  ProfileDestructionWaiter profile_destruction_waiter(new_profile);
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      new_profile_path, base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  CloseBrowserSynchronously(new_browser);
  profile_destruction_waiter.Wait();

  EXPECT_FALSE(profile_manager->IsValidProfile(new_profile));
}
#endif

class GlicWarmingCellularBrowserTest : public GlicWarmingPoolBrowserTest {
 public:
  void SetUp() override {
    GlicProfileManager::ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::CONNECTION_3G);
    GlicWarmingPoolBrowserTest::SetUp();
  }

  void TearDown() override {
    GlicWarmingPoolBrowserTest::TearDown();
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
  }
};

IN_PROC_BROWSER_TEST_F(GlicWarmingCellularBrowserTest, NoWarming) {
  // Verify it does not warm on cellular startup.
  WaitForWarmingDelay();

  EXPECT_FALSE(pool().HasWarmedContainerForTesting());
}

class GlicWarmingBlockedByAdminBrowserTest : public GlicWarmingPoolBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    GlicWarmingPoolBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PushProfilePolicyConnectorProviderForTesting(&policy_provider_);

    policy::PolicyMap policies;
    policies.Set(
        policy::key::kGeminiSettings, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
        base::Value(std::to_underlying(
            optimization_guide::prefs::GeminiSettingsPolicyState::kDisabled)),
        nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(GlicWarmingBlockedByAdminBrowserTest, NoWarming) {
  // Verify it does not warm when disabled by admin policy from startup.
  WaitForWarmingDelay();

  EXPECT_FALSE(pool().HasWarmedContainerForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest, PRE_NoWarmingIfUnpinned) {
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicPinnedToTabstrip, false);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest, NoWarmingIfUnpinned) {
  WaitForWarmingDelay();

  EXPECT_FALSE(pool().HasWarmedContainerForTesting());
}

// TODO(b/496609005): Skip on ChromeOS due to profile ineligibility timeouts.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ClearedOnMemoryPressure DISABLED_ClearedOnMemoryPressure
#else
#define MAYBE_ClearedOnMemoryPressure ClearedOnMemoryPressure
#endif
IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest,
                       MAYBE_ClearedOnMemoryPressure) {
  // Wait for initial preload to complete.
  EXPECT_TRUE(
      RunUntil([this]() { return pool().HasWarmedContainerForTesting(); },
               "Wait for initial preload"));

  // Simulate critical memory pressure.
  base::MemoryPressureListener::SimulatePressureNotification(
      base::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Verify it is cleared immediately by the coordinator.
  EXPECT_FALSE(pool().HasWarmedContainerForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicWarmingPoolBrowserTest, IncognitoCheck) {
  Profile* incognito =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  base::test::TestFuture<GlicPrewarmingChecksResult> future;
  GlicProfileManager::GetInstance()->ShouldPreloadForProfile(
      incognito, future.GetCallback());
  EXPECT_EQ(future.Get(), GlicPrewarmingChecksResult::kProfileNotEligible);
}

class GlicWarmingDisabledBrowserTest
    : public GlicBrowserTestMixin<PlatformBrowserTest> {
 public:
  GlicWarmingDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kGlicWarming, features::kGlicWebContentsWarming});
  }

  GlicWebContentsWarmingPool& pool() {
    return coordinator().GetWebContentsWarmingPoolForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicWarmingDisabledBrowserTest, NoWarming) {
  // Verify it never warms.
  EXPECT_FALSE(pool().HasWarmedContainerForTesting());

  // Trigger preload attempt manually.
  GlicKeyedService::Get(GetProfile())->TryPreload();

  WaitForWarmingDelay();

  EXPECT_FALSE(pool().HasWarmedContainerForTesting());
}

}  // namespace glic
