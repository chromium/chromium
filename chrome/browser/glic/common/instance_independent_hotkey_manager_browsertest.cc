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

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/wm/window_pin_util.h"
#include "ui/aura/window.h"
#endif

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

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       CanHandleAcceleratorsReturnsFalseWhenLockedFullscreen) {
  auto* hotkey_manager = coordinator().GetHotkeyManagerForTesting();
  EXPECT_TRUE(hotkey_manager->CanHandleAccelerators());

  // Pin the browser window in trusted (locked) fullscreen mode.
  ash::PinWindow(GetBrowser()->GetWindow()->GetNativeWindow(),
                 /*trusted=*/true);
  EXPECT_FALSE(hotkey_manager->CanHandleAccelerators());

  // Unpin the window and verify accelerators can be handled again.
  ash::UnpinWindow(GetBrowser()->GetWindow()->GetNativeWindow());
  EXPECT_TRUE(hotkey_manager->CanHandleAccelerators());
}
#endif

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

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       CaptureRegionHotkeyOpensGlicWhenPromptBoxDisabled) {
  CreateAndActivateTab(GetSimpleTestUrl());
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());

  TriggerHotkey(LocalHotkeyManager::Command::kCaptureRegion);

  ASSERT_OK(WaitForGlicOpen());
}

class InstanceIndependentHotkeyManagerPromptBoxBrowserTest
    : public InstanceIndependentHotkeyManagerBrowserTest {
 public:
  InstanceIndependentHotkeyManagerPromptBoxBrowserTest() {
    prompt_box_feature_list_.InitWithFeatures(
        {::features::kGlicCaptureRegion,
         features::kGlicSelectionOverlayPromptBox},
        {});
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    g_browser_process->local_state()->SetBoolean(
        prefs::kGlicHotkeyGlobalScopeEnabled, false);
  }

 private:
  base::test::ScopedFeatureList prompt_box_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerPromptBoxBrowserTest,
                       CaptureRegionHotkeyOpensOverlayWithoutPanelWhenClosed) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  auto* controller =
      SelectionOverlayController::FromTabWebContents(tab->GetContents());
  ASSERT_TRUE(controller);
  EXPECT_FALSE(GlicSidePanelCoordinator::IsShowing(tab));
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());

  base::HistogramTester histogram_tester;
  TriggerHotkey(LocalHotkeyManager::Command::kCaptureRegion);

  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 0u);
  EXPECT_FALSE(GlicSidePanelCoordinator::IsShowing(tab));
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
  histogram_tester.ExpectBucketCount(
      "Glic.Invoke.InvocationSource",
      mojom::InvocationSource::kCaptureRegionHotkey, 0);

  controller->Close();
}

IN_PROC_BROWSER_TEST_F(
    InstanceIndependentHotkeyManagerPromptBoxBrowserTest,
    CaptureRegionHotkeyPreselectsTextSelectionWhenPanelClosed) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  content::WebContents* web_contents = tab->GetContents();
  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  ASSERT_TRUE(controller);
  auto* selection_observer = GlicSelectionObserver::From(tab);
  ASSERT_TRUE(selection_observer);

  web_contents->SelectAll();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return selection_observer->GetCurrentSelectionBounds().has_value();
  }));
  EXPECT_FALSE(GlicSidePanelCoordinator::IsShowing(tab));

  base::HistogramTester histogram_tester;
  TriggerHotkey(LocalHotkeyManager::Command::kCaptureRegion);

  ASSERT_OK(RunUntilEqual(
      [&]() { return controller->state(); },
      SelectionOverlayController::State::kOverlay,
      "Timeout waiting for SelectionOverlayController state to be kOverlay"));
  EXPECT_EQ(controller->GetSelectedRegionCount(), 1u);
  EXPECT_FALSE(GlicSidePanelCoordinator::IsShowing(tab));
  EXPECT_FALSE(coordinator().IsAnyPanelShowing());
  histogram_tester.ExpectBucketCount(
      "Glic.Invoke.InvocationSource",
      mojom::InvocationSource::kCaptureRegionHotkey, 0);

  controller->Close();
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerPromptBoxBrowserTest,
                       CaptureRegionHotkeyInvokesGlicWhenPanelOpen) {
  tabs::TabInterface* tab = CreateAndActivateTab(GetSimpleTestUrl());
  ASSERT_TRUE(OpenGlicForActiveTab().has_value());
  ASSERT_TRUE(GlicSidePanelCoordinator::IsShowing(tab));

  base::HistogramTester histogram_tester;
  TriggerHotkey(LocalHotkeyManager::Command::kCaptureRegion);

  histogram_tester.ExpectUniqueSample(
      "Glic.Invoke.InvocationSource",
      mojom::InvocationSource::kCaptureRegionHotkey, 1);
  EXPECT_TRUE(GlicSidePanelCoordinator::IsShowing(tab));
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

// On ChromeOS, the global hotkey is registered with ash's accelerator
// controller, which FocusManager::ProcessAccelerator() falls through to, so
// triggering the hotkey from the browser opens the panel via the global path.
#if !BUILDFLAG(IS_CHROMEOS)
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
#endif

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
