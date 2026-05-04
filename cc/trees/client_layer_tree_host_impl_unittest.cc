// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/client_layer_tree_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/memory_pressure_listener_registry.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/angle_conversions.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/base/features.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/input_handler.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/input/scroll_elasticity_helper.h"
#include "cc/input/scroll_utils.h"
#include "cc/input/scrollbar_controller.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/metrics/compositor_frame_reporting_controller.h"
#include "cc/metrics/frame_info.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/event_metrics_test_creator.h"
#include "cc/test/fake_frame_info.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/mock_latency_info_swap_promise_monitor.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/test/test_paint_worklet_layer_painter.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/frame_data.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host_impl_delegate.h"
#include "cc/trees/layer_tree_host_impl_test_base.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/render_frame_metadata.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "cc/view_transition/view_transition_request.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/region_capture_bounds.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/layers/viz_layer_tree_host_impl.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/latency/latency_info.h"

#define EXPECT_SCOPED(statements) \
  {                               \
    SCOPED_TRACE("");             \
    statements;                   \
  }

using media::VideoFrame;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Pointer;
using ::testing::Pointwise;
using ::testing::Property;
using ::testing::Range;
using ::testing::Return;
using ::testing::StrictMock;

using ScrollThread = cc::InputHandler::ScrollThread;

namespace cc {
namespace {

MATCHER(UniquePtrMatches, negation ? "do not match" : "match") {
  return std::get<0>(arg).get() == std::get<1>(arg);
}

class TestInputHandlerClient : public InputHandlerClient {
 public:
  TestInputHandlerClient() = default;
  ~TestInputHandlerClient() override = default;

  // InputHandlerClient implementation.
  void WillShutdown() override {}
  void Animate(base::TimeTicks time) override {}
  void ReconcileElasticOverscrollAndRootScroll() override {}
  void SetPrefersReducedMotion(bool prefers_reduced_motion) override {}
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::PointF& total_scroll_offset,
      const gfx::PointF& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {
    DCHECK(total_scroll_offset.x() <= max_scroll_offset.x());
    DCHECK(total_scroll_offset.y() <= max_scroll_offset.y());
    last_set_scroll_offset_ = total_scroll_offset;
    max_scroll_offset_ = max_scroll_offset;
    scrollable_size_ = scrollable_size;
    page_scale_factor_ = page_scale_factor;
    min_page_scale_factor_ = min_page_scale_factor;
    max_page_scale_factor_ = max_page_scale_factor;
  }
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override {}
  void DeliverInputForHighLatencyMode() override {}
  void DeliverInputForDeadline() override {}
  void DidFinishImplFrame() override {}
  bool HasQueuedInput() const override { return false; }
  void SetScrollEventDispatchMode(
      InputHandlerClient::ScrollEventDispatchMode mode,
      double scroll_deadline_ratio) override {}

  gfx::PointF last_set_scroll_offset() { return last_set_scroll_offset_; }
  gfx::PointF max_scroll_offset() const { return max_scroll_offset_; }
  gfx::SizeF scrollable_size() const { return scrollable_size_; }
  float page_scale_factor() const { return page_scale_factor_; }
  float min_page_scale_factor() const { return min_page_scale_factor_; }
  float max_page_scale_factor() const { return max_page_scale_factor_; }

 private:
  gfx::PointF last_set_scroll_offset_;
  gfx::PointF max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_ = 0;
  float min_page_scale_factor_ = -1;
  float max_page_scale_factor_ = -1;
};

// Test fixture that runs in all tree modes except for TreesInViz Service
// mode.
class ClientModeLayerTreeHostImplTest : public LayerTreeHostImplTest {};

class LayerTreeHostImplTestMultiScrollable : public LayerTreeHostImplTest {
 public:
  void SetUpLayers(LayerTreeSettings settings) {
    is_aura_scrollbar_ =
        settings.scrollbar_animator == LayerTreeSettings::AURA_OVERLAY;
    gfx::Size viewport_size(300, 200);
    gfx::Size content_size(1000, 1000);
    gfx::Size child_layer_size(250, 150);
    gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
    gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetDeviceScaleFactor(1);
    SetupViewportLayers(host_impl_->active_tree(), viewport_size, content_size,
                        content_size);
    LayerImpl* root_scroll = OuterViewportScrollLayer();

    // scrollbar_1 on root scroll.
    scrollbar_1_ = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), ScrollbarOrientation::kVertical, 15, 0,
        true);
    SetupScrollbarLayer(root_scroll, scrollbar_1_);
    scrollbar_1_->SetBounds(scrollbar_size_1);
    TouchActionRegion touch_action_region;
    touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
    scrollbar_1_->SetTouchActionRegion(touch_action_region);

    // scrollbar_2 on child.
    auto* child =
        AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
    GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

    scrollbar_2_ = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), ScrollbarOrientation::kVertical, 15, 0,
        true);
    SetupScrollbarLayer(child, scrollbar_2_);
    scrollbar_2_->SetBounds(scrollbar_size_2);

    host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();

    ResetScrollbars();
  }

  void ResetScrollbars() {
    GetEffectNode(scrollbar_1_.get())->opacity = 0;
    GetEffectNode(scrollbar_2_.get())->opacity = 0;
    UpdateDrawProperties(host_impl_->active_tree());

    if (is_aura_scrollbar_) {
      animation_task_.Reset();
    }
  }

  bool is_aura_scrollbar_;
  raw_ptr<SolidColorScrollbarLayerImpl> scrollbar_1_;
  raw_ptr<SolidColorScrollbarLayerImpl> scrollbar_2_;
};

class LayerTreeHostImplBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  LayerTreeHostImplBrowserControlsTest()
      // Make the clip size the same as the layer (content) size so the layer is
      // non-scrollable.
      : layer_size_(10, 10), clip_size_(layer_size_) {
    viewport_size_ = gfx::Size(clip_size_.width(),
                               clip_size_.height() + top_controls_height_);
  }

  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    bool init = LayerTreeHostImplTest::CreateHostImpl(
        settings, std::move(layer_tree_frame_sink));
    if (init) {
      host_impl_->active_tree()->SetBrowserControlsParams(
          {top_controls_height_, 0, 0, 0, false, false});
      host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
      host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    }
    return init;
  }

 protected:
  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->active_tree(), inner_viewport_size, outer_viewport_size,
        scroll_layer_size);
  }

  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      LayerTreeImpl* tree_impl,
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    tree_impl->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, true});
    tree_impl->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
    tree_impl->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    host_impl_->DidChangeBrowserControlsPosition();

    SetupViewportLayers(tree_impl, inner_viewport_size, outer_viewport_size,
                        scroll_layer_size);
  }

  gfx::Size layer_size_;
  gfx::Size clip_size_;
  gfx::Size viewport_size_;
  float top_controls_height_ = 50;
};  // class LayerTreeHostImplBrowserControlsTest

class LayerTreeHostImplWithImplicitLimitsTest : public LayerTreeHostImplTest {
 public:
  LayerTreeSettings DefaultSettings() override {
    LayerTreeSettings settings = LayerTreeHostImplTest::DefaultSettings();
    settings.max_memory_for_prepaint_percentage = 50;
    return settings;
  }
};

