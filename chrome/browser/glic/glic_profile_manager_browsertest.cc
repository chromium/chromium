// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <type_traits>

#include "base/memory/memory_pressure_monitor.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
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
  MockGlicKeyedService(content::BrowserContext* browser_context,
                       signin::IdentityManager* identity_manager,
                       ProfileManager* profile_manager,
                       GlicProfileManager* glic_profile_manager)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager,
                         glic_profile_manager) {}
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
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
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
        GlicProfileManager::GetInstance());
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
  GlicProfileManagerPreloadingTest() {
    if (IsPreloadingEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                                features::kGlicWarming},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kGlic,
                                features::kTabstripComboButton},
          /*disabled_features=*/{features::kGlicWarming});
    }
    GlicProfileManager::ForceMemoryPressureForTesting(&memory_pressure_);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    GlicProfileManager::ForceProfileForLaunchForTesting(browser()->profile());
    ForceSigninAndModelExecutionCapability(browser()->profile());
  }

  void TearDown() override {
    GlicProfileManager::ForceProfileForLaunchForTesting(nullptr);
    GlicProfileManager::ForceMemoryPressureForTesting(nullptr);
    InProcessBrowserTest::TearDown();
  }

  bool IsPreloadingEnabled() const { return GetParam(); }

  void ResetMemoryPressure() {
    memory_pressure_ = base::MemoryPressureMonitor::MemoryPressureLevel::
        MEMORY_PRESSURE_LEVEL_NONE;
  }

 private:
  // We initialize memory pressure to moderate to prevent any premature
  // preloading.
  base::MemoryPressureMonitor::MemoryPressureLevel memory_pressure_ =
      base::MemoryPressureMonitor::MemoryPressureLevel::
          MEMORY_PRESSURE_LEVEL_MODERATE;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Success) {
  ResetMemoryPressure();
  const bool should_preload = IsPreloadingEnabled();
  auto* profile_manager = GlicProfileManager::GetInstance();
  EXPECT_EQ(should_preload,
            profile_manager->ShouldPreloadForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_NotSupportedProfile) {
  ResetMemoryPressure();
  GlicProfileManager::ForceProfileForLaunchForTesting(nullptr);
  SetModelExecutionCapability(browser()->profile(), false);
  auto* profile_manager = GlicProfileManager::GetInstance();
  EXPECT_FALSE(profile_manager->ShouldPreloadForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  ResetMemoryPressure();
  browser()->profile()->NotifyWillBeDestroyed();
  auto* profile_manager = GlicProfileManager::GetInstance();
  EXPECT_FALSE(profile_manager->ShouldPreloadForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_MemoryPressure) {
  // Note: we keep memory pressure at moderate here.
  auto* profile_manager = GlicProfileManager::GetInstance();
  EXPECT_FALSE(profile_manager->ShouldPreloadForProfile(browser()->profile()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerPreloadingTest,
                         ::testing::Bool());

}  // namespace
}  // namespace glic
