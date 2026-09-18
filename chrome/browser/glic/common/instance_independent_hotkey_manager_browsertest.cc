// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

// TODO(crbug.com/537331304): Add test coverage for the Android bypass of the
// kGlicHotkeyGlobalScopeEnabled preference.

namespace {

class InstanceIndependentHotkeyManagerBrowserTest : public GlicBrowserTest {
 public:
  InstanceIndependentHotkeyManagerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGlicHotkeyLocalScope);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kGlicDev);
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
    g_browser_process->local_state()->SetBoolean(
        prefs::kGlicHotkeyGlobalScopeEnabled, false);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       CanHandleAcceleratorsReturnsFalseWhenFreNotCompleted) {
  // Override the FRE status to not completed.
  GetProfile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kNotStarted));

  // Should return false because FRE is not completed.
  EXPECT_FALSE(
      coordinator().GetHotkeyManagerForTesting()->CanHandleAccelerators());
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       AcceleratorPressedLaunchesGlicInLocalScope) {
  EXPECT_TRUE(
      coordinator().GetHotkeyManagerForTesting()->CanHandleAccelerators());

  // Simulate the accelerator being pressed in default local scope
  // (launcher=true, global=false).
  TriggerHotkey(LocalHotkeyManager::Command::kPanelToggle);

  // Verify that the panel actually opens.
  ASSERT_OK(WaitForGlicOpen());
}

IN_PROC_BROWSER_TEST_F(
    InstanceIndependentHotkeyManagerBrowserTest,
    AcceleratorPressedLaunchesGlicWhenLauncherDisabledAndLocalScopeEnabled) {
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);

  EXPECT_TRUE(
      coordinator().GetHotkeyManagerForTesting()->CanHandleAccelerators());

  // Even if the launcher is disabled, local scoped hotkeys should still launch
  // Glic.
  TriggerHotkey(LocalHotkeyManager::Command::kPanelToggle);

  // Verify that the panel actually opens.
  ASSERT_OK(WaitForGlicOpen());
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       AcceleratorPressedReturnsFalseWhenGlobalScopeEnabled) {
  g_browser_process->local_state()->SetBoolean(
      prefs::kGlicHotkeyGlobalScopeEnabled, true);

  // Since global scope is enabled, the local manager should return false
  // (pass-through).
  EXPECT_FALSE(coordinator().GetHotkeyManagerForTesting()->AcceleratorPressed(
      LocalHotkeyManager::Command::kPanelToggle));

  // Verify that the panel is not showing.
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
}
#endif

class InstanceIndependentHotkeyManagerFeatureDisabledBrowserTest
    : public GlicBrowserTest {
 public:
  InstanceIndependentHotkeyManagerFeatureDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kGlicHotkeyLocalScope);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kGlicDev);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    InstanceIndependentHotkeyManagerFeatureDisabledBrowserTest,
    AcceleratorPressedDoesNotLaunchGlicWhenLauncherEnabled) {
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);

  // If the feature is disabled, the local manager should return false (pass
  // through to the global manager) even if the launcher is enabled.
  EXPECT_FALSE(coordinator().GetHotkeyManagerForTesting()->AcceleratorPressed(
      LocalHotkeyManager::Command::kPanelToggle));

  // Hotkey is global and should not be handled locally.
  TriggerHotkey(LocalHotkeyManager::Command::kPanelToggle);

  // Verify that the panel is not showing.
  WaitForDuration(base::Milliseconds(300));
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
}

IN_PROC_BROWSER_TEST_F(
    InstanceIndependentHotkeyManagerFeatureDisabledBrowserTest,
    AcceleratorPressedDoesNotLaunchGlicWhenLauncherDisabled) {
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);

  // When launcher is disabled and feature is disabled, hotkeys are disabled.
  EXPECT_FALSE(
      coordinator().GetHotkeyManagerForTesting()->CanHandleAccelerators());

  TriggerHotkey(LocalHotkeyManager::Command::kPanelToggle);

  // Verify that the panel is not showing.
  WaitForDuration(base::Milliseconds(300));
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
}

}  // namespace
}  // namespace glic
