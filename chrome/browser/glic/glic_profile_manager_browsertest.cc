// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <string>
#include <type_traits>

#include "base/memory/memory_pressure_monitor.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
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
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ozone_buildflags.h"

namespace glic {
namespace {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  MockGlicKeyedService(
      content::BrowserContext* browser_context,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager,
                         glic_profile_manager,
                         contextual_cueing_service) {}
  MOCK_METHOD(void, ClosePanel, (), (override));

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
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                              features::kGlicRollout},
        /*disabled_features=*/{features::kDestroyProfileOnBrowserClose});

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &GlicProfileManagerBrowserTest::SetTestingFactory,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ForceSigninAndModelExecutionCapability(browser()->profile());
  }

  MockGlicKeyedService* GetMockGlicKeyedService(Profile* profile) {
    auto* service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
    return static_cast<MockGlicKeyedService*>(service);
  }

  Profile* CreateNewProfile() {
    auto* profile_manager = g_browser_process->profile_manager();
    auto new_path = profile_manager->GenerateNextProfileDirectoryPath();
    profiles::testing::CreateProfileSync(profile_manager, new_path);
    return profile_manager->GetProfile(new_path);
  }

 private:
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
    return std::make_unique<MockGlicKeyedService>(
        context, identitity_manager, g_browser_process->profile_manager(),
        GlicProfileManager::GetInstance(),
        /*contextual_cueing_service=*/nullptr);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       SetActiveGlic_SameProfile) {
  auto* service0 = GetMockGlicKeyedService(browser()->profile());
  GlicProfileManager::GetInstance()->SetActiveGlic(service0);
  // Opening glic twice for the same profile shouldn't cause it to close.
  EXPECT_CALL(*service0, ClosePanel()).Times(0);
  GlicProfileManager::GetInstance()->SetActiveGlic(service0);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       SetActiveGlic_DifferentProfiles) {
  auto* service0 = GetMockGlicKeyedService(browser()->profile());

  auto* profile1 = CreateNewProfile();
  ForceSigninAndModelExecutionCapability(profile1);
  auto* service1 = GetMockGlicKeyedService(profile1);

  auto* profile_manager = GlicProfileManager::GetInstance();
  profile_manager->SetActiveGlic(service0);

  // Tell the mock glic to pretend that the window is open (otherwise, we won't
  // attempt to close it).
  service0->SetWindowShowing();

  // Opening glic from a second profile should make the profile manager close
  // the first one.
  EXPECT_CALL(*service0, ClosePanel());
  profile_manager->SetActiveGlic(service1);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_WithDetachedGlic) {
  auto* service0 = GetMockGlicKeyedService(browser()->profile());

  // Setup Profile 1
  auto* profile1 = CreateNewProfile();
  ForceSigninAndModelExecutionCapability(profile1);

  auto* profile_manager = GlicProfileManager::GetInstance();
  // Profile 0 is the last used Glic and Profile 1 is the last used window.
  // Profile 1 should be selected for launch.
  profile_manager->SetActiveGlic(service0);
  CreateBrowser(profile1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // Simulate showing detached for Profile 0.
  // Profile 0 should now be selected for launch.
  service0->SetWindowDetached();
  EXPECT_EQ(browser()->profile(), profile_manager->GetProfileForLaunch());
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_BasedOnActivationOrder) {
  // Setup Profile 1
  auto* profile1 = CreateNewProfile();
  ForceSigninAndModelExecutionCapability(profile1);

  // Setup Profile 2 (not glic compliant)
  auto* profile2 = CreateNewProfile();

  auto* profile_manager = GlicProfileManager::GetInstance();
  // profile0 is the most recently used profile
  EXPECT_EQ(browser()->profile(), profile_manager->GetProfileForLaunch());

  // profile1 is the most recently used profile
  [[maybe_unused]] auto* browser1 = CreateBrowser(profile1);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

  // profile2 is the most recently used profile but it isn't
  // compliant, so still using profile1
  CreateBrowser(profile2);
  EXPECT_EQ(profile1, profile_manager->GetProfileForLaunch());

#if !(BUILDFLAG(IS_OZONE_WAYLAND))
  // profile0 is the most recently used profile
  browser()->window()->Activate();
  ui_test_utils::WaitForBrowserSetLastActive(browser());
  EXPECT_EQ(browser()->profile(), profile_manager->GetProfileForLaunch());
#endif
}

class GlicProfileManagerPreloadingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  explicit GlicProfileManagerPreloadingTest(const std::string& delay_ms) {
    if (IsPreloadingEnabled()) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kGlic, {}},
                                {features::kTabstripComboButton, {}},
                                {features::kGlicRollout, {}},
                                {features::kGlicWarming,
                                 {{features::kGlicWarmingDelayMs.name,
                                   delay_ms},
                                  {features::kGlicWarmingJitterMs.name, "0"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                                features::kGlicRollout},
          /*disabled_features=*/{features::kGlicWarming});
    }

    // We initialize memory pressure to moderate to prevent any premature
    // preloading.
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_MODERATE);
    GlicProfileManager::ForceConnectionTypeForTesting(
        network::mojom::ConnectionType::CONNECTION_WIFI);
  }

  GlicProfileManagerPreloadingTest() : GlicProfileManagerPreloadingTest("0") {}

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GlicProfileManager::ForceProfileForLaunchForTesting(browser()->profile());
    ForceSigninAndModelExecutionCapability(browser()->profile());
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  void TearDownOnMainThread() override { run_loop_.reset(); }

  void TearDown() override {
    GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
    GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
    GlicProfileManager::ForceConnectionTypeForTesting(std::nullopt);
    InProcessBrowserTest::TearDown();
  }

  bool IsPreloadingEnabled() const { return GetParam(); }

  void ResetMemoryPressure() {
    GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_NONE);
  }

  bool WaitForShouldPreload() {
    auto* profile_manager = GlicProfileManager::GetInstance();
    profile_manager->ShouldPreloadForProfile(
        browser()->profile(),
        base::BindOnce(&GlicProfileManagerPreloadingTest::OnShouldPreload,
                       base::Unretained(this)));
    run_loop_->Run();
    return should_preload_;
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    GlicProfileManager::ForceConnectionTypeForTesting(connection_type);
  }

 private:
  void OnShouldPreload(Profile* profile, bool should_preload) {
    should_preload_ = should_preload;
    run_loop_->Quit();
  }

  bool should_preload_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Success) {
  ResetMemoryPressure();
  const bool should_preload = IsPreloadingEnabled();
  EXPECT_EQ(should_preload, WaitForShouldPreload());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_NotSupportedProfile) {
  ResetMemoryPressure();
  GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
  SetModelExecutionCapability(browser()->profile(), false);
  EXPECT_FALSE(WaitForShouldPreload());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  ResetMemoryPressure();
  browser()->profile()->NotifyWillBeDestroyed();
  EXPECT_FALSE(WaitForShouldPreload());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_MemoryPressure) {
  // Note: we keep memory pressure at moderate here.
  EXPECT_FALSE(WaitForShouldPreload());
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Cellular) {
  ResetMemoryPressure();
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_2G);
  EXPECT_FALSE(WaitForShouldPreload());
}

