// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_service.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WallpaperSearchServiceBrowserTest : public InProcessBrowserTest {
 public:
  WallpaperSearchServiceBrowserTest() = default;
  ~WallpaperSearchServiceBrowserTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {optimization_guide::features::internal::
             kWallpaperSearchSettingsVisibility,
         ntp_features::kCustomizeChromeWallpaperSearch,
         optimization_guide::features::kOptimizationGuideModelExecution},
        {});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// PRE_ simulates a browser restart.
IN_PROC_BROWSER_TEST_F(WallpaperSearchServiceBrowserTest,
                       PRE_EnablingWallpaperSearchEnablesGM3) {
  optimization_guide::EnableSigninAndModelExecutionCapability(
      browser()->profile());

  // Enable Wallpaper Search via Optimization Guide Prefs.
  // GM3 should enable itself when the browser restarts.
  browser()->profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::proto::ModelExecutionFeature::
              MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));
}

IN_PROC_BROWSER_TEST_F(WallpaperSearchServiceBrowserTest,
                       EnablingWallpaperSearchEnablesGM3) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kChromeRefresh2023));
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kChromeWebuiRefresh2023));
  EXPECT_TRUE(features::IsChromeWebuiRefresh2023());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
class WallpaperSearchServiceBrowserChromeAshTest
    : public WallpaperSearchServiceBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  bool IsDeviceOwner() const { return std::get<0>(GetParam()); }
  // Chrome OS builds sometimes run on non-Chrome OS environments.
  bool IsRunningOnChromeOS() const { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         WallpaperSearchServiceBrowserChromeAshTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

IN_PROC_BROWSER_TEST_P(WallpaperSearchServiceBrowserChromeAshTest,
                       PRE_EnablingWallpaperSearchEnablesGM3) {
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(browser()->profile()),
      "test@example.com", signin::ConsentLevel::kSync);
  WallpaperSearchServiceFactory::GetForProfile(browser()->profile())
      ->SkipChromeOSDeviceCheckForTesting(IsRunningOnChromeOS());

  // Enable Wallpaper Search via Optimization Guide Prefs.
  // GM3 should enable itself when the browser restarts.
  browser()->profile()->GetPrefs()->SetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::proto::ModelExecutionFeature::
              MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH),
      static_cast<int>(optimization_guide::prefs::FeatureOptInState::kEnabled));

  // Declare if the user is the device owner.
  ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
      browser()->profile()->GetOriginalProfile())
      ->RunPendingIsOwnerCallbacksForTesting(IsDeviceOwner());
}

IN_PROC_BROWSER_TEST_P(WallpaperSearchServiceBrowserChromeAshTest,
                       EnablingWallpaperSearchEnablesGM3) {
#if !BUILDFLAG(IS_CHROMEOS_DEVICE)
  // This test edits flags in chrome://flags, which doesn't work for
  // Chrome-OS-on-Linux. It might cause parts of this test to flake.
  // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md#flags
  if (!IsRunningOnChromeOS()) {
    EXPECT_TRUE(base::FeatureList::IsEnabled(features::kChromeRefresh2023));
  }
#else
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kChromeRefresh2023));
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kChromeWebuiRefresh2023));
  EXPECT_TRUE(features::IsChromeWebuiRefresh2023());
#endif  // !BUILDFLAG(IS_CHROMEOS_DEVICE)
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
