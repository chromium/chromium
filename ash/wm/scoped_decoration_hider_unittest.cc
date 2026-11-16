// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/scoped_decoration_hider.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ui/base/app_types.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/decoration.h"
#include "ui/wm/core/decoration_controller.h"

namespace ash {

using ScopedDecorationHiderTest = AshTestBase;

// Tests that the decoration is hidden until all the hiders are destroyed.
TEST_F(ScopedDecorationHiderTest, HideDecoration) {
  auto window =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  auto* decoration =
      Shell::Get()->decoration_controller()->GetDecorationForWindow(
          window.get());
  ASSERT_TRUE(decoration);
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());

  auto hider1 = std::make_unique<ScopedDecorationHider>(window.get());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  auto hider2 = std::make_unique<ScopedDecorationHider>(window.get());
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  hider1.reset();
  EXPECT_FALSE(decoration->layer()->GetTargetVisibility());

  hider2.reset();
  EXPECT_TRUE(decoration->layer()->GetTargetVisibility());
}

// Tests that the hider can outlive the window.
TEST_F(ScopedDecorationHiderTest, WindowDestroyedFirst) {
  auto window =
      CreateWindowWithAppType(chromeos::AppType::SYSTEM_APP, {200, 300});
  auto hider = std::make_unique<ScopedDecorationHider>(window.get());
  window.reset();
  hider.reset();
}

}  // namespace ash