// Test implementation of RenderFrameMetadataObserver which can optionally
// request the frame-token to be sent to the embedder during frame submission.
class TestRenderFrameMetadataObserver : public RenderFrameMetadataObserver {
 public:
  explicit TestRenderFrameMetadataObserver(bool increment_counter)
      : increment_counter_(increment_counter) {}
  TestRenderFrameMetadataObserver(const TestRenderFrameMetadataObserver&) =
      delete;
  ~TestRenderFrameMetadataObserver() override = default;

  TestRenderFrameMetadataObserver& operator=(
      const TestRenderFrameMetadataObserver&) = delete;

  void BindToCurrentSequence() override {}
  void OnRenderFrameSubmission(
      const RenderFrameMetadata& render_frame_metadata,
      viz::CompositorFrameMetadata* compositor_frame_metadata,
      bool force_send) override {
    if (increment_counter_) {
      compositor_frame_metadata->send_frame_token_to_embedder = true;
    }
    last_metadata_ = render_frame_metadata;
  }
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  void DidEndScroll() override {}
#endif

  const std::optional<RenderFrameMetadata>& last_metadata() const {
    return last_metadata_;
  }

 private:
  bool increment_counter_;
  std::optional<RenderFrameMetadata> last_metadata_;
};

#define EXPECT_VIEWPORT_GEOMETRIES(expected_browser_controls_shown_ratio)    \
  do {                                                                       \
    auto* tree = host_impl_->active_tree();                                  \
    auto* property_trees = tree->property_trees();                           \
    EXPECT_EQ(expected_browser_controls_shown_ratio,                         \
              tree->CurrentTopControlsShownRatio());                         \
    EXPECT_EQ(                                                               \
        tree->top_controls_height() * expected_browser_controls_shown_ratio, \
        host_impl_->browser_controls_manager()->ContentTopOffset());         \
    int delta =                                                              \
        (tree->top_controls_height() + tree->bottom_controls_height()) *     \
        (1 - expected_browser_controls_shown_ratio);                         \
    int scaled_delta = delta / tree->min_page_scale_factor();                \
    gfx::Size inner_scroll_bounds = tree->InnerViewportScrollNode()->bounds; \
    inner_scroll_bounds.Enlarge(0, scaled_delta);                            \
    EXPECT_EQ(inner_scroll_bounds, InnerViewportScrollLayer()->bounds());    \
    EXPECT_EQ(gfx::RectF(gfx::SizeF(inner_scroll_bounds)),                   \
              tree->OuterViewportClipNode()->clip);                          \
    EXPECT_EQ(gfx::Vector2dF(0, delta),                                      \
              property_trees->inner_viewport_container_bounds_delta());      \
    EXPECT_EQ(gfx::Vector2dF(0, scaled_delta),                               \
              property_trees->outer_viewport_container_bounds_delta());      \
  } while (false)

// Checks that we use the memory limits provided.
// TODO(crbug.com/458776836): Review memory limit related cc_unittests for
// TreesInViz Service mode
TEST_P(ClientModeLayerTreeHostImplTest, MemoryLimits) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  const size_t kGpuByteLimit = 1234321;
  const size_t kGpuResourceLimit = 2345432;
  const gpu::MemoryAllocation::PriorityCutoff kGpuCutoff =
      gpu::MemoryAllocation::CUTOFF_ALLOW_EVERYTHING;

  const TileMemoryLimitPolicy kGpuTileCutoff =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(kGpuCutoff);
  const TileMemoryLimitPolicy kNothingTileCutoff =
      ManagedMemoryPolicy::PriorityCutoffToTileMemoryLimitPolicy(
          gpu::MemoryAllocation::CUTOFF_ALLOW_NOTHING);
  EXPECT_NE(kGpuTileCutoff, kNothingTileCutoff);

  LayerTreeSettings settings = DefaultSettings();
  settings.memory_policy =
      ManagedMemoryPolicy(kGpuByteLimit, kGpuCutoff, kGpuResourceLimit);
  host_impl_ = CreateLayerTreeHostImplForTesting(
      settings, this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
      nullptr, nullptr);
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl_));

  // Gpu compositing.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::Create3d();
  host_impl_->SetVisible(true);
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }

  // Not visible, drops to 0.
  host_impl_->SetVisible(false);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(0u, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kNothingTileCutoff, state.memory_limit_policy);
  }

  // Visible, is the gpu limit again.
  host_impl_->SetVisible(true);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
  }

  // Software compositing.
  host_impl_->ReleaseLayerTreeFrameSink();
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }

  // Not visible, drops to 0.
  host_impl_->SetVisible(false);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(0u, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kNothingTileCutoff, state.memory_limit_policy);
  }

  // Visible, is the software limit again.
  host_impl_->SetVisible(true);
  {
    const auto& state = host_impl_->global_tile_state();
    EXPECT_EQ(kGpuByteLimit, state.hard_memory_limit_in_bytes);
    EXPECT_EQ(kGpuResourceLimit, state.num_resources_limit);
    EXPECT_EQ(kGpuTileCutoff, state.memory_limit_policy);
  }
}

// UIResource tests do not apply to TIV Service.
TEST_P(ClientModeLayerTreeHostImplTest, UIResourceManagement) {
  auto test_context_provider = viz::TestContextProvider::CreateRaster();
  gpu::TestSharedImageInterface* sii =
      test_context_provider->SharedImageInterface();
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::Create3d(
                                        std::move(test_context_provider)));

  EXPECT_EQ(0u, sii->shared_image_count());

  UIResourceId ui_resource_id = 1;
  bool is_opaque = false;
  UIResourceBitmap bitmap(gfx::Size(1, 1), is_opaque);
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(viz::kInvalidResourceId, id1);

  // Multiple requests with the same id is allowed.  The previous texture is
  // deleted.
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id2 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(viz::kInvalidResourceId, id2);
  EXPECT_NE(id1, id2);

  // Deleting invalid UIResourceId is allowed and does not change state.
  host_impl_->DeleteUIResource(-1);
  EXPECT_EQ(1u, sii->shared_image_count());

  // Should return zero for invalid UIResourceId.  Number of textures should
  // not change.
  EXPECT_EQ(viz::kInvalidResourceId, host_impl_->ResourceIdForUIResource(-1));
  EXPECT_EQ(1u, sii->shared_image_count());

  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(viz::kInvalidResourceId,
            host_impl_->ResourceIdForUIResource(ui_resource_id));
  EXPECT_EQ(0u, sii->shared_image_count());

  // Should not change state for multiple deletion on one UIResourceId
  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, sii->shared_image_count());
}

TEST_P(ClientModeLayerTreeHostImplTest, CreateETC1UIResource) {
  auto test_context_provider = viz::TestContextProvider::CreateRaster();
  gpu::TestSharedImageInterface* sii =
      test_context_provider->SharedImageInterface();
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::Create3d(
                                        std::move(test_context_provider)));

  EXPECT_EQ(0u, sii->shared_image_count());

  gfx::Size size(4, 4);
  SkImageInfo info =
      SkImageInfo::Make(4, 4, kAlpha_8_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkPixelRef> pixel_ref(SkMallocPixelRef::MakeAllocate(info, 0));
  pixel_ref->setImmutable();
  UIResourceBitmap bitmap(std::move(pixel_ref), size);
  UIResourceId ui_resource_id = 1;
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, sii->shared_image_count());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(viz::kInvalidResourceId, id1);
}

