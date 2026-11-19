// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_background_mode_manager.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/scoped_glic_provider_override.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/wm/window_pin_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_switches.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/command.h"
#include "ui/views/widget/widget.h"
#endif

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

// Latches every profile to Gemini Enterprise, which doesn't support the OS
// launcher or the global hotkey.
class GlicBackgroundModeManagerGeminiEnterpriseBrowserTest
    : public GlicBackgroundModeManagerBrowserTest {
 private:
  ScopedGlicProviderOverride provider_override_{
      GlicProvider::kGeminiEnterprise};
};

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerGeminiEnterpriseBrowserTest,
                       DoesNotEnterBackgroundMode) {
  auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  auto* profile_manager = g_browser_process->profile_manager();
  Profile* profile = GetProfile();
  ASSERT_EQ(GlicProviderLatch::GetProvider(*profile),
            GlicProvider::kGeminiEnterprise);
  ASSERT_TRUE(GlicEnabling::IsEnabledAndConsentForProfile(profile));

  // Neither the launcher pref nor the global hotkey scope enables the OS
  // launcher or the global hotkey.
  g_browser_process->local_state()->SetBoolean(
      prefs::kGlicHotkeyGlobalScopeEnabled, true);
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);

  EXPECT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      profile, ProfileKeepAliveOrigin::kGlicView));
}

#if BUILDFLAG(IS_CHROMEOS)
class GlicBackgroundModeManagerLockedFullscreenBrowserTest
    : public GlicBackgroundModeManagerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBackgroundModeManagerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kGlicDev);
  }

  void TearDownOnMainThread() override {
    g_browser_process->local_state()->SetBoolean(
        prefs::kGlicHotkeyGlobalScopeEnabled, false);
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    GlicBackgroundModeManagerBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerLockedFullscreenBrowserTest,
                       HandleHotkeyIgnoredWhenLockedFullscreen) {
  base::HistogramTester histogram_tester;

  g_browser_process->local_state()->SetBoolean(
      prefs::kGlicHotkeyGlobalScopeEnabled, true);
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);

  aura::Window* browser_window = GetBrowser()->GetWindow()->GetNativeWindow();
  views::Widget* browser_widget =
      views::Widget::GetWidgetForNativeWindow(browser_window);
  ASSERT_TRUE(browser_widget);

  ui::Accelerator default_hotkey = LocalHotkeyManager::GetDefaultAccelerator(
      LocalHotkeyManager::Command::kPanelToggle);

  // Enter trusted-pinned locked fullscreen (what lockWindowFullscreenPrivate /
  // Boca-OnTask do).
  ash::PinWindow(browser_window, /*trusted=*/true);
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(GetBrowser()));
  ASSERT_FALSE(coordinator().IsAnyPanelShowing());

  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  ASSERT_TRUE(manager);

  // Trigger global hotkey while locked fullscreen.
  manager->HandleHotkey(default_hotkey);
  WaitForDuration(base::Milliseconds(300));

  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
  EXPECT_TRUE(platform_util::IsBrowserLockedFullscreen(GetBrowser()));
  EXPECT_TRUE(browser_widget->IsActive());
  histogram_tester.ExpectTotalCount("Glic.Usage.Hotkey", 0);

  // Unpin the window for clean teardown.
  ash::UnpinWindow(browser_window);
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(GetBrowser()));
}
#endif  // BUILDFLAG(IS_CHROMEOS)
}  // namespace glic
