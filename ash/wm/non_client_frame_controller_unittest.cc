// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/non_client_frame_controller.h"

#include "ash/public/cpp/ash_layout_constants.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/top_level_window_factory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "services/ws/test_change_tracker.h"
#include "services/ws/test_window_tree_client.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/aura_window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/fake_context_factory.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

gfx::Rect GetQuadBoundsInScreen(const viz::DrawQuad* quad) {
  return cc::MathUtil::MapEnclosingClippedRect(
      quad->shared_quad_state->quad_to_target_transform, quad->visible_rect);
}

bool FindAnyQuad(const viz::CompositorFrame& frame,
                 const gfx::Rect& screen_rect) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const auto& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

bool FindColorQuad(const viz::CompositorFrame& frame,
                   const gfx::Rect& screen_rect,
                   SkColor color) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const auto& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (quad->material != viz::DrawQuad::Material::SOLID_COLOR)
      continue;

    auto* color_quad = viz::SolidColorDrawQuad::MaterialCast(quad);
    if (color_quad->color != color)
      continue;
    if (GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

bool FindTiledContentQuad(const viz::CompositorFrame& frame,
                          const gfx::Rect& screen_rect) {
  DCHECK_EQ(1u, frame.render_pass_list.size());
  const auto& quad_list = frame.render_pass_list[0]->quad_list;
  for (const auto* quad : quad_list) {
    if (quad->material == viz::DrawQuad::Material::TILED_CONTENT &&
        GetQuadBoundsInScreen(quad) == screen_rect)
      return true;
  }
  return false;
}

}  // namespace

class NonClientFrameControllerMashTest : public AshTestBase {
 public:
  NonClientFrameControllerMashTest() = default;
  ~NonClientFrameControllerMashTest() override = default;

  const viz::CompositorFrame& GetLastCompositorFrame() const {
    return context_factory_.GetLastCompositorFrame();
  }

  // AshTestBase:
  void SetUp() override {
    aura::Env* env = aura::Env::GetInstance();
    DCHECK(env);
    context_factory_to_restore_ = env->context_factory();
    env->set_context_factory(&context_factory_);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    aura::Env::GetInstance()->set_context_factory(context_factory_to_restore_);
  }

 private:
  ui::FakeContextFactory context_factory_;
  ui::ContextFactory* context_factory_to_restore_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameControllerMashTest);
};