TEST_P(ClientModeLayerTreeHostImplTest,
       SelectionBoundsPassedToRenderFrameMetadata) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  auto observer = std::make_unique<TestRenderFrameMetadataObserver>(false);
  auto* observer_ptr = observer.get();
  host_impl_->SetRenderFrameObserver(std::move(observer));
  EXPECT_FALSE(observer_ptr->last_metadata());

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  DrawFrame();

  // Ensure the selection bounds propagated to the render frame metadata
  // represent an empty selection.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_1 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.start.type());
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.end.type());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_end());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_start());
  EXPECT_FALSE(selection_1.start.visible());
  EXPECT_FALSE(selection_1.end.visible());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_start(5, 0);
  gfx::Point selection_end(5, 5);
  LayerSelection selection;
  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root->id();
  selection.start.edge_end = selection_end;
  selection.start.edge_start = selection_start;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  DrawFrame();

  // Ensure the selection bounds have propagated to the render frame metadata.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_2 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(selection.start.type, selection_2.start.type());
  EXPECT_EQ(selection.end.type, selection_2.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_2.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_2.start.edge_start());
  EXPECT_TRUE(selection_2.start.visible());
  EXPECT_TRUE(selection_2.end.visible());
}

TEST_P(ClientModeLayerTreeHostImplTest,
       VerticalScrollDirectionChangesPassedToRenderFrameMetadata) {
  // Set up the viewport.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));

  // Set up the render frame metadata observer.
  auto observer = std::make_unique<TestRenderFrameMetadataObserver>(false);
  auto* observer_ptr = observer.get();
  host_impl_->SetRenderFrameObserver(std::move(observer));
  EXPECT_FALSE(observer_ptr->last_metadata());

  // Our test will be comprised of multiple legs, each leg consisting of a
  // distinct scroll event and an expectation regarding the vertical scroll
  // direction passed to the render frame metadata.
  typedef struct {
    gfx::Vector2d scroll_delta;
    viz::VerticalScrollDirection expected_vertical_scroll_direction;
  } TestLeg;

  std::vector<TestLeg> test_legs;

  // Initially, vertical scroll direction should be |kNull| indicating absence.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Scrolling to the right should not affect vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(10, 0),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // After scrolling down, the vertical scroll direction should be |kDown|.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, 10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kDown});

  // If we scroll down again, the vertical scroll direction should be |kNull| as
  // there was no change in vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, 10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Scrolling to the left should not affect last vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(-10, 0),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // After scrolling up, the vertical scroll direction should be |kUp|.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, -10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kUp});

  // If we scroll up again, the vertical scroll direction should be |kNull| as
  // there was no change in vertical scroll direction.
  test_legs.push_back({/*scroll_delta=*/gfx::Vector2d(0, -10),
                       /*expected_vertical_scroll_direction=*/viz::
                           VerticalScrollDirection::kNull});

  // Iterate over all legs of our test.
  for (auto& test_leg : test_legs) {
    // If the test leg contains a scroll, perform it.
    if (!test_leg.scroll_delta.IsZero()) {
      GetInputHandler().ScrollBegin(
          BeginState(gfx::Point(), test_leg.scroll_delta,
                     ui::ScrollInputType::kWheel)
              .get(),
          ui::ScrollInputType::kWheel);
      GetInputHandler().ScrollUpdate(UpdateState(
          gfx::Point(), test_leg.scroll_delta, ui::ScrollInputType::kWheel));
    }

    // Trigger draw.
    host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                               /*skip_if_inside_draw=*/false);
    DrawFrame();

    // Assert our expectation regarding the vertical scroll direction.
    EXPECT_EQ(test_leg.expected_vertical_scroll_direction,
              observer_ptr->last_metadata()->new_vertical_scroll_direction);

    // If the test leg contains a scroll, end it.
    if (!test_leg.scroll_delta.IsZero()) {
      GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    }
  }
}

TEST_P(ClientModeLayerTreeHostImplTest,
       ImplThreadPhaseUponImplSideInvalidation) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // In general invalidation should never be ran outside the impl frame.
  auto args = viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->WillBeginImplFrame(args);
  // Expect no crash because the operation is within an impl frame.
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())
        ->InvalidateContentOnImplSide();
  }

  // Once the impl frame is finished the impl thread phase is set to IDLE.
  host_impl_->DidFinishImplFrame(args);

  settings.using_synchronous_renderer_compositor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // Expect no crash when using synchronous renderer compositor regardless the
  // impl thread phase.
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())
        ->InvalidateContentOnImplSide();
  }

  // Test passes when there is no crash.
}

// Scroll operations on a non-composited scroller are eligible
// for rasterization. This tests if rasterization is detected
// and requested by the LTHI. The scroll is applied to the
// pending tree, so activation will be required before the scroll
// is presented.
TEST_P(ClientModeLayerTreeHostImplTest, NonCompositedScrollUsesRaster) {
  gfx::Size scrollable_content_bounds(100, 100);
  gfx::Size container_bounds(50, 50);
  if (!CommitsToActiveTree()) {
    CreatePendingTree();
  }

  // Create root and scroll layers so that we can set up a
  // non-composited scrollable node, eligible for raster scroll.
  auto* sync_tree_root = SetupRootLayer<LayerImpl>(host_impl_->sync_tree(),
                                                   scrollable_content_bounds);
  sync_tree_root->SetNeedsPushProperties();
  auto* scrolling_layer =
      AddScrollableLayer(sync_tree_root, container_bounds, gfx::Size());
  scrolling_layer->SetNeedsPushProperties();
  CreateScrollNodeForNonCompositedScroller(
      host_impl_->sync_tree()->property_trees(), sync_tree_root->id(),
      scrolling_layer->element_id(), scrollable_content_bounds,
      container_bounds);

  // Draw at least one frame before ScrollBegin.
  host_impl_->sync_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl_->sync_tree());
  host_impl_->ActivateSyncTree();
  DrawFrame();

  // Scrolling on this non-composited tree should be marked as raster-inducing.
  ScrollStateData scroll_state_data;
  scroll_state_data.set_current_native_scrolling_element(
      scrolling_layer->element_id());
  scroll_state_data.is_beginning = true;
  std::unique_ptr<ScrollState> scroll_state =
      std::make_unique<ScrollState>(scroll_state_data);

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      scroll_state.get(), ui::ScrollInputType::kWheel);
  EXPECT_EQ(true, status.raster_inducing);

  // We always want to start applying the scroll offset to the active tree.
  host_impl_->active_tree()->DidUpdateScrollOffset(
      scrolling_layer->element_id(), /*pushed_from_main_or_pending_tree=*/true);

  // Draw the next frame of the scroll.
  {
    host_impl_->NotifyInputEvent(/*is_fling=*/false);
    host_impl_->SetFullViewportDamage();
    host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                               /*skip_if_inside_draw=*/false);
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

    // This call sets the invalidate_raster_scroll bit.
    // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
    if (!host_impl_->settings().trees_in_viz_in_viz_process) {
      static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())
          ->InvalidateContentOnImplSide();
    }
    if (!CommitsToActiveTree()) {
      // Activate the pending tree before drawing layers.
      host_impl_->ActivateSyncTree();
    }
    std::optional<SubmitInfo> draw_layers_state =
        host_impl_->DrawLayers(&frame);
    EXPECT_EQ(true, draw_layers_state->invalidate_raster_scroll);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
}

