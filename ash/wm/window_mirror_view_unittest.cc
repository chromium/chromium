// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using WindowMirrorViewTest = AshTestBase;

TEST_F(WindowMirrorViewTest, LocalWindowOcclusionMadeVisible) {
  auto widget = CreateTestWidget();
  widget->Hide();
  aura::Window* widget_window = widget->GetNativeWindow();
  widget_window->TrackOcclusionState();
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            widget_window->occlusion_state());

  auto mirror_widget = CreateTestWidget();
  auto mirror_view = std::make_unique<WindowMirrorView>(
      widget_window, /*trilinear_filtering_on_init=*/false);
  mirror_widget->widget_delegate()->GetContentsView()->AddChildView(
      mirror_view.get());

  // Even though the widget is hidden, the occlusion state is considered
  // visible. This is to ensure renderers still produce content.
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            widget_window->occlusion_state());
}

}  // namespace
}  // namespace ash
