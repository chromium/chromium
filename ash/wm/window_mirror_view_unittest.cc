// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/test/ash_test_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

using WindowMirrorViewTest = AshTestBase;

TEST_F(WindowMirrorViewTest, LocalWindowOcclusionMadeVisible) {
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->Hide();
  aura::Window* widget_window = widget->GetNativeWindow();
  widget_window->TrackOcclusionState();
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            widget_window->GetOcclusionState());

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto mirror_view = std::make_unique<WindowMirrorView>(widget_window);
  mirror_widget->widget_delegate()->GetContentsView()->AddChildView(
      mirror_view.get());

  // Even though the widget is hidden, the occlusion state is considered
  // visible. This is to ensure renderers still produce content.
  EXPECT_EQ(aura::Window::OcclusionState::VISIBLE,
            widget_window->GetOcclusionState());
}

// Tests that a mirror view that mirrors a window with an existing transform
// does not copy that transform onto its mirror layer (and then putting the
// mirror layer offscreen). Regression test for https://crbug.com/1113429.
TEST_F(WindowMirrorViewTest, MirrorLayerHasNoTransformWhenNonClientViewShown) {
  // Create a window that has a transform already. When the layer is mirrored,
  // the transform will be copied with it.
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  aura::Window* widget_window = widget->GetNativeWindow();
  const auto transform = gfx::Transform::MakeTranslation(100.f, 100.f);
  widget_window->SetTransform(transform);

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto mirror_view = std::make_unique<WindowMirrorView>(
      widget_window, /*show_non_client_view=*/true);
  mirror_view->RecreateMirrorLayers();

  EXPECT_TRUE(
      mirror_view->GetMirrorLayerForTesting()->transform().IsIdentity());
}

}  // namespace
}  // namespace ash
