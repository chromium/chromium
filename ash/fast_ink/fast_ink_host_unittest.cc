// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_host.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/frame_sink/test/test_begin_frame_source.h"
#include "ash/frame_sink/test/test_layer_tree_frame_sink.h"
#include "ash/frame_sink/ui_resource.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class FastInkHostTest
    : public AshTestBase,
      public ::testing::WithParamInterface<std::tuple<std::string,
                                                      bool,
                                                      gfx::Rect,
                                                      gfx::Rect,
                                                      gfx::Rect,
                                                      bool,
                                                      bool>> {
 public:
  FastInkHostTest()
      : first_display_specs_(std::get<0>(GetParam())),
        auto_update_(std::get<1>(GetParam())),
        content_rect_(std::get<2>(GetParam())),
        expected_quad_rect_(std::get<3>(GetParam())),
        expected_quad_layer_rect_(std::get<4>(GetParam())),
        use_one_si_for_fast_ink_host_resources_(std::get<5>(GetParam())),
        enable_mappable_si_for_fast_ink_host_(std::get<6>(GetParam())) {
    std::vector<base::test::FeatureRef> enabled_features = {};
    std::vector<base::test::FeatureRef> disabled_features = {};
    if (use_one_si_for_fast_ink_host_resources_) {
      enabled_features.push_back(
          features::kUseOneSharedImageForFastInkHostResources);
    } else {
      disabled_features.push_back(
          features::kUseOneSharedImageForFastInkHostResources);
    }

    if (enable_mappable_si_for_fast_ink_host_) {
      enabled_features.push_back(features::kEnableMappableSIForFastInkHost);
    } else {
      disabled_features.push_back(features::kEnableMappableSIForFastInkHost);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  FastInkHostTest(const FastInkHostTest&) = delete;
  FastInkHostTest& operator=(const FastInkHostTest&) = delete;

  ~FastInkHostTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay(first_display_specs_);

    auto* root_window = ash_test_helper()->GetHost()->window();
    gfx::Rect screen_bounds = root_window->GetBoundsInScreen();

    widget_ = CreateTestWidget(nullptr, kShellWindowId_OverlayContainer);
    widget_->SetBounds(screen_bounds);
    host_window_ = widget_->GetNativeWindow();

    auto layer_tree_frame_sink = std::make_unique<TestLayerTreeFrameSink>();
    layer_tree_frame_sink_ = layer_tree_frame_sink.get();

    fast_ink_host_ = std::make_unique<FastInkHost>();
    fast_ink_host_->InitForTesting(host_window_,
                                   std::move(layer_tree_frame_sink));

    begin_frame_source_ = std::make_unique<TestBeginFrameSource>();
    layer_tree_frame_sink_->client()->SetBeginFrameSource(
        begin_frame_source_.get());
  }

  // AshTestBase:
  void TearDown() override {
    widget_.reset();
    AshTestBase::TearDown();
  }

  void OnBeginFrame() {
    // Request a frame from FrameSinkHost.
    begin_frame_source_->GetBeginFrameObserver()->OnBeginFrame(
        CreateValidBeginFrameArgsForTesting());
  }

 protected:
  std::string first_display_specs_;
  bool auto_update_ = false;
  gfx::Rect content_rect_;
  gfx::Rect expected_quad_rect_;
  gfx::Rect expected_quad_layer_rect_;
  bool use_one_si_for_fast_ink_host_resources_;
  bool enable_mappable_si_for_fast_ink_host_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<aura::Window, DanglingUntriaged> host_window_;
  std::unique_ptr<FastInkHost> fast_ink_host_;
  raw_ptr<TestLayerTreeFrameSink, DanglingUntriaged> layer_tree_frame_sink_;
  std::unique_ptr<TestBeginFrameSource> begin_frame_source_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(FastInkHostTest, CorrectFrameSubmittedToLayerTreeFrameSink) {
  // Request the first frame.
  OnBeginFrame();

  SCOPED_TRACE(base::StringPrintf(
      "Test params: first_display_specs=%s | auto_update=%s | content_rect=%s "
      "| expected_quad_rect=%s | expected_quad_layer_rect=%s",
      first_display_specs_.c_str(), auto_update_ ? "true" : "false",
      content_rect_.ToString().c_str(), expected_quad_rect_.ToString().c_str(),
      expected_quad_layer_rect_.ToString().c_str()));

  constexpr gfx::Rect kTestTotalDamageRectInDIP = gfx::Rect(0, 0, 50, 25);

  if (auto_update_) {
    fast_ink_host_->AutoUpdateSurface(content_rect_, kTestTotalDamageRectInDIP);

    // When host auto updates, it only submits a frame when requested by
    // LayerTreeFrameSink via a BeginFrameSource.
    begin_frame_source_->GetBeginFrameObserver()->OnBeginFrame(
        CreateValidBeginFrameArgsForTesting());
  } else {
    fast_ink_host_->UpdateSurface(content_rect_, kTestTotalDamageRectInDIP,
                                  /*synchronous_draw=*/true);
  }

  const viz::CompositorFrame& frame =
      layer_tree_frame_sink_->GetLatestReceivedFrame();

  const viz::CompositorRenderPassList& render_pass_list =
      frame.render_pass_list;

  ASSERT_EQ(render_pass_list.size(), 1u);
  auto& quad_list = render_pass_list.front()->quad_list;

  ASSERT_EQ(quad_list.size(), 1u);

  viz::DrawQuad* quad = quad_list.back();
  EXPECT_EQ(quad->material, viz::DrawQuad::Material::kTextureContent);

  EXPECT_EQ(quad->rect, expected_quad_rect_);
  EXPECT_EQ(quad->visible_rect, expected_quad_rect_);

  ASSERT_EQ(render_pass_list.front()->shared_quad_state_list.size(), 1u);
  auto* shared_quad_state =
      render_pass_list.front()->shared_quad_state_list.front();

  EXPECT_EQ(shared_quad_state->quad_layer_rect, expected_quad_layer_rect_);
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect,
            expected_quad_layer_rect_);

  EXPECT_EQ(frame.resource_list.back().is_overlay_candidate, auto_update_);
}

TEST_P(FastInkHostTest, DelayPaintingUntilReceivingFirstBeginFrame) {
  // Buffer is not initialized when there is no begin frame received.
  ASSERT_FALSE(fast_ink_host_->gpu_memory_buffer_for_test());
  ASSERT_FALSE(fast_ink_host_->client_si_for_test());
  EXPECT_EQ(fast_ink_host_->get_pending_bitmaps_size_for_test(), 0);

  int pending_bitmaps_size = 0;
  for (SkColor color : {SK_ColorRED, SK_ColorYELLOW, SK_ColorGREEN}) {
    {
      const gfx::Rect damage_rect_in_window =
          gfx::Rect(host_window_->bounds().size());
      auto paint = fast_ink_host_->CreateScopedPaint(damage_rect_in_window);
      paint->canvas().DrawRect(gfx::RectF(damage_rect_in_window), color);
    }
    // The bitmap is waiting to be drawn because no gpu memory buffer is
    // initialized.
    ++pending_bitmaps_size;
    EXPECT_EQ(fast_ink_host_->get_pending_bitmaps_size_for_test(),
              pending_bitmaps_size);
  }

  // Request the first frame.
  OnBeginFrame();

  bool should_be_using_gmb = !use_one_si_for_fast_ink_host_resources_ ||
                             !enable_mappable_si_for_fast_ink_host_;

  if (should_be_using_gmb) {
    // Buffer should be initialized after receiving the first begin frame.
    gfx::GpuMemoryBuffer* gpu_memory_buffer =
        fast_ink_host_->gpu_memory_buffer_for_test();
    if (use_one_si_for_fast_ink_host_resources_) {
      ASSERT_TRUE(fast_ink_host_->client_si_for_test());
    } else {
      ASSERT_FALSE(fast_ink_host_->client_si_for_test());
    }

    ASSERT_TRUE(gpu_memory_buffer);
    // Pending bitmaps should be drawn and cleared.
    EXPECT_EQ(fast_ink_host_->get_pending_bitmaps_size_for_test(), 0);

    ASSERT_TRUE(gpu_memory_buffer->Map());
    // Pending bitmaps should be correctly copied to the gpu memory buffer.
    EXPECT_EQ(*static_cast<SkColor*>(gpu_memory_buffer->memory(0)),
              SK_ColorGREEN);
    gpu_memory_buffer->Unmap();
  } else {
    // MappableSI should be initialized after receiving the first begin frame.
    ASSERT_TRUE(fast_ink_host_->client_si_for_test());
    // Pending bitmaps should be drawn and cleared.
    EXPECT_EQ(fast_ink_host_->get_pending_bitmaps_size_for_test(), 0);

    auto mapping = fast_ink_host_->client_si_for_test()->Map();
    ASSERT_TRUE(mapping);
    // Pending bitmaps should be correctly copied to the MappableSI's buffer.
    EXPECT_EQ(*static_cast<SkColor*>(mapping->Memory(0)), SK_ColorGREEN);
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    FastInkHostTest,
    testing::Values(
        // When auto updating surface, we update the full surface, ignoring the
        // content_rect.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        // Run a test with `use_one_si_for_fast_ink_host_resources` set to true.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/true,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        // Run a test to verify that MappableSI is not used if we are not using
        // a single SharedImage for FastInkHost UiResources.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/true),
        // Run a test with MappableSI used.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/true,
            /*enable_mappable_si_for_fast_ink_host=*/true),
        std::make_tuple(
            /*first_display_specs=*/"1000x500*2",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        std::make_tuple(
            /*first_display_specs=*/"1000x500*2/r",
            /*auto_update=*/true,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 500, 1000),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        // When auto updating is off, we update the surface enclosed by
        // content_rect.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/false,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 10, 10),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        std::make_tuple(
            /*first_display_specs=*/"1000x500*2",
            /*auto_update=*/false,
            /*content_rect=*/gfx::Rect(10, 10),
            /*expected_quad_rect=*/gfx::Rect(0, 0, 20, 20),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        std::make_tuple(
            /*first_display_specs=*/"1000x500*2/l",
            /*auto_update=*/false,
            /*content_rect=*/gfx::Rect(10, 15),
            /*expected_quad_rect=*/gfx::Rect(0, 480, 30, 20),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 500, 1000),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false),
        // If content rect is partially outside of the buffer, quad rect is
        // clipped by buffer size.
        std::make_tuple(
            /*first_display_specs=*/"1000x500",
            /*auto_update=*/false,
            /*content_rect=*/gfx::Rect(995, 0, 10, 10),
            /*expected_quad_rect=*/gfx::Rect(995, 0, 5, 10),
            /*expected_quad_layer_rect=*/gfx::Rect(0, 0, 1000, 500),
            /*use_one_si_for_fast_ink_host_resources=*/false,
            /*enable_mappable_si_for_fast_ink_host=*/false)));

}  // namespace
}  // namespace ash
