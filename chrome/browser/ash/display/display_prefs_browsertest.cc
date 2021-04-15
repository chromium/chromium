// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"

class DisplayPrefsBrowserTest : public InProcessBrowserTest {
 public:
  DisplayPrefsBrowserTest() = default;
  ~DisplayPrefsBrowserTest() override = default;

  void SetUpOnMainThread() override {
    local_state_ = g_browser_process->local_state();
  }

 protected:
  const base::Value* GetDisplayProperties(int index) {
    int64_t display_id =
        ash::Shell::Get()->display_manager()->GetDisplayAt(index).id();
    const base::DictionaryValue* display_properties =
        local_state_->GetDictionary(ash::prefs::kDisplayProperties);
    return display_properties ? display_properties->FindKeyOfType(
                                    base::NumberToString(display_id),
                                    base::Value::Type::DICTIONARY)
                              : nullptr;
  }

  display::Display::Rotation GetRotation(int index) {
    const base::Value* properties = GetDisplayProperties(index);
    EXPECT_TRUE(properties);
    display::Display::Rotation result = display::Display::ROTATE_0;
    const base::Value* rot_value =
        properties->FindKeyOfType("rotation", base::Value::Type::INTEGER);
    EXPECT_TRUE(rot_value);
    if (rot_value)
      result = static_cast<display::Display::Rotation>(rot_value->GetInt());
    return result;
  }

  PrefService* local_state_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayPrefsBrowserTest);
};

// Test that display prefs are registered in the browser local_state
// and persisted across sessions.
IN_PROC_BROWSER_TEST_F(DisplayPrefsBrowserTest, PRE_PowerState) {
  std::string power_state =
      local_state_->GetString(ash::prefs::kDisplayPowerState);
  EXPECT_EQ("all_on", power_state);
  local_state_->Set(ash::prefs::kDisplayPowerState,
                    base::Value("internal_off_external_on"));
}

IN_PROC_BROWSER_TEST_F(DisplayPrefsBrowserTest, PowerState) {
  std::string power_state =
      local_state_->GetString(ash::prefs::kDisplayPowerState);
  EXPECT_EQ("internal_off_external_on", power_state);
}

// Test that changing display rotation changes the pref state and
// persists across sessions.
IN_PROC_BROWSER_TEST_F(DisplayPrefsBrowserTest, PRE_DisplayRotation) {
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  // Verify initial rotation and pref value.
  display::Display::Rotation rotation = GetRotation(0);
  EXPECT_EQ(display::Display::ROTATE_0, rotation);

  const display::Display& display = display_manager->GetDisplayAt(0);
  EXPECT_EQ(display::Display::ROTATE_0, display.rotation());

  // Change rotation.
  display_manager->SetDisplayRotation(display.id(),
                                      display::Display::ROTATE_180,
                                      display::Display::RotationSource::USER);
  base::RunLoop().RunUntilIdle();

  // Verify new rotation and pref value.
  rotation = GetRotation(0);
  EXPECT_EQ(display::Display::ROTATE_180, rotation);

  EXPECT_EQ(display::Display::ROTATE_180,
            display_manager->GetDisplayAt(0).rotation());
}

IN_PROC_BROWSER_TEST_F(DisplayPrefsBrowserTest, DisplayRotation) {
  display::Display::Rotation rotation = GetRotation(0);
  EXPECT_EQ(display::Display::ROTATE_180, rotation);

  const display::Display& display =
      ash::Shell::Get()->display_manager()->GetDisplayAt(0);
  EXPECT_EQ(display::Display::ROTATE_180, display.rotation());
}
