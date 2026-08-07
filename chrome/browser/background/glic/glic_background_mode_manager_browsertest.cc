// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_background_mode_manager.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {

class GlicBackgroundModeManagerBrowserTest : public GlicBrowserTest {
 public:
  void TearDownOnMainThread() override {
    GlicProfileManager::ForceProfileForLaunchForTesting(std::nullopt);
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    GlicBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       KeepAliveStateFollowsLauncherPref) {
  auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  auto* profile_manager = g_browser_process->profile_manager();
  Profile* profile = GetProfile();

  // Ensure global hotkey scope is disabled for this test so state is tied
  // directly to the launcher pref.
  g_browser_process->local_state()->SetBoolean(
      prefs::kGlicHotkeyGlobalScopeEnabled, false);
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);

  ASSERT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      profile, ProfileKeepAliveOrigin::kGlicView));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      profile, ProfileKeepAliveOrigin::kGlicView));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  EXPECT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !profile_manager->HasKeepAliveForTesting(
        profile, ProfileKeepAliveOrigin::kGlicView);
  }));
}

// ChromeOS does not support creating multiple profiles without user session
// setup.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       KeepAliveTransfersWhenLaunchProfileChanges) {
  auto* profile_manager = g_browser_process->profile_manager();
  Profile* profile0 = GetProfile();

  // Create a second profile.
  base::FilePath path1 = profile_manager->GenerateNextProfileDirectoryPath();
  Profile& profile1 =
      profiles::testing::CreateProfileSync(profile_manager, path1);

  g_browser_process->local_state()->SetBoolean(
      prefs::kGlicHotkeyGlobalScopeEnabled, false);

  // Force launch profile to profile0.
  GlicProfileManager::ForceProfileForLaunchForTesting(profile0);
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);

  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      profile0, ProfileKeepAliveOrigin::kGlicView));
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      &profile1, ProfileKeepAliveOrigin::kGlicView));

  // Change launch profile to profile1 and re-enter background mode.
  GlicProfileManager::ForceProfileForLaunchForTesting(&profile1);
  auto* background_manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  ASSERT_TRUE(background_manager);
  background_manager->EnterBackgroundMode(/*show_status_icon=*/false);

  // Keepalive should have transferred to profile1.
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      &profile1, ProfileKeepAliveOrigin::kGlicView));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return !profile_manager->HasKeepAliveForTesting(
        profile0, ProfileKeepAliveOrigin::kGlicView);
  }));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace glic