INSTANTIATE_CLIENT_MODE_TREE_TEST_P(ClientModeLayerTreeHostImplTest);

TEST_P(LayerTreeHostImplTestMultiScrollable,
       ScrollbarFlashAfterAnyScrollUpdate) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_after_any_scroll_update = true;
  settings.scrollbar_flash_once_after_scroll_update = false;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0);

  // Scroll on root should flash all scrollbars.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(20, 20), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
  ResetScrollbars();

  // Scroll on child should flash all scrollbars.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(70, 70), gfx::Vector2dF(0, 100),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(70, 70), gfx::Vector2d(0, 100)));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
}

TEST_P(LayerTreeHostImplTestMultiScrollable,
       ScrollbarFlashOnceAfterAnyScrollUpdate) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_after_any_scroll_update = false;
  settings.scrollbar_flash_once_after_scroll_update = true;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0);

  // Beginning of scroll on root should flash all scrollbars.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(20, 20), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
  ResetScrollbars();

  // Scrolling on root again mustn't flash other than the root scrollbar.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(70, 70), gfx::Vector2dF(0, 100),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(70, 70), gfx::Vector2d(0, 100), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
  ResetScrollbars();

  // Yet another scroll on child should flash only the child scrollbar.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(70, 70), gfx::Vector2dF(0, 100),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(70, 70), gfx::Vector2d(0, 100)));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_FALSE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
}

TEST_P(LayerTreeHostImplTestMultiScrollable,
       ScrollbarFlashOnceEnteredViewport) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_after_any_scroll_update = false;
  settings.scrollbar_flash_once_after_scroll_update = false;
  settings.scrollbar_flash_once_visible_on_viewport = true;

  SetUpLayers(settings);

  raw_ptr<SolidColorScrollbarLayerImpl> scrollbar3 = nullptr;

  {
    // Create another child scroll element at (10, 210) with size 50x150
    LayerImpl* root_scroll = OuterViewportScrollLayer();

    auto* child = AddScrollableLayer(root_scroll, gfx::Size(100, 100),
                                     gfx::Size(250, 150));
    GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

    scrollbar3 = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), ScrollbarOrientation::kVertical, 15, 0,
        true);
    SetupScrollbarLayer(child, scrollbar3);
    scrollbar3->SetBounds(gfx::Size(50, 150));
    scrollbar3->SetOffsetToTransformParent(gfx::Vector2dF(10, 210));
    host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();
  }

  EXPECT_EQ(scrollbar_1_->Opacity(), 0);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0);
  EXPECT_EQ(scrollbar3->Opacity(), 0);

  // First scroll: root down by 10, visible scrollbars flash.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(20, 20), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Scroll is less than threshold, so no flash.
  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_FALSE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();

  // Second scroll: root down by another 200 right till where the third child
  // is. Both the root and the scrollbar2 should flash as the scrollbar2 is
  // one the viewport now.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(150, 100), gfx::Vector2dF(0, 200),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(150, 100),
                                             gfx::Vector2d(0, 200),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());
  EXPECT_FALSE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();

  // Scroll down a bit more. Now, the scrollbar3 should flash as it is visible
  // now.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(150, 100), gfx::Vector2dF(0, 30),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(150, 100), gfx::Vector2d(0, 30), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_TRUE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();

  // Scroll down more so that that last scrollbar is not visible anymore.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(150, 100), gfx::Vector2dF(0, 300),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(150, 100),
                                             gfx::Vector2d(0, 300),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_FALSE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();

  // Scroll back so that scrollbar3 is visible again.
  // Scroll down more so that that last scrollbar is not visible anymore.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(150, 100), gfx::Vector2dF(0, -310),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(150, 100),
                                             gfx::Vector2d(0, -310),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_FALSE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();

  // Scroll up a bit more. Now, the scrollbar3 should flash as it is visible
  // now and the threshold is passed.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(150, 100), gfx::Vector2dF(0, 25),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(150, 100), gfx::Vector2d(0, 25), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_TRUE(scrollbar3->Opacity());
  EXPECT_FALSE(animation_task_.is_null());

  // Reset scrollbars
  GetEffectNode(scrollbar3.get())->opacity = 0;
  ResetScrollbars();
}

TEST_P(LayerTreeHostImplTestMultiScrollable, ScrollbarFlashWhenMouseEnter) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_when_mouse_enter = true;

  SetUpLayers(settings);

  constexpr size_t kNumberOfRepeats = 3;
  for (size_t i = 0; i < kNumberOfRepeats; i++) {
    ScrollbarAnimationController* scrollbar_animation_controller =
        host_impl_->ScrollbarAnimationControllerForElementId(
            scrollbar_1_->scroll_element_id());

    const float kMouseMoveDistanceToTriggerFadeIn =
        scrollbar_animation_controller
            ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
            .MouseMoveDistanceToTriggerFadeIn();
    const int thumb_thickness = scrollbar_1_->ThumbThickness();

    GetInputHandler().MouseMoveAt(
        gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerFadeIn + 1, 1));
    EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(
        ScrollbarOrientation::kVertical));
    EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbarThumb(
        ScrollbarOrientation::kVertical));

    EXPECT_FALSE(scrollbar_1_->Opacity());
    EXPECT_FALSE(scrollbar_2_->Opacity());

    ResetScrollbars();

    ScrollbarAnimationController* scrollbar_animation_controller2 =
        host_impl_->ScrollbarAnimationControllerForElementId(
            scrollbar_2_->scroll_element_id());

    const float kMouseMoveDistanceToTriggerFadeInChild =
        scrollbar_animation_controller2
            ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
            .MouseMoveDistanceToTriggerFadeIn();
    const int kThumbThicknessChild = scrollbar_2_->ThumbThickness();

    GetInputHandler().MouseMoveAt(gfx::Point(
        kThumbThicknessChild + kMouseMoveDistanceToTriggerFadeInChild + 50,
        50));
    EXPECT_FALSE(scrollbar_animation_controller2->MouseIsNearScrollbar(
        ScrollbarOrientation::kVertical));
    EXPECT_FALSE(scrollbar_animation_controller2->MouseIsNearScrollbarThumb(
        ScrollbarOrientation::kVertical));

    EXPECT_FALSE(scrollbar_1_->Opacity());
    EXPECT_TRUE(scrollbar_2_->Opacity());

    ResetScrollbars();
  }
}

INSTANTIATE_CLIENT_MODE_TREE_TEST_P(LayerTreeHostImplTestMultiScrollable);

