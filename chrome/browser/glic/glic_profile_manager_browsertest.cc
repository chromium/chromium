// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_manager.h"

#include <memory>
#include <type_traits>

#include "base/memory/memory_pressure_monitor.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "glic_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class MockGlicKeyedService : public GlicKeyedService {
 public:
  MockGlicKeyedService(content::BrowserContext* browser_context,
                       signin::IdentityManager* identity_manager,
                       GlicProfileManager* profile_manager)
      : GlicKeyedService(Profile::FromBrowserContext(browser_context),
                         identity_manager,
                         profile_manager) {}
  MOCK_METHOD(void, ClosePanel, (), (override));
};

class GlicProfileManagerBrowserTest : public InProcessBrowserTest {
 public:
  GlicProfileManagerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{features::kDestroyProfileOnBrowserClose});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       SetActiveGlic_SameProfile) {
  GlicProfileManager profile_manager;
  signin::IdentityTestEnvironment identity_test_environment;
  TestingProfile profile;
  MockGlicKeyedService service(
      &profile, identity_test_environment.identity_manager(), &profile_manager);

  profile_manager.SetActiveGlic(&service);

  // Opening glic twice for the same profile shouldn't cause it to close.
  EXPECT_CALL(service, ClosePanel()).Times(0);
  profile_manager.SetActiveGlic(&service);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       SetActiveGlic_DifferentProfiles) {
  GlicProfileManager profile_manager;
  signin::IdentityTestEnvironment identity_test_environment;
  TestingProfile profile1;
  TestingProfile profile2;
  MockGlicKeyedService service1(&profile1,
                                identity_test_environment.identity_manager(),
                                &profile_manager);
  MockGlicKeyedService service2(&profile2,
                                identity_test_environment.identity_manager(),
                                &profile_manager);

  profile_manager.SetActiveGlic(&service1);

  // Opening glic from a second profile should make the profile manager close
  // the first one.
  EXPECT_CALL(service1, ClosePanel());
  profile_manager.SetActiveGlic(&service2);
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_WithActiveGlic) {
  GlicProfileManager profile_manager;
  signin::IdentityTestEnvironment identity_test_environment;
  TestingProfile profile1;
  TestingProfile profile2;
  MockGlicKeyedService service1(&profile1,
                                identity_test_environment.identity_manager(),
                                &profile_manager);
  MockGlicKeyedService service2(&profile2,
                                identity_test_environment.identity_manager(),
                                &profile_manager);

  profile_manager.SetActiveGlic(&service1);
  EXPECT_EQ(&profile1, profile_manager.GetProfileForLaunch());

  profile_manager.SetActiveGlic(&service2);
  EXPECT_EQ(&profile2, profile_manager.GetProfileForLaunch());
}

IN_PROC_BROWSER_TEST_F(GlicProfileManagerBrowserTest,
                       ProfileForLaunch_BasedOnActivationOrder) {
  GlicProfileManager profile_manager;
  signin::IdentityTestEnvironment identity_test_environment;
  TestingProfile profile1, profile2, profile3;

  ForceSigninAndModelExecutionCapability(&profile1);
  ForceSigninAndModelExecutionCapability(&profile2);

  Browser::CreateParams browser_params1(&profile1, false);
  auto browser1 = CreateBrowserWithTestWindowForParams(browser_params1);

  Browser::CreateParams browser_params2(&profile2, false);
  auto browser2 = CreateBrowserWithTestWindowForParams(browser_params2);

  Browser::CreateParams browser_params3(&profile3, false);
  auto browser3 = CreateBrowserWithTestWindowForParams(browser_params3);

  // profile1 is the most recently used profile
  BrowserList::SetLastActive(browser1.get());
  EXPECT_EQ(&profile1, profile_manager.GetProfileForLaunch());

  // profile2 is the most recently used profile
  BrowserList::SetLastActive(browser2.get());
  EXPECT_EQ(&profile2, profile_manager.GetProfileForLaunch());

  // profile1 is the most recently used profile
  BrowserList::SetLastActive(browser1.get());
  EXPECT_EQ(&profile1, profile_manager.GetProfileForLaunch());

  // profile3 is the most recently used profile but it isn't
  // compliant, so still using profile1
  BrowserList::SetLastActive(browser3.get());
  EXPECT_EQ(&profile1, profile_manager.GetProfileForLaunch());
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
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_manager_ = std::make_unique<GlicProfileManager>();
    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    profile_ = std::make_unique<TestingProfile>();
    GlicProfileManager::ForceProfileForLaunchForTesting(profile());
    GlicProfileManager::ForceMemoryPressureForTesting(&memory_pressure_);
    ForceSigninAndModelExecutionCapability(profile());
  }

  void TearDown() override {
    GlicProfileManager::ForceProfileForLaunchForTesting(nullptr);
    GlicProfileManager::ForceMemoryPressureForTesting(nullptr);
    profile_.reset();
    identity_test_environment_.reset();
    profile_manager_.reset();
    InProcessBrowserTest::TearDown();
  }

  bool IsPreloadingEnabled() const { return GetParam(); }

  TestingProfile* profile() { return profile_.get(); }
  GlicProfileManager& profile_manager() { return *profile_manager_; }
  base::MemoryPressureMonitor::MemoryPressureLevel& memory_pressure() {
    return memory_pressure_;
  }

  void DestroyProfile() { profile_.reset(); }

 private:
  std::unique_ptr<GlicProfileManager> profile_manager_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::MemoryPressureMonitor::MemoryPressureLevel memory_pressure_ = base::
      MemoryPressureMonitor::MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_Success) {
  const bool should_preload = IsPreloadingEnabled();
  EXPECT_EQ(should_preload,
            profile_manager().ShouldPreloadForProfile(profile()));
  DestroyProfile();
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_NotLaunchProfile) {
  GlicProfileManager::ForceProfileForLaunchForTesting(nullptr);
  EXPECT_FALSE(profile_manager().ShouldPreloadForProfile(profile()));
  DestroyProfile();
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  profile()->NotifyWillBeDestroyed();
  EXPECT_FALSE(profile_manager().ShouldPreloadForProfile(profile()));
  DestroyProfile();
}

IN_PROC_BROWSER_TEST_P(GlicProfileManagerPreloadingTest,
                       ShouldPreloadForProfile_MemoryPressure) {
  memory_pressure() = base::MemoryPressureMonitor::MemoryPressureLevel::
      MEMORY_PRESSURE_LEVEL_MODERATE;
  EXPECT_FALSE(profile_manager().ShouldPreloadForProfile(profile()));
  DestroyProfile();
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicProfileManagerPreloadingTest,
                         ::testing::Bool());

}  // namespace
}  // namespace glic
