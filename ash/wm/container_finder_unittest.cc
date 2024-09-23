// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/container_finder.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContainerFinderTest = AshTestBase;

TEST_F(ContainerFinderTest, GetContainerForWindow) {
  // Create a normal widget in the default container.
  std::unique_ptr<views::Widget> widget = CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(1, 2, 3, 4));
  aura::Window* window = widget->GetNativeWindow();

  // The window itself is not a container.
  EXPECT_EQ(kShellWindowId_Invalid, window->GetId());

  // Container lookup finds the default container.
  aura::Window* container = GetContainerForWindow(window);
  ASSERT_TRUE(container);
  EXPECT_EQ(desks_util::GetActiveDeskContainerId(), container->GetId());
}

}  // namespace ash