// Tests that, on a page with content the same size as the viewport, hiding
// the browser controls also increases the ScrollableSize (i.e. the content
// size). Since the viewport got larger, the effective scrollable "content" also
// did. This ensures, for one thing, that the overscroll glow is shown in the
// right place.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsExpandsScrollableSize) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();
  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1);
  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0.5f);
  EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());

  // Fully hide the browser controls.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0);
  EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());

  // Scrolling additionally shouldn't have any effect.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_VIEWPORT_GEOMETRIES(0);
  EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());

  host_impl_->browser_controls_manager()->ScrollEnd();
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsExpandsClipAncestorsOfReplacedOuterScroller) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(180, 180), gfx::Size(180, 180), gfx::Size(180, 180));

  LayerTreeImpl* active_tree = host_impl_->active_tree();
  PropertyTrees* property_trees = active_tree->property_trees();
  LayerImpl* original_outer_scroll = OuterViewportScrollLayer();

  LayerImpl* parent_clip_layer = AddLayerInActiveTree();
  CopyProperties(original_outer_scroll, parent_clip_layer);
  parent_clip_layer->SetBounds(gfx::Size(160, 160));
  CreateClipNode(parent_clip_layer);
  LayerImpl* clip_layer = AddLayerInActiveTree();
  clip_layer->SetBounds(gfx::Size(150, 150));
  CopyProperties(parent_clip_layer, clip_layer);
  CreateClipNode(clip_layer);
  LayerImpl* scroll_layer =
      AddScrollableLayer(clip_layer, gfx::Size(150, 150), gfx::Size(300, 300));
  GetScrollNode(scroll_layer)->scrolls_outer_viewport = true;
  ClipNode* original_outer_clip = GetClipNode(original_outer_scroll);
  ClipNode* parent_clip = GetClipNode(parent_clip_layer);
  ClipNode* scroll_clip = GetClipNode(clip_layer);

  auto viewport_property_ids = active_tree->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_clip = clip_layer->clip_tree_index();
  viewport_property_ids.outer_scroll = scroll_layer->scroll_tree_index();
  active_tree->SetViewportPropertyIds(viewport_property_ids);
  UpdateDrawProperties(active_tree);

  EXPECT_EQ(scroll_layer, OuterViewportScrollLayer());
  EXPECT_EQ(GetScrollNode(scroll_layer),
            active_tree->OuterViewportScrollNode());

  EXPECT_EQ(1, active_tree->CurrentTopControlsShownRatio());
  EXPECT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->outer_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::SizeF(300, 300), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::RectF(0, 0, 180, 180), original_outer_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 160, 160), parent_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 150, 150), scroll_clip->clip);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_EQ(0.5f, active_tree->CurrentTopControlsShownRatio());
  EXPECT_EQ(25, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->outer_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::SizeF(300, 300), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::RectF(0, 0, 150, 175), scroll_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 160, 175), parent_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 180, 180), original_outer_clip->clip);

  // Fully hide the browser controls.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_EQ(0, active_tree->CurrentTopControlsShownRatio());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->outer_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::SizeF(300, 300), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::RectF(0, 0, 150, 200), scroll_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 160, 200), parent_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 180, 200), original_outer_clip->clip);

  // Scrolling additionally shouldn't have any effect.
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
  EXPECT_EQ(0, active_tree->CurrentTopControlsShownRatio());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->outer_viewport_container_bounds_delta());
  EXPECT_EQ(gfx::SizeF(300, 300), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::RectF(0, 0, 150, 200), scroll_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 160, 200), parent_clip->clip);
  EXPECT_EQ(gfx::RectF(0, 0, 180, 200), original_outer_clip->clip);

  host_impl_->browser_controls_manager()->ScrollEnd();
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsAdjustsSnapFling) {
  gfx::Size view_size(100, 100);
  gfx::Size content_size(100, 1000);
  gfx::RectF snap_area_1(0, 0, 100, 700);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(view_size, view_size,
                                                        content_size);

  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 100, 100), gfx::PointF(0, 900));
  ScrollSnapAlign start = ScrollSnapAlign(SnapAlignment::kStart);
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_1, false, false, ElementId(10)));
  host_impl_->OuterViewportScrollNode()->snap_container_data.emplace(container);

  DrawFrame();
  EXPECT_VIEWPORT_GEOMETRIES(1.0f);

  LayerTreeImpl* active_tree = host_impl_->active_tree();
  PropertyTrees* property_trees = active_tree->property_trees();
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  InputHandler& handler = GetInputHandler();
  gfx::PointF initial_offset, target_offset;
  gfx::Point position(50, 50);
  ui::ScrollInputType type = ui::ScrollInputType::kTouchscreen;

  handler.ScrollBegin(&*BeginState(position, gfx::Vector2dF(0, 15), type),
                      type);
  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 15), type));

  // The browser controls, now partially hidden, are consuming all of the scroll
  // delta so far.
  EXPECT_FLOAT_EQ(0.7, active_tree->CurrentTopControlsShownRatio());
  EXPECT_EQ(gfx::Vector2dF(0, 15),
            property_trees->outer_viewport_container_bounds_delta());
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));

  // Enter "constrained native fling" mode inside snap_area_1.
  EXPECT_FALSE(handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 30), gfx::Vector2dF(0, 600), &initial_offset,
      &target_offset));

  // Finish hiding the browser controls and start scrolling the content.
  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 435), type));
  EXPECT_POINTF_EQ(gfx::PointF(0, 400),
                   CurrentScrollOffset(outer_scroll_layer));

  // Try to scroll past the bottom of snap_area_1.
  EXPECT_TRUE(handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 300), gfx::Vector2dF(0, 1000), &initial_offset,
      &target_offset));

  // The fling constraint should take us to 550, which aligns with the bottom of
  // snap_area_1 using the expanded viewport size of 100x150 from hiding browser
  // controls, even though SnapContainerData is still based on 100x100 viewport.
  EXPECT_TRUE(handler.animating_for_snap_for_testing(
      host_impl_->OuterViewportScrollNode()->element_id));
  EXPECT_POINTF_EQ(gfx::PointF(0, 400), initial_offset);
  EXPECT_POINTF_EQ(gfx::PointF(0, 550), target_offset);

  handler.ScrollEnd(false, std::nullopt);
}

// Ensure that moving the browser controls (i.e. omnibox/url-bar on mobile) on
// pages with a non-1 minimum page scale factor (e.g. legacy desktop page)
// correctly scales the clipping adjustment performed to show the newly exposed
// region of the page.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       MovingBrowserControlsOuterClipDeltaScaled) {
  gfx::Size inner_size = gfx::Size(100, 100);
  gfx::Size outer_size = gfx::Size(100, 100);
  gfx::Size content_size = gfx::Size(200, 1000);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(inner_size, outer_size,
                                                        content_size);

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a content layer beneath the outer viewport scroll layer.
  LayerImpl* content = AddLayerInActiveTree();
  content->SetBounds(gfx::Size(100, 100));
  CopyProperties(OuterViewportScrollLayer(), content);
  active_tree->PushPageScaleFromMainThread(0.5f, 0.5f, 4);

  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1.0f);
  EXPECT_EQ(gfx::SizeF(200, 1000), active_tree->ScrollableSize());

  ASSERT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  // Hide the browser controls by 25px. The outer clip should expand by 50px as
  // because the outer viewport is sized based on the minimum scale, in this
  // case 0.5. Therefore, changes to the outer viewport need to be divided by
  // the minimum scale as well.
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(0, 25),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_VIEWPORT_GEOMETRIES(0.5f);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Tests that browser controls affect the position of horizontal scrollbars.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsAdjustsScrollbarPosition) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a horizontal scrollbar.
  gfx::Size scrollbar_size(gfx::Size(50, 15));
  auto* scrollbar_layer = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kHorizontal, 3, 20,
      false);
  SetupScrollbarLayer(OuterViewportScrollLayer(), scrollbar_layer);
  scrollbar_layer->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size));
  scrollbar_layer->SetTouchActionRegion(touch_action_region);
  scrollbar_layer->SetOffsetToTransformParent(gfx::Vector2dF(0, 35));
  host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();
  UpdateDrawProperties(host_impl_->active_tree());

  DrawFrame();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_VIEWPORT_GEOMETRIES(1.0f);
  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 3), scrollbar_layer->ComputeThumbQuadRect());

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 25),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    EXPECT_VIEWPORT_GEOMETRIES(0.5f);
    EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 25, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Fully hide the browser controls.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    EXPECT_VIEWPORT_GEOMETRIES(0);
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Additional scrolling shouldn't have any effect.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 25));
    EXPECT_VIEWPORT_GEOMETRIES(0);
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  host_impl_->browser_controls_manager()->ScrollEnd();
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplBrowserControlsTest,
       ScrollBrowserControlsByFractionalAmount) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Size(10, 10));
  DrawFrame();

  gfx::Vector2dF top_controls_scroll_delta(0, 5.25f);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  // Make the test scroll delta a fractional amount, to verify that the
  // fixed container size delta is (1) non-zero, and (2) fractional, and
  // (3) matches the movement of the browser controls.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollEnd();

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->inner_viewport_container_bounds_delta().y());
}

