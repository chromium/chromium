// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names_internal.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

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
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       AcceleratorPressedInvokesGlic) {
  InstanceIndependentHotkeyManager manager(&service()->instance_coordinator(),
                                           GetBrowser()->GetProfile());

  // Simulate the accelerator being pressed.
  EXPECT_TRUE(
      manager.AcceleratorPressed(LocalHotkeyManager::Command::kCaptureRegion));

  // Verify that the panel actually opens.
  EXPECT_TRUE(WaitForGlicOpen().has_value());
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       CanHandleAcceleratorsReturnsTrueWhenEnabled) {
  InstanceIndependentHotkeyManager manager(&service()->instance_coordinator(),
                                           GetBrowser()->GetProfile());
  EXPECT_TRUE(manager.CanHandleAccelerators());
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       CanHandleAcceleratorsReturnsFalseWhenFreNotCompleted) {
  // Override the FRE status to not completed.
  GetBrowser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kGlicCompletedFre,
      std::to_underlying(prefs::FreStatus::kNotStarted));

  InstanceIndependentHotkeyManager manager(&service()->instance_coordinator(),
                                           GetBrowser()->GetProfile());
  // Should return false because FRE is not completed.
  EXPECT_FALSE(manager.CanHandleAccelerators());
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       AcceleratorPressedLaunchesGlicInLocalScope) {
  GlicKeyedService* service = GlicKeyedServiceFactory::GetGlicKeyedService(
      GetBrowser()->GetProfile(), /*create=*/true);
  ASSERT_TRUE(service);

  InstanceIndependentHotkeyManager manager(&service->instance_coordinator(),
                                           GetBrowser()->GetProfile());

  // Simulate the accelerator being pressed.
  EXPECT_TRUE(
      manager.AcceleratorPressed(LocalHotkeyManager::Command::kPanelToggle));

  // Verify that the panel actually opens.
  EXPECT_TRUE(WaitForGlicOpen().has_value());
}

IN_PROC_BROWSER_TEST_F(InstanceIndependentHotkeyManagerBrowserTest,
                       AcceleratorPressedReturnsFalseWhenLauncherDisabled) {
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  InstanceIndependentHotkeyManager manager(&service()->instance_coordinator(),
                                           GetBrowser()->GetProfile());
  EXPECT_FALSE(
      manager.AcceleratorPressed(LocalHotkeyManager::Command::kPanelToggle));
}

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
    AcceleratorPressedDoesNotLaunchGlicWhenFeatureDisabled) {
  GlicKeyedService* service = GlicKeyedServiceFactory::GetGlicKeyedService(
      GetBrowser()->GetProfile(), /*create=*/true);
  ASSERT_TRUE(service);

  InstanceIndependentHotkeyManager manager(&service->instance_coordinator(),
                                           GetBrowser()->GetProfile());

  // Even though hotkey is pressed, it should behave as global (return false)
  // because the feature is disabled (default behavior).
  EXPECT_FALSE(
      manager.AcceleratorPressed(LocalHotkeyManager::Command::kPanelToggle));

  // Verify that the panel is not showing.
  EXPECT_FALSE(service->instance_coordinator().IsAnyPanelShowing());
}

}  // namespace
}  // namespace glic
