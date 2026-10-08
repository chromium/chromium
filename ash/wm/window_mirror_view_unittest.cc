// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mirror_view.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/decoration.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace {

// Returns the number of nine-patch layers in the layer tree rooted at `layer`.
int CountNinePatchLayers(ui::Layer* layer) {
  if (!layer) {
    return 0;
  }
  int count = layer->type() == ui::LayerType::LAYER_NINE_PATCH ? 1 : 0;
  for (ui::Layer* child : layer->children()) {
    count += CountNinePatchLayers(child);
  }
  return count;
}

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

TEST_F(WindowMirrorViewTest, ExcludeShadow) {
  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect{0, 0, 100, 100});
  aura::Window* window = widget->GetNativeWindow();
  ::wm::SetShadowElevation(window, ::wm::kShadowElevationInactiveWindow);

  // Change the elevation while the shadow animation is running. The old shadow
  // layer fades out and is kept until the animation ends, so that the shadow
  // container has two child layers.
  gfx::ScopedAnimationDurationScaleMode animation_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  ::wm::SetShadowElevation(window, ::wm::kShadowElevationActiveWindow);

  ui::Decoration* shadow_decoration =
      ::wm::ShadowController::GetShadowDecorationForWindow(window);
  ASSERT_TRUE(shadow_decoration);
  ASSERT_EQ(2u, shadow_decoration->layer()->children().size());
  ASSERT_EQ(2, CountNinePatchLayers(shadow_decoration->layer()));

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* contents_view = mirror_widget->widget_delegate()->GetContentsView();

  // 1. With exclude_shadow = false, the shadow layers (nine-patch) are
  // included.
  {
    auto* mirror_view = contents_view->AddChildView(
        std::make_unique<WindowMirrorView>(window,
                                           /*show_non_client_view=*/true,
                                           /*sync_bounds=*/false,
                                           /*exclude_shadow=*/false));
    mirror_view->RecreateMirrorLayers();
    EXPECT_EQ(2, CountNinePatchLayers(mirror_view->GetMirrorLayerForTesting()));
    contents_view->RemoveChildViewT(mirror_view);
  }

  // 2. With exclude_shadow = true, the shadow container and all of its
  // sublayers are excluded.
  {
    auto* mirror_view = contents_view->AddChildView(
        std::make_unique<WindowMirrorView>(window,
                                           /*show_non_client_view=*/true,
                                           /*sync_bounds=*/false,
                                           /*exclude_shadow=*/true));
    mirror_view->RecreateMirrorLayers();
    EXPECT_EQ(0, CountNinePatchLayers(mirror_view->GetMirrorLayerForTesting()));
    contents_view->RemoveChildViewT(mirror_view);
  }
}

TEST_F(WindowMirrorViewTest, MinimizedWindowPreservesHiddenViewLayers) {
  constexpr char kHiddenViewLayerName[] = "HiddenViewLayer";
  constexpr char kChildWindowLayerName[] = "ChildWindowLayer";
  constexpr char kGrandchildWindowLayerName[] = "GrandchildWindowLayer";

  auto widget = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget->SetBounds(gfx::Rect(0, 0, 200, 200));
  auto* widget_contents = widget->widget_delegate()->GetContentsView();

  // Add an intentionally hidden views::View with a layer.
  auto* hidden_view =
      widget_contents->AddChildView(std::make_unique<views::View>());
  hidden_view->SetPaintToLayer();
  hidden_view->layer()->SetName(kHiddenViewLayerName);
  hidden_view->SetVisible(false);

  // Add a visible NativeViewHost hosting a child aura::Window and a
  // grandchild aura::Window.
  auto* native_view_host =
      widget_contents->AddChildView(std::make_unique<views::NativeViewHost>());
  native_view_host->SetBounds(0, 0, 100, 100);

  auto child_window = std::make_unique<aura::Window>(nullptr);
  child_window->SetType(aura::client::WINDOW_TYPE_CONTROL);
  child_window->Init(ui::LAYER_NOT_DRAWN);
  child_window->layer()->SetName(kChildWindowLayerName);

  auto grandchild_window = std::make_unique<aura::Window>(nullptr);
  grandchild_window->SetType(aura::client::WINDOW_TYPE_CONTROL);
  grandchild_window->Init(ui::LAYER_SURFACE);
  grandchild_window->layer()->SetName(kGrandchildWindowLayerName);
  grandchild_window->Show();
  child_window->AddChild(grandchild_window.get());

  native_view_host->Attach(child_window.get());

  // Minimize the widget and hide the descendant window.
  widget->Minimize();
  grandchild_window->Hide();
  ASSERT_FALSE(widget->GetNativeWindow()->layer()->visible());
  ASSERT_FALSE(child_window->layer()->visible());
  ASSERT_FALSE(grandchild_window->layer()->visible());
  ASSERT_FALSE(hidden_view->layer()->visible());

  auto mirror_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* mirror_view =
      mirror_widget->widget_delegate()->GetContentsView()->AddChildView(
          std::make_unique<WindowMirrorView>(widget->GetNativeWindow()));
  mirror_view->RecreateMirrorLayers();

  ui::Layer* mirror_root = mirror_view->GetMirrorLayerForTesting();
  ASSERT_TRUE(mirror_root);
  EXPECT_TRUE(mirror_root->visible());
  EXPECT_EQ(1.f, mirror_root->opacity());

  ui::Layer* mirrored_hidden_view =
      FindLayerWithName(mirror_root, kHiddenViewLayerName);
  ASSERT_TRUE(mirrored_hidden_view);
  EXPECT_FALSE(mirrored_hidden_view->visible());

  ui::Layer* mirrored_child_window =
      FindLayerWithName(mirror_root, kChildWindowLayerName);
  ASSERT_TRUE(mirrored_child_window);
  EXPECT_TRUE(mirrored_child_window->visible());

  ui::Layer* mirrored_grandchild_window =
      FindLayerWithName(mirror_root, kGrandchildWindowLayerName);
  ASSERT_TRUE(mirrored_grandchild_window);
  EXPECT_TRUE(mirrored_grandchild_window->visible());

  native_view_host->Detach();
}

}  // namespace
}  // namespace ash