// In this test, the outer viewport is initially unscrollable. We test that a
// scroll initiated on the inner viewport, causing the browser controls to show
// and thus making the outer viewport scrollable, still scrolls the outer
// viewport.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsOuterViewportBecomesScrollable) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  outer_scroll->SetDrawsContent(true);
  host_impl_->active_tree()->PushPageScaleFromMainThread(2, 1, 2);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, 50), ui::ScrollInputType::kTouchscreen));

  // The entire scroll delta should have been used to hide the browser controls.
  // The viewport layers should be resized back to their full sizes.
  EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());
  EXPECT_EQ(100, inner_scroll->bounds().height());
  EXPECT_EQ(100, outer_scroll->bounds().height());

  // The inner viewport should be scrollable by 50px * page_scale.
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, 100), ui::ScrollInputType::kTouchscreen));
  EXPECT_EQ(50, CurrentScrollOffset(inner_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(gfx::PointF(), MaxScrollOffset(outer_scroll));

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, -50),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            outer_scroll->scroll_tree_index());

  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, -50), ui::ScrollInputType::kTouchscreen));

  // The entire scroll delta should have been used to show the browser controls.
  // The outer viewport should be resized to accommodate and scrolled to the
  // bottom of the document to keep the viewport in place.
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(50, inner_scroll->bounds().height());
  EXPECT_EQ(100, outer_scroll->bounds().height());
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(25, CurrentScrollOffset(inner_scroll).y());

  // Now when we continue scrolling, make sure the outer viewport gets scrolled
  // since it wasn't scrollable when the scroll began.
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, -20), ui::ScrollInputType::kTouchscreen));
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(15, CurrentScrollOffset(inner_scroll).y());

  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, -30), ui::ScrollInputType::kTouchscreen));
  EXPECT_EQ(25, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());

  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -50),
                  ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(0, CurrentScrollOffset(outer_scroll).y());
  EXPECT_EQ(0, CurrentScrollOffset(inner_scroll).y());
}

// Test that the fixed position container delta is appropriately adjusted
// by the browser controls showing/hiding and page scale doesn't affect it.
TEST_P(LayerTreeHostImplBrowserControlsTest, FixedContainerDelta) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  float page_scale = 1.5f;
  // Zoom in, since the fixed container is the outer viewport, the delta should
  // not be scaled.
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1, 2);

  gfx::Vector2dF top_controls_scroll_delta(0, 20);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  // Scroll down, the browser controls hiding should expand the viewport size so
  // the delta should be equal to the scroll distance.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_FLOAT_EQ(top_controls_height_ - top_controls_scroll_delta.y(),
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->outer_viewport_container_bounds_delta().y());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Scroll past the maximum extent. The delta shouldn't be greater than the
  // browser controls height.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), top_controls_scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, top_controls_height_),
                      property_trees->outer_viewport_container_bounds_delta());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Scroll in the direction to make the browser controls show.
  EXPECT_EQ(
      ScrollThread::kScrollOnImplThread,
      GetInputHandler()
          .ScrollBegin(BeginState(gfx::Point(), -top_controls_scroll_delta,
                                  ui::ScrollInputType::kTouchscreen)
                           .get(),
                       ui::ScrollInputType::kTouchscreen)
          .thread);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(-top_controls_scroll_delta);
  EXPECT_EQ(top_controls_scroll_delta.y(),
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR2DF_EQ(
      gfx::Vector2dF(0, top_controls_height_ - top_controls_scroll_delta.y()),
      property_trees->outer_viewport_container_bounds_delta());
  host_impl_->browser_controls_manager()->ScrollEnd();
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Push a browser controls ratio from the main thread that we didn't send as a
// delta and make sure that the ratio is clamped to the [0, 1] range.
TEST_P(LayerTreeHostImplBrowserControlsTest, BrowserControlsPushUnsentRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(1, 1);
  ASSERT_EQ(1.0f, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.5f, 0.5f);
  ASSERT_EQ(0.5f, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(0, 0);

  ASSERT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
}

// Test that if a scrollable sublayer doesn't consume the scroll,
// browser controls should hide when scrolling down.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollableSublayer) {
  gfx::Size sub_content_size(100, 400);
  gfx::Size sub_content_layer_size(100, 300);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 50), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();

  // Show browser controls
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  LayerImpl* outer_viewport_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* child = AddLayerInActiveTree();

  child->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  child->SetElementId(LayerIdToElementIdForTesting(child->id()));
  child->SetBounds(sub_content_size);
  child->SetDrawsContent(true);

  CopyProperties(outer_viewport_scroll_layer, child);
  CreateTransformNode(child);
  CreateScrollNode(child, sub_content_layer_size);
  UpdateDrawProperties(host_impl_->active_tree());

  // Scroll child to the limit.
  SetScrollOffsetDelta(child, gfx::Vector2dF(0, 100));

  // Scroll 25px to hide browser controls
  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Browser controls should be hidden
  EXPECT_EQ(scroll_delta.y(),
            top_controls_height_ -
                host_impl_->browser_controls_manager()->ContentTopOffset());
}

// Ensure setting the browser controls position explicitly using the setters on
// the TreeImpl correctly affects the browser controls manager and viewport
// bounds for the active tree.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       PositionBrowserControlsToActiveTreeExplicitly) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      30 / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  EXPECT_FLOAT_EQ(30,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-20,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-50,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->DidChangeBrowserControlsPosition();

  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
}

// Ensure setting the browser controls position explicitly using the setters on
// the TreeImpl correctly affects the browser controls manager and viewport
// bounds for the pending tree.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       PositionBrowserControlsToPendingTreeExplicitly) {
  EnsureSyncTree();
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      host_impl_->sync_tree(), layer_size_, layer_size_, layer_size_);

  // Changing SetCurrentBrowserControlsShownRatio is one way to cause the
  // pending tree to update it's viewport.
  host_impl_->SetCurrentBrowserControlsShownRatio(0, 0);
  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->sync_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->SetCurrentBrowserControlsShownRatio(0.5f, 0.5f);
  EXPECT_FLOAT_EQ(0.5f * top_controls_height_,
                  host_impl_->sync_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->SetCurrentBrowserControlsShownRatio(1, 1);
  EXPECT_FLOAT_EQ(0, host_impl_->sync_tree()
                         ->property_trees()
                         ->inner_viewport_container_bounds_delta()
                         .y());

  // Pushing changes from the main thread is the second way. These values are
  // added to the 1 set above.
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(-0.5f, -0.5f);
  EXPECT_FLOAT_EQ(0.5f * top_controls_height_,
                  host_impl_->sync_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(-1, -1);
  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->sync_tree()
                      ->property_trees()
                      ->inner_viewport_container_bounds_delta()
                      .y());
}