// See *Deferred* below. Checks that we don't defer preloading when there's no
// delay.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_DoNotDefer) {
  ResetMemoryPressure();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  service->TryPreload();
  base::RunLoop run_loop;
  // Since we have no delay, running until idle should mean that we do warm
  // (provided warming is enabled).
  run_loop.RunUntilIdle();
  const bool should_preload = IsPreloadingEnabled();
  EXPECT_EQ(should_preload, service->window_controller().IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerPreloadingTest,
                         ::testing::Bool());

class GlicProfileManagerDeferredPreloadingTest
    : public GlicProfileManagerPreloadingTest {
 public:
  // This sets the delay to 10 seconds (60 * 10 * 1000).
  GlicProfileManagerDeferredPreloadingTest()
      : GlicProfileManagerPreloadingTest(/*delay_ms=*/"600000") {}
  ~GlicProfileManagerDeferredPreloadingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This is really a keyed service test, but it is convenient to locate it here
// for now. It just checks that if we have a preload delay, that we won't
// preload immediately.
IN_PROC_BROWSER_TEST_P(GlicProfileManagerDeferredPreloadingTest,
                       ShouldPreloadForProfile_Defer) {
  ResetMemoryPressure();
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  service->TryPreload();
  base::RunLoop run_loop;
  // Since we shouldn't preload until after the delay, we shouldn't be warmed
  // after running until idle.
  run_loop.RunUntilIdle();
  EXPECT_FALSE(service->window_controller().IsWarmed());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerDeferredPreloadingTest,
                         ::testing::Bool());

}  // namespace
}  // namespace glic
