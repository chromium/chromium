// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background/glic/glic_launcher_configuration.h"

#include "base/test/task_environment.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/common/chrome_features.h"
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
  MOCK_METHOD1(OnGlobalHotkeyChanged, void(ui::Accelerator));
};
}  // namespace

class GlicLauncherConfigurationTest : public testing::Test {
 public:
  GlicLauncherConfigurationTest() = default;
  ~GlicLauncherConfigurationTest() override = default;

  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

TEST_F(GlicLauncherConfigurationTest, IsEnabled) {
  EXPECT_FALSE(GlicLauncherConfiguration::IsEnabled());

  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  EXPECT_TRUE(GlicLauncherConfiguration::IsEnabled());
}

TEST_F(GlicLauncherConfigurationTest, GetGlobalHotkey_Default) {
  const ui::Accelerator accelerator =
      GlicLauncherConfiguration::GetGlobalHotkey();
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

TEST_F(GlicLauncherConfigurationTest, GetGlobalHotkey_Invalid) {
  const ui::Accelerator invalid_hotkey(ui::VKEY_G, ui::EF_NONE);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(invalid_hotkey));
  EXPECT_EQ(GlicLauncherConfiguration::GetGlobalHotkey(), ui::Accelerator());
}

TEST_F(GlicLauncherConfigurationTest, Observer) {
  MockObserver observer;
  GlicLauncherConfiguration glic_launcher_configuration{&observer};
  EXPECT_CALL(observer, OnEnabledChanged(true)).Times(1);
  local_state()->SetBoolean(prefs::kGlicLauncherEnabled, true);

  EXPECT_CALL(observer, OnGlobalHotkeyChanged(testing::_)).Times(1);
  const ui::Accelerator hotkey(ui::VKEY_K, ui::EF_ALT_DOWN);
  local_state()->SetString(prefs::kGlicLauncherHotkey,
                           ui::Command::AcceleratorToString(hotkey));
}

}  // namespace glic
