// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_launcher_configuration.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/command.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {
class MockObserver : public GlicLauncherConfiguration::Observer {
 public:
  // void OnEnabledChanged(bool enabled) override {}
  MOCK_METHOD1(OnEnabledChanged, void(bool));
  MOCK_METHOD0(OnGlobalHotkeyChanged, void());
};
}  // namespace

class GlicLauncherConfigurationTest : public testing::Test {
 public:
  GlicLauncherConfigurationTest() = default;
  ~GlicLauncherConfigurationTest() override = default;

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  void SetUp() override {
    local_state()->ClearPref(prefs::kGlicLauncherEnabled);
    local_state()->ClearPref(prefs::kGlicLauncherHotkey);
    local_state()->ClearPref(prefs::kGlicHotkeyGlobalScopeEnabled);
    local_state()->ClearPref(prefs::kGlicHotkeyGlobalScopeMigratedV2);
  }

  void TearDown() override {
    local_state()->ClearPref(prefs::kGlicLauncherEnabled);
    local_state()->ClearPref(prefs::kGlicLauncherHotkey);
    local_state()->ClearPref(prefs::kGlicHotkeyGlobalScopeEnabled);
    local_state()->ClearPref(prefs::kGlicHotkeyGlobalScopeMigratedV2);
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(GlicLauncherConfigurationTest, IsLauncherIconEnabled) {
  EXPECT_FALSE(GlicLauncherConfiguration::IsLauncherIconEnabled());

  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  EXPECT_TRUE(GlicLauncherConfiguration::IsLauncherIconEnabled());
}

TEST_F(GlicLauncherConfigurationTest, GetToggleHotkey_Default) {
  const ui::Accelerator accelerator =
      GlicLauncherConfiguration::GetToggleHotkey();
  EXPECT_EQ(accelerator.key_code(), ui::VKEY_G);
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(accelerator.IsCtrlDown());
  EXPECT_FALSE(accelerator.IsAltDown());
  EXPECT_FALSE(accelerator.IsCmdDown());
#elif BUILDFLAG(IS_CHROMEOS)
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_FALSE(accelerator.IsAltDown());
  EXPECT_TRUE(accelerator.IsCmdDown());
#else
  EXPECT_FALSE(accelerator.IsCtrlDown());
  EXPECT_TRUE(accelerator.IsAltDown());
  EXPECT_FALSE(accelerator.IsCmdDown());
#endif
}

TEST_F(GlicLauncherConfigurationTest, GetToggleHotkey_Invalid) {
  const ui::Accelerator invalid_hotkey(ui::VKEY_G, ui::EF_NONE);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(invalid_hotkey));
  EXPECT_EQ(GlicLauncherConfiguration::GetToggleHotkey(), ui::Accelerator());
}

TEST_F(GlicLauncherConfigurationTest, Observer) {
  MockObserver observer;
  GlicLauncherConfiguration glic_launcher_configuration{&observer};
  EXPECT_CALL(observer, OnEnabledChanged(true)).Times(1);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  EXPECT_CALL(observer, OnGlobalHotkeyChanged()).Times(1);
  const ui::Accelerator hotkey(ui::VKEY_K, ui::EF_ALT_DOWN);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(hotkey));
}

TEST_F(GlicLauncherConfigurationTest, HotkeyScope_Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kGlicHotkeyLocalScope);

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));

  const ui::Accelerator hotkey(ui::VKEY_K, ui::EF_ALT_DOWN);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(hotkey));
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
}

TEST_F(GlicLauncherConfigurationTest, HotkeyScope_Enabled_Empty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  local_state()->SetString(prefs::kGlicLauncherHotkey, "");

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
}

TEST_F(GlicLauncherConfigurationTest,
       HotkeyScope_Migration_ExistingUser_DefaultHotkey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  // Existing user has launcher enabled and default hotkey.
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  // Should migrate to true (Global scope).
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeMigratedV2));
  EXPECT_EQ(GlicLauncherConfiguration::GetToggleHotkey(),
            LocalHotkeyManager::GetDefaultAccelerator(
                LocalHotkeyManager::Command::kPanelToggle));
}

TEST_F(GlicLauncherConfigurationTest,
       HotkeyScope_Migration_ExistingUser_CustomHotkey) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  // Existing user has launcher enabled and custom hotkey.
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);
  const ui::Accelerator hotkey(ui::VKEY_K, ui::EF_ALT_DOWN);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(hotkey));

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  // Should migrate to true.
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeMigratedV2));
  EXPECT_EQ(GlicLauncherConfiguration::GetToggleHotkey(), hotkey);
}

TEST_F(GlicLauncherConfigurationTest,
       HotkeyScope_Migration_ExistingUser_LauncherDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  // Existing user has launcher explicitly disabled.
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, false);

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  // Hotkey should be cleared to respect disabled state, and scope should be
  // false.
  EXPECT_TRUE(GlicLauncherConfiguration::GetToggleHotkey().IsEmpty());
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeMigratedV2));
}

TEST_F(GlicLauncherConfigurationTest, HotkeyScope_Migration_NewUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  // New users have neither launcher nor hotkey pref explicitly set.
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kGlicLauncherEnabled));
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kGlicLauncherHotkey));

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  // Should migrate to false (Local scope).
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
  EXPECT_TRUE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeMigratedV2));
  // Default hotkey is still preserved for local scope.
  EXPECT_EQ(GlicLauncherConfiguration::GetToggleHotkey(),
            LocalHotkeyManager::GetDefaultAccelerator(
                LocalHotkeyManager::Command::kPanelToggle));
}

TEST_F(GlicLauncherConfigurationTest, HotkeyScope_AlreadyMigrated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kGlicHotkeyLocalScope);

  // Already migrated, enabled is false, custom hotkey.
  const ui::Accelerator hotkey(ui::VKEY_K, ui::EF_ALT_DOWN);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(hotkey));
  local_state()->SetBoolean(prefs::kGlicHotkeyGlobalScopeMigratedV2, true);
  local_state()->SetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled, false);

  MockObserver observer;
  GlicLauncherConfiguration config{&observer};
  // Should NOT change because already migrated.
  EXPECT_FALSE(local_state()->GetBoolean(prefs::kGlicHotkeyGlobalScopeEnabled));
}

}  // namespace glic
