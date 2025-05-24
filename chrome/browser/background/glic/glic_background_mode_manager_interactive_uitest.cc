// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/background/glic/glic_background_mode_manager.h"
#include "chrome/browser/background/glic/glic_launcher_configuration.h"
#include "chrome/browser/background/startup_launch_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {
class TestStartupLaunchManager : public StartupLaunchManager {
 public:
  TestStartupLaunchManager() = default;
  MOCK_METHOD1(UpdateLaunchOnStartup, void(bool should_launch_on_startup));
};
}  // namespace

namespace glic {

class GlicBackgroundModeManagerUiTest : public test::InteractiveGlicTest {
 public:
  void TearDownOnMainThread() override {
    // Disable glic so that the glic_background_mode_manager won't prevent the
    // browser process from closing which causes the test to hang.
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
    test::InteractiveGlicTest::TearDownOnMainThread();
  }

  bool IsHotkeySupported() {
    auto* const global_shortcut_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    return global_shortcut_listener != nullptr &&
           !global_shortcut_listener->IsRegistrationHandledExternally();
  }

  void RegisterHotkey(ui::Accelerator updated_hotkey) {
    g_browser_process->local_state()->SetString(
        prefs::kGlicLauncherHotkey,
        ui::Command::AcceleratorToString(updated_hotkey));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that modifying the pref propagates to KeepAliveRegistry.
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest, KeepAlive) {
  auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  ASSERT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  EXPECT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
}

// Checks that the status icon exists when the pref is enabled.
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest, StatusIcon) {
  ASSERT_FALSE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  EXPECT_FALSE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest,
                       UpdateHotkeyWhileEnabled) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_EQ(ui::Accelerator(ui::VKEY_G,
#if BUILDFLAG(IS_MAC)
                            ui::EF_CONTROL_DOWN
#else
                            ui::EF_ALT_DOWN
#endif
                            ),
            manager->RegisteredHotkeyForTesting());

  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  RegisterHotkey(updated_hotkey);
  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest,
                       UpdateHotkeyWhileDisabled) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  PrefService* const pref_service = g_browser_process->local_state();
  ASSERT_FALSE(pref_service->GetBoolean(prefs::kGlicLauncherEnabled));
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_TRUE(manager->RegisteredHotkeyForTesting().IsEmpty());

  // If the hotkey pref were to somehow change even while glic was disabled,
  // the manager should not register the hotkey.
  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  RegisterHotkey(updated_hotkey);
  EXPECT_TRUE(manager->RegisteredHotkeyForTesting().IsEmpty());

  // Re-enabling glic should register the updated hotkey pref.
  pref_service->SetBoolean(prefs::kGlicLauncherEnabled, true);
  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest,
                       RegisterInvalidAccelerator) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();

  // Registering an invalid hotkey should fail.
  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_NONE);
  RegisterHotkey(updated_hotkey);
  EXPECT_NE(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest,
                       SuspendShortcutAndRegisterAccelerator) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();

  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  global_accelerator_listener->SetShortcutHandlingSuspended(true);
  ui::Accelerator updated_hotkey(ui::VKEY_A,
                                 ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  RegisterHotkey(updated_hotkey);

  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
  EXPECT_TRUE(global_accelerator_listener->IsShortcutHandlingSuspended());
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest, LaunchOnStartup) {
  auto launch_manager = std::make_unique<TestStartupLaunchManager>();
  StartupLaunchManager::SetInstanceForTesting(launch_manager.get());

  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(true))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  testing::Mock::VerifyAndClearExpectations(launch_manager.get());
  EXPECT_CALL(*launch_manager, UpdateLaunchOnStartup(false))
      .Times(testing::Exactly(1));
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  testing::Mock::VerifyAndClearExpectations(launch_manager.get());
}
#endif

// Test that hotkey is logged when pressed.
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest, HotkeyPressed) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  base::HistogramTester histogram_tester;
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();

  ui::Accelerator default_hotkey =
      GlicLauncherConfiguration::GetDefaultHotkey();
  EXPECT_EQ(default_hotkey, manager->RegisteredHotkeyForTesting());
  manager->OnKeyPressed(default_hotkey);
  histogram_tester.ExpectBucketCount("Glic.Usage.Hotkey", HotkeyUsage::kDefault,
                                     1);
  histogram_tester.ExpectBucketCount("Glic.Usage.Hotkey", HotkeyUsage::kCustom,
                                     0);

  ui::Accelerator custom_hotkey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  RegisterHotkey(custom_hotkey);
  EXPECT_EQ(custom_hotkey, manager->RegisteredHotkeyForTesting());
  manager->OnKeyPressed(custom_hotkey);
  histogram_tester.ExpectBucketCount("Glic.Usage.Hotkey", HotkeyUsage::kDefault,
                                     1);
  histogram_tester.ExpectBucketCount("Glic.Usage.Hotkey", HotkeyUsage::kCustom,
                                     1);
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerUiTest, DeleteEligibleProfile) {
  GlicBackgroundModeManager* const background_mode_manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());

  // Create a second browser with a different profile that didn't complete Glic
  // fre yet.
  ProfileManager* const profile_manager = g_browser_process->profile_manager();
  Profile& second_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  Browser* const second_browser = CreateBrowser(&second_profile);
  SigninWithPrimaryAccount(&second_profile);
  SetModelExecutionCapability(&second_profile, true);

  // Delete the first profile and the glic launcher should not be in the
  // background since there are no profiles that are eligible to use glic.
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      browser()->profile()->GetPath(), base::DoNothing(),
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_FALSE(background_mode_manager->IsInBackgroundModeForTesting());
  EXPECT_TRUE(g_browser_process->local_state()->GetBoolean(
      prefs::kGlicLauncherEnabled));

  // The GlicBackgroundModeManager should go into background mode after
  // completing the fre in the second profile since the glic launcher local pref
  // has already been set to enabled.
  GlicKeyedService* const second_keyed_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(second_browser->profile());
  EXPECT_FALSE(second_keyed_service->enabling().HasConsented());
  second_keyed_service->window_controller().fre_controller()->AcceptFre();
  EXPECT_TRUE(second_keyed_service->enabling().HasConsented());
  EXPECT_TRUE(background_mode_manager->IsInBackgroundModeForTesting());
}
}  // namespace glic
