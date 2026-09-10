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
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->Hide();
  aura::Window* widget_window = widget->GetNativeWindow();
  widget_window->TrackOcclusionState();
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            widget_window->GetOcclusionState());

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
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
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  aura::Window* widget_window = widget->GetNativeWindow();
  const auto transform = gfx::Transform::MakeTranslation(100.f, 100.f);
  widget_window->SetTransform(transform);

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto mirror_view = std::make_unique<WindowMirrorView>(
      widget_window, /*show_non_client_view=*/true);
  mirror_view->RecreateMirrorLayers();

  EXPECT_TRUE(
      mirror_view->GetMirrorLayerForTesting()->transform().IsIdentity());
}

TEST_F(WindowMirrorViewTest, Clipping) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  const gfx::Rect window_bounds(0, 0, 400, 400);
  widget->SetBounds(window_bounds);
  aura::Window* widget_window = widget->GetNativeWindow();

  // Set a top view inset to define a specific client area.
  const int kTopInset = 32;
  widget_window->SetProperty(aura::client::kTopViewInset, kTopInset);

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* contents_view = mirror_widget->widget_delegate()->GetContentsView();

  {
    // 1. Test with show_non_client_view = true.
    auto* mirror_view = contents_view->AddChildView(
        std::make_unique<WindowMirrorView>(widget_window,
                                           /*show_non_client_view=*/true));
    mirror_view->RecreateMirrorLayers();

    ASSERT_TRUE(mirror_view->layer());
    // The view layer should NOT mask to bounds, allowing shadows/overflows.
    EXPECT_FALSE(mirror_view->layer()->GetMasksToBounds());
    // The mirror layer should NOT have a clip rect in this mode.
    EXPECT_TRUE(mirror_view->GetMirrorLayerForTesting()->clip_rect().IsEmpty());

    contents_view->RemoveChildViewT(mirror_view);
  }

  {
    // 2. Test with show_non_client_view = false.
    auto* mirror_view = contents_view->AddChildView(
        std::make_unique<WindowMirrorView>(widget_window,
                                           /*show_non_client_view=*/false));
    mirror_view->RecreateMirrorLayers();

    ASSERT_TRUE(mirror_view->layer());
    // The view layer should still NOT mask to bounds.
    EXPECT_FALSE(mirror_view->layer()->GetMasksToBounds());

    // The mirror layer SHOULD have a clip rect that matches the client area.
    EXPECT_EQ(gfx::Rect(0, 32, 400, 368),
              mirror_view->GetMirrorLayerForTesting()->clip_rect());
  }
}

TEST_F(WindowMirrorViewTest, ChangingBoundsUpdatesClipRect) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 400, 400));
  aura::Window* widget_window = widget->GetNativeWindow();

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* contents_view = mirror_widget->widget_delegate()->GetContentsView();

  auto* mirror_view = contents_view->AddChildView(
      std::make_unique<WindowMirrorView>(widget_window,
                                         /*show_non_client_view=*/false));

  // 1. Set an initial top view inset.
  const int kTopInset = 30;
  widget_window->SetProperty(aura::client::kTopViewInset, kTopInset);
  mirror_view->RecreateMirrorLayers();

  // Initial expected clip.
  EXPECT_EQ(gfx::Rect(0, 30, 400, 370),
            mirror_view->GetMirrorLayerForTesting()->clip_rect());

  // 2. Change the source window's bounds instead of changing top_insets.
  widget->SetBounds(gfx::Rect(0, 0, 500, 500));

  // 3. Change the mirror view's bounds to trigger Layout().
  mirror_view->SetBounds(0, 0, 200, 200);

  // The clip rect should have updated to match the new client area.
  EXPECT_EQ(gfx::Rect(0, 30, 500, 470),
            mirror_view->GetMirrorLayerForTesting()->clip_rect());
}

}  // namespace
}  // namespace ash
