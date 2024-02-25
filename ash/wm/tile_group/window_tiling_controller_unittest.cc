// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tile_group/window_tiling_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace {

class WindowTilingControllerTest : public AshTestBase {
 public:
  WindowTilingControllerTest()
      : scoped_features_(features::kTilingWindowResize) {}
  WindowTilingControllerTest(const WindowTilingControllerTest&) = delete;
  WindowTilingControllerTest& operator=(const WindowTilingControllerTest&) =
      delete;
  ~WindowTilingControllerTest() override = default;

 protected:
  WindowTilingController* controller() {
    return Shell::Get()->window_tiling_controller();
  }

  base::test::ScopedFeatureList scoped_features_;
};

}  // namespace

TEST_F(WindowTilingControllerTest, CanTilingResizeNormalWindow) {
  auto window = CreateToplevelTestWindow(gfx::Rect(10, 20, 450, 350));
  ASSERT_TRUE(WindowState::Get(window.get())->IsNormalStateType());

  EXPECT_TRUE(controller()->CanTilingResize(window.get()));
}

}  // namespace ash