// Test that the top_controls delta and sent delta are appropriately
// applied on sync tree activation. The total browser controls offset shouldn't
// change after the activation.
TEST_P(LayerTreeHostImplBrowserControlsTest, ApplyDeltaOnTreeActivation) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      20 / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(
      15 / top_controls_height_, 15 / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PullDeltaForMainThread(
      /* next_bmf */ false);
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);
  EnsureSyncTree();
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(
      15 / top_controls_height_, 15 / top_controls_height_);

  host_impl_->DidChangeBrowserControlsPosition();
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->ActivateSyncTree();

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(CommitsToActiveTree() ? gfx::Vector2dF(0, 50) : gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_FLOAT_EQ(
      -15, host_impl_->active_tree()->top_controls_shown_ratio()->Delta() *
               top_controls_height_);
  EXPECT_FLOAT_EQ(
      15, host_impl_->active_tree()->top_controls_shown_ratio()->ActiveBase() *
              top_controls_height_);
}

// Test that changing the browser controls layout height is correctly applied to
// the inner viewport container bounds. That is, the browser controls layout
// height is the amount that the inner viewport container was shrunk outside
// the compositor to accommodate the browser controls.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsLayoutHeightChanged) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  EnsureSyncTree();
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(1, 1);
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {top_controls_height_, 0, 0, 0, false, true});

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(1);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0, 0);

  host_impl_->DidChangeBrowserControlsPosition();
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->ActivateSyncTree();

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());

  // The total bounds should remain unchanged since the bounds delta should
  // account for the difference between the layout height and the current
  // browser controls offset.
  EXPECT_EQ(CommitsToActiveTree() ? gfx::Vector2dF(0, 50) : gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1, 1);
  host_impl_->DidChangeBrowserControlsPosition();

  EXPECT_EQ(1, host_impl_->browser_controls_manager()->TopControlsShownRatio());
  EXPECT_EQ(50, host_impl_->browser_controls_manager()->TopControlsHeight());
  EXPECT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());
}

// Test that showing/hiding the browser controls when the viewport is fully
// scrolled doesn't incorrectly change the viewport offset due to clamping from
// changing viewport bounds.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsViewportOffsetClamping) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100));

  gfx::PointF viewport_offset = host_impl_->active_tree()->TotalScrollOffset();
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(), viewport_offset);

  // Hide the browser controls by 25px.
  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));

  // scrolling down at the max extents no longer hides the browser controls
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());

  // forcefully hide the browser controls by 25px
  host_impl_->browser_controls_manager()->ScrollBy(scroll_delta);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  // We should still be fully scrolled.
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());

  viewport_offset = host_impl_->active_tree()->TotalScrollOffset();

  // Bring the browser controls down by 25px.
  scroll_delta = gfx::Vector2dF(0, -25);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The viewport offset shouldn't have changed.
  EXPECT_EQ(viewport_offset, host_impl_->active_tree()->TotalScrollOffset());

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100));
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());
}

// Test that the browser controls coming in and out maintains the same aspect
// ratio between the inner and outer viewports.
TEST_P(LayerTreeHostImplBrowserControlsTest, BrowserControlsAspectRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 2);
  DrawFrame();

  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  gfx::Vector2dF scroll_delta(0, 25);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  // Browser controls were hidden by 25px so the inner viewport should have
  // expanded by that much.
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->inner_viewport_container_bounds_delta());

  // Outer viewport should match inner's aspect ratio. The bounds are ceiled.
  float aspect_ratio = 100.0f / 125.0f;
  auto expected = gfx::ToCeiledSize(gfx::SizeF(200, 200 / aspect_ratio));
  EXPECT_EQ(expected, InnerViewportScrollLayer()->bounds());
}

TEST_P(LayerTreeHostImplBrowserControlsTest, NoShrinkNotUserScrollable) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(100, 100), gfx::Size(100, 200));

  GetScrollNode(OuterViewportScrollLayer())->user_scrollable_vertical = false;
  DrawFrame();

  gfx::Vector2dF delta(0, 15);
  ui::ScrollInputType type = ui::ScrollInputType::kTouchscreen;
  auto& handler = GetInputHandler();

  handler.ScrollBegin(BeginState(gfx::Point(), delta, type).get(), type);
  handler.ScrollUpdate(UpdateState(gfx::Point(), delta, type));
  handler.ScrollEnd(false, std::nullopt);

  // Outer viewport has overflow, but is not user-scrollable. Make sure the
  // browser controls did not shrink when we tried to scroll.
  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());
}

// Test that scrolling the outer viewport affects the browser controls.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollOuterViewport) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());

  // Send a gesture scroll that will scroll the outer viewport, make sure the
  // browser controls get scrolled.
  gfx::Vector2dF scroll_delta(0, 15);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));

  EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  scroll_delta = gfx::Vector2dF(0, 50);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Position the viewports such that the inner viewport will be scrolled.
  gfx::Vector2dF inner_viewport_offset(0, 25);
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2dF());
  SetScrollOffsetDelta(InnerViewportScrollLayer(), inner_viewport_offset);

  scroll_delta = gfx::Vector2dF(0, -65);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(
      inner_viewport_offset.y() + (scroll_delta.y() + top_controls_height_),
      ScrollDelta(InnerViewportScrollLayer()).y());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplBrowserControlsTest,
       ScrollNonScrollableRootWithBrowserControls) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0, 50));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_EQ(gfx::Vector2dF(0, 50),
            property_trees->inner_viewport_container_bounds_delta());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, -25),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  float scroll_increment_y = -25;
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0, scroll_increment_y));
  EXPECT_FLOAT_EQ(-scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(gfx::Vector2dF(0, 25),
            property_trees->inner_viewport_container_bounds_delta());

  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0, scroll_increment_y));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_FLOAT_EQ(-2 * scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(gfx::Vector2dF(),
            property_trees->inner_viewport_container_bounds_delta());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Verify the layer is once-again non-scrollable.
  EXPECT_EQ(gfx::PointF(), MaxScrollOffset(InnerViewportScrollLayer()));

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
}