TEST_F(NonClientFrameControllerMashTest, ContentRegionNotDrawnForClient) {
  if (!::features::IsSingleProcessMash() && !::features::IsMultiProcessMash())
    return;

  std::map<std::string, std::vector<uint8_t>> properties;
  std::unique_ptr<aura::Window> window(CreateAndParentTopLevelWindow(
      ws::mojom::WindowType::WINDOW,
      /* property_converter */ nullptr, &properties));
  ASSERT_TRUE(window);

  NonClientFrameController* controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(controller);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window.get());
  ASSERT_TRUE(widget);

  const int caption_height =
      GetAshLayoutSize(AshLayoutSize::kNonBrowserCaption).height();
  const gfx::Size tile_size = cc::LayerTreeSettings().default_tile_size;
  const int tile_width = tile_size.width();
  const int tile_height = tile_size.height();
  const int tile_x = tile_width;
  const int tile_y = tile_height;

  const gfx::Rect kTileBounds(gfx::Point(tile_x, tile_y), tile_size);
  ui::Compositor* compositor = widget->GetCompositor();

  // Give the ui::Compositor a LocalSurfaceId so that it does not defer commit
  // when a draw is scheduled.
  viz::LocalSurfaceId local_surface_id(1, base::UnguessableToken::Create());
  compositor->SetLocalSurfaceId(local_surface_id, base::TimeTicks());

  // Without the window visible, there should be a tile for the wallpaper at
  // (tile_x, tile_y) of size |tile_size|.
  compositor->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
  {
    const viz::CompositorFrame& frame = GetLastCompositorFrame();
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_TRUE(FindColorQuad(frame, kTileBounds, SK_ColorBLACK));
  }

  // Show |widget|, and position it so that it covers that wallpaper tile.
  const gfx::Rect widget_bound(tile_x - 10, tile_y - 10, tile_width + 20,
                               tile_height + 20);
  widget->SetBounds(widget_bound);
  widget->Show();
  compositor->ScheduleDraw();
  ui::DrawWaiterForTest::WaitForCompositingEnded(compositor);
  {
    // This time, that tile for the wallpaper will not be drawn.
    const viz::CompositorFrame& frame = GetLastCompositorFrame();
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_FALSE(FindColorQuad(frame, kTileBounds, SK_ColorBLACK));

    // Any solid-color quads for the widget are transparent and will be
    // optimized away.
    const gfx::Rect top_left(widget_bound.origin(), tile_size);
    const gfx::Rect top_right(
        top_left.top_right(),
        gfx::Size(widget_bound.width() - top_left.width(), top_left.height()));
    const gfx::Rect bottom_left(
        top_left.bottom_left(),
        gfx::Size(top_left.width(), widget_bound.height() - top_left.height()));
    const gfx::Rect bottom_right(
        top_left.bottom_right(),
        gfx::Size(top_right.width(), bottom_left.height()));
    EXPECT_FALSE(FindAnyQuad(frame, top_left));
    EXPECT_FALSE(FindAnyQuad(frame, top_right));
    EXPECT_FALSE(FindAnyQuad(frame, bottom_left));
    EXPECT_FALSE(FindAnyQuad(frame, bottom_right));

    // And there will be a content quad for the window caption.
    gfx::Rect caption_bound(widget_bound);
    caption_bound.set_height(caption_height);
    EXPECT_TRUE(FindTiledContentQuad(frame, caption_bound));
  }
}

using NonClientFrameControllerTest = AshTestBase;

TEST_F(NonClientFrameControllerTest, CallsRequestClose) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  non_client_frame_controller->GetWidget()->Close();
  // Close should not have been scheduled on the widget yet (because the request
  // goes to the remote client).
  EXPECT_FALSE(non_client_frame_controller->GetWidget()->IsClosed());
  auto* changes = GetTestWindowTreeClient()->tracker()->changes();
  ASSERT_FALSE(changes->empty());
  // The remote client should have a request to close the window.
  EXPECT_EQ("RequestClose", ws::ChangeToDescription(changes->back()));
}

TEST_F(NonClientFrameControllerTest, WindowTitle) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  ASSERT_TRUE(non_client_frame_controller);
  EXPECT_TRUE(non_client_frame_controller->ShouldShowWindowTitle());
  EXPECT_TRUE(non_client_frame_controller->GetWindowTitle().empty());

  // Verify GetWindowTitle() mirrors window->SetTitle().
  const base::string16 title = base::ASCIIToUTF16("X");
  window->SetTitle(title);
  EXPECT_EQ(title, non_client_frame_controller->GetWindowTitle());

  // ShouldShowWindowTitle() mirrors |aura::client::kTitleShownKey|.
  window->SetProperty(aura::client::kTitleShownKey, false);
  EXPECT_FALSE(non_client_frame_controller->ShouldShowWindowTitle());
}

TEST_F(NonClientFrameControllerTest, ExposesChildTreeIdToAccessibility) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  const std::string ax_tree_id = "123";
  window->SetProperty(ui::kChildAXTreeID, new std::string(ax_tree_id));
  NonClientFrameController* non_client_frame_controller =
      NonClientFrameController::Get(window.get());
  views::View* contents_view = non_client_frame_controller->GetContentsView();
  ui::AXNodeData ax_node_data;
  contents_view->GetAccessibleNodeData(&ax_node_data);
  EXPECT_EQ(ax_tree_id, ax_node_data.GetStringAttribute(
                            ax::mojom::StringAttribute::kChildTreeId));
  EXPECT_EQ(ax::mojom::Role::kClient, ax_node_data.role);
}

}  // namespace ash
