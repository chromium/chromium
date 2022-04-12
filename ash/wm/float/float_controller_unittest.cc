// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/wm/features.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WindowFloatTest : public AshTestBase {
 public:
  WindowFloatTest() = default;

  WindowFloatTest(const WindowFloatTest&) = delete;
  WindowFloatTest& operator=(const WindowFloatTest&) = delete;

  ~WindowFloatTest() override = default;

  void SetUp() override {
    // Ensure float feature is enabled.
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kFloatWindow);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test float/unfloat window.
TEST_F(WindowFloatTest, WindowFloatingSwitch) {
  std::unique_ptr<aura::Window> window_1(CreateTestWindow());
  std::unique_ptr<aura::Window> window_2(CreateTestWindow());
  FloatController* controller = Shell::Get()->float_controller();

  // Activate 'window_1' and perform floating.
  wm::ActivateWindow(window_1.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(controller->IsFloated(window_1.get()));

  // Activate 'window_2' and perform floating.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(controller->IsFloated(window_2.get()));

  // Only one floated window is allowed so when a different window is floated,
  // the previously floated window will be unfloated.
  EXPECT_FALSE(controller->IsFloated(window_1.get()));

  // When try to float the already floated 'window_2', it will unfloat this
  // window.
  wm::ActivateWindow(window_2.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_FALSE(controller->IsFloated(window_2.get()));
}

}  // namespace ash