// Tests that activating a pending tree while there's a bounds_delta on the
// viewport layers from browser controls doesn't cause a scroll jump. This bug
// was occurring because the UpdateViewportContainerSizes was being called
// before the property trees were updated with the bounds_delta.
// crbug.com/597266.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       ViewportBoundsDeltaOnTreeActivation) {
  // This test needs to create both the pending tree and the active tree.
  if (CommitsToActiveTree()) {
    GTEST_SKIP();
  }

  const gfx::Size inner_viewport_size(1000, 1000);
  const gfx::Size outer_viewport_size(1000, 1000);
  const gfx::Size content_size(2000, 2000);

  // Initialization
  {
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        inner_viewport_size, outer_viewport_size, content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 1);

    // Start off with the browser controls hidden on both main and impl.
    host_impl_->active_tree()->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, false});
    host_impl_->active_tree()->PushBrowserControlsFromMainThread(0, 0);

    CreatePendingTree();
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->pending_tree(), inner_viewport_size, outer_viewport_size,
        content_size);
    host_impl_->pending_tree()->SetBrowserControlsParams(
        {top_controls_height_, 0, 0, 0, false, false});
    UpdateDrawProperties(host_impl_->pending_tree());

    // Fully scroll the viewport.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(75, 75), gfx::Vector2dF(0, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2d(0, 2000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  ASSERT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  ASSERT_EQ(1000, MaxScrollOffset(outer_scroll).y());
  ASSERT_EQ(1000, CurrentScrollOffset(outer_scroll).y());

  // Kick off an animation to show the browser controls.
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, true,
      std::nullopt);
  base::TimeTicks start_time = base::TimeTicks::Now();
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // The first animation frame will not produce any delta, it will establish
  // the animation.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);
    float delta =
        host_impl_->active_tree()->top_controls_shown_ratio()->Delta();
    ASSERT_EQ(delta, 0);
  }

  // Pump an animation frame to put some delta in the browser controls.
  {
    begin_frame_args.frame_time = start_time + base::Milliseconds(50);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Pull the browser controls delta and get it back to the pending tree so that
  // when we go to activate the pending tree we cause a change to browser
  // controls.
  {
    float delta =
        host_impl_->active_tree()->top_controls_shown_ratio()->Delta();
    ASSERT_GT(delta, 0);
    ASSERT_LT(delta, 1);
    host_impl_->active_tree()
        ->top_controls_shown_ratio()
        ->PullDeltaForMainThread(/* next_bmf */ false);
    host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
        delta);
  }

  // 200 is the kShowHideMaxDurationMs value from browser_controls_manager.cc so
  // the browser controls should be fully animated in this frame.
  {
    begin_frame_args.frame_time = start_time + base::Milliseconds(200);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    ASSERT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
    ASSERT_EQ(1050, MaxScrollOffset(outer_scroll).y());
    // NEAR because clip layer bounds are truncated in MaxScrollOffset so we
    // lose some precision in the intermediate animation steps.
    ASSERT_NEAR(1050, CurrentScrollOffset(outer_scroll).y(), 1);
  }

  // Activate the pending tree which should have the same scroll value as the
  // active tree.
  {
    host_impl_->pending_tree()
        ->property_trees()
        ->scroll_tree_mutable()
        .SetScrollOffsetDeltaForTesting(outer_scroll->element_id(),
                                        gfx::Vector2dF(0, 1050));
    host_impl_->ActivateSyncTree();

    // Make sure we don't accidentally clamp the outer offset based on a bounds
    // delta that hasn't yet been updated.
    EXPECT_NEAR(1050, CurrentScrollOffset(outer_scroll).y(), 1);
  }
}

// Tests that when we set a child scroller (e.g. a scrolling div) as the outer
// viewport, scrolling it controls the browser controls.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       ReplacedOuterViewportScrollsBrowserControls) {
  const gfx::Size scroll_content_size(400, 400);
  const gfx::Size root_layer_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      viewport_size, viewport_size, root_layer_size);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  // Initialization: Add a child scrolling layer to the outer scroll layer and
  // set its scroll layer as the outer viewport. This simulates setting a
  // scrolling element as the root scroller on the page.
  LayerImpl* clip_layer = AddLayerInActiveTree();
  clip_layer->SetBounds(root_layer_size);
  CopyProperties(outer_scroll, clip_layer);
  CreateClipNode(clip_layer);
  LayerImpl* scroll_layer =
      AddScrollableLayer(clip_layer, root_layer_size, scroll_content_size);
  GetScrollNode(scroll_layer)->scrolls_outer_viewport = true;

  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_clip = clip_layer->clip_tree_index();
  viewport_property_ids.outer_scroll = scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);
  DrawFrame();

  ASSERT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());

  // Scrolling should scroll the child content and the browser controls. The
  // original outer viewport should get no scroll.
  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll));
    EXPECT_POINTF_EQ(gfx::PointF(100, 50), CurrentScrollOffset(scroll_layer));
    EXPECT_EQ(0, layer_tree_impl->CurrentTopControlsShownRatio());
  }
}

// Check if LayerTreeImpl::GetActivelyScrollingType() returns kPrecise even
// when BrowserControl is consuming ScrollUpdate.
TEST_P(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsActivelyScrollingType) {
  // This test creates layer on the active tree with a raster source, which
  // doesn't work if we commit to the pending tree.
  if (!CommitsToActiveTree()) {
    GTEST_SKIP();
  }

  gfx::Size inner_size = gfx::Size(100, 100);
  gfx::Size outer_size = gfx::Size(100, 100);
  gfx::Size content_size = gfx::Size(100, 200);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(inner_size, outer_size,
                                                        content_size);

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a content layer beneath the outer viewport scroll layer.
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(content_size));

  auto* picture_layer =
      AddLayer<FakePictureLayerImpl>(host_impl_->active_tree(), raster_source);
  CopyProperties(OuterViewportScrollLayer(), picture_layer);
  picture_layer->SetBounds(content_size);
  picture_layer->SetDrawsContent(true);
  picture_layer->SetNeedsPushProperties();
  active_tree->PushPageScaleFromMainThread(1.0f, 1.0f, 2.0f);
  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  // shownratio == 1
  EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(host_impl_->active_tree()->GetActivelyScrollingType(),
            ActivelyScrollingType::kNone);

  // 0 < shownratio <1
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, 25), ui::ScrollInputType::kTouchscreen));
  EXPECT_GT(host_impl_->active_tree()->CurrentTopControlsShownRatio(), 0);
  EXPECT_LT(host_impl_->active_tree()->CurrentTopControlsShownRatio(), 1);
  EXPECT_EQ(host_impl_->active_tree()->GetActivelyScrollingType(),
            ActivelyScrollingType::kPrecise);

  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, 30), ui::ScrollInputType::kTouchscreen));
  // now shownratio == 0
  EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(host_impl_->active_tree()->GetActivelyScrollingType(),
            ActivelyScrollingType::kPrecise);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  // scroll end, shownratio == 0
  EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(host_impl_->active_tree()->GetActivelyScrollingType(),
            ActivelyScrollingType::kNone);
}

INSTANTIATE_CLIENT_MODE_TREE_TEST_P(LayerTreeHostImplBrowserControlsTest);

// TODO(crbug.com/458776836): Review memory limit related cc_unittests for
// TreesInViz Service mode
TEST_P(LayerTreeHostImplWithImplicitLimitsTest, ImplicitMemoryLimits) {
  // Set up a memory policy and percentages which could cause
  // 32-bit integer overflows.
  ManagedMemoryPolicy mem_policy(300 * 1024 * 1024);  // 300MB

  // Verify implicit limits are calculated correctly with no overflows
  host_impl_->SetMemoryPolicy(mem_policy);
  EXPECT_EQ(host_impl_->global_tile_state().hard_memory_limit_in_bytes,
            300u * 1024u * 1024u);
  EXPECT_EQ(host_impl_->global_tile_state().soft_memory_limit_in_bytes,
            150u * 1024u * 1024u);
}

INSTANTIATE_CLIENT_MODE_TREE_TEST_P(LayerTreeHostImplWithImplicitLimitsTest);
}  // namespace
}  // namespace cc
