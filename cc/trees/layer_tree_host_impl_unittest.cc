// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

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
#include "cc/trees/client_layer_tree_host_impl.h"
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

viz::SurfaceId MakeSurfaceId(const viz::FrameSinkId& frame_sink_id,
                             uint32_t parent_id) {
  return viz::SurfaceId(
      frame_sink_id,
      viz::LocalSurfaceId(parent_id,
                          base::UnguessableToken::CreateForTesting(0, 1u)));
}

void ClearMainThreadDeltasForTesting(LayerTreeHostImpl* host) {
  host->active_tree()->ApplySentScrollAndScaleDeltasFromAbortedCommit(
      /* next_bmf */ false, /* main_frame_applied_deltas */ false);
}

MATCHER(UniquePtrMatches, negation ? "do not match" : "match") {
  return std::get<0>(arg).get() == std::get<1>(arg);
}

}  // namespace

class CommitToActiveTreeLayerTreeHostImplTest
    : public LayerTreeHostImplTestBase {
 public:
  LayerTreeSettings DefaultSettings() override {
    LayerTreeSettings settings = LayerTreeHostImplTestBase::DefaultSettings();
    settings.commit_to_active_tree = true;
    return settings;
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplTest);

// Test fixture that runs in all tree modes except TreesInViz Client mode,
// that, as an exception from other modes, does not produce complete
// CompositorFrames.
class CompositorFrameProducingLayerTreeHostImplTest
    : public LayerTreeHostImplTest {};

INSTANTIATE_COMPOSITOR_FRAME_PRODUCING_TREE_TEST_P(
    CompositorFrameProducingLayerTreeHostImplTest);

class AnimationsLayerTreeHostImplTest : public LayerTreeHostImplTest {};

INSTANTIATE_ANIMATIONS_TREE_TEST_P(AnimationsLayerTreeHostImplTest);

class OccludedSurfaceThrottlingLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  LayerTreeSettings DefaultSettings() override {
    LayerTreeSettings settings = LayerTreeHostImplTest::DefaultSettings();
    settings.enable_compositing_based_throttling = true;
    return settings;
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(
    OccludedSurfaceThrottlingLayerTreeHostImplTest);

// A test fixture for new animation timelines tests.
// TODO(487287578): Remove this once we've stabilized our use of
// ScopedFeatureList on android-x86-rel.
class LayerTreeHostImplTimelinesTest : public LayerTreeHostImplTest {};

INSTANTIATE_ANIMATIONS_TREE_TEST_P(LayerTreeHostImplTimelinesTest);

// TODO(468470705): Switch this to use LayerTreeHostImplTest.
class FluentOverlayScrollbarLayerTreeHostImplTest
    : public LayerTreeHostImplTestBase {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.enable_fluent_overlay_scrollbar = true;
    settings.enable_fluent_scrollbar = true;
    settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
    settings.scrollbar_fade_delay = base::Milliseconds(500);
    settings.scrollbar_fade_duration = base::Milliseconds(300);
    settings.idle_thickness_scale = 0.4f;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
  }

  PaintedScrollbarLayerImpl* CreateAndRegisterPaintedScrollbarLayer() {
    // Set up the viewport.
    const gfx::Size viewport_size = gfx::Size(360, 600);
    const gfx::Size content_size = gfx::Size(345, 3800);
    SetupViewportLayersOuterScrolls(viewport_size, content_size);
    LayerImpl* scroll_layer = OuterViewportScrollLayer();

    // Create the scrollbar layer object and register it.
    LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
    auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
        layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
    // SetupScrollbarLayerCommon will register the scrollbar, which sets the
    // layer's opacity to 0. An effect node for the scrollbar layer object needs
    // to be registered in the EffectTree before this happens.
    auto& effect_node = CreateEffectNode(
        scrollbar, layer_tree_impl->root_layer()->effect_tree_index());
    SetupScrollbarLayerCommon(scroll_layer, scrollbar);
    // SetupScrollbarLayerCommon calls CopyProperties which overrides the effect
    // tree node registered to the scrollbar layer. We need to reset it to the
    // one we registered above.
    scrollbar->SetEffectTreeIndex(effect_node.id);
    scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

    // Set up scrollbar layer dimensions.
    scrollbar->SetBounds(gfx::Size(15, 600));
    scrollbar->SetThumbThickness(9);
    scrollbar->SetMinimumThumbLength(50);
    scrollbar->SetTrackRect(gfx::Rect(0, 12, 15, 575));
    scrollbar->SetForwardButtonRect(gfx::Rect(0, 584, 15, 16));
    scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

    // Set up track resource id.
    UIResourceId ui_resource_id = 1;
    UIResourceBitmap bitmap(gfx::Size(1, 1), true);
    host_impl_->CreateUIResource(ui_resource_id, bitmap);
    scrollbar->set_track_and_buttons_ui_resource_id(ui_resource_id);
    scrollbar->SetThumbColor(SkColors::kRed);
    scrollbar->set_is_web_test(true);
    UpdateDrawProperties(host_impl_->active_tree());

    return scrollbar;
  }
};

class FluentOverlayScrollbarOpacityLayerTreeHostImplTest
    : public FluentOverlayScrollbarLayerTreeHostImplTest,
      public testing::WithParamInterface<int> {
 public:
  constexpr static int kParamSteps = 10;

  void VerifyCorrectOpacityForThickness(PaintedScrollbarLayerImpl* scrollbar,
                                        float thickness,
                                        float expected_opacity) {
    scrollbar->SetThumbThicknessScaleFactor(thickness);
    auto render_pass = viz::CompositorRenderPass::Create();
    AppendQuadsData append_quads_data;
    scrollbar->AppendQuads(AppendQuadsContext{DRAW_MODE_HARDWARE, {}, false},
                           render_pass.get(), &append_quads_data);
    if (expected_opacity == 0.f) {
      // If the opacity of the track is expected to be zero, the layer code
      // makes an early return and doesn't append the track's quads.
      // Verify that there is only one quad (the thumb's) appended to the render
      // pass.
      EXPECT_EQ(render_pass->quad_list.size(), 1u);
    } else {
      EXPECT_EQ(render_pass->quad_list.size(), 2u);
      EXPECT_FLOAT_EQ(expected_opacity,
                      render_pass->shared_quad_state_list.back()->opacity);
    }
  }
};

class TestInputHandlerClient : public InputHandlerClient {
 public:
  TestInputHandlerClient()
      : page_scale_factor_(0),
        min_page_scale_factor_(-1),
        max_page_scale_factor_(-1) {}
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
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
};

TEST_P(LayerTreeHostImplTest, LocalAndExternalPinchState) {
  // PinchGestureBegin/End update pinch_gesture_active() properly.
  EXPECT_FALSE(GetInputHandler().pinch_gesture_active());
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kTouchscreen);
  EXPECT_TRUE(GetInputHandler().pinch_gesture_active());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  EXPECT_FALSE(GetInputHandler().pinch_gesture_active());

  // set_external_pinch_gesture_active updates pinch_gesture_active() properly.
  GetInputHandler().set_external_pinch_gesture_active(true);
  EXPECT_TRUE(GetInputHandler().pinch_gesture_active());
  GetInputHandler().set_external_pinch_gesture_active(false);
  EXPECT_FALSE(GetInputHandler().pinch_gesture_active());

  // Clearing external_pinch_gesture_active doesn't affect
  // pinch_gesture_active() if it was set by PinchGestureBegin().
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kTouchscreen);
  EXPECT_TRUE(GetInputHandler().pinch_gesture_active());
  GetInputHandler().set_external_pinch_gesture_active(false);
  EXPECT_TRUE(GetInputHandler().pinch_gesture_active());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  EXPECT_FALSE(GetInputHandler().pinch_gesture_active());
}

TEST_P(LayerTreeHostImplTest, NotifyIfCanDrawChanged) {
  // Note: It is not possible to disable the renderer once it has been set,
  // so we do not need to test that disabling the renderer notifies us
  // that can_draw changed.
  EXPECT_FALSE(host_impl_->CanDraw());
  on_can_draw_state_changed_called_ = false;

  // Set up the root layer, which allows us to draw.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the root layer to make sure it toggles can_draw
  ClearLayersAndPropertyTrees(host_impl_->active_tree());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the device viewport size to make sure it toggles can_draw.
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;
}

// TODO(crbug.com/458781783): Review resourceless draw tests that fail in
// TreesInViz Client mode. These tests use FakeLayerTreeFrameSink properties
// that are not set this mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       ResourcelessDrawWithEmptyViewport) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  EXPECT_TRUE(host_impl_->CanDraw());
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect());
  EXPECT_FALSE(host_impl_->CanDraw());

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  EXPECT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 0u);
  gfx::Transform identity;
  gfx::Rect viewport(100, 100);
  const bool resourceless_software_draw = true;
  host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
  ASSERT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 1u);
}

TEST_P(LayerTreeHostImplTest, ScrollDeltaNoLayers) {
  host_impl_->active_tree()->SetRootLayerForTesting(nullptr);

  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  ASSERT_EQ(commit_data->scrolls.size(), 0u);
}

TEST_P(LayerTreeHostImplTest, ScrollDeltaTreeButNoChanges) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  {
    LayerImpl* child1 = AddLayerInActiveTree();
    CopyProperties(root, child1);
    LayerImpl* child2 = AddLayerInActiveTree();
    CopyProperties(root, child2);
    LayerImpl* grand_child1 = AddLayerInActiveTree();
    CopyProperties(child2, grand_child1);
    LayerImpl* grand_child2 = AddLayerInActiveTree();
    CopyProperties(child2, grand_child2);
    LayerImpl* great_grand_child = AddLayerInActiveTree();
    CopyProperties(grand_child1, great_grand_child);
  }

  ExpectClearedScrollDeltasRecursive(root);

  std::unique_ptr<CompositorCommitData> commit_data;
  std::unique_ptr<AnimationHost> main_thread_animation_host(
      AnimationHost::CreateForTesting(ThreadInstance::kMain));

  // Null mutator_host implies non-pipelined main frame
  commit_data = host_impl_->ProcessCompositorDeltas(
      /* main_thread_mutator_host */ nullptr);
  ASSERT_EQ(commit_data->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);

  // Non-null mutator_host implies pipelined main frame
  commit_data =
      host_impl_->ProcessCompositorDeltas(main_thread_animation_host.get());
  ASSERT_EQ(commit_data->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest, ScrollDeltaRepeatedScrolls) {
  gfx::PointF scroll_offset(20, 30);
  auto* root = SetupDefaultRootLayer(gfx::Size(110, 110));
  root->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CreateScrollNode(root, gfx::Size(10, 10));
  root->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(root->element_id(), scroll_offset);
  UpdateDrawProperties(host_impl_->active_tree());

  gfx::Vector2dF scroll_delta(11, -15);
  std::unique_ptr<CompositorCommitData> commit_data1;
  std::unique_ptr<AnimationHost> main_thread_animation_host(
      AnimationHost::CreateForTesting(ThreadInstance::kMain));

  root->ScrollBy(scroll_delta);
  // Null mutator_host implies non-pipelined main frame
  commit_data1 = host_impl_->ProcessCompositorDeltas(
      /* main_thread_mutator_host */ nullptr);
  ASSERT_EQ(commit_data1->scrolls.size(), 1u);
  EXPECT_TRUE(
      ScrollInfoContains(*commit_data1, root->element_id(), scroll_delta));

  std::unique_ptr<CompositorCommitData> commit_data2;
  gfx::Vector2dF scroll_delta2(-5, 27);
  root->ScrollBy(scroll_delta2);
  // Non-null mutator_host implies pipelined main frame
  commit_data2 =
      host_impl_->ProcessCompositorDeltas(main_thread_animation_host.get());
  ASSERT_EQ(commit_data2->scrolls.size(), 1u);
  EXPECT_TRUE(
      ScrollInfoContains(*commit_data2, root->element_id(), scroll_delta2));

  // Simulate first commit by pushing base scroll offsets to pending tree
  PushScrollOffsetsToPendingTree(
      {{root->element_id(), gfx::PointAtOffsetFromOrigin(scroll_delta)}});
  ClearNonScrollSyncTreeDeltasForTesting();
  EXPECT_EQ(host_impl_->sync_tree()
                ->property_trees()
                ->scroll_tree()
                .GetScrollOffsetDeltaForTesting(root->element_id()),
            scroll_delta2);

  // Simulate second commit by pushing base scroll offsets to pending tree
  PushScrollOffsetsToPendingTree(
      {{root->element_id(), gfx::PointAtOffsetFromOrigin(scroll_delta2)}});
  EXPECT_EQ(host_impl_->sync_tree()
                ->property_trees()
                ->scroll_tree()
                .GetScrollOffsetDeltaForTesting(root->element_id()),
            gfx::Vector2dF(0, 0));
}

// This test verifies that we drop a scroll (and don't crash) if a scroll is
// received before the root layer has been attached. https://crbug.com/895817.
TEST_P(LayerTreeHostImplTest, ScrollBeforeRootLayerAttached) {
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);

  status = GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
}

// Tests that receiving ScrollUpdate and ScrollEnd calls that don't have a
// matching ScrollBegin are just dropped and are a no-op. This can happen due
// to pre-commit input deferral which causes some input events to be dropped
// before the first commit in a renderer has occurred. See the flag
// kAllowPreCommitInput and how it's used.
TEST_P(LayerTreeHostImplTest, ScrollUpdateAndEndNoOpWithoutBegin) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(1000, 1000));

  // Simulate receiving a gesture mid-stream so that the Begin wasn't ever
  // processed. This shouldn't cause any state change but we should crash or
  // DCHECK.
  {
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Ensure a new gesture is now able to correctly scroll.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());

    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
    EXPECT_POINTF_EQ(gfx::PointF(0, 10),
                     CurrentScrollOffset(OuterViewportScrollLayer()));

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// Test that specifying a scroller to ScrollBegin (i.e. avoid hit testing)
// returns the correct status if the scroller cannot be scrolled on the
// compositor thread.
TEST_P(LayerTreeHostImplTest, TargetMainThreadScroller) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(1000, 1000));

  ScrollStateData scroll_state_data;
  scroll_state_data.set_current_native_scrolling_element(
      host_impl_->OuterViewportScrollNode()->element_id);
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));

  // Try on a node that should scroll on the compositor. Confirm it works.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        scroll_state.get(), ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(host_impl_->OuterViewportScrollNode(),
              host_impl_->CurrentlyScrollingNode());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Now add a main-thread repaint reason. ScrollBegin should still succeed.
  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kPreferNonCompositedScrolling;

  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        scroll_state.get(), ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    EXPECT_EQ(
        MainThreadScrollingReason::kPreferNonCompositedScrolling,
        host_impl_->CurrentlyScrollingNode()->main_thread_repaint_reasons);
  }
}

TEST_P(LayerTreeHostImplTest, ScrollRootCallsCommitAndRedraw) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);

  EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);
}

// Ensure correct semantics for the GetActivelyScrollingType method. This
// method is used to determine scheduler policy so it wants to report true only
// when real scrolling is occurring (i.e. the compositor is consuming scroll
// delta, the page isn't handling the events itself).
TEST_P(LayerTreeHostImplTest, ActivelyScrollingOnlyAfterScrollMovement) {
  SetupViewportLayersOuterScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  // Ensure a touch scroll reports true but only after some delta has been
  // consumed.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kNone);

    // There is no extent upwards so the scroll won't consume any delta.
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                    ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kNone);

    // This should scroll so ensure the bit flips to true.
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kPrecise);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kNone);
  }

  ASSERT_EQ(10, CurrentScrollOffset(OuterViewportScrollLayer()).y());

  // Ensure an animated wheel scroll only causes the bit to flip when enabling
  // smoothness mode (i.e. the value of GetParam().animate);
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kNone);

    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(0, 10)));
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kAnimated);

    base::TimeTicks cur_time = base::TimeTicks() + base::Milliseconds(100);
    viz::BeginFrameArgs begin_frame_args =
        viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

#define ANIMATE(time_ms)                            \
  cur_time += base::Milliseconds(time_ms);          \
  begin_frame_args.frame_time = (cur_time);         \
  begin_frame_args.frame_id.sequence_number++;      \
  host_impl_->WillBeginImplFrame(begin_frame_args); \
  host_impl_->Animate();                            \
  host_impl_->UpdateAnimationState(true);           \
  host_impl_->DidFinishImplFrame(begin_frame_args);

    // The animation is setup in the first frame so tick at least twice to
    // actually animate it.
    ANIMATE(0);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kAnimated);
    ANIMATE(200);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kAnimated);
    ANIMATE(1000);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kAnimated);

#undef ANIMATE

    ASSERT_EQ(20, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_EQ(host_impl_->GetActivelyScrollingType(),
              ActivelyScrollingType::kNone);
  }
}

TEST_P(LayerTreeHostImplTest, ScrollWithoutRootLayer) {
  // We should not crash when trying to scroll an empty layer tree.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
}

TEST_P(LayerTreeHostImplTest, ScrollWithoutRenderer) {
  auto compositor_context = viz::TestContextProvider::CreateRaster();
  compositor_context->UnboundTestRasterInterface()->LoseContextCHROMIUM(
      GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);

  // Initialization will fail.
  EXPECT_FALSE(CreateHostImpl(
      DefaultSettings(),
      FakeLayerTreeFrameSink::Create3d(std::move(compositor_context))));

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  // We should not crash when trying to scroll after the renderer initialization
  // fails.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
}

TEST_P(LayerTreeHostImplTest, ReplaceTreeWhileScrolling) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  // We should not crash if the tree is replaced while we are scrolling.
  gfx::Vector2dF scroll_delta(0, 10);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  ClearLayersAndPropertyTrees(host_impl_->active_tree());

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                 scroll_delta));
}

TEST_P(LayerTreeHostImplTest, ScrollBlocksOnWheelEventHandlers) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll = InnerViewportScrollLayer();
  scroll->SetWheelEventHandlerRegion(Region(gfx::Rect(20, 20)));
  DrawFrame();

  // Wheel handlers determine whether mouse events block scroll.
  host_impl_->active_tree()->set_event_listener_properties(
      EventListenerClass::kMouseWheel, EventListenerProperties::kBlocking);
  EXPECT_EQ(EventListenerProperties::kBlocking,
            GetInputHandler().GetEventListenerProperties(
                EventListenerClass::kMouseWheel));

  // LTHI should know the wheel event handler region and only block mouse events
  // in that region.
  EXPECT_TRUE(
      GetInputHandler().HasBlockingWheelEventHandlerAt(gfx::Point(10, 10)));
  EXPECT_FALSE(
      GetInputHandler().HasBlockingWheelEventHandlerAt(gfx::Point(30, 30)));

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, ScrollBlocksOnTouchEventHandlers) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* root = root_layer();
  LayerImpl* scroll = InnerViewportScrollLayer();
  LayerImpl* child = AddLayerInActiveTree();
  child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(50, 50));
  CopyProperties(scroll, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(0, 20));

  // Touch handler regions determine whether touch events block scroll.
  TouchAction touch_action;
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kPanLeft, gfx::Rect(0, 0, 100, 100));
  touch_action_region.Union(TouchAction::kPanRight,
                            gfx::Rect(25, 25, 100, 100));
  root->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::kHandler,
            GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
                gfx::Rect(gfx::Point(10, 10), gfx::Size()), &touch_action));
  EXPECT_EQ(TouchAction::kPanLeft, touch_action);

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::kHandler,
            GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
                gfx::Rect(gfx::Point(10, 30), gfx::Size()), &touch_action));
  root->SetTouchActionRegion(TouchActionRegion());
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::kNoHandler,
            GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
                gfx::Rect(gfx::Point(10, 30), gfx::Size()), &touch_action));
  EXPECT_EQ(TouchAction::kAuto, touch_action);
  touch_action_region = TouchActionRegion();
  touch_action_region.Union(TouchAction::kPanX, gfx::Rect(0, 0, 50, 50));
  child->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::kHandler,
            GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
                gfx::Rect(gfx::Point(10, 30), gfx::Size()), &touch_action));
  EXPECT_EQ(TouchAction::kPanX, touch_action);
}

TEST_P(LayerTreeHostImplTest, ShouldScrollOnMainThread) {
  SetupViewportLayersOuterScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            host_impl_->CurrentlyScrollingNode()->main_thread_repaint_reasons);

  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            host_impl_->CurrentlyScrollingNode()->main_thread_repaint_reasons);
}

TEST_P(LayerTreeHostImplTest, ScrollWithOverlappingNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(false);
}

TEST_P(LayerTreeHostImplTest,
       ScrollWithOverlappingTransparentNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(true);
}

TEST_P(LayerTreeHostImplTest, ScrolledOverlappingDrawnScrollbarLayer) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size content_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(345, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  SetupViewportLayersNoScrolls(content_size);
  LayerImpl* scroll = AddScrollableLayer(OuterViewportScrollLayer(),
                                         content_size, scroll_content_size);

  auto* drawn_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayer(scroll, drawn_scrollbar);
  drawn_scrollbar->SetBounds(scrollbar_size);
  drawn_scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  // squash1 has mixed hit test opaqueness and the same scroll tree index as
  // the scroller.
  LayerImpl* squash1 = AddLayerInActiveTree();
  squash1->SetBounds(gfx::Size(140, 200));
  squash1->SetDrawsContent(true);
  squash1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(scroll, squash1);
  squash1->SetOffsetToTransformParent(gfx::Vector2dF(220, 0));

  // squash2 has mixed hit test opaqueness and escapes the scroll state of the
  // scroller.
  LayerImpl* squash2 = AddLayerInActiveTree();
  squash2->SetBounds(gfx::Size(140, 200));
  squash2->SetDrawsContent(true);
  squash2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(scroll, squash2);
  squash2->SetScrollTreeIndex(OuterViewportScrollLayer()->scroll_tree_index());
  squash2->SetOffsetToTransformParent(gfx::Vector2dF(220, 200));

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();

  // The point hits squash1 and also scrollbar layer. Because both layers will
  // scroll the same scroll node, we scroll on the impl thread.
  auto status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 150), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The point hits squash2 and also scrollbar layer. Because they will scroll
  // different scroll nodes, the scroll is not reliable.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 350), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The point hits the drawn scrollbar layer completely and should scroll on
  // the impl thread.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 500), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

gfx::PresentationFeedback ExampleFeedback() {
  auto feedback = gfx::PresentationFeedback(
      base::TimeTicks() + base::Milliseconds(42), base::Microseconds(18),
      gfx::PresentationFeedback::Flags::kVSync |
          gfx::PresentationFeedback::Flags::kHWCompletion);
#if BUILDFLAG(IS_MAC)
  feedback.ca_layer_error_code = gfx::kCALayerFailedQuadBlendMode;
#endif
  return feedback;
}

class LayerTreeHostImplTestInvokePresentationCallbacks
    : public LayerTreeHostImplTest {
 public:
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::PendingCallbacks activated,
      const viz::FrameTimingDetails& details) override {
    host_impl_->NotifyDidPresentCompositorFrameOnImplThread(
        frame_token, std::move(activated.compositor_successful_callbacks),
        details);
    for (auto& callback : activated.main_callbacks)
      std::move(callback).Run(details.presentation_feedback);
    for (auto& callback : activated.main_successful_callbacks)
      std::move(callback).Run(details);
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(
    LayerTreeHostImplTestInvokePresentationCallbacks);

// Tests that, when the LayerTreeHostImpl receives presentation feedback, the
// feedback gets routed to a properly registered callback.
TEST_P(LayerTreeHostImplTestInvokePresentationCallbacks,
       PresentationFeedbackCallbacksFire) {
  bool compositor_successful_callback_fired = false;
  bool main_callback_fired = false;
  bool main_successful_callback_fired = false;
  base::TimeTicks compositor_successful_callback_presentation_timestamp;
  gfx::PresentationFeedback main_callback_presentation_feedback;
  base::TimeTicks main_successful_callback_presentation_timestamp;

  constexpr uint32_t frame_token_1 = 1;
  constexpr uint32_t frame_token_2 = 2;
  constexpr uint32_t frame_token_3 = 3;

  // Register a compositor-thread successful presentation callback to run when
  // the frame for `frame_token_1` gets presented.
  host_impl_
      ->RegisterCompositorThreadSuccessfulPresentationTimeCallbackForTesting(
          frame_token_1,
          base::BindLambdaForTesting(
              [&](base::TimeTicks presentation_timestamp) {
                DCHECK(compositor_successful_callback_presentation_timestamp
                           .is_null());
                DCHECK(!presentation_timestamp.is_null());
                compositor_successful_callback_fired = true;
                compositor_successful_callback_presentation_timestamp =
                    presentation_timestamp;
              }));

  // Register a main-thread presentation callback to run when the presentation
  // feedback for `frame_token_2` is received.
  host_impl_->RegisterMainThreadPresentationTimeCallbackForTesting(
      frame_token_2, base::BindLambdaForTesting(
                         [&](const gfx::PresentationFeedback& feedback) {
                           main_callback_fired = true;
                           main_callback_presentation_feedback = feedback;
                         }));

  // Register a main-thread successful presentation callback to run when the
  // frame for `frame_token_2` gets presented.
  host_impl_->RegisterMainThreadSuccessfulPresentationTimeCallbackForTesting(
      frame_token_2,
      base::BindLambdaForTesting([&](const viz::FrameTimingDetails& details) {
        base::TimeTicks presentation_timestamp =
            details.presentation_feedback.timestamp;
        DCHECK(main_successful_callback_presentation_timestamp.is_null());
        DCHECK(!presentation_timestamp.is_null());
        main_successful_callback_fired = true;
        main_successful_callback_presentation_timestamp =
            presentation_timestamp;
      }));

  // Present frame for `frame_token_1` successfully.
  viz::FrameTimingDetails mock_details;
  mock_details.presentation_feedback = ExampleFeedback();
  host_impl_->DidPresentCompositorFrame(frame_token_1, mock_details);

  // Only callbacks registered for `frame_token_1` should be called.
  EXPECT_TRUE(compositor_successful_callback_fired);
  EXPECT_FALSE(main_callback_fired);
  EXPECT_FALSE(main_successful_callback_fired);
  EXPECT_EQ(compositor_successful_callback_presentation_timestamp,
            mock_details.presentation_feedback.timestamp);
  EXPECT_EQ(main_callback_presentation_feedback, gfx::PresentationFeedback());
  EXPECT_TRUE(main_successful_callback_presentation_timestamp.is_null());

  // Fail presentation of frame for `frame_token_2`.
  mock_details.presentation_feedback = gfx::PresentationFeedback::Failure();
  host_impl_->DidPresentCompositorFrame(frame_token_2, mock_details);

  // Only callbacks that are allowed to run on failed presentations should be
  // called.
  EXPECT_TRUE(main_callback_fired);
  EXPECT_FALSE(main_successful_callback_fired);
  EXPECT_EQ(main_callback_presentation_feedback,
            mock_details.presentation_feedback);
  EXPECT_TRUE(main_successful_callback_presentation_timestamp.is_null());

  // Present frame for `frame_token_2` successfully.
  mock_details.presentation_feedback = ExampleFeedback();
  host_impl_->DidPresentCompositorFrame(frame_token_3, mock_details);

  // Now the callbacks for successful presentation should be called, too.
  EXPECT_TRUE(main_successful_callback_fired);
  EXPECT_EQ(main_successful_callback_presentation_timestamp,
            mock_details.presentation_feedback.timestamp);
}

TEST_P(LayerTreeHostImplTest, MainThreadScrollHitTestRegionBasic) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetMainThreadScrollHitTestRegion(gfx::Rect(0, 0, 50, 50));

  DrawFrame();

  // All scroll types inside the MainThreadScrollHitTestRegion should fail.
  // These scrolls succeed but request a main thread hit test.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
            status.main_thread_hit_test_reasons);

  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
            status.main_thread_hit_test_reasons);

  // All scroll types outside this region should succeed.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(75, 75), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);

  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(75, 75), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, MainThreadScrollHitTestRegionInNonScrollingRoot) {
  LayerImpl* root_layer = SetupDefaultRootLayer(gfx::Size(50, 50));
  auto AddNewLayer = [&]() {
    LayerImpl* layer = AddLayerInActiveTree();
    layer->SetBounds(gfx::Size(50, 50));
    layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
    CopyProperties(root_layer, layer);
    return layer;
  };
  LayerImpl* layers[3] = {AddNewLayer(), AddNewLayer(), AddNewLayer()};
  DrawFrame();

  auto& input_handler = GetInputHandler();
  std::unique_ptr<ScrollState> begin_state = BeginState(
      gfx::Point(20, 20), gfx::Vector2dF(0, 10), ui::ScrollInputType::kWheel);
  InputHandler::ScrollStatus status;

  for (LayerImpl* layer : layers) {
    layer->SetMainThreadScrollHitTestRegion(gfx::Rect(10, 10, 20, 20));
    status = input_handler.ScrollBegin(begin_state.get(),
                                       ui::ScrollInputType::kWheel);
    input_handler.ScrollEnd(/*should_snap=*/false, std::nullopt);
    layer->SetMainThreadScrollHitTestRegion(Region());

    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
  }
}

TEST_P(LayerTreeHostImplTest, MainThreadScrollHitTestRegionWithOffset) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  outer_scroll->SetMainThreadScrollHitTestRegion(gfx::Rect(0, 0, 50, 50));
  SetPostTranslation(outer_scroll, gfx::Vector2dF(-25, 0));
  outer_scroll->SetDrawsContent(true);

  DrawFrame();

  // This point would fall into the MainThreadScrollHitTestRegion except that
  // we've moved the layer left by 25 pixels.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(40, 10), gfx::Vector2d(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);

  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 1),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // This point is still inside the MainThreadScrollHitTestRegion.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
            status.main_thread_hit_test_reasons);
}

// Tests the following tricky case:
// - Scrolling Layer A with scrolling children:
//    - Ordinary Layer B with MainThreadScrollHitTestRegion
//    - Ordinary Layer C
//
//                   +---------+
//         +---------|         |+
//         | Layer A |         ||
//         |   +-----+-----+   ||
//         |   |  Layer C  |   ||
//         |   +-----+-----+   ||
//         |         | Layer B ||
//         +---------|         |+
//                   +---------+
//
//
// Both B and C scroll with A but overlap each other and C appears above B. If
// we try scrolling over C, we need to check if we intersect the NFSR on B
// because C may not be fully opaque to hit testing (e.g. the layer may be for
// |pointer-events:none| or be a squashing layer with "holes").
TEST_P(LayerTreeHostImplTest,
       LayerOverlapsMainThreadScrollHitTestRegionInLayer) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  // The viewport will be layer A in the description above.
  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  // Layer B is a 50x50 layer filled with a MainThreadScrollHitTestRegion. It
  // occupies the right half of the viewport.
  auto* layer_b = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_b->SetBounds(gfx::Size(50, 100));
  layer_b->SetDrawsContent(true);
  layer_b->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(outer_scroll, layer_b);
  layer_b->SetOffsetToTransformParent(gfx::Vector2dF(50, 0));
  layer_b->SetMainThreadScrollHitTestRegion(gfx::Rect(0, 0, 50, 100));

  // Do a sanity check - scrolling over layer b should cause main thread
  // hit testing because of the MainThreadScrollHitTestRegion.
  {
    ASSERT_EQ(layer_b, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    ASSERT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Layer C is a 50x50 layer initially centered in the viewport. The right
  // half overlaps Layer B.
  auto* layer_c = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_c->SetBounds(gfx::Size(50, 50));
  layer_c->SetDrawsContent(true);
  layer_c->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(outer_scroll, layer_c);
  layer_c->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // Do a sanity check - scrolling over layer c where it doesn't overlap B
  // should cause scrolling on the viewport. It doesn't matter whether the
  // point hits a "hit-test transparent" part of layer C because will cause
  // scrolling in Layer A in either case since C scrolls with A.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(40, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(40, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
              host_impl_->OuterViewportScrollNode());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Now perform the real test - scrolling in the overlapping region should
  // fallback to the main thread. In this case, we really do need to know
  // whether the point hits a "hit-test transparent" part of Layer C because if
  // it does it'll hit the MainThreadScrollHitTestRegion but if it doesn't it
  // targets Layer C which scrolls the viewport.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// Similar to the above test but this time layer B does not scroll with layer
// A. This is an edge case where the CSS painting algorithm allows a sibling of
// an overflow scroller to appear on top of the scroller itself but below some
// of the scroller's children. e.g. https://output.jsbin.com/tejulip/quiet.
//
// <div id="scroller" style="position:relative">
//   <div id="child" style="position:relative; z-index:2"></div>
// </div>
// <div id="sibling" style="position:absolute; z-index: 1"></div>
//
// So we setup:
//
// - Scrolling Layer A with scrolling child:
//    - Ordinary Layer C
// - Ordinary layer B with a MainThreadScrollHitTestRegion
//
//                   +---------+
//         +---------|         |+
//         | Layer A |         ||
//         |   +-----+-----+   ||
//         |   |  Layer C  |   ||
//         |   +-----+-----+   ||
//         |         | Layer B ||
//         +---------|         |+
//                   +---------+
//
//
// Only C scrolls with A but C appears above B. If we try scrolling over C, we
// need to check if we intersect the NFSR on B because C may not be fully
// opaque to hit testing (e.g. the layer may be for |pointer-events:none| or be
// a squashing layer with "holes").
TEST_P(LayerTreeHostImplTest,
       LayerOverlapsMainThreadScrollHitTestRegionInNonScrollAncestorLayer) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  LayerImpl* layer_a = AddScrollableLayer(outer_scroll, gfx::Size(100, 100),
                                          gfx::Size(200, 200));

  auto* layer_b = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_b->SetBounds(gfx::Size(50, 100));
  layer_b->SetDrawsContent(true);
  layer_b->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(outer_scroll, layer_b);
  layer_b->SetOffsetToTransformParent(gfx::Vector2dF(50, 0));

  // Do a sanity check - scrolling over layer b should fallback to the main
  // thread because the first hit scrolling layer is layer a which is not a
  // scroll ancestor of b.
  {
    ASSERT_EQ(layer_b, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(75, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(75, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Layer C is a 50x50 layer initially centered in the viewport. Layer C
  // scrolls with Layer A and appears on top of Layer B. The right half of it
  // overlaps Layer B.
  auto* layer_c = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_c->SetBounds(gfx::Size(50, 50));
  layer_c->SetDrawsContent(true);
  layer_c->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(layer_a, layer_c);
  layer_c->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // Do a sanity check - scrolling over layer c where it doesn't overlap B
  // should cause scrolling of Layer A. It doesn't matter whether the
  // point hits a "hit-test transparent" part of layer C because will cause
  // scrolling in Layer A in either case since C scrolls with A.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(40, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(40, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    ASSERT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              layer_a->scroll_tree_index());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Now perform the real test - scrolling in the overlapping region should
  // fallback to the main thread. In this case, we really do need to know
  // whether the point hits a "hit-test transparent" part of Layer C because if
  // it does it'll hit Layer B which scrolls the outer viewport  but if it
  // doesn't it targets Layer C which scrolls Layer A.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// - Scrolling Layer A with scrolling child:
//    - Ordinary Layer B with MainThreadScrollHitTestRegion
// - Fixed (scrolls with inner viewport) ordinary Layer C.
//
//         +---------+---------++
//         | Layer A |         ||
//         |   +-----+-----+   ||
//         |   |  Layer C  |   ||
//         |   +-----+-----+   ||
//         |         | Layer B ||
//         +---------+---------++
//
//
// B scrolls with A but C, which is fixed, appears above B. If we try scrolling
// over C, we need to check if we intersect the NFSR on B because C may not be
// fully opaque to hit testing (e.g. the layer may be for |pointer-events:none|
// or be a squashing layer with "holes"). This is similar to the cases above
// but uses a fixed Layer C to exercise the case where we hit the viewport via
// the inner viewport.
TEST_P(LayerTreeHostImplTest,
       FixedLayerOverlapsMainThreadScrollHitTestRegionInLayer) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  // The viewport will be layer A in the description above.
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  // Layer B is a 50x50 layer filled with a MainThreadScrollHitTestRegion. It
  // occupies the right half of the viewport.
  auto* layer_b = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_b->SetBounds(gfx::Size(50, 100));
  layer_b->SetDrawsContent(true);
  layer_b->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(outer_scroll, layer_b);
  layer_b->SetOffsetToTransformParent(gfx::Vector2dF(50, 0));
  layer_b->SetMainThreadScrollHitTestRegion(gfx::Rect(0, 0, 50, 100));

  // Do a sanity check - scrolling over layer b should cause main thread
  // hit testing because of the MainThreadScrollHitTestRegion.
  {
    ASSERT_EQ(layer_b, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    ASSERT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Layer C is a 50x50 layer initially centered in the viewport. The right
  // half overlaps Layer B.
  auto* layer_c = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_c->SetBounds(gfx::Size(50, 50));
  layer_c->SetDrawsContent(true);
  layer_c->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(inner_scroll, layer_c);
  layer_c->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // Do a sanity check - scrolling over layer c where it doesn't overlap B
  // should cause scrolling on the viewport. It doesn't matter whether the
  // point hits a "hit-test transparent" part of layer C because will cause
  // scrolling in Layer A in either case since C scrolls with A.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(40, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(40, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
              host_impl_->OuterViewportScrollNode());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Now perform the real test - scrolling in the overlapping region should
  // fallback to the main thread. In this case, we really do need to know
  // whether the point hits a "hit-test transparent" part of Layer C because if
  // it does it'll hit the MainThreadScrollHitTestRegion but if it doesn't it
  // targets Layer C which scrolls the viewport.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// - Scrolling Layer A with scrolling child:
//    - Ordinary Layer B
// - Fixed (scrolls with inner viewport) ordinary Layer C.
//
//         +---------+---------++
//         | Layer A |         ||
//         |   +-----+-----+   ||
//         |   |  Layer C  |   ||
//         |   +-----+-----+   ||
//         |         | Layer B ||
//         +---------+---------++
//
//  This test simply ensures that a scroll over the region where layer C and
//  layer B overlap can be handled on the compositor thread. Both of these
//  layers have the viewport as the first scrolling ancestor but C has the
//  inner viewport while B has the outer viewport as an ancestor. Ensure we
//  treat these as equivalent.
TEST_P(LayerTreeHostImplTest, FixedLayerOverNonFixedLayer) {
  SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(200, 200));

  // The viewport will be layer A in the description above.
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  // Layer B is a 50x50 layer filled with a MainThreadScrollHitTestRegion. It
  // occupies the right half of the viewport.
  auto* layer_b = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_b->SetBounds(gfx::Size(50, 100));
  layer_b->SetDrawsContent(true);
  layer_b->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(outer_scroll, layer_b);
  layer_b->SetOffsetToTransformParent(gfx::Vector2dF(50, 0));

  // Layer C is a 50x50 layer initially centered in the viewport. The right
  // half overlaps Layer B.
  auto* layer_c = AddLayer<LayerImpl>(host_impl_->active_tree());
  layer_c->SetBounds(gfx::Size(50, 50));
  layer_c->SetDrawsContent(true);
  layer_c->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(inner_scroll, layer_c);
  layer_c->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // A scroll in the overlapping region should not fallback to the main thread
  // since we'll scroll the viewport regardless which layer we really should
  // hit.
  {
    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    ASSERT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
              host_impl_->OuterViewportScrollNode());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // However, if we have a non-default root scroller, the inner and outer
  // viewports really should be treated as separate scrollers (i.e. if you
  // "bubble" up to the inner viewport, we shouldn't cause scrolling in the
  // outer viewport because the outer viewport is not an ancestor of the
  // scrolling Element so that would be unexpected). So now the scroll node to
  // use for scrolling depends on whether Layer B or Layer C is actually hit so
  // the main thread needs to decide.
  {
    GetScrollNode(InnerViewportScrollLayer())
        ->prevent_viewport_scrolling_from_inner = true;

    ASSERT_EQ(layer_c, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                           gfx::PointF(60, 50)));
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(60, 50), gfx::Vector2d(0, 10),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

TEST_P(LayerTreeHostImplTest, ScrollUpdateReturnsCorrectValue) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(-10, 0),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);

  // Trying to scroll to the left/top will not succeed.
  EXPECT_FALSE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_FALSE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_FALSE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);

  // Scrolling to the right/bottom will succeed.
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 0),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, 10),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);

  // Scrolling to left/top will now succeed.
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -10),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);

  // Scrolling diagonally against an edge will succeed.
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(10, -10),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_TRUE(GetInputHandler()
                  .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0),
                                            ui::ScrollInputType::kTouchscreen))
                  .did_scroll);
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(-10, 10),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);

  // Trying to scroll more than the available space will also succeed.
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(5000, 5000),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
}

// TODO(crbug.com/487287578): Re-enable on Android once it's non-flaky.
#if BUILDFLAG(IS_ANDROID)
#define DISABLED_ON_ANDROID(test_name) DISABLED_##test_name
#else
#define DISABLED_ON_ANDROID(test_name) test_name
#endif

TEST_P(LayerTreeHostImplTest,
       DISABLED_ON_ANDROID(
           ScrollEndMainThreadRepaintFastPathScrollFeatureDisabled)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ::features::kScrollEndRepaintFollowsScrollUpdate);

  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  EXPECT_FALSE(GetInputHandler()
                   .ScrollEnd(/*should_snap=*/false,
                              /*compensated_scroll_delta=*/std::nullopt)
                   .updates_need_main_thread_repaint);
}

TEST_P(LayerTreeHostImplTest,
       DISABLED_ON_ANDROID(
           ScrollEndMainThreadRepaintFastPathScrollFeatureEnabled)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ::features::kScrollEndRepaintFollowsScrollUpdate);

  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kNotScrollingOnMain;

  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  EXPECT_FALSE(GetInputHandler()
                   .ScrollEnd(/*should_snap=*/false,
                              /*compensated_scroll_delta=*/std::nullopt)
                   .updates_need_main_thread_repaint);
}

TEST_P(LayerTreeHostImplTest,
       DISABLED_ON_ANDROID(
           ScrollEndMainThreadRepaintSlowPathScrollFeatureDisabled)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      ::features::kScrollEndRepaintFollowsScrollUpdate);

  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;

  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  EXPECT_FALSE(GetInputHandler()
                   .ScrollEnd(/*should_snap=*/false,
                              /*compensated_scroll_delta=*/std::nullopt)
                   .updates_need_main_thread_repaint);
}

TEST_P(LayerTreeHostImplTest,
       DISABLED_ON_ANDROID(
           ScrollEndMainThreadRepaintSlowPathScrollFeatureEnabled)) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ::features::kScrollEndRepaintFollowsScrollUpdate);

  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  host_impl_->OuterViewportScrollNode()->main_thread_repaint_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;

  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  EXPECT_TRUE(GetInputHandler()
                  .ScrollEnd(/*should_snap=*/false,
                             /*compensated_scroll_delta=*/std::nullopt)
                  .updates_need_main_thread_repaint);
}

// TODO(sunyunjia): Move scroll snap tests to a separate file.
// https://crbug.com/851690
TEST_P(LayerTreeHostImplTest, ScrollSnapOnX) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(20, 0);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1000));

  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(50, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest, ScrollSnapOnY) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF y_delta(0, 20);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, y_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1000));

  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(0, 50), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest, ScrollSnapOnBoth) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(20, 20);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);

  // Snap target should not be set until the end of the animation.
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1000));

  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

// Simulate a ScrollBegin and ScrollEnd without any intervening ScrollUpdate.
// This test passes if it doesn't crash.
TEST_P(LayerTreeHostImplTest, SnapAfterEmptyScroll) {
  CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF y_delta(0, 20);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, y_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, ScrollSnapAfterAnimatedScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(20, 20);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(AnimatedUpdateState(pointer_position, delta));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(overflow->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the wheel scroll.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  gfx::PointF current_offset = CurrentScrollOffset(overflow);
  EXPECT_LT(0, current_offset.x());
  EXPECT_GT(20, current_offset.x());
  EXPECT_LT(0, current_offset.y());
  EXPECT_GT(20, current_offset.y());
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // The scroll animation is finished, so the snap animation should begin.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1000));

  // The snap target should not be set until the end of the animation.
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // Finish the animation.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1500));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50), CurrentScrollOffset(overflow));
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest, SnapAnimationCancelledByScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(20, 0);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);
  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the snap.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(100));
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  gfx::PointF current_offset = CurrentScrollOffset(overflow);
  EXPECT_GT(50, current_offset.x());
  EXPECT_LT(20, current_offset.x());
  EXPECT_EQ(0, current_offset.y());

  // Interrupt the snap animation with ScrollBegin.
  auto begin_state =
      BeginState(pointer_position, x_delta, ui::ScrollInputType::kWheel);
  begin_state->data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(150));
  EXPECT_POINTF_EQ(current_offset, CurrentScrollOffset(overflow));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1000));
  // Ensure that the snap target was not updated at the end of the scroll
  // animation.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest,
       SnapAnimationShouldNotStartWhenScrollEndsAtSnapTarget) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF x_delta(50, 0);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // There is a snap target at 50, scroll to it directly.
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  GetInputHandler().ScrollEnd(/*should_snap=*/true, std::nullopt);

  // No animation is created, but the snap target should be updated.
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // We are already at a snap target so we should not animate for snap.
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));

  // Verify that we are not actually animating by running one frame and ensuring
  // scroll offset has not changed.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(100));
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(50, 0), CurrentScrollOffset(overflow));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId()),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest,
       GetSnapFlingInfoAndSetAnimatingSnapTargetWhenZoomed) {
  LayerImpl* overflow = CreateLayerForSnapping();
  // Scales the page to its 1/5.
  host_impl_->active_tree()->PushPageScaleFromMainThread(0.2f, 0.1f, 5);

  // Should be (10, 10) in the scroller's coordinate.
  gfx::Point pointer_position(2, 2);
  gfx::Vector2dF delta(4, 4);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // Should be (20, 20) in the scroller's coordinate.
  InputHandlerScrollResult result = GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel));
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(overflow));
  EXPECT_POINTF_EQ(gfx::PointF(4, 4), result.current_visual_offset);

  gfx::PointF initial_offset, target_offset;
  EXPECT_TRUE(GetInputHandler().GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(1, 1), gfx::Vector2dF(10, 10), &initial_offset,
      &target_offset));
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(4, 4), initial_offset);
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), target_offset);
  // Snap targets shouldn't be set until the fling animation is complete.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  GetInputHandler().ScrollUpdate(UpdateState(
      pointer_position, gfx::Vector2dF(6, 6), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEndForSnapFling(true /* did_finish */);
  EXPECT_FALSE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(ElementId(10), ElementId(10)),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest, SnapFlingAnimationEndWithoutFinishing) {
  LayerImpl* overflow = CreateLayerForSnapping();
  // Scales the page to its 1/5.
  host_impl_->active_tree()->PushPageScaleFromMainThread(0.2f, 0.1f, 5.f);

  // Should be (10, 10) in the scroller's coordinate.
  gfx::Point pointer_position(2, 2);
  gfx::Vector2dF delta(4, 4);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // Should be (20, 20) in the scroller's coordinate.
  InputHandlerScrollResult result = GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel));
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(overflow));
  EXPECT_POINTF_EQ(gfx::PointF(4, 4), result.current_visual_offset);

  gfx::PointF initial_offset, target_offset;
  EXPECT_TRUE(GetInputHandler().GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(1, 1), gfx::Vector2dF(10, 10), &initial_offset,
      &target_offset));
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(4, 4), initial_offset);
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), target_offset);
  // Snap targets shouldn't be set until the fling animation is complete.
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());

  // The snap targets should not be set if the snap fling did not finish.
  GetInputHandler().ScrollEndForSnapFling(false /* did_finish */);
  EXPECT_TRUE(
      GetInputHandler().animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_EQ(TargetSnapAreaElementIds(),
            GetSnapContainerData(overflow)->GetTargetSnapAreaElementIds());
}

TEST_P(LayerTreeHostImplTest, NativeFlingInSnapArea) {
  gfx::Size view_size(100, 100);
  gfx::Size overflow_size(100, 1000);
  gfx::RectF snap_area_1(0, 0, 100, 700);
  gfx::RectF snap_area_2(0, 700, 100, 100);

  SetupViewportLayersInnerScrolls(view_size, view_size);
  LayerImpl* overflow =
      AddScrollableLayer(OuterViewportScrollLayer(), view_size, overflow_size);

  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 100, 100), gfx::PointF(0, 900));
  ScrollSnapAlign start = ScrollSnapAlign(SnapAlignment::kStart);
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_1, false, false, ElementId(10)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_2, false, false, ElementId(20)));
  GetScrollNode(overflow)->snap_container_data.emplace(container);
  DrawFrame();

  auto& handler = GetInputHandler();
  gfx::PointF initial_offset, target_offset;
  gfx::Point position(50, 50);
  ui::ScrollInputType type = ui::ScrollInputType::kTouchscreen;

  handler.ScrollBegin(&*BeginState(position, gfx::Vector2dF(0, 100), type),
                      type);
  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 100), type));
  EXPECT_POINTF_EQ(gfx::PointF(0, 100), CurrentScrollOffset(overflow));

  // 100 (current offset) + 400 (predicted fling) keeps us inside snap_area_1.
  // The input handler should return false to trigger native fling physics.
  EXPECT_FALSE(handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 10), gfx::Vector2dF(0, 400), &initial_offset,
      &target_offset));
  EXPECT_FALSE(handler.animating_for_snap_for_testing(overflow->element_id()));

  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 450), type));
  EXPECT_POINTF_EQ(gfx::PointF(0, 550), CurrentScrollOffset(overflow));

  // 550 (current offset) + 100 (new delta) takes us outside snap_area_1.
  // Constrain to snap_area_1 by triggering snap fling to the maximum offset
  // such that snap_area_1 still covers the viewport (600).
  EXPECT_TRUE(handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 100), gfx::Vector2dF(0, 4000), &initial_offset,
      &target_offset));
  EXPECT_TRUE(handler.animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(0, 550), initial_offset);
  EXPECT_POINTF_EQ(gfx::PointF(0, 600), target_offset);

  // Reset for a new fling from 550.
  GetInputHandler().ScrollEndForSnapFling(false /* did_finish */);
  EXPECT_POINTF_EQ(gfx::PointF(0, 550), CurrentScrollOffset(overflow));

  handler.ScrollBegin(&*BeginState(position, gfx::Vector2dF(0, 100), type),
                      type);

  // This fling is predicted to land in snap_area_2 (offset 700).
  EXPECT_TRUE(handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 10), gfx::Vector2dF(0, 400), &initial_offset,
      &target_offset));
  EXPECT_TRUE(handler.animating_for_snap_for_testing(overflow->element_id()));
  EXPECT_POINTF_EQ(gfx::PointF(0, 550), initial_offset);
  EXPECT_POINTF_EQ(gfx::PointF(0, 700), target_offset);
}

TEST_P(LayerTreeHostImplTest, OverscrollBehaviorPreventsPropagation) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(200, 200);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow = AddScrollableLayer(OuterViewportScrollLayer(),
                                           gfx::Size(100, 100), overflow_size);
  SetScrollOffset(scroll_layer, gfx::PointF(30, 30));

  DrawFrame();
  gfx::Point pointer_position(50, 50);
  gfx::Vector2dF x_delta(-10, 0);
  gfx::Vector2dF y_delta(0, -10);
  gfx::Vector2dF diagonal_delta(-10, -10);

  // OverscrollBehaviorTypeAuto shouldn't prevent scroll propagation.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(30, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->overscroll_behavior = OverscrollBehavior(
      OverscrollBehavior::Type::kContain, OverscrollBehavior::Type::kAuto);

  DrawFrame();

  // OverscrollBehaviorContain on x should prevent propagations of scroll
  // on x.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // OverscrollBehaviorContain on x shouldn't prevent propagations of
  // scroll on y.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, y_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, diagonal_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(UpdateState(pointer_position, diagonal_delta,
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // Changing scroll-boundary-behavior to y axis.
  GetScrollNode(overflow)->overscroll_behavior = OverscrollBehavior(
      OverscrollBehavior::Type::kAuto, OverscrollBehavior::Type::kContain);

  DrawFrame();

  // OverscrollBehaviorContain on y shouldn't prevent propagations of
  // scroll on x.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // OverscrollBehaviorContain on y should prevent propagations of scroll
  // on y.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, y_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, diagonal_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(UpdateState(pointer_position, diagonal_delta,
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // Gesture scroll should latch to the first scroller that has non-auto
  // overscroll-behavior.
  GetScrollNode(overflow)->overscroll_behavior = OverscrollBehavior(
      OverscrollBehavior::Type::kContain, OverscrollBehavior::Type::kContain);

  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, -x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, -y_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), CurrentScrollOffset(overflow));
}

TEST_P(LayerTreeHostImplTest, OverscrollBehaviorChainPropagatesScroll) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(200, 200);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow = AddScrollableLayer(OuterViewportScrollLayer(),
                                           gfx::Size(100, 100), overflow_size);
  SetScrollOffset(scroll_layer, gfx::PointF(30, 30));

  DrawFrame();
  gfx::Point pointer_position(50, 50);
  gfx::Vector2dF x_delta(-10, 0);
  gfx::Vector2dF y_delta(0, -10);

  // OverscrollBehaviorChain should allow scroll propagation.
  GetScrollNode(overflow)->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::Type::kChain);

  DrawFrame();

  // Propagation on x.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, x_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, x_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 30), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  // Propagation on y.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(pointer_position, y_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, y_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(20, 20), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));
}

TEST_P(LayerTreeHostImplTest, ScrollWithUserUnscrollableLayers) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(200, 200);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow =
      AddScrollableLayer(scroll_layer, gfx::Size(100, 100), overflow_size);

  DrawFrame();
  gfx::Point scroll_position(10, 10);
  gfx::Vector2dF scroll_delta(10, 10);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(scroll_position, scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->user_scrollable_horizontal = false;

  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(scroll_position, scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(overflow));

  GetScrollNode(overflow)->user_scrollable_vertical = false;
  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(scroll_position, scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(overflow));

  GetInputHandler().ScrollUpdate(
      UpdateState(scroll_position, scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_POINTF_EQ(gfx::PointF(10, 10), CurrentScrollOffset(scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(10, 20), CurrentScrollOffset(overflow));
}

// Test that if a scroll node doesn't have an associated Layer.
TEST_P(LayerTreeHostImplTest, ScrollNodeWithoutScrollLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  ScrollNode* scroll_node = host_impl_->OuterViewportScrollNode();
  // Change the scroll node so that it no longer has an associated layer.
  scroll_node->element_id = ElementId(42);

  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(25, 25), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  // We don't have a layer for the scroller but we didn't hit a
  // MainThreadScrollHitTestRegion or fail hit testing the layer - we don't
  // need a main thread hit test in this case.
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest,
       AnimationSchedulingCommitToActiveTree) {
  auto* root = SetupDefaultRootLayer(gfx::Size(50, 50));

  auto* child = AddLayerInActiveTree();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 3, 0);

  // Set up the property trees so that UpdateDrawProperties will work in
  // CommitComplete below.
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())->CommitComplete();
  }

  // Animations on the active tree should be started and ticked, and a new frame
  // should be requested to continue ticking them.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  // Delete the LayerTreeHostImpl before the TaskRunnerProvider goes away.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_P(AnimationsLayerTreeHostImplTest, AnimationSchedulingOnLayerDestruction) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(50, 50));

  LayerImpl* child = AddLayerInActiveTree();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();
  CopyProperties(root, child);
  CreateTransformNode(child);

  // Add a translate animation.
  gfx::TransformOperations start;
  start.AppendTranslate(6, 7, 0);
  gfx::TransformOperations end;
  end.AppendTranslate(8, 9, 0);
  scoped_refptr<Animation> animation =
      Animation::Create(AnimationIdProvider::NextAnimationId());
  timeline()->AttachAnimation(animation);
  animation->AttachElement(child->element_id());
  int keyframe_model_id =
      AddAnimatedTransformToAnimation(animation.get(), 4.0, start, end);
  UpdateDrawProperties(host_impl_->active_tree());

  base::TimeTicks now = base::TimeTicks::Now();
  host_impl_->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, now));
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  host_impl_->ActivateAnimations();
  // On activating an animation, we should request another frame so that we'll
  // continue ticking the animation.
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  // The next frame after activating, we'll tick the animation again.
  host_impl_->Animate();
  // An animation exists on the active layer. Doing Animate() requests another
  // frame after the current one.
  EXPECT_TRUE(did_request_next_frame_);
  did_request_next_frame_ = false;

  // In the real code, animations are removed at the same time as their property
  // tree nodes are.
  animation->RemoveKeyframeModel(keyframe_model_id);
  host_impl_->ActivateAnimations();
  EXPECT_FALSE(did_request_next_frame_);

  // In the real code, you cannot remove a child on LayerImpl, but a child
  // removed on Layer will force a full tree sync which will rebuild property
  // trees without that child's property tree nodes. Clear active_tree (which
  // also clears property trees) to simulate the rebuild that would happen
  // before/during the commit.
  host_impl_->active_tree()->property_trees()->clear();

  // Updating state requires no animation update.
  host_impl_->UpdateAnimationState(true);
  EXPECT_FALSE(did_request_next_frame_);

  // Doing Animate() doesn't request any frames after the animation is removed.
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
}

class IncompleteRecordingLayer : public LayerImpl {
 public:
  static std::unique_ptr<IncompleteRecordingLayer> Create(
      LayerTreeImpl* tree_impl,
      int id) {
    return std::make_unique<IncompleteRecordingLayer>(tree_impl, id);
  }
  IncompleteRecordingLayer(LayerTreeImpl* layer_tree_impl, int id)
      : LayerImpl(layer_tree_impl, id) {}

  bool ComputeCheckerboardedNeedsRecord() override { return true; }

  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    append_quads_data->visible_layer_area += 200;
  }
};

// TODO(crbug.com/401566175) implement checkerboard tracking for TreesInViz
// mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       ScrollCheckerboardsIncompleteRecordingPerScroll) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNewContentForCheckerboardedScrolls,
      {{"mode", features::kNewContentForCheckerboardedScrollsPerScroll}});
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.25f, 4);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);

  // Add layer that draws content and has checkerboarded areas.
  auto* scroll_layer =
      AddLayer<IncompleteRecordingLayer>(host_impl_->active_tree());
  CopyProperties(inner_scroll_layer, scroll_layer);
  scroll_layer->SetBounds(gfx::Size(500, 500));
  scroll_layer->SetDrawsContent(true);
  scroll_layer->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  host_impl_->active_tree()->SetElementIdsForTesting();

  UpdateDrawProperties(host_impl_->active_tree());

  DrawFrame();

  // No scroll has taken place so this should be false.
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());

  // Send scroll begin.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);

  DrawFrame();

  // Even though a ScrollBegin has been processed, we still don't consider the
  // interaction to be "actively scrolling". Expect this to be false.
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());

  gfx::Vector2dF scroll_delta(0, 10);

  // Send scroll update.
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(10, 10), scroll_delta,
                                             ui::ScrollInputType::kWheel));

  host_impl_->SetFullViewportDamage();
  DrawFrame();

  // Now that a scroll update has been processed and the latest
  // CalculateRenderPasses run has computed significant visible checkerboarding,
  // expect this flag to be true.
  EXPECT_TRUE(host_impl_->PrioritizeNewContentDueToCheckerboarding());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Expect state to be reset after a scroll end.
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());
}

// TODO(crbug.com/401566175) implement checkerboard tracking for TreesInViz
// mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       ScrollCheckerboardsIncompleteRecordingPerFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNewContentForCheckerboardedScrolls,
      {{"mode", features::kNewContentForCheckerboardedScrollsPerFrame}});
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.25f, 4);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);

  // Add layer that draws content and has checkerboarded areas.
  auto* scroll_layer =
      AddLayer<IncompleteRecordingLayer>(host_impl_->active_tree());
  CopyProperties(inner_scroll_layer, scroll_layer);
  scroll_layer->SetBounds(gfx::Size(500, 500));
  scroll_layer->SetDrawsContent(true);
  scroll_layer->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  host_impl_->active_tree()->SetElementIdsForTesting();

  UpdateDrawProperties(host_impl_->active_tree());

  DrawFrame();

  // No scroll has taken place so this should be false.
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());

  // Send scroll begin.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);

  DrawFrame();

  // Even though a ScrollBegin has been processed, we still don't consider the
  // interaction to be "actively scrolling". Expect this to be false.
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());

  gfx::Vector2dF scroll_delta(0, 10);

  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .SetScrollingContentsCullRect(
          host_impl_->CurrentlyScrollingNode()->element_id, gfx::Rect(50, 50));

  // Send scroll update.
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(10, 10), scroll_delta,
                                             ui::ScrollInputType::kWheel));
  // Now that a scroll update has been processed and checkerboarding has been
  // detected expect this flag to be true.
  EXPECT_TRUE(host_impl_->PrioritizeNewContentDueToCheckerboarding());
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  // After drawing a frame the value remains true because the frame had
  // checkerboarding (the top layer was IncompleteRecordingLayer).
  EXPECT_TRUE(host_impl_->PrioritizeNewContentDueToCheckerboarding());
}

// Verifies that kNewContentForCheckerboardedScrolls doesn't set the flag to
// to change tree priority when the checkerboarding happens outside of the
// screen's rect.
TEST_P(LayerTreeHostImplTest,
       ScrollCheckerboardsIncompleteRecordingOutOfScreen) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kNewContentForCheckerboardedScrolls,
      {{"mode", features::kNewContentForCheckerboardedScrollsPerFrame}});
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.25f, 4);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  auto* scroll_layer =
      AddLayer<IncompleteRecordingLayer>(host_impl_->active_tree());
  CopyProperties(inner_scroll_layer, scroll_layer);
  scroll_layer->SetBounds(gfx::Size(250, 250));
  scroll_layer->SetDrawsContent(true);
  scroll_layer->SetHitTestOpaqueness(HitTestOpaqueness::kTransparent);
  host_impl_->active_tree()->SetElementIdsForTesting();
  UpdateDrawProperties(host_impl_->active_tree());
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(260, 260), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  // The `cull_rect` will start 20px down from the `visible_rect` (after the
  // scroll). The `visible_rect`'s origin will be 20px above the screen, so
  // those 20pxs wouldn't be rendered. Even though the `cull_rect` won't contain
  // the entirety of the `visible_rect`, it will contain the entirety of the
  // `visible_rect`'s portion that is is rendered inside the screen, which means
  // that the checkerboarding won't be visible to the user and
  // `PrioritizeNewContentDueToCheckerboarding()` should be false.
  const gfx::Vector2dF scroll_delta(0, 20);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .SetScrollingContentsCullRect(
          host_impl_->CurrentlyScrollingNode()->element_id,
          gfx::Rect(scroll_delta.x(), scroll_delta.y(), viewport_size.width(),
                    viewport_size.height()));
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(10, 10), scroll_delta,
                                             ui::ScrollInputType::kWheel));
  EXPECT_FALSE(host_impl_->PrioritizeNewContentDueToCheckerboarding());
}

TEST_P(LayerTreeHostImplTest, ImplPinchZoom) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  EXPECT_EQ(gfx::Size(50, 50), root_layer()->bounds());

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;

  // The impl-based pinch zoom should adjust the max scroll position.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;

    // TODO(bokan): What are the delta_hints for a GSB that's sent for a pinch
    // gesture that doesn't cause (initial) scrolling?
    // https://crbug.com/1030262
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(50, 50),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    GetInputHandler().PinchGestureEnd(gfx::Point(50, 50));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);
    EXPECT_EQ(gfx::Size(50, 50), root_layer()->bounds());

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);

    EXPECT_EQ(gfx::PointF(75.0, 75.0), MaxScrollOffset(scroll_layer));
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Scrolling after a pinch gesture should always be in local space.  The
  // scroll deltas have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                             ui::ScrollInputType::kTouchscreen)
                                      .get(),
                                  ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point());
    GetInputHandler().PinchGestureEnd(gfx::Point());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    gfx::Vector2d scroll_delta(0, 10);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_TRUE(ScrollInfoContains(
        *commit_data.get(), scroll_layer->element_id(),
        gfx::Vector2dF(0, scroll_delta.y() / page_scale_delta)));
  }
}

TEST_P(LayerTreeHostImplTest, ViewportScrollbarGeometry) {
  // Tests for correct behavior of solid color scrollbars on unscrollable pages
  // under tricky fractional scale/size issues.

  // Nexus 6 viewport size.
  const gfx::Size viewport_size(412, 604);

  // The content size of a non-scrollable page we'd expect given Android's
  // behavior of a 980px layout width on non-mobile pages (often ceiled to 981
  // due to fractions resulting from DSF). Due to floating point error,
  // viewport_size.height() / minimum_scale ~= 1438.165 < 1439. Ensure we snap
  // correctly and err on the side of not showing the scrollbars.
  const gfx::Size content_size(981, 1439);
  const float minimum_scale =
      viewport_size.width() / static_cast<float>(content_size.width());

  // Since the page is unscrollable, the outer viewport matches the content
  // size.
  const gfx::Size outer_viewport_size = content_size;

  // Setup
  LayerTreeImpl* active_tree = host_impl_->active_tree();
  active_tree->PushPageScaleFromMainThread(1, minimum_scale, 4);

  // When Chrome on Android loads a non-mobile page, it resizes the main
  // frame (outer viewport) such that it matches the width of the content,
  // preventing horizontal scrolling. Replicate that behavior here.
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* scroll = OuterViewportScrollLayer();
  ASSERT_EQ(GetScrollNode(scroll)->container_bounds, outer_viewport_size);
  scroll->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  ClipNode* outer_clip = host_impl_->active_tree()->OuterViewportClipNode();
  ASSERT_EQ(gfx::SizeF(outer_viewport_size), outer_clip->clip.size());

  // Add scrollbars. They will always exist - even if unscrollable - but their
  // visibility will be determined by whether the content can be scrolled.
  auto* v_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      active_tree, ScrollbarOrientation::kVertical, false, true);
  auto* h_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      active_tree, ScrollbarOrientation::kHorizontal, false, true);
  SetupScrollbarLayer(scroll, v_scrollbar);
  SetupScrollbarLayer(scroll, h_scrollbar);

  host_impl_->active_tree()->DidBecomeActive();

  // Zoom out to the minimum scale. The scrollbars shoud not be scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(0);
  EXPECT_FALSE(v_scrollbar->CanScrollOrientation());
  EXPECT_FALSE(h_scrollbar->CanScrollOrientation());

  // Zoom in a little and confirm that they're now scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(minimum_scale * 1.05f);
  EXPECT_TRUE(v_scrollbar->CanScrollOrientation());
  EXPECT_TRUE(h_scrollbar->CanScrollOrientation());
}

TEST_P(LayerTreeHostImplTest, ViewportScrollOrder) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.25f, 4);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_POINTF_EQ(gfx::PointF(500, 500), MaxScrollOffset(outer_scroll_layer));

  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, gfx::Point(0, 0));
  GetInputHandler().PinchGestureEnd(gfx::Point(0, 0));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Sanity check - we're zoomed in, starting from the origin.
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll_layer));

  // Scroll down - only the inner viewport should scroll.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                  ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));

  // Scroll down - outer viewport should start scrolling after the inner is at
  // its maximum.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                  ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(250, 250),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(300, 300),
                   CurrentScrollOffset(outer_scroll_layer));
}

// Make sure scrolls smaller than a unit applied to the viewport don't get
// dropped. crbug.com/539334.
TEST_P(LayerTreeHostImplTest, ScrollViewportWithFractionalAmounts) {
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  // Sanity checks.
  EXPECT_POINTF_EQ(gfx::PointF(500, 500), MaxScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll_layer));

  // Scroll only the layout viewport.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(0.125f, 0.125f),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(0.125f, 0.125f),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_POINTF_EQ(gfx::PointF(0.125f, 0.125f),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll_layer));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  host_impl_->active_tree()->PushPageScaleFromMainThread(2, 1, 2);

  // Now that we zoomed in, the scroll should be applied to the inner viewport.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(0.5f, 0.5f),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(0.5f, 0.5f),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_POINTF_EQ(gfx::PointF(0.125f, 0.125f),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0.25f, 0.25f),
                   CurrentScrollOffset(inner_scroll_layer));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Tests that scrolls during a pinch gesture (i.e. "two-finger" scrolls) work
// as expected. That is, scrolling during a pinch should bubble from the inner
// to the outer viewport.
TEST_P(LayerTreeHostImplTest, ScrollDuringPinchGesture) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_POINTF_EQ(gfx::PointF(500, 500), MaxScrollOffset(outer_scroll_layer));

  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(250, 250),
                                      ui::ScrollInputType::kWheel);

  GetInputHandler().PinchGestureUpdate(2, gfx::Point(250, 250));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(125, 125),
                   CurrentScrollOffset(inner_scroll_layer));

  // Needed so that the pinch is accounted for in draw properties.
  DrawFrame();

  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(10, 10),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(130, 130),
                   CurrentScrollOffset(inner_scroll_layer));

  DrawFrame();

  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(400, 400),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_POINTF_EQ(gfx::PointF(80, 80),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(250, 250),
                   CurrentScrollOffset(inner_scroll_layer));

  GetInputHandler().PinchGestureEnd(gfx::Point(250, 250));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Tests the "snapping" of pinch-zoom gestures to the screen edge. That is, when
// a pinch zoom is anchored within a certain margin of the screen edge, we
// should assume the user means to scroll into the edge of the screen.
TEST_P(LayerTreeHostImplTest, PinchZoomSnapsToScreenEdge) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  int offsetFromEdge = Viewport::kPinchZoomSnapMarginDips - 5;
  gfx::Point anchor(viewport_size.width() - offsetFromEdge,
                    viewport_size.height() - offsetFromEdge);

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // bottom and right.
  GetInputHandler().ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(anchor, ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, anchor);
  GetInputHandler().PinchGestureEnd(anchor);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(250, 250),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // top and left.
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  GetInputHandler().ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(100, 100),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, anchor);
  GetInputHandler().PinchGestureEnd(anchor);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  GetInputHandler().ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(anchor, ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, anchor);
  GetInputHandler().PinchGestureEnd(anchor);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(InnerViewportScrollLayer()));

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1);
  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(viewport_size.width() - offsetFromEdge,
                      viewport_size.height() - offsetFromEdge);
  GetInputHandler().ScrollBegin(
      BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(anchor, ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, anchor);
  GetInputHandler().PinchGestureEnd(anchor);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(200, 200),
                   CurrentScrollOffset(InnerViewportScrollLayer()));
}

TEST_P(LayerTreeHostImplTest, ImplPinchZoomWheelBubbleBetweenViewports) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(10, 20),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(0, 0), gfx::Vector2dF(10, 20), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(5, 10), CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(0, 0), gfx::Vector2dF(100, 100), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(5, 10), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(0, 0), gfx::Vector2dF(190, 180),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(0, 0), gfx::Vector2dF(190, 180), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_POINTF_EQ(gfx::PointF(100, 100),
                   CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
}

TEST_P(LayerTreeHostImplTest, ScrollWithSwapPromises) {
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));

  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 10),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));
  host_impl_->QueueSwapPromiseForMainThreadScrollUpdate(
      std::move(swap_promise));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_EQ(1u, commit_data->swap_promises.size());
  EXPECT_EQ(latency_info.trace_id(),
            commit_data->swap_promises[0]->GetTraceId());
}

// Test that scrolls targeting a layer with a non-null scroll_parent() don't
// bubble up.
TEST_P(LayerTreeHostImplTest, ScrollDoesntBubble) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* viewport_scroll = InnerViewportScrollLayer();

  // Set up two scrolling children of the root, one of which is a scroll parent
  // to the other. Scrolls shouldn't bubbling from the child.
  LayerImpl* scroll_parent =
      AddScrollableLayer(viewport_scroll, gfx::Size(5, 5), gfx::Size(10, 10));

  LayerImpl* scroll_child_clip = AddLayerInActiveTree();
  // scroll_child_clip scrolls in scroll_parent, but under viewport_scroll's
  // effect.
  CopyProperties(scroll_parent, scroll_child_clip);
  scroll_child_clip->SetEffectTreeIndex(viewport_scroll->effect_tree_index());

  LayerImpl* scroll_child =
      AddScrollableLayer(scroll_child_clip, gfx::Size(5, 5), gfx::Size(10, 10));
  GetTransformNode(scroll_child)->post_translation = gfx::Vector2d(20, 20);

  DrawFrame();

  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(21, 21), gfx::Vector2d(5, 5),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(5, 5),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(100, 100),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // The child should be fully scrolled by the first ScrollUpdate.
    EXPECT_POINTF_EQ(gfx::PointF(5, 5), CurrentScrollOffset(scroll_child));

    // The scroll_parent shouldn't receive the second ScrollUpdate.
    EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_parent));

    // The viewport shouldn't have been scrolled at all.
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(InnerViewportScrollLayer()));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(OuterViewportScrollLayer()));
  }

  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(21, 21), gfx::Vector2d(3, 4),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(3, 4),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // The ScrollUpdate's should scroll the parent to its extent.
    EXPECT_POINTF_EQ(gfx::PointF(5, 5), CurrentScrollOffset(scroll_parent));

    // The viewport shouldn't receive any scroll delta.
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(InnerViewportScrollLayer()));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(OuterViewportScrollLayer()));
  }
}

TEST_P(LayerTreeHostImplTest, PinchGesture) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1;
  float max_page_scale = 4;

  // Basic pinch zoom in gesture
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(50, 50),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    GetInputHandler().PinchGestureEnd(gfx::Point(50, 50));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Zoom-in clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    float page_scale_delta = 10;

    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(50, 50), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(50, 50),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    GetInputHandler().PinchGestureEnd(gfx::Point(50, 50));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, max_page_scale);
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Zoom-out clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .CollectScrollDeltasForTesting();
    ClearMainThreadDeltasForTesting(host_impl_.get());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(50, 50));

    float page_scale_delta = 0.1f;
    GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                             ui::ScrollInputType::kTouchscreen)
                                      .get(),
                                  ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point());
    GetInputHandler().PinchGestureEnd(gfx::Point());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, min_page_scale);

    EXPECT_TRUE(commit_data->scrolls.empty());
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Two-finger panning should not happen based on pinch events only
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .CollectScrollDeltasForTesting();
    ClearMainThreadDeltasForTesting(host_impl_.get());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(20, 20));

    float page_scale_delta = 1;
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(10, 10), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(10, 10),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    GetInputHandler().PinchGestureEnd(gfx::Point(20, 20));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(commit_data->scrolls.empty());
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Two-finger panning should work with interleaved scroll events
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .CollectScrollDeltasForTesting();
    ClearMainThreadDeltasForTesting(host_impl_.get());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(20, 20));

    float page_scale_delta = 1;
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(10, 10), gfx::Vector2dF(-10, -10),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(10, 10),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(10, 10), gfx::Vector2d(-10, -10),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    GetInputHandler().PinchGestureEnd(gfx::Point(20, 20));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                   gfx::Vector2dF(-10, -10)));
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  // Two-finger panning should work when starting fully zoomed out.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(0.5f, 0.5f, 4);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .CollectScrollDeltasForTesting();
    ClearMainThreadDeltasForTesting(host_impl_.get());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(0, 0));

    GetInputHandler().ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(),
                                             ui::ScrollInputType::kTouchscreen)
                                      .get(),
                                  ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(0, 0),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(2, gfx::Point(0, 0));
    GetInputHandler().PinchGestureUpdate(1, gfx::Point(0, 0));

    // Needed so layer transform includes page scale.
    DrawFrame();

    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2d(10, 10),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().PinchGestureUpdate(1, gfx::Point(10, 10));
    GetInputHandler().PinchGestureEnd(gfx::Point(10, 10));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, 2);
    EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                   gfx::Vector2dF(10, 10)));
  }
}

TEST_P(LayerTreeHostImplTest, SyncSubpixelScrollDelta) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1;
  float max_page_scale = 4;

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                         max_page_scale);
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .CollectScrollDeltasForTesting();
  ClearMainThreadDeltasForTesting(host_impl_.get());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(0, 20));

  float page_scale_delta = 1;
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(10, 10),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(10, 10), gfx::Vector2dF(0, -1.001f),
                  ui::ScrollInputType::kTouchscreen));
  GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(10, 9));
  GetInputHandler().PinchGestureEnd(gfx::Point(10, 9));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);
  EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                 gfx::Vector2dF(0, -1)));

  // Verify this scroll delta is consistent with the snapped position of the
  // scroll layer.
  draw_property_utils::ComputeTransforms(
      &scroll_layer->layer_tree_impl()
           ->property_trees()
           ->transform_tree_mutable(),
      scroll_layer->layer_tree_impl()->viewport_property_ids());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, -19),
                      scroll_layer->ScreenSpaceTransform().To2dTranslation());
}

TEST_P(LayerTreeHostImplTest, SyncSubpixelScrollFromFractionalActiveBase) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .CollectScrollDeltasForTesting();
  ClearMainThreadDeltasForTesting(host_impl_.get());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(0, 20.5f));

  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(10, 10), gfx::Vector2dF(0, -1),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(10, 10), gfx::Vector2dF(0, -1), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  gfx::PointF active_base =
      host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree()
          .GetScrollOffsetBaseForTesting(scroll_layer->element_id());
  EXPECT_POINTF_EQ(active_base, gfx::PointF(0, 20.5));
  // Fractional active base should not affect the scroll delta.
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, -1),
                      commit_data->inner_viewport_scroll.scroll_delta);
}

TEST_P(LayerTreeHostImplTest, PinchZoomTriggersPageScaleAnimation) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  float min_page_scale = 1;
  float max_page_scale = 4;
  float page_scale_delta = 1.04f;
  base::TimeTicks start_time = base::TimeTicks() + base::Seconds(1);
  base::TimeDelta duration = base::Milliseconds(200);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Zoom animation if page_scale is < 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    GetInputHandler().PinchGestureBegin(gfx::Point(50, 50),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    GetInputHandler().PinchGestureEnd(gfx::Point(50, 50));
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, 1);
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  start_time += base::Seconds(10);
  halfway_through_animation += base::Seconds(10);
  end_time += base::Seconds(10);
  page_scale_delta = 1.06f;

  // No zoom animation if page_scale is >= 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    GetInputHandler().PinchGestureBegin(gfx::Point(50, 50),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    GetInputHandler().PinchGestureEnd(gfx::Point(50, 50));
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, page_scale_delta);
  }
}

TEST_P(LayerTreeHostImplTest, PageScaleAnimation) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  base::TimeTicks start_time = base::TimeTicks() + base::Seconds(1);
  base::TimeDelta duration = base::Milliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Non-anchor zoom-in
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::make_unique<PendingPageScaleAnimation>(gfx::Point(), false, 2,
                                                    duration));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, 2);
    EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                   gfx::Vector2dF(-50, -50)));
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }

  start_time += base::Seconds(10);
  halfway_through_animation += base::Seconds(10);
  end_time += base::Seconds(10);

  // Anchor zoom-out
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::make_unique<PendingPageScaleAnimation>(gfx::Point(25, 25), true,
                                                    min_page_scale, duration));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    did_request_redraw_ = false;
    did_request_commit_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, min_page_scale);
    // Pushed to (0,0) via clamping against contents layer size.
    EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                   gfx::Vector2dF(-50, -50)));
  }
}

TEST_P(LayerTreeHostImplTest, PageScaleAnimationNoOp) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  base::TimeTicks start_time = base::TimeTicks() + base::Seconds(1);
  base::TimeDelta duration = base::Milliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Anchor zoom with unchanged page scale should not change scroll or scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree_mutable()
        .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                          gfx::PointF(50, 50));

    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::make_unique<PendingPageScaleAnimation>(gfx::Point(), true, 1,
                                                    duration));
    host_impl_->ActivateSyncTree();
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->DidFinishImplFrame(begin_frame_args);

    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    begin_frame_args.frame_time = end_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_EQ(commit_data->page_scale_delta, 1);
    ExpectNone(*commit_data, scroll_layer->element_id());
  }
}

TEST_P(LayerTreeHostImplTest, PageScaleAnimationTransferedOnSyncTreeActivate) {
  EnsureSyncTree();
  host_impl_->sync_tree()->PushPageScaleFromMainThread(1, 1, 1);
  SetupViewportLayers(host_impl_->sync_tree(), gfx::Size(50, 50),
                      gfx::Size(100, 100), gfx::Size(100, 100));
  host_impl_->ActivateSyncTree();
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4;
  EnsureSyncTree();
  host_impl_->sync_tree()->PushPageScaleFromMainThread(1, min_page_scale,
                                                       max_page_scale);
  host_impl_->ActivateSyncTree();

  base::TimeTicks start_time = base::TimeTicks() + base::Seconds(1);
  base::TimeDelta duration = base::Milliseconds(100);
  base::TimeTicks third_through_animation = start_time + duration / 3;
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;
  float target_scale = 2;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(50, 50));

  // Make sure TakePageScaleAnimation works properly.
  EnsureSyncTree();
  host_impl_->sync_tree()->SetPendingPageScaleAnimation(
      std::make_unique<PendingPageScaleAnimation>(gfx::Point(), false,
                                                  target_scale, duration));
  std::unique_ptr<PendingPageScaleAnimation> psa =
      host_impl_->sync_tree()->TakePendingPageScaleAnimation();
  EXPECT_EQ(target_scale, psa->scale);
  EXPECT_EQ(duration, psa->duration);
  EXPECT_EQ(nullptr, host_impl_->sync_tree()->TakePendingPageScaleAnimation());

  // Recreate the PSA. Nothing should happen here since the tree containing the
  // PSA hasn't been activated yet.
  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  host_impl_->sync_tree()->SetPendingPageScaleAnimation(
      std::make_unique<PendingPageScaleAnimation>(gfx::Point(), false,
                                                  target_scale, duration));
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Activate the sync tree. This should cause the animation to become enabled.
  // It should also clear the pointer on the sync tree.
  host_impl_->ActivateSyncTree();
  EnsureSyncTree();
  EXPECT_EQ(nullptr,
            host_impl_->sync_tree()->TakePendingPageScaleAnimation().get());
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);

  start_time += base::Seconds(10);
  third_through_animation += base::Seconds(10);
  halfway_through_animation += base::Seconds(10);
  end_time += base::Seconds(10);

  // From here on, make sure the animation runs as normal.
  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = third_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Another activation shouldn't have any effect on the animation.
  host_impl_->ActivateSyncTree();

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;
  begin_frame_args.frame_time = end_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);
  EXPECT_FALSE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame(begin_frame_args);
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_EQ(commit_data->page_scale_delta, target_scale);
  EXPECT_TRUE(ScrollInfoContains(*commit_data, scroll_layer->element_id(),
                                 gfx::Vector2dF(-50, -50)));
}

TEST_P(LayerTreeHostImplTest, PageScaleAnimationCompletedNotification) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  DrawFrame();

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  base::TimeTicks start_time = base::TimeTicks() + base::Seconds(1);
  base::TimeDelta duration = base::Milliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(50, 50));

  did_complete_page_scale_animation_ = false;
  host_impl_->active_tree()->SetPendingPageScaleAnimation(
      std::make_unique<PendingPageScaleAnimation>(gfx::Point(), false, 2,
                                                  duration));
  host_impl_->ActivateSyncTree();
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = end_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

TEST_P(LayerTreeHostImplTest, MaxScrollOffsetAffectedByViewportBoundsDelta) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();

  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  DCHECK(inner_scroll);
  EXPECT_EQ(gfx::SizeF(50, 50),
            host_impl_->active_tree()->ScrollableViewportSize());
  EXPECT_EQ(gfx::PointF(50, 50), MaxScrollOffset(inner_scroll));

  PropertyTrees* property_trees = host_impl_->active_tree()->property_trees();
  property_trees->SetInnerViewportContainerBoundsDelta(gfx::Vector2dF(15, 15));
  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(7, 7));
  // Container grows in response to the inner viewport bounds delta.
  EXPECT_EQ(gfx::SizeF(65, 65),
            host_impl_->active_tree()->ScrollableViewportSize());
  EXPECT_EQ(gfx::PointF(42, 42), MaxScrollOffset(inner_scroll));

  property_trees->SetInnerViewportContainerBoundsDelta(gfx::Vector2dF());
  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF());
  inner_scroll->SetBounds(gfx::Size());
  GetScrollNode(inner_scroll)->bounds = inner_scroll->bounds();
  DrawFrame();

  property_trees->SetOuterViewportContainerBoundsDelta(gfx::Vector2dF(60, 60));
  EXPECT_EQ(gfx::PointF(10, 10), MaxScrollOffset(inner_scroll));
}

// Ensures scroll gestures coming from scrollbars cause animations in the
// appropriate scenarios.
TEST_P(LayerTreeHostImplTest, AnimatedGranularityCausesSmoothScroll) {
  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  gfx::Point position(295, 195);
  gfx::Vector2dF offset(0, 50);

  std::vector<ui::ScrollInputType> types = {ui::ScrollInputType::kScrollbar,
                                            ui::ScrollInputType::kWheel};
  for (auto type : types) {
    auto begin_state = BeginState(position, offset, type);
    begin_state->data()->set_current_native_scrolling_element(
        host_impl_->OuterViewportScrollNode()->element_id);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    auto update_state = UpdateState(position, offset, type);
    update_state.data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    ASSERT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). It should cause an animation to be created.
    {
      GetInputHandler().ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      GetInputHandler().ScrollUpdate(update_state);
      EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

      GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    }

    GetImplAnimationHost()->ScrollAnimationAbort(
        host_impl_->OuterViewportScrollNode()->element_id);
    ASSERT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). This time we change the granularity to precise (as
    // if thumb-dragging). This should not cause an animation.
    {
      begin_state->data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPrecisePixel;
      update_state.data()->delta_granularity =
          ui::ScrollGranularity::kScrollByPrecisePixel;

      GetInputHandler().ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      GetInputHandler().ScrollUpdate(update_state);
      EXPECT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

      GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    }
  }
}

// Ensures scroll gestures coming from scrollbars don't cause animations if
// smooth scrolling is disabled.
TEST_P(LayerTreeHostImplTest, NonAnimatedGranularityCausesInstantScroll) {
  // Disable animated scrolling
  LayerTreeSettings settings = DefaultSettings();
  settings.enable_smooth_scroll = false;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  gfx::Point position(295, 195);
  gfx::Vector2dF offset(0, 50);

  std::vector<ui::ScrollInputType> types = {ui::ScrollInputType::kScrollbar,
                                            ui::ScrollInputType::kWheel};
  for (auto type : types) {
    auto begin_state = BeginState(position, offset, type);
    begin_state->data()->set_current_native_scrolling_element(
        host_impl_->OuterViewportScrollNode()->element_id);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    auto update_state = UpdateState(position, offset, type);
    update_state.data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    ASSERT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

    // Perform a scrollbar-like scroll (one injected by the
    // ScrollbarController). It should cause an animation to be created.
    {
      GetInputHandler().ScrollBegin(begin_state.get(), type);
      ASSERT_EQ(host_impl_->CurrentlyScrollingNode(),
                host_impl_->OuterViewportScrollNode());

      GetInputHandler().ScrollUpdate(update_state);
      EXPECT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

      GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    }
  }
}

class LayerTreeHostImplOverridePhysicalTime : public LayerTreeHostImpl {
 public:
  LayerTreeHostImplOverridePhysicalTime(
      const LayerTreeSettings& settings,
      LayerTreeHostImplDelegate* delegate,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* rendering_stats_instrumentation)
      : LayerTreeHostImpl(
            settings,
            delegate,
            task_runner_provider,
            rendering_stats_instrumentation,
            task_graph_runner,
            AnimationHost::CreateForTesting(ThreadInstance::kImpl),
            nullptr,
            0,
            nullptr,
            nullptr) {}

  const viz::BeginFrameArgs& CurrentBeginFrameArgs() const override {
    return current_begin_frame_args_;
  }

  void SetCurrentPhysicalTimeTicksForTest(base::TimeTicks fake_now) {
    current_begin_frame_args_ = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1, fake_now);
  }

 private:
  viz::BeginFrameArgs current_begin_frame_args_;
};

class LayerTreeHostImplTestScrollbarAnimation : public LayerTreeHostImplTest {
 protected:
  void SetupLayers(LayerTreeSettings settings) {
    host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_ = nullptr;

    gfx::Size viewport_size(50, 50);
    gfx::Size content_size(100, 100);

    LayerTreeHostImplOverridePhysicalTime* host_impl_override_time =
        new LayerTreeHostImplOverridePhysicalTime(
            settings, this, &task_runner_provider_, &task_graph_runner_,
            &stats_instrumentation_);
    InputHandler::Create(
        static_cast<CompositorDelegateForInput&>(*host_impl_override_time));
    host_impl_ = base::WrapUnique(host_impl_override_time);
    layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
    host_impl_->SetVisible(true);
    host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());

    SetupViewportLayersInnerScrolls(viewport_size, content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 4);

    auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
        host_impl_->active_tree(), ScrollbarOrientation::kVertical, 10, 0,
        false);
    SetupScrollbarLayer(OuterViewportScrollLayer(), scrollbar);

    host_impl_->active_tree()->DidBecomeActive();
    host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();
    host_impl_->active_tree()->HandleScrollbarShowRequests();
    host_impl_->active_tree()->SetLocalSurfaceIdFromParent(viz::LocalSurfaceId(
        1, base::UnguessableToken::CreateForTesting(2u, 3u)));

    DrawFrame();

    // SetScrollElementId will initialize the scrollbar which will cause it to
    // show and request a redraw.
    did_request_redraw_ = false;
  }

  void RunTest(LayerTreeSettings::ScrollbarAnimator animator) {
    LayerTreeSettings settings = DefaultSettings();
    settings.scrollbar_animator = animator;
    settings.scrollbar_fade_delay = base::Milliseconds(20);
    settings.scrollbar_fade_duration = base::Milliseconds(20);

    // If no animator is set, scrollbar won't show and no animation is expected.
    bool expecting_animations = animator != LayerTreeSettings::NO_ANIMATOR;

    SetupLayers(settings);

    base::TimeTicks fake_now = base::TimeTicks::Now();

    // Android Overlay Scrollbar does not have a initial show and fade out.
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      // A task will be posted to fade the initial scrollbar.
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_TRUE(animation_task_.is_null());
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    }

    // If no scroll happened during a scroll gesture, it should have no effect.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(), ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());

    // For Aura Overlay Scrollbar, if no scroll happened during a scroll
    // gesture, shows scrollbars and schedules a delay fade out.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(), gfx::Vector2dF(), ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2dF(0, 0), ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      EXPECT_EQ(base::Milliseconds(20), requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    // Before the scrollbar animation exists, we should not get redraws.
    viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 2, fake_now);
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_next_frame_);
    did_request_next_frame_ = false;
    EXPECT_FALSE(did_request_redraw_);
    did_request_redraw_ = false;
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());
    host_impl_->DidFinishImplFrame(begin_frame_args);

    // After a scroll, a scrollbar animation should be scheduled about 20ms from
    // now.
    GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 5),
                                             ui::ScrollInputType::kWheel)
                                      .get(),
                                  ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2dF(0, 5), ui::ScrollInputType::kWheel));
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    did_request_redraw_ = false;
    if (expecting_animations) {
      EXPECT_EQ(base::Milliseconds(20), requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::Milliseconds(20), requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }

    if (expecting_animations) {
      // Before the scrollbar animation begins, we should not get redraws.
      begin_frame_args = viz::CreateBeginFrameArgsForTesting(
          BEGINFRAME_FROM_HERE, 0, 3, fake_now);
      host_impl_->WillBeginImplFrame(begin_frame_args);
      host_impl_->Animate();
      EXPECT_FALSE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_FALSE(did_request_redraw_);
      did_request_redraw_ = false;
      host_impl_->DidFinishImplFrame(begin_frame_args);

      // Start the scrollbar animation.
      fake_now += requested_animation_delay_;
      requested_animation_delay_ = base::TimeDelta();
      std::move(animation_task_).Run();
      EXPECT_TRUE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_FALSE(did_request_redraw_);

      // After the scrollbar animation begins, we should start getting redraws.
      begin_frame_args = viz::CreateBeginFrameArgsForTesting(
          BEGINFRAME_FROM_HERE, 0, 4, fake_now);
      host_impl_->WillBeginImplFrame(begin_frame_args);
      host_impl_->Animate();
      EXPECT_TRUE(did_request_next_frame_);
      did_request_next_frame_ = false;
      EXPECT_TRUE(did_request_redraw_);
      did_request_redraw_ = false;
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
      host_impl_->DidFinishImplFrame(begin_frame_args);
    }

    // Setting the scroll offset outside a scroll should not cause the
    // scrollbar to appear or schedule a scrollbar animation.
    if (host_impl_->active_tree()
            ->property_trees()
            ->scroll_tree_mutable()
            .UpdateScrollOffsetBaseForTesting(
                InnerViewportScrollLayer()->element_id(), gfx::PointF(5, 5)))
      host_impl_->active_tree()->DidUpdateScrollOffset(
          InnerViewportScrollLayer()->element_id(),
          /*pushed_from_main_or_pending_tree=*/false);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.is_null());

    // Changing page scale triggers scrollbar animation.
    host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 4);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(1.1f);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::Milliseconds(20), requested_animation_delay_);
      EXPECT_FALSE(animation_task_.is_null());
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Reset();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.is_null());
    }
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplTestScrollbarAnimation);

TEST_P(LayerTreeHostImplTestScrollbarAnimation, Android) {
  RunTest(LayerTreeSettings::ANDROID_OVERLAY);
}

TEST_P(LayerTreeHostImplTestScrollbarAnimation, AuraOverlay) {
  RunTest(LayerTreeSettings::AURA_OVERLAY);
}

TEST_P(LayerTreeHostImplTestScrollbarAnimation, NoAnimator) {
  RunTest(LayerTreeSettings::NO_ANIMATOR);
}

TEST_P(LayerTreeHostImplTest, ScrollHitTestOnScrollbar) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::NO_ANIMATOR;

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size child_layer_size(250, 150);
  gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
  gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(1);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();

  // scrollbar_1 on root scroll.
  auto* scrollbar_1 = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, true, true);
  SetupScrollbarLayer(root_scroll, scrollbar_1);
  scrollbar_1->SetBounds(scrollbar_size_1);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
  scrollbar_1->SetTouchActionRegion(touch_action_region);

  LayerImpl* child =
      AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
  GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

  // scrollbar_2 on child.
  auto* scrollbar_2 = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, true, true);
  SetupScrollbarLayer(child, scrollbar_2);
  scrollbar_2->SetBounds(scrollbar_size_2);
  scrollbar_2->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  // Wheel scroll on root scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(1, 1), gfx::Vector2dF(),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Touch scroll on root scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(1, 1), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
  }

  // Wheel scroll on scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(51, 51), gfx::Vector2dF(),
                   ui::ScrollInputType::kWheel)
            .get(),
        ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Touch scroll on scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(51, 51), gfx::Vector2dF(),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
  }
}

// This test verifies that we don't crash when a scrollbar layer is hit for
// scroll but the scroller layer can't be found.
TEST_P(LayerTreeHostImplTest, NullScrollerLayerForScrollbarLayer) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  SetupDefaultRootLayer(gfx::Size(200, 200));
  LayerImpl* scroll = AddScrollableLayer(root_layer(), gfx::Size(100, 100),
                                         gfx::Size(1000, 1000));
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayer(scroll, scrollbar);
  scrollbar->SetBounds(gfx::Size(15, 100));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(85, 0));
  UpdateDrawProperties(layer_tree_impl);

  // A successful hit test first.
  auto status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(90, 50), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Set the scrollbar's scroll element id to be different from the scroll
  // layer's. The scroll should be ignored without crash.
  scrollbar->SetScrollElementId(
      ElementId(scrollbar->scroll_element_id().GetInternalValue() + 123));
  UpdateDrawProperties(layer_tree_impl);
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(90, 50), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, ScrollbarInnerLargerThanOuter) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size inner_viewport_size(315, 200);
  gfx::Size outer_viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();
  auto* horiz_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kHorizontal, true, true);
  SetupScrollbarLayer(root_scroll, horiz_scrollbar);
  LayerImpl* child = AddLayerInActiveTree();
  child->SetBounds(content_size);
  child->SetBounds(inner_viewport_size);

  host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();

  EXPECT_EQ(300, horiz_scrollbar->clip_layer_length());
}

TEST_P(LayerTreeHostImplTest, ScrollbarDeviceLargerThanOuter) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  auto* active_tree = host_impl_->active_tree();

  gfx::Size device_size(600, 800);
  gfx::Size outer_viewport_size(300, 400);
  gfx::Size content_size(300, 1000);

  SetupViewportLayers(active_tree, /* inner_viewport_size */ device_size,
                      outer_viewport_size, content_size);
  active_tree->PushPageScaleFromMainThread(/* page_scale_factor */ 2,
                                           /* min_page_scale_factor */ 2,
                                           /* max_page_scale_factor */ 4);

  LayerImpl* root_scroll = OuterViewportScrollLayer();
  auto* horiz_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      active_tree, ScrollbarOrientation::kHorizontal, true, true);
  auto* vert_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      active_tree, ScrollbarOrientation::kVertical, true, true);
  SetupScrollbarLayer(root_scroll, horiz_scrollbar);
  SetupScrollbarLayer(root_scroll, vert_scrollbar);
  LayerImpl* child = AddLayerInActiveTree();
  child->SetBounds(content_size);

  host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();

  EXPECT_EQ(300, horiz_scrollbar->clip_layer_length());
  EXPECT_EQ(300, horiz_scrollbar->scroll_layer_length());
  EXPECT_EQ(400, vert_scrollbar->clip_layer_length());
  EXPECT_EQ(1000, vert_scrollbar->scroll_layer_length());
}

TEST_P(LayerTreeHostImplTest, ScrollbarRegistration) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::ANDROID_OVERLAY;
  settings.scrollbar_fade_delay = base::Milliseconds(20);
  settings.scrollbar_fade_duration = base::Milliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  auto* container = InnerViewportScrollLayer();
  auto* root_scroll = OuterViewportScrollLayer();
  auto* vert_1_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, 5, 5, true);
  CopyProperties(container, vert_1_scrollbar);

  auto* horiz_1_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kHorizontal, 5, 5, true);
  CopyProperties(container, horiz_1_scrollbar);

  auto* vert_2_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, 5, 5, true);
  CopyProperties(container, vert_2_scrollbar);

  auto* horiz_2_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kHorizontal, 5, 5, true);
  CopyProperties(container, horiz_2_scrollbar);

  UpdateDrawProperties(host_impl_->active_tree());

  // Check scrollbar registration on the viewport layers.
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll->element_id()));
  vert_1_scrollbar->SetScrollElementId(root_scroll->element_id());
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      root_scroll->element_id()));
  horiz_1_scrollbar->SetScrollElementId(root_scroll->element_id());
  EXPECT_EQ(2ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      root_scroll->element_id()));

  // Scrolling the viewport should result in a scrollbar animation update.
  animation_task_.Reset();
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(10, 10),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(10, 10), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_FALSE(animation_task_.is_null());
  animation_task_.Reset();

  // Check scrollbar registration on a sublayer.
  LayerImpl* child =
      AddScrollableLayer(root_scroll, viewport_size, gfx::Size(200, 200));
  ElementId child_scroll_element_id = child->element_id();
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         child_scroll_element_id));
  vert_2_scrollbar->SetScrollElementId(child_scroll_element_id);
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      child_scroll_element_id));
  horiz_2_scrollbar->SetScrollElementId(child_scroll_element_id);
  EXPECT_EQ(2ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      child_scroll_element_id));

  // Changing one of the child layers should result in a scrollbar animation
  // update.
  animation_task_.Reset();
  host_impl_->active_tree()->RequestShowScrollbars(child_scroll_element_id);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->HandleScrollbarShowRequests();
  EXPECT_FALSE(animation_task_.is_null());
  animation_task_.Reset();

  // Check scrollbar unregistration.
  ElementId root_scroll_element_id = root_scroll->element_id();
  host_impl_->active_tree()->DetachLayers();
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(root_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll_element_id));
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll_element_id));
}

TEST_P(LayerTreeHostImplTest, ScrollBeforeMouseMove) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_fade_delay = base::Milliseconds(20);
  settings.scrollbar_fade_duration = base::Milliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  auto* root_scroll = OuterViewportScrollLayer();

  const int kScrollbarThickness = 5;
  auto* vert_scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical,
      kScrollbarThickness, 0, false);
  SetupScrollbarLayer(root_scroll, vert_scrollbar);
  vert_scrollbar->SetBounds(gfx::Size(10, 200));
  vert_scrollbar->SetOffsetToTransformParent(
      gfx::Vector2dF(300 - kScrollbarThickness, 0));

  host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();

  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  auto* scrollbar_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kDistanceToTriggerThumb =
      vert_scrollbar->ComputeThumbQuadRect().height() +
      scrollbar_controller
          ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
          .MouseMoveDistanceToTriggerExpand();

  // Move the mouse near the thumb while its at the viewport top.
  auto near_thumb_at_top = gfx::Point(295, kDistanceToTriggerThumb - 1);
  GetInputHandler().MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  // Move the mouse away from the thumb.
  GetInputHandler().MouseMoveAt(gfx::Point(295, kDistanceToTriggerThumb + 1));
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  // Scroll the page down which moves the thumb down to the viewport bottom.
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 800),
                                           ui::ScrollInputType::kWheel)
                                    .get(),
                                ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 800), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Move the mouse near the thumb in the top position.
  GetInputHandler().MouseMoveAt(near_thumb_at_top);
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  // Scroll the page up which moves the thumb back up.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, -800),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -800), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Move the mouse near the thumb in the top position.
  GetInputHandler().MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
}



TEST_P(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf1) {
  SetupMouseMoveAtWithDeviceScale(1);
}

TEST_P(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf2) {
  SetupMouseMoveAtWithDeviceScale(2);
}

// This test verifies that only SurfaceLayers in the viewport and have fallbacks
// that are different are included in viz::CompositorFrameMetadata's
// |activation_dependencies|.
// These tests use FakeLayerTreeFrameSink properties that are not set in
// TreesInViz Client mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       ActivationDependenciesInMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* root = root_layer();

  std::vector<viz::SurfaceId> primary_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(1, 1), 1),
      MakeSurfaceId(viz::FrameSinkId(2, 2), 2),
      MakeSurfaceId(viz::FrameSinkId(3, 3), 3)};

  std::vector<viz::SurfaceId> fallback_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(4, 4), 1),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 2),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 3)};

  for (size_t i = 0; i < primary_surfaces.size(); ++i) {
    auto* child = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
    child->SetBounds(gfx::Size(1, 1));
    child->SetDrawsContent(true);
    child->SetRange(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]), 2u);
    CopyProperties(root, child);
    child->SetOffsetToTransformParent(gfx::Vector2dF(25.0f * i, 0));
  }

  base::flat_set<viz::SurfaceRange> surfaces_set;
  // |fallback_surfaces| and |primary_surfaces| should have same size
  for (size_t i = 0; i < fallback_surfaces.size(); ++i) {
    surfaces_set.insert(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]));
  }

  host_impl_->active_tree()->SetSurfaceRanges(std::move(surfaces_set));
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    auto* fake_layer_tree_frame_sink = static_cast<FakeLayerTreeFrameSink*>(
        host_impl_->layer_tree_frame_sink());
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.activation_dependencies,
                testing::UnorderedElementsAre(primary_surfaces[0],
                                              primary_surfaces[1]));
    EXPECT_THAT(
        metadata.referenced_surfaces,
        testing::UnorderedElementsAre(
            viz::SurfaceRange(fallback_surfaces[0], primary_surfaces[0]),
            viz::SurfaceRange(fallback_surfaces[1], primary_surfaces[1]),
            viz::SurfaceRange(fallback_surfaces[2], primary_surfaces[2])));
    EXPECT_EQ(2u, metadata.deadline.deadline_in_frames());
    EXPECT_FALSE(metadata.deadline.use_default_lower_bound_deadline());
  }

  // Verify that on the next frame generation that the deadline is reset.
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    auto* fake_layer_tree_frame_sink = static_cast<FakeLayerTreeFrameSink*>(
        host_impl_->layer_tree_frame_sink());
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.activation_dependencies,
                testing::UnorderedElementsAre(primary_surfaces[0],
                                              primary_surfaces[1]));
    EXPECT_THAT(
        metadata.referenced_surfaces,
        testing::UnorderedElementsAre(
            viz::SurfaceRange(fallback_surfaces[0], primary_surfaces[0]),
            viz::SurfaceRange(fallback_surfaces[1], primary_surfaces[1]),
            viz::SurfaceRange(fallback_surfaces[2], primary_surfaces[2])));
    EXPECT_EQ(0u, metadata.deadline.deadline_in_frames());
    EXPECT_FALSE(metadata.deadline.use_default_lower_bound_deadline());
  }
}

// Verify that updating the set of referenced surfaces for the active tree
// causes a new CompositorFrame to be submitted, even if there is no other
// damage.
// These tests use FakeLayerTreeFrameSink properties that are not set in
// TreesInViz Client mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       SurfaceReferencesChangeCausesDamage) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // Submit an initial CompositorFrame with an empty set of referenced surfaces.
  host_impl_->active_tree()->SetSurfaceRanges({});
  host_impl_->SetFullViewportDamage();
  DrawFrame();

  {
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.referenced_surfaces, testing::IsEmpty());
  }

  const viz::SurfaceId surface_id = MakeSurfaceId(viz::FrameSinkId(1, 1), 1);

  // Update the set of referenced surfaces to contain |surface_id| but don't
  // make any other changes that would cause damage. This mimics updating the
  // SurfaceLayer for an offscreen tab.
  host_impl_->active_tree()->SetSurfaceRanges({viz::SurfaceRange(surface_id)});
  DrawFrame();

  {
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(surface_id)));
  }
}

TEST_P(LayerTreeHostImplTest, CompositorFrameMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::PointF(), metadata.root_scroll_offset);
    EXPECT_EQ(1, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(50, 50), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kWheel));
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
  }
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
  }

  // Page scale should update metadata correctly (shrinking only the viewport).
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, gfx::Point());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(2, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(25, 25), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessCompositorDeltas(/* main_thread_mutator_host */ nullptr);
  host_impl_->active_tree()->PushPageScaleFromMainThread(4, 0.5f, 4);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(4);
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(4, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
  }
}

// These tests rely on LayerTreeHostImpl to produce CompositorFrame quad data,
// which is not enabled for TreesInViz Client mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       DamageShouldNotCareAboutContributingLayers) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));

  // Make a child layer that draws.
  auto* layer = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  layer->SetBounds(gfx::Size(10, 10));
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SkColors::kRed);
  CopyProperties(root, layer);

  UpdateDrawProperties(host_impl_->active_tree());

  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_NE(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }

  // Stops the child layer from drawing. We should have damage from this but
  // should not have any quads. This should clear the damaged area.
  layer->SetDrawsContent(false);
  GetEffectNode(root)->opacity = 0;

  UpdateDrawProperties(host_impl_->active_tree());
  // The background is default to transparent. If the background is opaque, we
  // would fill the frame with background colour when no layers are contributing
  // quads. This means we would end up with 0 quad.
  EXPECT_EQ(host_impl_->active_tree()->background_color(),
            SkColors::kTransparent);

  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }

  // Now tries to draw again. Nothing changes, so should have no damage, no
  // render pass, and no quad.
  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

    EXPECT_TRUE(frame.has_no_damage);
    EXPECT_EQ(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
}

TEST_P(LayerTreeHostImplTest, WillDrawReturningFalseDoesNotCall) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  auto* layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  CopyProperties(root, layer);

  DrawFrame();
  EXPECT_TRUE(layer->will_draw_returned_true());
  EXPECT_EQ(layer->append_quads_called(),
            !host_impl_->GetSettings().TreesInVizInClientProcess());
  EXPECT_TRUE(layer->did_draw_called());

  host_impl_->SetViewportDamage(gfx::Rect(10, 10));

  layer->set_will_draw_returns_false();
  layer->ClearDidDrawCheck();
  DrawFrame();
  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->append_quads_called());
  EXPECT_FALSE(layer->did_draw_called());
}

TEST_P(LayerTreeHostImplTest, DidDrawNotCalledOnHiddenLayer) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  CreateClipNode(root);
  auto* layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  layer->SetBounds(gfx::Size(10, 10));
  CopyProperties(root, layer);
  // Ensure visible_layer_rect for layer is not empty
  layer->SetOffsetToTransformParent(gfx::Vector2dF(100, 100));
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  DrawFrame();

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(layer->visible_layer_rect().IsEmpty());

  // Ensure visible_layer_rect for layer is not empty
  layer->SetOffsetToTransformParent(gfx::Vector2dF());
  layer->NoteLayerPropertyChanged();
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  DrawFrame();

  EXPECT_TRUE(layer->will_draw_returned_true());
  EXPECT_TRUE(layer->did_draw_called());

  EXPECT_FALSE(layer->visible_layer_rect().IsEmpty());
}

TEST_P(LayerTreeHostImplTest, WillDrawNotCalledOnOccludedLayer) {
  gfx::Size big_size(1000, 1000);
  auto* root =
      SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(), big_size);

  auto* occluded_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  CopyProperties(root, occluded_layer);
  auto* top_layer = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  // This layer covers the occluded_layer above. Make this layer large so it can
  // occlude.
  top_layer->SetBounds(big_size);
  top_layer->SetContentsOpaque(true);
  CopyProperties(occluded_layer, top_layer);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_FALSE(top_layer->will_draw_returned_true());
  EXPECT_FALSE(top_layer->did_draw_called());

  DrawFrame();

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_TRUE(top_layer->will_draw_returned_true());
  EXPECT_TRUE(top_layer->did_draw_called());
}

TEST_P(LayerTreeHostImplTest, DidDrawCalledOnAllLayers) {
  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  auto* layer1 = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());
  auto* layer2 = AddLayer<DidDrawCheckLayer>(host_impl_->active_tree());

  CopyProperties(root, layer1);
  CreateTransformNode(layer1).flattens_inherited_transform = true;
  CreateEffectNode(layer1).render_surface_reason = RenderSurfaceReason::kTest;
  CopyProperties(layer1, layer2);

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_FALSE(root->did_draw_called());
  EXPECT_FALSE(layer1->did_draw_called());
  EXPECT_FALSE(layer2->did_draw_called());

  DrawFrame();

  EXPECT_TRUE(root->did_draw_called());
  EXPECT_TRUE(layer1->did_draw_called());
  EXPECT_TRUE(layer2->did_draw_called());

  EXPECT_NE(GetRenderSurface(root), GetRenderSurface(layer1));
  EXPECT_TRUE(GetRenderSurface(layer1));
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
 public:
  static std::unique_ptr<MissingTextureAnimatingLayer> Create(
      LayerTreeImpl* tree_impl,
      int id,
      bool tile_missing,
      bool animating,
      scoped_refptr<AnimationTimeline> timeline) {
    return base::WrapUnique(new MissingTextureAnimatingLayer(
        tree_impl, id, tile_missing, animating, timeline));
  }

  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    LayerImpl::AppendQuads(context, render_pass, append_quads_data);
    if (tile_missing_) {
      append_quads_data->num_missing_tiles++;
    }
  }

 private:
  MissingTextureAnimatingLayer(LayerTreeImpl* tree_impl,
                               int id,
                               bool tile_missing,
                               bool animating,
                               scoped_refptr<AnimationTimeline> timeline)
      : DidDrawCheckLayer(tree_impl, id), tile_missing_(tile_missing) {
    if (animating) {
      this->SetElementId(LayerIdToElementIdForTesting(id));
      AddAnimatedTransformToElementWithAnimation(this->element_id(), timeline,
                                                 10.0, 3, 0);
    }
  }

  bool tile_missing_;
};

struct PrepareToDrawSuccessTestCase {
  explicit PrepareToDrawSuccessTestCase(DrawResult result)
      : expected_result(result) {}

  struct State {
    bool has_missing_tile = false;
    bool is_animating = false;
  };

  bool high_res_required = false;
  bool has_view_transition_save_directive = false;
  State layer_before;
  State layer_between;
  State layer_after;
  DrawResult expected_result;
};

class LayerTreeHostImplPrepareToDrawTest : public LayerTreeHostImplTest {
 public:
  void CreateLayerFromState(DidDrawCheckLayer* root,
                            const scoped_refptr<AnimationTimeline>& timeline,
                            const PrepareToDrawSuccessTestCase::State& state) {
    auto* layer = AddLayer<MissingTextureAnimatingLayer>(
        root->layer_tree_impl(), state.has_missing_tile, state.is_animating,
        timeline);
    CopyProperties(root, layer);
    if (state.is_animating) {
      CreateTransformNode(layer).has_potential_animation = true;
    }

    // Setup TileManager correctly by adding tiles to the layer, but only if
    // we want to simulate missing tiles and we are in a mode where we can
    // safely set a raster source on the active tree.
    if (CommitsToActiveTree() &&
        !host_impl_->GetSettings().trees_in_viz_in_viz_process &&
        state.has_missing_tile) {
      layer->SetBounds(gfx::Size(10, 10));
      layer->SetDrawsContent(true);
      layer->set_has_valid_tile_priorities(true);

      layer->SetRasterSource(FakeRasterSource::CreateFilled(gfx::Size(10, 10)),
                             Region());
      layer->AddTiling(gfx::AxisTransform2d(1.f, gfx::Vector2dF()));
      layer->CreateAllTiles();

      // Update draw properties so TileManager sees the priorities.
      UpdateDrawProperties(root->layer_tree_impl());
    }
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplPrepareToDrawTest);

class DisabledForVizClientLayerTreeHostImplPrepareToDrawTest
    : public LayerTreeHostImplPrepareToDrawTest {
 public:
  bool InVizService() {
    return host_impl_->GetSettings().trees_in_viz_in_viz_process;
  }
};

INSTANTIATE_COMPOSITOR_FRAME_PRODUCING_TREE_TEST_P(
    DisabledForVizClientLayerTreeHostImplPrepareToDrawTest);

TEST_P(DisabledForVizClientLayerTreeHostImplPrepareToDrawTest,
       PrepareToDrawSucceedsAndFails) {
  CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());

  std::vector<PrepareToDrawSuccessTestCase> cases;
  // 0. Default case.
  cases.emplace_back(DrawResult::kSuccess);
  // 1. Animated layer first.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_before.is_animating = true;
  // 2. Animated layer between.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_between.is_animating = true;
  // 3. Animated layer last.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_after.is_animating = true;
  // 4. Missing tile first.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_before.has_missing_tile = true;
  // 5. Missing tile between.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_between.has_missing_tile = true;
  // 6. Missing tile last.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_after.has_missing_tile = true;
  // 7. Animation with missing tile.
  cases.emplace_back(CommitsToActiveTree()
                         ? DrawResult::kSuccess
                         : DrawResult::kAbortedCheckerboardAnimations);
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 8 . High res required.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().high_res_required = true;
  // 9 . High res required with missing tile.
  cases.emplace_back((InVizService() || !CommitsToActiveTree())
                         ? DrawResult::kSuccess
                         : DrawResult::kAbortedMissingHighResContent);
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  // 10. High res required is higher priority than animating missing tiles.
  cases.emplace_back(InVizService()
                         ? DrawResult::kSuccess
                         : (CommitsToActiveTree()
                                ? DrawResult::kAbortedMissingHighResContent
                                : DrawResult::kAbortedCheckerboardAnimations));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_after.has_missing_tile = true;
  cases.back().layer_after.is_animating = true;
  // 11. High res required is higher priority than animating missing tiles.
  cases.emplace_back(InVizService()
                         ? DrawResult::kSuccess
                         : (CommitsToActiveTree()
                                ? DrawResult::kAbortedMissingHighResContent
                                : DrawResult::kAbortedCheckerboardAnimations));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_before.has_missing_tile = true;
  cases.back().layer_before.is_animating = true;
  // 12. checkerboarded animated content with a view transition save directive.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().has_view_transition_save_directive = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;

  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  DrawFrame();

  for (size_t i = 0; i < cases.size(); ++i) {
    // Clean up host_impl_ state.
    const auto& testcase = cases[i];
    host_impl_->active_tree()->DetachLayersKeepingRootLayerForTesting();
    timeline()->ClearAnimations();

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);
    UpdateDrawProperties(host_impl_->active_tree());

    if (testcase.has_view_transition_save_directive) {
      host_impl_->active_tree()->AddViewTransitionRequest(
          ViewTransitionRequest::CreateCapture(
              blink::ViewTransitionToken(), false, {},
              base::DoNothingAs<void(
                  const viz::ViewTransitionElementResourceRects&)>(),
              false));
    }

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(testcase.expected_result, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
}

TEST_P(LayerTreeHostImplPrepareToDrawTest,
       PrepareToDrawWhenDrawAndSwapFullViewportEveryFrame) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());

  const gfx::Transform external_transform;
  const gfx::Rect external_viewport;
  const bool resourceless_software_draw = true;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  std::vector<PrepareToDrawSuccessTestCase> cases;

  // 0. Default case.
  cases.emplace_back(DrawResult::kSuccess);
  // 1. Animation with missing tile.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 2. High res required with missing tile.
  cases.emplace_back(DrawResult::kSuccess);
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;

  auto* root = SetupRootLayer<DidDrawCheckLayer>(host_impl_->active_tree(),
                                                 gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  for (size_t i = 0; i < cases.size(); ++i) {
    const auto& testcase = cases[i];
    host_impl_->active_tree()->DetachLayersKeepingRootLayerForTesting();

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    host_impl_->OnDraw(external_transform, external_viewport,
                       resourceless_software_draw, false);
  }
}

TEST_P(LayerTreeHostImplTest, ScrollRootIgnored) {
  SetupDefaultRootLayer(gfx::Size(10, 10));
  DrawFrame();

  // Scroll event is ignored because layer is not scrollable and there is no
  // viewport.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_P(LayerTreeHostImplTest, ScrollNonCompositedRoot) {
  // Test the configuration where a non-composited root layer is embedded in a
  // scrollable outer layer.
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);

  LayerImpl* scroll_container_layer = AddContentLayer();
  CreateEffectNode(scroll_container_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  LayerImpl* scroll_layer =
      AddScrollableLayer(scroll_container_layer, surface_size, contents_size);

  LayerImpl* content_layer = AddLayerInActiveTree();
  content_layer->SetDrawsContent(true);
  content_layer->SetBounds(contents_size);
  CopyProperties(scroll_layer, content_layer);

  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);
}

TEST_P(LayerTreeHostImplTest, ScrollChildCallsCommitAndRedraw) {
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);

  LayerImpl* content_root = AddContentLayer();
  CreateEffectNode(content_root).render_surface_reason =
      RenderSurfaceReason::kTest;
  AddScrollableLayer(content_root, surface_size, contents_size);

  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);
}

TEST_P(LayerTreeHostImplTest, ScrollMissesChild) {
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);
  SetupViewportLayersOuterScrolls(viewport_size, surface_size);
  AddScrollableLayer(OuterViewportScrollLayer(), viewport_size, surface_size);
  DrawFrame();

  // A scroll that doesn't hit any layers should fall back to viewport
  // scrolling.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(15, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode(),
            host_impl_->OuterViewportScrollNode());

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_P(LayerTreeHostImplTest, ScrollMissesBackfacingChild) {
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);

  SetupViewportLayersOuterScrolls(viewport_size, surface_size);
  LayerImpl* child = AddScrollableLayer(OuterViewportScrollLayer(),
                                        viewport_size, surface_size);

  gfx::Transform matrix;
  matrix.RotateAboutXAxis(180.0);

  GetTransformNode(child)->local = matrix;
  CreateEffectNode(child).double_sided = false;
  DrawFrame();

  // The scroll shouldn't hit the child layer because the it isn't facing the
  // viewer. The hit test should go through it and hit the outer viewport.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode(),
            host_impl_->OuterViewportScrollNode());

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_P(LayerTreeHostImplTest, ScrollLayerWithMainThreadReason) {
  gfx::Size scroll_container_size(5, 5);
  gfx::Size surface_size(10, 10);
  LayerImpl* root = SetupDefaultRootLayer(surface_size);

  // Note: we can use the same clip layer for both since both calls to
  // AddScrollableLayer() use the same surface size.
  LayerImpl* scroll_layer =
      AddScrollableLayer(root, scroll_container_size, surface_size);
  LayerImpl* content_layer =
      AddScrollableLayer(scroll_layer, scroll_container_size, surface_size);
  GetScrollNode(content_layer)->main_thread_repaint_reasons =
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            host_impl_->CurrentlyScrollingNode()->main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_repaint_reasons);
}

TEST_P(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnMainThread) {
  gfx::Size inner_viewport_size(20, 20);
  gfx::Size outer_viewport_size(40, 40);
  gfx::Size content_size(80, 80);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2dF expected_scroll_delta(scroll_delta);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  gfx::PointF expected_max_scroll = MaxScrollOffset(outer_scroll);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Set new page scale from main thread.
  float page_scale = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1, 2);

  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, MaxScrollOffset(outer_scroll));

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1, host_impl_->active_tree()->page_scale_delta());
}

TEST_P(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnImplThread) {
  gfx::Size inner_viewport_size(20, 20);
  gfx::Size outer_viewport_size(40, 40);
  gfx::Size content_size(80, 80);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2dF expected_scroll_delta(scroll_delta);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  gfx::PointF expected_max_scroll = MaxScrollOffset(outer_scroll);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Set new page scale on impl thread by pinching.
  float page_scale = 2;
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(page_scale, gfx::Point());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  DrawOneFrame();

  // The scroll delta is not scaled because the main thread did not scale.
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, MaxScrollOffset(outer_scroll));

  // The page scale delta should match the new scale on the impl side.
  EXPECT_EQ(page_scale, host_impl_->active_tree()->current_page_scale_factor());
}

TEST_P(LayerTreeHostImplTest, PageScaleDeltaAppliedToRootScrollLayerOnly) {
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 2);
  gfx::Size viewport_size(5, 5);
  gfx::Size surface_size(10, 10);
  float default_page_scale = 1;
  gfx::Transform default_page_scale_matrix;
  default_page_scale_matrix.Scale(default_page_scale, default_page_scale);

  float new_page_scale = 2;
  gfx::Transform new_page_scale_matrix;
  new_page_scale_matrix.Scale(new_page_scale, new_page_scale);

  SetupViewportLayersInnerScrolls(viewport_size, surface_size);
  LayerImpl* root = root_layer();
  auto* inner_scroll = InnerViewportScrollLayer();
  auto* outer_scroll = OuterViewportScrollLayer();

  // Create a normal scrollable root layer and another scrollable child layer.
  LayerImpl* scrollable_child_clip = AddLayerInActiveTree();
  CopyProperties(inner_scroll, scrollable_child_clip);
  AddScrollableLayer(scrollable_child_clip, viewport_size, surface_size);
  UpdateDrawProperties(host_impl_->active_tree());

  // Set new page scale on impl thread by pinching.
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(new_page_scale, gfx::Point());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  DrawOneFrame();

  // Make sure all the layers are drawn with the page scale delta applied, i.e.,
  // the page scale delta on the root layer is applied hierarchically.
  DrawFrame();

  EXPECT_EQ(1, root->DrawTransform().rc(0, 0));
  EXPECT_EQ(1, root->DrawTransform().rc(1, 1));
  EXPECT_EQ(new_page_scale, inner_scroll->DrawTransform().rc(0, 0));
  EXPECT_EQ(new_page_scale, inner_scroll->DrawTransform().rc(1, 1));
  EXPECT_EQ(new_page_scale, outer_scroll->DrawTransform().rc(0, 0));
  EXPECT_EQ(new_page_scale, outer_scroll->DrawTransform().rc(1, 1));
}

TEST_P(LayerTreeHostImplTest, ScrollChildAndChangePageScaleOnMainThread) {
  SetupViewportLayers(host_impl_->active_tree(), gfx::Size(15, 15),
                      gfx::Size(30, 30), gfx::Size(50, 50));
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::Vector2dF expected_scroll_delta(scroll_delta);
  gfx::PointF expected_max_scroll(MaxScrollOffset(outer_scroll));
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  float page_scale = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1,
                                                         page_scale);
  DrawOneFrame();

  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should not have changed.
  EXPECT_EQ(MaxScrollOffset(outer_scroll), expected_max_scroll);

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1, host_impl_->active_tree()->page_scale_delta());
}

TEST_P(LayerTreeHostImplTest, ScrollChildBeyondLimit) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);

  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* top = AddContentLayer();
  CreateEffectNode(top).render_surface_reason = RenderSurfaceReason::kTest;
  LayerImpl* child_layer = AddScrollableLayer(top, surface_size, content_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, surface_size, content_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(grand_child_layer->element_id(),
                                        gfx::PointF(0, 5));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                        gfx::PointF(3, 0));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(-8, -7);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);

    // The grand child should have scrolled up to its limit.
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(),
                                   grand_child_layer->element_id(),
                                   gfx::Vector2dF(0, -5)));

    // The child should not have scrolled.
    ExpectNone(*commit_data.get(), child_layer->element_id());
  }
}

TEST_P(LayerTreeHostImplTimelinesTest, ScrollAnimatedLatchToChild) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(100, 100);
  gfx::Size content_size(150, 150);

  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* top = AddContentLayer();
  CreateEffectNode(top).render_surface_reason = RenderSurfaceReason::kTest;
  LayerImpl* child_layer = AddScrollableLayer(top, surface_size, content_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, surface_size, content_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(grand_child_layer->element_id(),
                                        gfx::PointF(0, 30));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                        gfx::PointF(0, 50));

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(10);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, -100),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, -100)));

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should have started scrolling.
  EXPECT_NE(gfx::PointF(0, 30), CurrentScrollOffset(grand_child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::PointF(0, 0), CurrentScrollOffset(grand_child_layer));
  EXPECT_EQ(gfx::PointF(0, 50), CurrentScrollOffset(child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Second ScrollAnimated should remain latched to the grand_child_layer.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, -100)));

  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(450);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::PointF(0, 0), CurrentScrollOffset(grand_child_layer));
  EXPECT_EQ(gfx::PointF(0, 50), CurrentScrollOffset(child_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest, ScrollWithoutBubbling) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // the scroll doesn't bubble up to the parent layer.
  gfx::Size surface_size(20, 20);
  gfx::Size viewport_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* child_layer = AddScrollableLayer(InnerViewportScrollLayer(),
                                              viewport_size, surface_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, viewport_size, surface_size);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  gfx::PointF grand_child_base(0, 2);
  gfx::Vector2dF grand_child_delta;
  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(grand_child_layer->element_id(),
                                        grand_child_base);
  gfx::PointF child_base(0, 3);
  gfx::Vector2dF child_delta;
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(child_layer->element_id(), child_base);

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
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

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);

    // The grand child should have scrolled up to its limit.
    grand_child_delta = gfx::Vector2dF(0, -2);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(),
                                   grand_child_layer->element_id(),
                                   grand_child_delta));

    // The child should not have scrolled.
    ExpectNone(*commit_data.get(), child_layer->element_id());

    grand_child_base += grand_child_delta;
    child_base += child_delta;
    PushScrollOffsetsToPendingTree(
        {{child_layer->element_id(), child_base},
         {grand_child_layer->element_id(), grand_child_base}});

    // The next time we scroll we should only scroll the parent.
    scroll_delta = gfx::Vector2d(0, -3);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    ClearMainThreadDeltasForTesting(host_impl_.get());
    commit_data = host_impl_->ProcessCompositorDeltas(
        /* main_thread_mutator_host */ nullptr);

    // The child should have scrolled up to its limit.
    child_delta = gfx::Vector2dF(0, -3);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(),
                                   child_layer->element_id(), child_delta));

    // The grand child should not have scrolled.
    grand_child_delta = gfx::Vector2dF();
    ExpectNone(*commit_data.get(), grand_child_layer->element_id());

    child_base += child_delta;
    grand_child_base += grand_child_delta;
    PushScrollOffsetsToPendingTree(
        {{grand_child_layer->element_id(), grand_child_base},
         {child_layer->element_id(), child_base}});

    // After scrolling the parent, another scroll on the opposite direction
    // should still scroll the child.
    scroll_delta = gfx::Vector2d(0, 7);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    ClearMainThreadDeltasForTesting(host_impl_.get());
    commit_data = host_impl_->ProcessCompositorDeltas(
        /* main_thread_mutator_host */ nullptr);

    // The grand child should have scrolled.
    grand_child_delta = gfx::Vector2dF(0, 7);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(),
                                   grand_child_layer->element_id(),
                                   grand_child_delta));

    // The child should not have scrolled.
    child_delta = gfx::Vector2dF();
    ExpectNone(*commit_data.get(), child_layer->element_id());

    grand_child_base += grand_child_delta;
    child_base += child_delta;
    PushScrollOffsetsToPendingTree(
        {{grand_child_layer->element_id(), grand_child_base},
         {child_layer->element_id(), child_base}});

    // Scrolling should be adjusted from viewport space.
    host_impl_->active_tree()->PushPageScaleFromMainThread(2, 2, 2);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(2);

    scroll_delta = gfx::Vector2d(0, -2);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(1, 1), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(grand_child_layer->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    ClearMainThreadDeltasForTesting(host_impl_.get());
    commit_data = host_impl_->ProcessCompositorDeltas(
        /* main_thread_mutator_host */ nullptr);

    // Should have scrolled by half the amount in layer space (-2/2)
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(),
                                   grand_child_layer->element_id(),
                                   gfx::Vector2dF(0, -1)));
  }
}

// Ensure that layers who's scroll parent is the InnerViewportScrollNode are
// still able to scroll on the compositor.
TEST_P(LayerTreeHostImplTest, ChildrenOfInnerScrollNodeCanScrollOnThread) {
  gfx::Size viewport_size(10, 10);
  gfx::Size content_size(20, 20);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  // Simulate adding a "fixed" layer to the tree.
  {
    LayerImpl* fixed_layer = AddLayerInActiveTree();
    fixed_layer->SetBounds(viewport_size);
    fixed_layer->SetDrawsContent(true);
    CopyProperties(InnerViewportScrollLayer(), fixed_layer);
  }

  host_impl_->active_tree()->DidBecomeActive();
  DrawFrame();
  {
    gfx::Vector2dF scroll_delta(0, 4);
    // Scrolling should be able to happen on the compositor thread here.
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);

    // The outer viewport should have scrolled.
    ASSERT_EQ(commit_data->scrolls.size(), 1u);
    EXPECT_TRUE(ScrollInfoContains(
        *commit_data.get(), host_impl_->OuterViewportScrollNode()->element_id,
        scroll_delta));
  }
}

TEST_P(LayerTreeHostImplTest, ScrollEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible.
  gfx::Size viewport_size(10, 10);
  gfx::Size content_size(20, 20);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  // Add a scroller whose scroll bounds and scroll container bounds are equal.
  // Since the max_scroll_offset is 0, scrolls will bubble.
  LayerImpl* scroll_child_clip = AddContentLayer();
  AddScrollableLayer(scroll_child_clip, gfx::Size(10, 10), gfx::Size(10, 10));

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();
  DrawFrame();
  {
    gfx::Vector2dF scroll_delta(0, 4);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);

    // Only the root scroll should have scrolled.
    ASSERT_EQ(commit_data->scrolls.size(), 1u);
    EXPECT_TRUE(ScrollInfoContains(
        *commit_data.get(), host_impl_->OuterViewportScrollNode()->element_id,
        scroll_delta));
  }
}

TEST_P(LayerTreeHostImplTest, ScrollBeforeRedraw) {
  gfx::Size surface_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  // Draw one frame and then immediately rebuild the layer tree to mimic a tree
  // synchronization.
  DrawFrame();

  ClearLayersAndPropertyTrees(host_impl_->active_tree());
  SetupViewportLayersNoScrolls(surface_size);

  // Scrolling should still work even though we did not draw yet.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(5, 5), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest, ScrollAxisAlignedRotatedLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->SetDrawsContent(true);

  // Rotate the root layer 90 degrees counter-clockwise about its center.
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(-90.0);
  // Set external transform.
  host_impl_->OnDraw(rotate_transform, gfx::Rect(0, 0, 50, 50), false, false);
  DrawFrame();

  // Scroll to the right in screen coordinates with a gesture.
  gfx::Vector2d gesture_scroll_delta(10, 0);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gesture_scroll_delta,
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gesture_scroll_delta, ui::ScrollInputType::kTouchscreen));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The layer should have scrolled down in its local coordinates.
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), scroll_layer->element_id(),
                                 gfx::Vector2dF(0, gesture_scroll_delta.x())));

  // Push scrolls to pending tree
  PushScrollOffsetsToPendingTree(
      {{scroll_layer->element_id(), gfx::PointF(10, 0)}});
  ClearNonScrollSyncTreeDeltasForTesting();

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::Vector2dF wheel_scroll_delta(0, 10);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), wheel_scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), wheel_scroll_delta,
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The layer should have scrolled down in its local coordinates.
  commit_data = host_impl_->ProcessCompositorDeltas(
      /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest,
       ScrollNonAxisAlignedRotatedLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  float child_layer_angle = -20;

  // Create a child layer that is rotated to a non-axis-aligned angle.
  // Only allow vertical scrolling.
  gfx::Size content_size = scroll_layer->bounds();
  gfx::Size scroll_container_bounds(content_size.width(),
                                    content_size.height() / 2);
  LayerImpl* clip_layer = AddLayerInActiveTree();
  clip_layer->SetBounds(scroll_container_bounds);
  CopyProperties(scroll_layer, clip_layer);
  gfx::Transform rotate_transform;
  rotate_transform.Translate(-50.0, -50.0);
  rotate_transform.Rotate(child_layer_angle);
  rotate_transform.Translate(50.0, 50.0);
  auto& clip_layer_transform_node = CreateTransformNode(clip_layer);
  // The rotation depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer_transform_node.origin = gfx::Point3F(
      clip_layer->bounds().width() * 0.5f, clip_layer->bounds().height(), 0);
  clip_layer_transform_node.local = rotate_transform;

  LayerImpl* child =
      AddScrollableLayer(clip_layer, scroll_container_bounds, content_size);

  ElementId child_scroll_id = child->element_id();

  DrawFrame();
  {
    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d gesture_scroll_delta(0, 10);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(gfx::Point(1, 1), gesture_scroll_delta,
                                    ui::ScrollInputType::kTouchscreen)
                             .get(),
                         ui::ScrollInputType::kTouchscreen)
            .thread);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gesture_scroll_delta, ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::Vector2dF expected_scroll_delta(
        0, std::floor(gesture_scroll_delta.y() *
                      std::cos(base::DegToRad(child_layer_angle))));
    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(commit_data->scrolls.size(), 1u);

    PushScrollOffsetsToPendingTree(
        {{child_scroll_id,
          gfx::PointAtOffsetFromOrigin(expected_scroll_delta)}});
    ClearNonScrollSyncTreeDeltasForTesting();
  }
  {
    // Now reset and scroll the same amount horizontally.
    SetScrollOffsetDelta(child, gfx::Vector2dF());
    gfx::Vector2d gesture_scroll_delta(10, 0);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(gfx::Point(1, 1), gesture_scroll_delta,
                                    ui::ScrollInputType::kTouchscreen)
                             .get(),
                         ui::ScrollInputType::kTouchscreen)
            .thread);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gesture_scroll_delta, ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::Vector2dF expected_scroll_delta(
        0, std::floor(-gesture_scroll_delta.x() *
                      std::sin(base::DegToRad(child_layer_angle))));
    std::unique_ptr<CompositorCommitData> commit_data =
        host_impl_->ProcessCompositorDeltas(
            /* main_thread_mutator_host */ nullptr);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer shouldn't have scrolled.
    ExpectNone(*commit_data.get(), scroll_layer->element_id());

    PushScrollOffsetsToPendingTree(
        {{child_scroll_id,
          gfx::PointAtOffsetFromOrigin(expected_scroll_delta)}});
  }
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest,
       ScrollPerspectiveTransformedLayer) {
  // When scrolling an element with perspective, the distance scrolled
  // depends on the point at which the scroll begins.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // Create a child layer that is rotated on its x axis, with perspective.
  LayerImpl* clip_layer = AddLayerInActiveTree();
  clip_layer->SetBounds(gfx::Size(50, 50));
  CopyProperties(scroll_layer, clip_layer);
  gfx::Transform perspective_transform;
  perspective_transform.Translate(-50.0, -50.0);
  perspective_transform.ApplyPerspectiveDepth(20);
  perspective_transform.RotateAboutXAxis(45);
  perspective_transform.Translate(50.0, 50.0);
  auto& clip_layer_transform_node = CreateTransformNode(clip_layer);
  // The transform depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer_transform_node.origin = gfx::Point3F(
      clip_layer->bounds().width(), clip_layer->bounds().height(), 0);
  clip_layer_transform_node.local = perspective_transform;

  LayerImpl* child = AddScrollableLayer(clip_layer, clip_layer->bounds(),
                                        scroll_layer->bounds());

  UpdateDrawProperties(host_impl_->active_tree());

  std::unique_ptr<CompositorCommitData> commit_data;

  std::array<gfx::Vector2dF, 4> gesture_scroll_deltas;
  gesture_scroll_deltas[0] = gfx::Vector2dF(4, 10);
  gesture_scroll_deltas[1] = gfx::Vector2dF(4, 10);
  gesture_scroll_deltas[2] = gfx::Vector2dF(10, 0);
  gesture_scroll_deltas[3] = gfx::Vector2dF(10, 0);

  std::array<gfx::Vector2dF, 4> expected_scroll_deltas;
  // Perspective affects the vertical delta by a different
  // amount depending on the vertical position of the |viewport_point|.
  expected_scroll_deltas[0] = gfx::Vector2dF(2, 9);
  expected_scroll_deltas[1] = gfx::Vector2dF(1, 4);
  // Deltas which start with the same vertical position of the
  // |viewport_point| are subject to identical perspective effects.
  expected_scroll_deltas[2] = gfx::Vector2dF(5, 0);
  expected_scroll_deltas[3] = gfx::Vector2dF(5, 0);

  gfx::Point viewport_point(1, 1);

  // Scroll in screen coordinates with a gesture. Each scroll starts
  // where the previous scroll ended, but the scroll position is reset
  // for each scroll.
  for (int i = 0; i < 4; ++i) {
    SetScrollOffsetDelta(child, gfx::Vector2dF());
    DrawFrame();
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(viewport_point, gesture_scroll_deltas[i],
                                    ui::ScrollInputType::kTouchscreen)
                             .get(),
                         ui::ScrollInputType::kTouchscreen)
            .thread);
    GetInputHandler().ScrollUpdate(
        UpdateState(viewport_point, gesture_scroll_deltas[i],
                    ui::ScrollInputType::kTouchscreen));
    viewport_point += gfx::ToFlooredVector2d(gesture_scroll_deltas[i]);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    commit_data = host_impl_->ProcessCompositorDeltas(
        /* main_thread_mutator_host */ nullptr);
    EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), child->element_id(),
                                   expected_scroll_deltas[i]));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(commit_data->scrolls.size(), 1u);

    PushScrollOffsetsToPendingTree(
        {{child->element_id(),
          gfx::PointAtOffsetFromOrigin(expected_scroll_deltas[i])}});
    ClearMainThreadDeltasForTesting(host_impl_.get());
  }
}

TEST_F(CommitToActiveTreeLayerTreeHostImplTest, ScrollScaledLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();

  // Scale the layer to twice its normal size.
  int scale = 2;
  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  // Set external transform above root.
  host_impl_->OnDraw(scale_transform, gfx::Rect(0, 0, 50, 50), false, false);
  DrawFrame();

  // Scroll down in screen coordinates with a gesture.
  gfx::Vector2d scroll_delta(0, 10);
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

  // The layer should have scrolled down in its local coordinates, but half the
  // amount.
  std::unique_ptr<CompositorCommitData> commit_data =
      host_impl_->ProcessCompositorDeltas(
          /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), scroll_layer->element_id(),
                                 gfx::Vector2dF(0, scroll_delta.y() / scale)));
  PushScrollOffsetsToPendingTree(
      {{scroll_layer->element_id(), gfx::PointAtOffsetFromOrigin(gfx::Vector2dF(
                                        0, scroll_delta.y() / scale))}});
  ClearNonScrollSyncTreeDeltasForTesting();

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::Vector2dF wheel_scroll_delta(0, 10);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), wheel_scroll_delta,
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), wheel_scroll_delta,
                                             ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // It should apply the scale factor to the scroll delta for the wheel event.
  commit_data = host_impl_->ProcessCompositorDeltas(
      /* main_thread_mutator_host */ nullptr);
  EXPECT_TRUE(ScrollInfoContains(*commit_data.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_P(LayerTreeHostImplTest, ScrollViewportRounding) {
  int width = 332;
  int height = 20;
  int scale = 3;
  gfx::Size container_bounds = gfx::Size(width * scale - 1, height * scale);
  SetupViewportLayersInnerScrolls(container_bounds, gfx::Size(width, height));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->active_tree()->SetDeviceScaleFactor(scale);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);

  LayerImpl* inner_viewport_scroll_layer = InnerViewportScrollLayer();
  EXPECT_EQ(gfx::PointF(0, 0), MaxScrollOffset(inner_viewport_scroll_layer));
}

void CheckLayerScrollOffset(LayerImpl* layer, gfx::Point scroll_offset) {
  const gfx::Transform target_space_transform =
      layer->draw_properties().target_space_transform;
  EXPECT_TRUE(target_space_transform.IsScaleOrTranslation());
  gfx::Point translated_point = target_space_transform.MapPoint(gfx::Point());
  EXPECT_EQ(-scroll_offset.x(), translated_point.x());
  EXPECT_EQ(-scroll_offset.y(), translated_point.y());
}

TEST_P(LayerTreeHostImplTest,
       ExternalRootLayerScrollOffsetDelegationReflectedInNextDraw) {
  SetupViewportLayersInnerScrolls(gfx::Size(10, 20), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->SetDrawsContent(true);

  // Draw first frame to clear any pending draws and check scroll.
  DrawFrame();
  CheckLayerScrollOffset(scroll_layer, gfx::Point(0, 0));
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Set external scroll delta on delegate and notify LayerTreeHost.
  gfx::PointF scroll_offset(10, 10);
  GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
  CheckLayerScrollOffset(scroll_layer, gfx::Point(0, 0));
  EXPECT_TRUE(host_impl_->active_tree()->needs_update_draw_properties());

  // Check scroll delta reflected in layer.
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
  EXPECT_FALSE(frame.has_no_damage);
  CheckLayerScrollOffset(scroll_layer, gfx::ToRoundedPoint(scroll_offset));
}

// Ensure the viewport correctly handles the user_scrollable bits. That is, if
// the outer viewport disables user scrolling, we should still be able to
// scroll the inner viewport.
TEST_P(LayerTreeHostImplTest, ViewportUserScrollable) {
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  auto* outer_scroll = OuterViewportScrollLayer();
  auto* inner_scroll = InnerViewportScrollLayer();

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree_mutable();
  ElementId inner_element_id = inner_scroll->element_id();
  ElementId outer_element_id = outer_scroll->element_id();

  DrawFrame();

  // "Zoom in" so that the inner viewport is scrollable.
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);

  // Disable scrolling the outer viewport horizontally. The inner viewport
  // should still be allowed to scroll.
  GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
  GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

  gfx::Vector2dF scroll_delta(30 * page_scale_factor, 0);
  {
    auto begin_state = BeginState(gfx::Point(), scroll_delta,
                                  ui::ScrollInputType::kTouchscreen);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(begin_state.get(), ui::ScrollInputType::kTouchscreen)
            .thread);

    // Try scrolling right, the inner viewport should be allowed to scroll.
    auto update_state = UpdateState(gfx::Point(), scroll_delta,
                                    ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(update_state);

    EXPECT_POINTF_EQ(gfx::PointF(30, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. The inner viewport should scroll until its extent,
    // however, the outer viewport should not accept any scroll.
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(update_state);
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(update_state);
    update_state = UpdateState(gfx::Point(), scroll_delta,
                               ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(update_state);

    EXPECT_POINTF_EQ(gfx::PointF(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Reset. Try the same test above but using animated scrolls.
  SetScrollOffset(outer_scroll, gfx::PointF(0, 0));
  SetScrollOffset(inner_scroll, gfx::PointF(0, 0));

  {
    auto begin_state =
        BeginState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
                  .thread);

    // Try scrolling right, the inner viewport should be allowed to scroll.
    auto update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    GetInputHandler().ScrollUpdate(update_state);

    base::TimeTicks cur_time = base::TimeTicks() + base::Milliseconds(100);
    viz::BeginFrameArgs begin_frame_args =
        viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

#define ANIMATE(time_ms)                            \
  cur_time += base::Milliseconds(time_ms);          \
  begin_frame_args.frame_time = (cur_time);         \
  begin_frame_args.frame_id.sequence_number++;      \
  host_impl_->WillBeginImplFrame(begin_frame_args); \
  host_impl_->Animate();                            \
  host_impl_->UpdateAnimationState(true);           \
  host_impl_->DidFinishImplFrame(begin_frame_args);

    // The animation is setup in the first frame so tick twice to actually
    // animate it.
    ANIMATE(0);
    ANIMATE(200);

    EXPECT_POINTF_EQ(gfx::PointF(30, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. The inner viewport should scroll until its extent,
    // however, the outer viewport should not accept any scroll.
    update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    GetInputHandler().ScrollUpdate(update_state);
    ANIMATE(10);
    ANIMATE(200);

    EXPECT_POINTF_EQ(gfx::PointF(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Continue scrolling. the outer viewport should still not scroll.
    update_state = AnimatedUpdateState(gfx::Point(), scroll_delta);
    GetInputHandler().ScrollUpdate(update_state);
    ANIMATE(10);
    ANIMATE(200);

    EXPECT_POINTF_EQ(gfx::PointF(50, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));

    // Fully scroll the inner viewport. We'll now try to start an animation on
    // the outer viewport in the vertical direction, which is scrollable. We'll
    // then try to update the curve to scroll horizontally. Ensure that doesn't
    // allow any horizontal scroll.
    SetScrollOffset(inner_scroll, gfx::PointF(50, 50));
    update_state = AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(0, 100));
    GetInputHandler().ScrollUpdate(update_state);
    ANIMATE(16);
    ANIMATE(64);

    // We don't care about the exact offset, we just want to make sure the
    // scroll is in progress but not finished.
    ASSERT_LT(0, scroll_tree.current_scroll_offset(outer_element_id).y());
    ASSERT_GT(50, scroll_tree.current_scroll_offset(outer_element_id).y());
    ASSERT_EQ(0, scroll_tree.current_scroll_offset(outer_element_id).x());
    EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));

    // Now when we scroll we should do so by updating the ongoing animation
    // curve. Ensure this doesn't allow any horizontal scrolling.
    update_state = AnimatedUpdateState(gfx::Point(), gfx::Vector2dF(100, 100));
    GetInputHandler().ScrollUpdate(update_state);
    ANIMATE(200);

    EXPECT_POINTF_EQ(gfx::PointF(0, 100),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));

#undef ANIMATE

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// Ensure that the SetSynchronousInputHandlerRootScrollOffset method used by
// the WebView API correctly respects the user_scrollable bits on both of the
// inner and outer viewport scroll nodes.
TEST_P(LayerTreeHostImplTest, SetRootScrollOffsetUserScrollable) {
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  auto* outer_scroll = OuterViewportScrollLayer();
  auto* inner_scroll = InnerViewportScrollLayer();

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree_mutable();
  ElementId inner_element_id = inner_scroll->element_id();
  ElementId outer_element_id = outer_scroll->element_id();

  DrawFrame();

  // Ensure that the scroll offset is interpreted as a content offset so it
  // should be unaffected by the page scale factor. See
  // https://crbug.com/973771.
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, page_scale_factor, page_scale_factor);

  // Disable scrolling the inner viewport. Only the outer should scroll.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(inner_scroll)->user_scrollable_vertical = false;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;

    gfx::PointF scroll_offset(25, 30);
    GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(scroll_offset,
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_TRUE(did_request_redraw_);

    // Reset
    did_request_redraw_ = false;
    GetScrollNode(inner_scroll)->user_scrollable_vertical = true;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = true;
    SetScrollOffset(outer_scroll, gfx::PointF(0, 0));
  }

  // Disable scrolling the outer viewport. The inner should scroll to its
  // extent but there should be no bubbling over to the outer viewport.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

    gfx::PointF scroll_offset(120, 140);
    GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_TRUE(did_request_redraw_);

    // Reset
    did_request_redraw_ = false;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
    SetScrollOffset(inner_scroll, gfx::PointF(0, 0));
  }

  // Disable both viewports. No scrolling should take place, no redraw should
  // be requested.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(inner_scroll)->user_scrollable_vertical = false;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;

    gfx::PointF scroll_offset(60, 70);
    GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_FALSE(did_request_redraw_);

    // Reset
    GetScrollNode(inner_scroll)->user_scrollable_vertical = true;
    GetScrollNode(inner_scroll)->user_scrollable_horizontal = true;
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
  }

  // If the inner is at its extent but the outer cannot scroll, we shouldn't
  // request a redraw.
  {
    ASSERT_FALSE(did_request_redraw_);
    GetScrollNode(outer_scroll)->user_scrollable_vertical = false;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = false;
    SetScrollOffset(inner_scroll, gfx::PointF(50, 50));

    gfx::PointF scroll_offset(60, 70);
    GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
    EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                     scroll_tree.current_scroll_offset(inner_element_id));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     scroll_tree.current_scroll_offset(outer_element_id));
    EXPECT_FALSE(did_request_redraw_);

    // Reset
    GetScrollNode(outer_scroll)->user_scrollable_vertical = true;
    GetScrollNode(outer_scroll)->user_scrollable_horizontal = true;
  }
}

// The SetSynchronousInputHandlerRootScrollOffset API can be called while there
// is no inner viewport set. This test passes if we don't crash.
TEST_P(LayerTreeHostImplTest, SetRootScrollOffsetNoViewportCrash) {
  auto* inner_scroll = InnerViewportScrollLayer();
  ASSERT_FALSE(inner_scroll);
  gfx::PointF scroll_offset(25, 30);
  GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
}

TEST_P(LayerTreeHostImplTest, OverscrollRoot) {
  InputHandlerScrollResult scroll_result;
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(),
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // In-bounds scrolling does not affect overscroll.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(),
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // Overscroll events are reflected immediately.
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 50), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // In-bounds scrolling resets accumulated overscroll for the scrolled axes.
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -50), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -10), ui::ScrollInputType::kWheel));
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(10, 0), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(-15, 0), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(-5, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, -10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 60), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, 10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(10, -60), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // Overscroll accumulates within the scope of ScrollBegin/ScrollEnd as long
  // as no scroll occurs.
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -20), ui::ScrollInputType::kWheel));
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -30),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -20), ui::ScrollInputType::kWheel));
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -50),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // Overscroll resets on valid scroll.
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, -20), ui::ScrollInputType::kWheel));
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            GetInputHandler().accumulated_root_overscroll_for_testing());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, OverscrollChildWithoutBubbling) {
  // Scroll child layers beyond their maximum scroll range and make sure root
  // overscroll does not accumulate.
  InputHandlerScrollResult scroll_result;
  gfx::Size scroll_container_size(5, 5);
  gfx::Size surface_size(10, 10);
  SetupViewportLayersNoScrolls(surface_size);
  LayerImpl* root = AddScrollableLayer(OuterViewportScrollLayer(),
                                       scroll_container_size, surface_size);
  LayerImpl* child_layer =
      AddScrollableLayer(root, scroll_container_size, surface_size);
  LayerImpl* grand_child_layer =
      AddScrollableLayer(child_layer, scroll_container_size, surface_size);

  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();

  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                        gfx::PointF(0, 3));
  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(grand_child_layer->element_id(),
                                        gfx::PointF(0, 2));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // The next time we scroll we should only scroll the parent, but overscroll
    // should still not reach the root layer.
    scroll_delta = gfx::Vector2d(0, -30);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // After scrolling the parent, another scroll on the opposite direction
    // should scroll the child.
    scroll_delta = gfx::Vector2d(0, 70);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

TEST_P(LayerTreeHostImplTest, OverscrollChildEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible. Overscroll should
  // be reflected only when it has bubbled up to the root scrolling layer.
  InputHandlerScrollResult scroll_result;
  SetupViewportLayersInnerScrolls(gfx::Size(10, 10), gfx::Size(20, 20));
  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 8);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(5, 5), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 6),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 14),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

TEST_P(LayerTreeHostImplTest, OverscrollAlways) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  SetupViewportLayersNoScrolls(gfx::Size(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(),
            GetInputHandler().accumulated_root_overscroll_for_testing());

  // Even though the layer can't scroll the overscroll still happens.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10),
            GetInputHandler().accumulated_root_overscroll_for_testing());
}

TEST_P(LayerTreeHostImplTest, NoOverscrollWhenNotAtEdge) {
  InputHandlerScrollResult scroll_result;
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  DrawFrame();
  {
    // Edge glow effect should be applicable only upon reaching Edges
    // of the content. unnecessary glow effect calls shouldn't be
    // called while scrolling up without reaching the edge of the content.
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 100),
                                    ui::ScrollInputType::kWheel)
                             .get(),
                         ui::ScrollInputType::kWheel)
            .thread);
    scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2dF(0, 100), ui::ScrollInputType::kWheel));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2dF(0, -2.30f), ui::ScrollInputType::kWheel));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    // unusedrootDelta should be subtracted from applied delta so that
    // unwanted glow effect calls are not called.
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 20),
                                    ui::ScrollInputType::kTouchscreen)
                             .get(),
                         ui::ScrollInputType::kTouchscreen)
            .thread);
    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, 20),
                    ui::ScrollInputType::kTouchscreen));
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(
        gfx::Vector2dF(0.000000f, 17.699997f),
        GetInputHandler().accumulated_root_overscroll_for_testing());

    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(0.02f, -0.01f),
                    ui::ScrollInputType::kTouchscreen));
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(
        gfx::Vector2dF(0.000000f, 17.699997f),
        GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    // TestCase to check  kEpsilon, which prevents minute values to trigger
    // gloweffect without reaching edge.
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(
                      BeginState(gfx::Point(0, 0), gfx::Vector2dF(-0.12f, 0.1f),
                                 ui::ScrollInputType::kWheel)
                          .get(),
                      ui::ScrollInputType::kWheel)
                  .thread);
    scroll_result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::Vector2dF(-0.12f, 0.1f),
                    ui::ScrollInputType::kWheel));
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(),
              GetInputHandler().accumulated_root_overscroll_for_testing());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

TEST_P(LayerTreeHostImplTest, NoOverscrollOnNonViewportLayers) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();
  // Initialization: Add a nested scrolling layer, simulating a scrolling div.
  LayerImpl* scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(400, 400));

  InputHandlerScrollResult scroll_result;
  DrawFrame();

  // Start a scroll gesture, ensure it's scrolling the subscroller.
  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                    ui::ScrollInputType::kTouchscreen));

    EXPECT_POINTF_EQ(gfx::PointF(100, 100), CurrentScrollOffset(scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(120, 140),
                    ui::ScrollInputType::kTouchscreen));

    EXPECT_POINTF_EQ(gfx::PointF(200, 200), CurrentScrollOffset(scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_FALSE(result.did_overscroll_root);
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(20, 40),
                    ui::ScrollInputType::kTouchscreen));

    EXPECT_POINTF_EQ(gfx::PointF(200, 200), CurrentScrollOffset(scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_FALSE(result.did_overscroll_root);
  }

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Test that scrolling the inner viewport directly works, as can happen when the
// scroll chains up to it from an sibling of the outer viewport.
TEST_P(LayerTreeHostImplTest, ScrollFromOuterViewportSibling) {
  const gfx::Size viewport_size(100, 100);

  SetupViewportLayersNoScrolls(viewport_size);
  host_impl_->active_tree()->SetBrowserControlsParams(
      {10, 0, 0, 0, false, false});
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Create a scrolling layer that's parented directly to the inner viewport.
  // This will test that scrolls that chain up to the inner viewport without
  // passing through the outer viewport still scroll correctly and affect
  // browser controls.
  LayerImpl* scroll_layer = AddScrollableLayer(
      inner_scroll_layer, viewport_size, gfx::Size(400, 400));

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Fully scroll the child.
  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_EQ(1, host_impl_->active_tree()->CurrentTopControlsShownRatio());
    EXPECT_POINTF_EQ(gfx::PointF(300, 300), CurrentScrollOffset(scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(inner_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }

  // Scrolling on the child now should chain up directly to the inner viewport.
  // Scrolling it should cause browser controls to hide. The outer viewport
  // should not be affected.
  {
    gfx::Vector2d scroll_delta(0, 10);
    GetInputHandler().ScrollBegin(BeginState(gfx::Point(0, 0), scroll_delta,
                                             ui::ScrollInputType::kTouchscreen)
                                      .get(),
                                  ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    EXPECT_EQ(0, host_impl_->active_tree()->CurrentTopControlsShownRatio());
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(inner_scroll_layer));

    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));

    EXPECT_POINTF_EQ(gfx::PointF(0, 10),
                     CurrentScrollOffset(inner_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport. This happens in the
// rootScroller proposal.
TEST_P(LayerTreeHostImplTest, ScrollChainingWithReplacedOuterViewport) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  // Initialization: Add two nested scrolling layers, simulating a scrolling div
  // with another scrolling div inside it. Set the outer "div" to be the outer
  // viewport.
  LayerImpl* scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(400, 400));
  GetScrollNode(scroll_layer)->scrolls_outer_viewport = true;
  LayerImpl* child_scroll_layer = AddScrollableLayer(
      scroll_layer, gfx::Size(300, 300), gfx::Size(500, 500));

  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_scroll = scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);
  UpdateDrawProperties(layer_tree_impl);

  // Scroll should target the nested scrolling layer in the content and then
  // chain to the parent scrolling layer which is now set as the outer
  // viewport. The original outer viewport layer shouldn't get any scroll here.
  {
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(200, 200),
                     CurrentScrollOffset(child_scroll_layer));

    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(200, 200),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));

    EXPECT_POINTF_EQ(gfx::PointF(200, 200), CurrentScrollOffset(scroll_layer));
  }

  // Now that the nested scrolling layers are fully scrolled, further scrolls
  // would normally chain up to the "outer viewport" but since we've set the
  // scrolling content as the outer viewport, it should stop chaining there.
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

    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }

  // Zoom into the page by a 2X factor so that the inner viewport becomes
  // scrollable.
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Reset the parent scrolling layer (i.e. the current outer viewport) so that
  // we can ensure viewport scrolling works correctly.
  scroll_layer->SetCurrentScrollOffset(gfx::PointF(0, 0));

  // Scrolling the content layer should now scroll the inner viewport first,
  // and then chain up to the current outer viewport (i.e. the parent scroll
  // layer).
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

    EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                     CurrentScrollOffset(inner_scroll_layer));

    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100, 100),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(50, 50), CurrentScrollOffset(scroll_layer));
  }
}

// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport but scrolls start from a layer
// that's not a descendant of the outer viewport. This happens in the
// rootScroller proposal.
TEST_P(LayerTreeHostImplTest, RootScrollerScrollNonDescendant) {
  const gfx::Size content_size(300, 300);
  const gfx::Size viewport_size(300, 300);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  // Initialization: Add a scrolling layer, simulating an ordinary DIV, to be
  // set as the outer viewport. Add a sibling scrolling layer that isn't a child
  // of the outer viewport scroll layer.
  LayerImpl* outer_scroll_layer =
      AddScrollableLayer(content_layer, content_size, gfx::Size(1200, 1200));
  LayerImpl* sibling_scroll_layer = AddScrollableLayer(
      content_layer, gfx::Size(600, 600), gfx::Size(1200, 1200));

  GetScrollNode(InnerViewportScrollLayer())
      ->prevent_viewport_scrolling_from_inner = true;
  GetScrollNode(OuterViewportScrollLayer())->scrolls_outer_viewport = false;
  GetScrollNode(outer_scroll_layer)->scrolls_outer_viewport = true;
  auto viewport_property_ids = layer_tree_impl->ViewportPropertyIdsForTesting();
  viewport_property_ids.outer_scroll = outer_scroll_layer->scroll_tree_index();
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);

  ASSERT_EQ(outer_scroll_layer,
            layer_tree_impl->OuterViewportScrollLayerForTesting());

  // Scrolls should target the non-descendant scroller. Chaining should not
  // propagate to the outer viewport scroll layer.
  {
    // This should fully scroll the layer.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));

    // Scrolling now should chain up but, since the outer viewport is a sibling
    // rather than an ancestor, we shouldn't chain to it.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000, 1000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);

  gfx::PointF viewport_bottom_right(viewport_size.width(),
                                    viewport_size.height());

  // Reset the scroll offset.
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::PointF());

  // Now pinch-zoom in. Anchoring should cause scrolling only on the inner
  // viewport layer.
  {
    // Pinch in to the middle of the screen. The inner viewport should scroll
    // to keep the gesture anchored but not the outer or the sibling scroller.
    page_scale_factor = 2;
    gfx::Point anchor(viewport_size.width() / 2, viewport_size.height() / 2);
    GetInputHandler().ScrollBegin(
        BeginState(anchor, gfx::Vector2dF(), ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(anchor, ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_factor, anchor);
    GetInputHandler().PinchGestureEnd(anchor);

    EXPECT_POINTF_EQ(gfx::PointF(anchor.x() / 2, anchor.y() / 2),
                     CurrentScrollOffset(inner_scroll_layer));

    GetInputHandler().ScrollUpdate(
        UpdateState(anchor, viewport_bottom_right.OffsetFromOrigin(),
                    ui::ScrollInputType::kTouchscreen));

    EXPECT_POINTF_EQ(
        gfx::ScalePoint(viewport_bottom_right, 1 / page_scale_factor),
        CurrentScrollOffset(inner_scroll_layer));
    // TODO(bokan): This doesn't yet work but we'll probably want to fix this
    // at some point.
    // EXPECT_VECTOR2DF_EQ(
    //     gfx::Vector2dF(),
    //     CurrentScrollOffset(outer_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(sibling_scroll_layer));

    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // Reset the scroll offsets
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::PointF());
  inner_scroll_layer->SetCurrentScrollOffset(gfx::PointF());
  outer_scroll_layer->SetCurrentScrollOffset(gfx::PointF());

  // Scrolls over the sibling while pinched in should scroll the sibling first,
  // but then chain up to the inner viewport so that the user can still pan
  // around. The outer viewport should be unaffected.
  {
    // This should fully scroll the sibling but, because we latch to the
    // scroller, it shouldn't chain up to the inner viewport yet.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(600, 600),
                     CurrentScrollOffset(sibling_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(inner_scroll_layer));

    // Scrolling now should chain up to the inner viewport.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(ScalePoint(viewport_bottom_right, 1 / page_scale_factor),
                     CurrentScrollOffset(inner_scroll_layer));
    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));

    // No more scrolling should be possible.
    GetInputHandler().ScrollBegin(
        BeginState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                   ui::ScrollInputType::kTouchscreen)
            .get(),
        ui::ScrollInputType::kTouchscreen);
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000, 2000),
                    ui::ScrollInputType::kTouchscreen));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(gfx::PointF(0, 0),
                     CurrentScrollOffset(outer_scroll_layer));
  }
}

TEST_P(LayerTreeHostImplTest, OverscrollOnImplThread) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size content_size(50, 50);
  SetupViewportLayersNoScrolls(content_size);

  // By default, no main thread scrolling reasons should exist.
  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  ScrollNode* scroll_node = GetScrollNode(scroll_layer);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scroll_node->main_thread_repaint_reasons);

  DrawFrame();

  // Overscroll initiated outside layers will be handled by the impl thread.
  EXPECT_EQ(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 60)));
  EXPECT_EQ(
      ScrollThread::kScrollOnImplThread,
      GetInputHandler()
          .ScrollBegin(BeginState(gfx::Point(0, 60), gfx::Vector2dF(0, 10),
                                  ui::ScrollInputType::kWheel)
                           .get(),
                       ui::ScrollInputType::kWheel)
          .thread);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Overscroll initiated inside layers will be handled by the impl thread.
  EXPECT_NE(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 0)));
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(0, 0), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
}

class BlendStateCheckLayer : public LayerImpl {
 public:
  static std::unique_ptr<BlendStateCheckLayer> Create(
      LayerTreeImpl* tree_impl,
      int id,
      viz::ClientResourceProvider* resource_provider) {
    return base::WrapUnique(
        new BlendStateCheckLayer(tree_impl, id, resource_provider));
  }

  BlendStateCheckLayer(LayerTreeImpl* tree_impl,
                       int id,
                       viz::ClientResourceProvider* resource_provider)
      : LayerImpl(tree_impl, id),
        resource_provider_(resource_provider),
        blend_(false),
        has_render_surface_(false),
        comparison_layer_(nullptr),
        quads_appended_(false),
        quad_rect_(5, 5, 5, 5),
        quad_visible_rect_(5, 5, 5, 5),
        shared_image_interface_(
            base::MakeRefCounted<gpu::TestSharedImageInterface>()) {
    auto shared_image =
        shared_image_interface_->CreateSharedImageForSoftwareCompositor(
            {viz::SinglePlaneFormat::kBGRA_8888, gfx::Size(1, 1),
             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
             "BlendStateCheckLayerTest"});
    auto sync_token = shared_image_interface_->GenUnverifiedSyncToken();
    viz::TransferableResource resource = viz::TransferableResource::Make(
        shared_image,
        viz::TransferableResource::ResourceSource::kTileRasterTask, sync_token);

    resource_id_ = resource_provider_->ImportResource(std::move(resource),
                                                      base::DoNothing());
    SetBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
  }

  void ReleaseResources() override {
    resource_provider_->RemoveImportedResource(resource_id_);
  }

  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    quads_appended_ = true;

    gfx::Rect opaque_rect;
    if (contents_opaque())
      opaque_rect = quad_rect_;
    else
      opaque_rect = opaque_content_rect_;
    gfx::Rect visible_quad_rect = quad_visible_rect_;
    bool needs_blending = !opaque_rect.Contains(visible_quad_rect);

    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    PopulateSharedQuadState(shared_quad_state, contents_opaque());

    auto* test_blending_draw_quad =
        render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
    test_blending_draw_quad->SetNew(
        shared_quad_state, quad_rect_, visible_quad_rect, needs_blending,
        resource_id_, gfx::RectF(0, 0, 1, 1), false, false);

    EXPECT_EQ(blend_, test_blending_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(has_render_surface_,
              GetRenderSurface(this) != GetRenderSurface(comparison_layer_));
  }

  void SetExpectation(bool blend,
                      bool has_render_surface,
                      LayerImpl* comparison_layer) {
    blend_ = blend;
    has_render_surface_ = has_render_surface;
    comparison_layer_ = comparison_layer;
    quads_appended_ = false;
  }

  bool quads_appended() const { return quads_appended_; }

  void SetQuadRect(const gfx::Rect& rect) { quad_rect_ = rect; }
  void SetQuadVisibleRect(const gfx::Rect& rect) { quad_visible_rect_ = rect; }
  void SetOpaqueContentRect(const gfx::Rect& rect) {
    opaque_content_rect_ = rect;
  }

 private:
  raw_ptr<viz::ClientResourceProvider> resource_provider_;
  bool blend_;
  bool has_render_surface_;
  raw_ptr<LayerImpl> comparison_layer_;
  bool quads_appended_;
  gfx::Rect quad_rect_;
  gfx::Rect opaque_content_rect_;
  gfx::Rect quad_visible_rect_;
  viz::ResourceId resource_id_;
  scoped_refptr<gpu::TestSharedImageInterface> shared_image_interface_;
};

TEST_P(LayerTreeHostImplTest, MayThrottleIfUnusedFrames) {
  // Make sure that the throttle bit gets set properly.
  viz::CompositorFrameMetadata metadata;

  // By default, throttling should be allowed.
  metadata = host_impl_->MakeCompositorFrameMetadata();
  EXPECT_TRUE(metadata.may_throttle_if_undrawn_frames);

  // If requested, frames should request no throttling.
  host_impl_->SetMayThrottleIfUndrawnFrames(false);
  metadata = host_impl_->MakeCompositorFrameMetadata();
  EXPECT_FALSE(metadata.may_throttle_if_undrawn_frames);

  // Explicitly set it back to the default, for completeness.
  host_impl_->SetMayThrottleIfUndrawnFrames(true);
  metadata = host_impl_->MakeCompositorFrameMetadata();
  EXPECT_TRUE(metadata.may_throttle_if_undrawn_frames);
}

class FakeDrawableLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new FakeDrawableLayerImpl(tree_impl, id));
  }

 protected:
  FakeDrawableLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

// Make sure damage tracking propagates all the way to the viz::CompositorFrame
// submitted to the LayerTreeFrameSink, where it should request to swap only
// the sub-buffer that is damaged.
// These tests use FakeLayerTreeFrameSink properties that are not set in
// TreesInViz Client mode.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       PartialSwapReceivesDamageRect) {
  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d();
  FakeLayerTreeFrameSink* fake_layer_tree_frame_sink =
      layer_tree_frame_sink.get();

  // This test creates its own LayerTreeHostImpl, so
  // that we can force partial swap enabled.
  LayerTreeSettings settings = DefaultSettings();
  std::unique_ptr<LayerTreeHostImpl> layer_tree_host_impl =
      CreateLayerTreeHostImplForTesting(
          settings, this, &task_runner_provider_, &stats_instrumentation_,
          &task_graph_runner_,
          AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
          nullptr, nullptr);
  if (layer_tree_host_impl->settings().trees_in_viz_in_viz_process) {
    // TODO(496580137): Move this to VizLayerTreeHostImpl specific tests.
    static_cast<TestVizLayerTreeHostImpl*>(layer_tree_host_impl.get())
        ->set_next_frame_token_from_client(1u);
  }
  layer_tree_host_impl->SetVisible(true);
  layer_tree_host_impl->InitializeFrameSink(layer_tree_frame_sink.get());

  LayerImpl* root = SetupRootLayer<LayerImpl>(
      layer_tree_host_impl->active_tree(), gfx::Size(500, 500));
  LayerImpl* child = AddLayer<LayerImpl>(layer_tree_host_impl->active_tree());
  child->SetBounds(gfx::Size(14, 15));
  child->SetDrawsContent(true);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(12, 13));
  layer_tree_host_impl->active_tree()->SetLocalSurfaceIdFromParent(
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2u, 3u)));
  UpdateDrawProperties(layer_tree_host_impl->active_tree());

  TestFrameData frame;

  // First frame, the entire screen should get swapped.
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);
  gfx::Rect expected_swap_rect(500, 500);
  EXPECT_EQ(expected_swap_rect, fake_layer_tree_frame_sink->last_swap_rect());

  // Second frame, only the damaged area should get swapped. Damage should be
  // the union of old and new child rects: gfx::Rect(26, 28).
  child->SetOffsetToTransformParent(gfx::Vector2dF());
  child->NoteLayerPropertyChanged();
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);

  expected_swap_rect = gfx::Rect(26, 28);
  EXPECT_EQ(expected_swap_rect, fake_layer_tree_frame_sink->last_swap_rect());

  layer_tree_host_impl->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
  // This will damage everything.
  root->SetBackgroundColor(SkColors::kBlack);
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  layer_tree_host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  layer_tree_host_impl->DidFinishImplFrame(args);

  expected_swap_rect = gfx::Rect(10, 10);
  EXPECT_EQ(expected_swap_rect, fake_layer_tree_frame_sink->last_swap_rect());

  layer_tree_host_impl->ReleaseLayerTreeFrameSink();
}

TEST_P(LayerTreeHostImplTest, RootLayerDoesntCreateExtraSurface) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));
  LayerImpl* child = AddLayerInActiveTree();
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  CopyProperties(root, child);

  UpdateDrawProperties(host_impl_->active_tree());

  TestFrameData frame;

  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  EXPECT_EQ(1u, frame.render_surface_list->size());
  EXPECT_EQ(1u, frame.render_passes.size());
  host_impl_->DidDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new FakeLayerWithQuads(tree_impl, id));
  }

  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    PopulateSharedQuadState(shared_quad_state, contents_opaque());

    SkColor4f gray = SkColors::kGray;
    gfx::Rect quad_rect(bounds());
    gfx::Rect visible_quad_rect(quad_rect);
    auto* my_quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    my_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, gray,
                    false);
  }

 private:
  FakeLayerWithQuads(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}
};

TEST_P(LayerTreeHostImplTest, LayersFreeTextures) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::CreateRaster();
  gpu::TestSharedImageInterface* sii = context_provider->SharedImageInterface();
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d(context_provider));
  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  LayerImpl* root_layer = SetupDefaultRootLayer(gfx::Size(10, 10));

  scoped_refptr<VideoFrame> softwareFrame = media::VideoFrame::CreateColorFrame(
      gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(softwareFrame);
  auto* video_layer = AddLayer<VideoLayerImpl>(
      host_impl_->active_tree(), &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  CopyProperties(root_layer, video_layer);

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_EQ(0u, sii->shared_image_count());

  DrawFrame();

  EXPECT_GT(sii->shared_image_count(), 0u);

  // Kill the layer tree.
  ClearLayersAndPropertyTrees(host_impl_->active_tree());

  // The FakeLayerTreeFrameSink holds the last frame, which holds the
  // TransferableResources, which hold the ClientSharedImages. We need to
  // release them to drop the ref count.
  auto* fake_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  fake_sink->ReturnResourcesHeldByParent();
  if (fake_sink->last_sent_frame()) {
    fake_sink->last_sent_frame()->resource_list.clear();
  }

  // There should be no textures left in use after.
  EXPECT_EQ(0u, sii->shared_image_count());
}

TEST_P(LayerTreeHostImplTest, HasTransparentBackground) {
  SetupDefaultRootLayer(gfx::Size(10, 10));
  host_impl_->active_tree()->set_background_color(SkColors::kWhite);
  UpdateDrawProperties(host_impl_->active_tree());

  // Verify one quad is drawn when transparent background set is not set.
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    if (host_impl_->settings().TreesInVizInClientProcess()) {
      ASSERT_EQ(0u, root_pass->quad_list.size());
    } else {
      ASSERT_EQ(1u, root_pass->quad_list.size());
      EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
                root_pass->quad_list.front()->material);
    }
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when transparent background is set.
  host_impl_->active_tree()->set_background_color(SkColors::kTransparent);
  host_impl_->SetFullViewportDamage();
  args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when semi-transparent background is set.
  host_impl_->active_tree()->set_background_color({1.0f, 0.0f, 0.0f, 0.1f});
  host_impl_->SetFullViewportDamage();
  host_impl_->WillBeginImplFrame(viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1)));
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
}

class LayerTreeHostImplTestDrawAndTestDamage : public LayerTreeHostImplTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }

  void DrawFrameAndTestDamage(const gfx::Rect& expected_damage,
                              const LayerImpl* child) {
    bool expect_to_draw = !expected_damage.IsEmpty();

    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(1));
    host_impl_->WillBeginImplFrame(args);
    EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));

    if (!expect_to_draw) {
      // With no damage, we don't draw, and no quads are created.
      ASSERT_EQ(0u, frame.render_passes.size());
    } else {
      ASSERT_EQ(1u, frame.render_passes.size());

      // Verify the damage rect for the root render pass.
      const viz::CompositorRenderPass* root_render_pass =
          frame.render_passes.back().get();
      EXPECT_EQ(expected_damage, root_render_pass->damage_rect);

      // Verify the root and child layers' quads are generated and not being
      // culled.
      ASSERT_EQ(2u, root_render_pass->quad_list.size());

      gfx::Rect expected_child_visible_rect(child->bounds());
      EXPECT_EQ(expected_child_visible_rect,
                root_render_pass->quad_list.front()->visible_rect);

      LayerImpl* root = root_layer();
      gfx::Rect expected_root_visible_rect(root->bounds());
      EXPECT_EQ(expected_root_visible_rect,
                root_render_pass->quad_list.ElementAt(1)->visible_rect);
    }

    EXPECT_EQ(expect_to_draw, host_impl_->DrawLayers(&frame).has_value());
    host_impl_->DidDrawAllLayers(frame);
    host_impl_->DidFinishImplFrame(args);
  }
};

// These tests rely on LayerTreeHostImpl to produce CompositorFrame quad data,
// which is not enabled for TreesInViz Client mode.
INSTANTIATE_COMPOSITOR_FRAME_PRODUCING_TREE_TEST_P(
    LayerTreeHostImplTestDrawAndTestDamage);

TEST_P(LayerTreeHostImplTestDrawAndTestDamage, FrameIncludesDamageRect) {
  auto* root = SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                                   gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->SetBackgroundColor(SkColors::kRed);

  // Child layer is in the bottom right corner.
  auto* child = AddLayer<SolidColorLayerImpl>(host_impl_->active_tree());
  child->SetBounds(gfx::Size(1, 1));
  child->SetDrawsContent(true);
  child->SetBackgroundColor(SkColors::kRed);
  CopyProperties(root, child);
  child->SetOffsetToTransformParent(gfx::Vector2dF(9, 9));

  UpdateDrawProperties(host_impl_->active_tree());

  // Draw a frame. In the first frame, the entire viewport should be damaged.
  gfx::Rect full_frame_damage(
      host_impl_->active_tree()->GetDeviceViewport().size());
  DrawFrameAndTestDamage(full_frame_damage, child);

  // The second frame has damage that doesn't touch the child layer. Its quads
  // should still be generated.
  gfx::Rect small_damage = gfx::Rect(0, 0, 1, 1);
  root->UnionUpdateRect(small_damage);
  DrawFrameAndTestDamage(small_damage, child);

  // The third frame should have no damage, so no quads should be generated.
  gfx::Rect no_damage;
  DrawFrameAndTestDamage(no_damage, child);
}



class CompositorFrameMetadataTest : public LayerTreeHostImplTest {
 public:
  CompositorFrameMetadataTest() = default;

  void DidReceiveCompositorFrameAckOnImplThread() override { acks_received_++; }

  int acks_received_ = 0;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(CompositorFrameMetadataTest);

TEST_P(CompositorFrameMetadataTest, CompositorFrameAckCountsAsSwapComplete) {
  SetupRootLayer<FakeLayerWithQuads>(host_impl_->active_tree(),
                                     gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());
  DrawFrame();
  host_impl_->ReclaimResources(std::vector<viz::ReturnedResource>());
  host_impl_->DidReceiveCompositorFrameAck();
  EXPECT_EQ(acks_received_, 1);
}

class CountingSoftwareDevice : public viz::SoftwareOutputDevice {
 public:
  CountingSoftwareDevice() : frames_began_(0), frames_ended_(0) {}

  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override {
    ++frames_began_;
    return viz::SoftwareOutputDevice::BeginPaint(damage_rect);
  }
  void EndPaint() override {
    viz::SoftwareOutputDevice::EndPaint();
    ++frames_ended_;
  }

  int frames_began_, frames_ended_;
};

TEST_P(LayerTreeHostImplTest,
       ForcedDrawToSoftwareDeviceSkipsUnsupportedLayers) {
  set_reduce_memory_result(false);
  EXPECT_TRUE(CreateHostImpl(DefaultSettings(),
                             FakeLayerTreeFrameSink::CreateSoftware()));

  const gfx::Transform external_transform;
  const gfx::Rect external_viewport;
  const bool resourceless_software_draw = true;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // SolidColorLayerImpl will be drawn.
  auto* root = SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                                   gfx::Size(10, 10));
  root->SetDrawsContent(true);

  // VideoLayerImpl will not be drawn.
  FakeVideoFrameProvider provider;
  LayerImpl* video_layer = AddLayer<VideoLayerImpl>(
      host_impl_->active_tree(), &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  CopyProperties(root, video_layer);
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  EXPECT_EQ(1u, last_on_draw_frame_->will_draw_layers.size());
  EXPECT_EQ(host_impl_->active_tree()->root_layer(),
            last_on_draw_frame_->will_draw_layers[0]);
}

namespace {
void ExpectFullDamageAndDraw(LayerTreeHostImpl* host_impl) {
  gfx::Rect full_frame_damage(
      host_impl->active_tree()->GetDeviceViewport().size());
  TestFrameData frame;
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  host_impl->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, host_impl->PrepareToDraw(&frame));
  ASSERT_EQ(1u, frame.render_passes.size());
  const viz::CompositorRenderPass* root_render_pass =
      frame.render_passes.back().get();
  EXPECT_EQ(full_frame_damage, root_render_pass->damage_rect);
  EXPECT_TRUE(host_impl->DrawLayers(&frame));
  host_impl->DidDrawAllLayers(frame);
  host_impl->DidFinishImplFrame(args);
}
}  // namespace

TEST_P(LayerTreeHostImplTestDrawAndTestDamage,
       RequireHighResAndRedrawWhenVisible) {
  ASSERT_TRUE(host_impl_->active_tree());

  LayerImpl* root = SetupRootLayer<SolidColorLayerImpl>(
      host_impl_->active_tree(), gfx::Size(10, 10));
  root->SetBackgroundColor(SkColors::kRed);
  UpdateDrawProperties(host_impl_->active_tree());

  // RequiresHighResToDraw is set when new output surface is used.
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());

  // Expect full frame damage for first frame.
  EXPECT_SCOPED(ExpectFullDamageAndDraw(host_impl_.get()));

  host_impl_->ResetRequiresHighResToDraw();

  host_impl_->SetVisible(false);
  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  host_impl_->SetVisible(true);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  host_impl_->SetVisible(false);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());

  host_impl_->ResetRequiresHighResToDraw();

  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  did_request_redraw_ = false;
  host_impl_->SetVisible(true);
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  // Expect redraw and full frame damage when becoming visible.
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_SCOPED(ExpectFullDamageAndDraw(host_impl_.get()));
}

class LayerTreeHostImplTestPrepareTiles : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    fake_host_impl_ = new FakeLayerTreeHostImpl(
        LayerTreeSettings(), &task_runner_provider_, &task_graph_runner_);
    host_impl_.reset(fake_host_impl_);
    layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
    host_impl_->SetVisible(true);
    host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
    host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
  }

  raw_ptr<FakeLayerTreeHostImpl> fake_host_impl_;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplTestPrepareTiles);

TEST_P(LayerTreeHostImplTestPrepareTiles, PrepareTilesWhenInvisible) {
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(false);
  EXPECT_FALSE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(true);
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
}

// This tests the case where hit testing only on scrollable layers returns a
// layer that's outside the scroll chain of the first hit test *any* layer. See
// LayerTreeHostImpl::IsInitialScrollHitTestReliable for details.
TEST_P(LayerTreeHostImplTest, ScrollHitTestIsNotReliable) {
  // If we ray cast a scroller that is not on the first layer's ancestor chain,
  // we should return ScrollThread::SCROLL_ON_MAIN_THREAD.
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* occluder_layer = AddLayerInActiveTree();
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  occluder_layer->SetBounds(content_size);

  // The parent of the occluder is *above* the scroller.
  CopyProperties(root_layer(), occluder_layer);
  occluder_layer->SetTransformTreeIndex(
      host_impl_->active_tree()->PageScaleTransformNode()->id);

  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);
}

// Similar but different case to above. See
// LayerTreeHostImpl::IsInitialScrollHitTestReliable for details.
TEST_P(LayerTreeHostImplTest, ScrollHitTestAncestorMismatch) {
  // If we ray cast a scroller this is on the first layer's ancestor chain, but
  // is not the first scroller we encounter when walking up from the layer, we
  // should also return ScrollThread::SCROLL_ON_MAIN_THREAD.
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  LayerImpl* child_scroll_clip = AddLayerInActiveTree();
  CopyProperties(scroll_layer, child_scroll_clip);

  LayerImpl* child_scroll =
      AddScrollableLayer(child_scroll_clip, viewport_size, content_size);
  child_scroll->SetOffsetToTransformParent(gfx::Vector2dF(10, 10));

  LayerImpl* occluder_layer = AddLayerInActiveTree();
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  occluder_layer->SetBounds(content_size);
  CopyProperties(child_scroll, occluder_layer);
  occluder_layer->SetOffsetToTransformParent(gfx::Vector2dF(-10, -10));

  DrawFrame();

  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);
}

TEST_P(LayerTreeHostImplTest, ScrollInvisibleScroller) {
  gfx::Size viewport_size(50, 50);
  gfx::Size content_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);

  LayerImpl* scroll_layer = InnerViewportScrollLayer();
  LayerImpl* child_scroll =
      AddScrollableLayer(scroll_layer, viewport_size, content_size);
  child_scroll->SetDrawsContent(false);

  DrawFrame();

  // We should have scrolled |child_scroll| even though it does not move
  // any layer that is a drawn RSLL member.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);

  EXPECT_EQ(child_scroll->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
}

// Make sure LatencyInfo carried by LatencyInfoSwapPromise are passed
// in viz::CompositorFrameMetadata.
TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       LatencyInfoPassedToCompositorFrameMetadata) {
  CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());
  SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                      gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      std::make_unique<LatencyInfoSwapPromise>(latency_info));
  host_impl_->active_tree()->QueuePinnedSwapPromise(std::move(swap_promise));

  host_impl_->SetFullViewportDamage();
  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  DrawFrame();

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  const auto* frame = fake_layer_tree_frame_sink->last_sent_frame();
  EXPECT_NE(frame, nullptr);
  if (frame) {
    const auto& metadata_latency_after = frame->metadata.latency_info;
    EXPECT_EQ(1u, metadata_latency_after.size());
    EXPECT_TRUE(metadata_latency_after[0].FindLatency(
        ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  }
}

TEST_P(CompositorFrameProducingLayerTreeHostImplTest,
       CompositorFrameMetadataFrameIntervalInputs) {
  CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());
  SetupRootLayer<SolidColorLayerImpl>(host_impl_->active_tree(),
                                      gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->NotifyInputEvent(/*is_fling=*/false);
  host_impl_->SetFullViewportDamage();
  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1234));
  DrawFrameWithArgs(args);

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  const auto* frame = fake_layer_tree_frame_sink->last_sent_frame();
  EXPECT_NE(frame, nullptr);
  if (frame) {
    const auto& frame_interval_inputs = frame->metadata.frame_interval_inputs;
    EXPECT_TRUE(frame_interval_inputs.has_input);
    EXPECT_EQ(args.frame_time, frame_interval_inputs.frame_time);
  }
}

#if BUILDFLAG(IS_ANDROID)
TEST_P(LayerTreeHostImplTest, SelectionBoundsPassedToCompositorFrameMetadata) {
  LayerImpl* root = SetupRootLayer<SolidColorLayerImpl>(
      host_impl_->active_tree(), gfx::Size(10, 10));
  UpdateDrawProperties(host_impl_->active_tree());

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

  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_after.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_after.start.edge_start());
  EXPECT_TRUE(selection_after.start.visible());
  EXPECT_TRUE(selection_after.end.visible());
}

TEST_P(LayerTreeHostImplTest, HiddenSelectionBoundsStayHidden) {
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(10, 10));

  UpdateDrawProperties(host_impl_->active_tree());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_start(5, 0);
  gfx::Point selection_end(5, 5);
  LayerSelection selection;

  // Mark the start as hidden.
  selection.start.hidden = true;

  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root->id();
  selection.start.edge_end = selection_end;
  selection.start.edge_start = selection_start;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                             /*skip_if_inside_draw=*/false);
  RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_end), selection_after.start.edge_end());
  EXPECT_EQ(gfx::PointF(selection_start), selection_after.start.edge_start());
  EXPECT_FALSE(selection_after.start.visible());
  EXPECT_FALSE(selection_after.end.visible());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_P(LayerTreeHostImplTest, SimpleSwapPromiseMonitor) {
  {
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                               /*skip_if_inside_draw=*/false);
  }

  {
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    // Redraw with damage.
    host_impl_->SetFullViewportDamage();
    host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                               /*skip_if_inside_draw=*/false);
  }

  {
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    // Redraw without damage.
    host_impl_->SetNeedsRedraw(/*animation_only=*/false,
                               /*skip_if_inside_draw=*/false);
  }

  {
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

    // Scrolling normally should not trigger any forwarding.
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_TRUE(
        GetInputHandler()
            .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                      ui::ScrollInputType::kTouchscreen))
            .did_scroll);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_TRUE(Mock::VerifyAndClearExpectations(&monitor));

    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    // Scrolling with a scroll handler should defer the swap to the main
    // thread.
    host_impl_->active_tree()->set_have_scroll_event_handlers(true);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    EXPECT_TRUE(
        GetInputHandler()
            .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                      ui::ScrollInputType::kTouchscreen))
            .did_scroll);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }
}

class LayerTreeHostImplWithBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetBrowserControlsParams(
        {static_cast<float>(top_controls_height_), 0, 0, 0, false, false});
    host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f, 1.f);
  }

 protected:
  void Scroll(float y) {
    ASSERT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 50),
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);
    ASSERT_TRUE(
        GetInputHandler()
            .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2dF(0, y),
                                      ui::ScrollInputType::kTouchscreen))
            .did_scroll);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  void RunAnimation() {
    viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
    do {
      did_request_next_frame_ = false;
      host_impl_->WillBeginImplFrame(begin_frame_args);
      host_impl_->Animate();
      host_impl_->DidFinishImplFrame(begin_frame_args);

      begin_frame_args.frame_time += base::Milliseconds(5);
      begin_frame_args.frame_id.sequence_number++;
    } while (did_request_next_frame_);
  }

  static const int top_controls_height_;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplWithBrowserControlsTest);

const int LayerTreeHostImplWithBrowserControlsTest::top_controls_height_ = 50;

TEST_P(LayerTreeHostImplWithBrowserControlsTest, NoIdleAnimations) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(0, 10));
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsHeightIsCommitted) {
  if (CommitsToActiveTree()) {
    GTEST_SKIP();
  }
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_FALSE(did_request_redraw_);
  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {100, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(100, host_impl_->browser_controls_manager()->TopControlsHeight());
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsStayFullyVisibleOnHeightChange) {
  if (CommitsToActiveTree()) {
    GTEST_SKIP();
  }
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams({0, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetBrowserControlsParams(
      {50, 0, 0, 0, false, false});
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationScheduling) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(0, 10));
  host_impl_->DidChangeBrowserControlsPosition();
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       ScrollHandledByBrowserControls) {
  InputHandlerScrollResult result;
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false,
      std::nullopt);
  DrawFrame();

  // First, scroll just the browser controls and verify that the scroll
  // succeeds.
  const float residue = 10;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  result = GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  // Scroll across the boundary
  const float content_scroll = 20;
  offset = residue + content_scroll;
  result = GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(0, content_scroll), CurrentScrollOffset(scroll_layer));

  // Now scroll back to the top of the content
  offset = -content_scroll;
  result = GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  // And scroll the browser controls completely into view
  offset = -top_controls_height_;
  result = GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  // And attempt to scroll past the end
  result = GetInputHandler().ScrollUpdate(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                  ui::ScrollInputType::kTouchscreen));
  EXPECT_FALSE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, -50));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       WheelUnhandledByBrowserControls) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  host_impl_->active_tree()->SetBrowserControlsParams(
      {top_controls_height_, 0, 0, 0, false, true});
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false,
      std::nullopt);
  DrawFrame();

  LayerImpl* viewport_layer = InnerViewportScrollLayer();

  const float delta = top_controls_height_;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, delta),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(viewport_layer));

  // Wheel scrolls should not affect the browser controls, and should pass
  // directly through to the viewport.
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, delta),
                                    ui::ScrollInputType::kWheel))
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_POINTF_EQ(gfx::PointF(0, delta), CurrentScrollOffset(viewport_layer));

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, delta),
                                    ui::ScrollInputType::kWheel))
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_POINTF_EQ(gfx::PointF(0, delta * 2),
                   CurrentScrollOffset(viewport_layer));
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAtOrigin) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false,
      std::nullopt);
  DrawFrame();

  const float residue = 35;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  // Scroll the browser controls partially.
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from their limit.
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  ASSERT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);

  // The browser controls should properly animate until finished, despite the
  // scroll offset being at the origin.
  viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
  while (did_request_next_frame_) {
    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;

    float old_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    begin_frame_args.frame_time += base::Milliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    // No commit is needed as the controls are animating the content offset,
    // not the scroll offset.
    EXPECT_FALSE(did_request_commit_);

    if (new_offset != old_offset)
      EXPECT_TRUE(did_request_redraw_);

    if (new_offset != 0) {
      EXPECT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
      EXPECT_TRUE(did_request_next_frame_);
    }
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->HasAnimation());
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAfterScroll) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false,
      std::nullopt);
  float initial_scroll_offset = 50;
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree_mutable()
      .UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                        gfx::PointF(0, initial_scroll_offset));
  DrawFrame();

  const float residue = 15;
  float offset = top_controls_height_ - residue;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(0, initial_scroll_offset),
            CurrentScrollOffset(scroll_layer));

  // Scroll the browser controls partially.
  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(0, initial_scroll_offset),
            CurrentScrollOffset(scroll_layer));

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from the limit.
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  ASSERT_TRUE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
            did_request_commit_);

  // Animate the browser controls to the limit.
  viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
  while (did_request_next_frame_) {
    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;

    float old_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    begin_frame_args.frame_time += base::Milliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    if (new_offset != old_offset) {
      EXPECT_TRUE(did_request_redraw_);
      EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
                did_request_commit_);
    }
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->HasAnimation());
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsScrollDeltaInOverScroll) {
  // Verifies that the overscroll delta should not have accumulated in
  // the browser controls if we do a hide and show without releasing finger.
  SetupViewportLayersInnerScrolls(gfx::Size(50, 100), gfx::Size(100, 200));
  auto* scroll_layer = InnerViewportScrollLayer();
  UpdateDrawProperties(host_impl_->active_tree());

  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false,
      std::nullopt);
  DrawFrame();

  float offset = 50;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, offset),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::PointF(), CurrentScrollOffset(scroll_layer));

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_EQ(gfx::PointF(0, offset), CurrentScrollOffset(scroll_layer));

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);

  // Should have fully scrolled
  EXPECT_EQ(gfx::PointF(0, MaxScrollOffset(scroll_layer).y()),
            CurrentScrollOffset(scroll_layer));

  float overscrollamount = 10;

  // Overscroll the content
  EXPECT_FALSE(GetInputHandler()
                   .ScrollUpdate(UpdateState(gfx::Point(),
                                             gfx::Vector2d(0, overscrollamount),
                                             ui::ScrollInputType::kTouchscreen))
                   .did_scroll);
  EXPECT_EQ(gfx::PointF(0, 2 * offset), CurrentScrollOffset(scroll_layer));
  EXPECT_EQ(gfx::Vector2dF(0, overscrollamount),
            GetInputHandler().accumulated_root_overscroll_for_testing());

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -2 * offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());

  EXPECT_TRUE(
      GetInputHandler()
          .ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, -offset),
                                    ui::ScrollInputType::kTouchscreen))
          .did_scroll);
  EXPECT_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));

  // Browser controls should be fully visible
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// Tests that when animating the top controls down, the viewport doesn't counter
// scroll if it's not already scrolled.
TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       AnimationDoesntScrollUnscrolledViewport) {
  // Initialize with 50px browser controls, 200px contents, and 100px viewport.
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(100, 200));

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  ASSERT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());

  // Scroll down 10px, which should partially hide the browser controls, but
  // not scroll the viewport.
  Scroll(10.f);
  EXPECT_EQ(0.8f, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(0.f, host_impl_->viewport().TotalScrollOffset().y());

  // Let the top controls animate back down.
  RunAnimation();

  EXPECT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());
  EXPECT_EQ(0.f, host_impl_->viewport().TotalScrollOffset().y());
}

// Tests that when animating the top controls down, the viewport counter scrolls
// if it's partially scrolled down.
TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       AnimationScrollsScrolledViewport) {
  // Initialize with 50px browser controls, 200px contents, and 100px viewport.
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(100, 200));

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  ASSERT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());

  // Scroll down 60px, then up 40px, which should hide the browser controls and
  // scroll the viewport 10px, then bring the browser controls down 40px.
  Scroll(60.f);
  EXPECT_EQ(0.f, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(10.f, host_impl_->viewport().TotalScrollOffset().y());

  Scroll(-40.f);
  EXPECT_EQ(0.8f, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(10.f, host_impl_->viewport().TotalScrollOffset().y());

  // Let the top controls animate back down.
  RunAnimation();

  EXPECT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());
  EXPECT_EQ(20.f, host_impl_->viewport().TotalScrollOffset().y());
}

// Tests that the page animates down with the top controls when
// BrowserControlParams.only_expand_top_controls_at_page_top is true.
TEST_P(LayerTreeHostImplWithBrowserControlsTest,
       AnimationScrollsViewportWhenOnlyExpandTopControlsAtPageTopIsSet) {
  // Initialize with 50px browser controls, 200px page contents, a 100px
  // viewport, and only_expand_top_controls_at_page_top enabled.
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(100, 200));
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  layer_tree_impl->SetBrowserControlsParams(
      {/*top_controls_height=*/50.f, 0.f, 0.f, 0.f, false, false,
       /*only_expand_top_controls_at_page_top=*/true});

  ASSERT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());

  // Scroll down 60px, then up 50px, which should hide the browser controls and
  // scroll the viewport 10px, then unscroll the viewport and bring the browser
  // controls down 40px.
  Scroll(60.f);
  EXPECT_EQ(0.f, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(10.f, host_impl_->viewport().TotalScrollOffset().y());

  Scroll(-50.f);
  EXPECT_EQ(0.8f, host_impl_->active_tree()->CurrentTopControlsShownRatio());
  EXPECT_EQ(0.f, host_impl_->viewport().TotalScrollOffset().y());

  // Let the top controls animate back down.
  RunAnimation();

  // With only_expand_top_controls_at_page_top set, the page shouldn't scroll.
  EXPECT_EQ(1, layer_tree_impl->CurrentTopControlsShownRatio());
  EXPECT_EQ(0.f, host_impl_->viewport().TotalScrollOffset().y());
}

TEST_P(LayerTreeHostImplTest, RootScrollBothInnerAndOuterLayer) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::PointF inner_expected;
    gfx::PointF outer_expected;
    EXPECT_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    gfx::PointF current_offset(70, 100);

    GetInputHandler().SetSynchronousInputHandlerRootScrollOffset(
        current_offset);
    EXPECT_EQ(gfx::PointF(25, 40), MaxScrollOffset(inner_scroll));
    EXPECT_EQ(gfx::PointF(50, 80), MaxScrollOffset(outer_scroll));

    // Inner viewport scrolls first. Then the rest is applied to the outer
    // viewport.
    EXPECT_EQ(gfx::PointF(25, 40), CurrentScrollOffset(inner_scroll));
    EXPECT_EQ(gfx::PointF(45, 60), CurrentScrollOffset(outer_scroll));
  }
}

TEST_P(LayerTreeHostImplTest, DiagonalScrollBubblesPerfectlyToInner) {
  gfx::Size content_size = gfx::Size(200, 320);
  gfx::Size outer_viewport = gfx::Size(100, 160);
  gfx::Size inner_viewport = gfx::Size(50, 80);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* inner_scroll = InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::PointF inner_expected;
    gfx::PointF outer_expected;
    EXPECT_POINTF_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_POINTF_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    gfx::Vector2dF scroll_delta(inner_viewport.width() / 2,
                                inner_viewport.height() / 2);

    // Make sure the scroll goes to the inner viewport first.
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                          ui::ScrollInputType::kTouchscreen)
                                   .get(),
                               ui::ScrollInputType::kTouchscreen)
                  .thread);

    // Scroll near the edge of the outer viewport.
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), scroll_delta, ui::ScrollInputType::kTouchscreen));
    inner_expected += scroll_delta;

    EXPECT_POINTF_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_POINTF_EQ(outer_expected, CurrentScrollOffset(outer_scroll));

    // Now diagonal scroll across the outer viewport boundary in a single event.
    // The entirety of the scroll should be consumed, as bubbling between inner
    // and outer viewport layers is perfect.
    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), gfx::ScaleVector2d(scroll_delta, 2),
                    ui::ScrollInputType::kTouchscreen));
    outer_expected += scroll_delta;
    inner_expected += scroll_delta;
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    EXPECT_POINTF_EQ(inner_expected, CurrentScrollOffset(inner_scroll));
    EXPECT_POINTF_EQ(outer_expected, CurrentScrollOffset(outer_scroll));
  }
}

TEST_P(LayerTreeHostImplTest,
       ScrollBeginEventThatTargetsViewportLayerSkipsHitTest) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);

  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* child_scroll =
      AddScrollableLayer(outer_scroll, inner_viewport, outer_viewport);

  UpdateDrawProperties(host_impl_->active_tree());
  DrawFrame();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .RootScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                            ui::ScrollInputType::kTouchscreen)
                                     .get(),
                                 ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode(),
            host_impl_->OuterViewportScrollNode());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            child_scroll->scroll_tree_index());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, NoOverscrollWhenInnerViewportCantScroll) {
  InputHandlerScrollResult scroll_result;
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport, outer_viewport,
                      content_size);
  // Make inner viewport unscrollable.
  LayerImpl* inner_scroll = InnerViewportScrollLayer();
  GetScrollNode(inner_scroll)->user_scrollable_horizontal = false;
  GetScrollNode(inner_scroll)->user_scrollable_vertical = false;

  DrawFrame();

  // Ensure inner viewport doesn't react to scrolls (test it's unscrollable).
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll));
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 100),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);
  scroll_result = GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(), gfx::Vector2dF(0, 100), ui::ScrollInputType::kTouchscreen));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll));

  // When inner viewport is unscrollable, a fling gives zero overscroll.
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(),
            GetInputHandler().accumulated_root_overscroll_for_testing());
}

TEST_P(LayerTreeHostImplTest, ExternalTransformReflectedInNextDraw) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  gfx::Transform external_transform;
  const gfx::Rect external_viewport(layer_size);
  const bool resourceless_software_draw = false;
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  auto* layer = InnerViewportScrollLayer();
  layer->SetDrawsContent(true);

  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORM_EQ(external_transform,
                      layer->draw_properties().target_space_transform);

  external_transform.Translate(20, 20);
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORM_EQ(external_transform,
                      layer->draw_properties().target_space_transform);
}

TEST_P(LayerTreeHostImplTest, ExternalTransformSetNeedsRedraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform transform_for_tile_priority;
  const gfx::Transform draw_transform;
  const gfx::Rect viewport_for_tile_priority1(viewport_size);
  const gfx::Rect viewport_for_tile_priority2(50, 50);
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->SetExternalTilePriorityConstraints(viewport_for_tile_priority1,
                                                 transform_for_tile_priority);
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Setting new constraints needs redraw.
  did_request_redraw_ = false;
  host_impl_->SetExternalTilePriorityConstraints(viewport_for_tile_priority2,
                                                 transform_for_tile_priority);
  EXPECT_TRUE(did_request_redraw_);
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

TEST_P(LayerTreeHostImplTest, OnDrawConstraintSetNeedsRedraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport1(viewport_size);
  const gfx::Rect draw_viewport2(50, 50);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->OnDraw(draw_transform, draw_viewport1, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Same draw params does not swap.
  did_request_redraw_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport1, resourceless_software_draw,
                     false);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(last_on_draw_frame_->has_no_damage);
  last_on_draw_frame_.reset();

  // Different draw params does swap.
  did_request_redraw_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport2, resourceless_software_draw,
                     false);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

// This test verifies that the viewport damage rect is the full viewport and not
// just part of the viewport in the presence of an external viewport.
TEST_P(LayerTreeHostImplTest, FullViewportDamageAfterOnDraw) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(gfx::Point(5, 5), viewport_size);
  bool resourceless_software_draw = false;

  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_EQ(draw_viewport, host_impl_->active_tree()->GetDeviceViewport());

  host_impl_->SetFullViewportDamage();
  EXPECT_EQ(host_impl_->active_tree()->internal_device_viewport(),
            host_impl_->viewport_damage_rect_for_testing());
}

TEST_P(LayerTreeHostImplTest, ExternalViewportAffectsVisibleRects) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* content_layer = AddContentLayer();

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(90, 90));
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_EQ(gfx::Rect(90, 90), content_layer->visible_layer_rect());

  gfx::Transform external_transform;
  gfx::Rect external_viewport(10, 20);
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(10, 20), content_layer->visible_layer_rect());

  // Clear the external viewport.
  external_viewport = gfx::Rect();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(90, 90), content_layer->visible_layer_rect());
}

TEST_P(LayerTreeHostImplTest, ExternalTransformAffectsVisibleRects) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* content_layer = AddContentLayer();

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());
  EXPECT_EQ(gfx::Rect(50, 50), content_layer->visible_layer_rect());

  gfx::Transform external_transform;
  external_transform.Translate(10, 10);
  external_transform.Scale(2, 2);
  gfx::Rect external_viewport;
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // Visible rects should now be shifted and scaled because of the external
  // transform.
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(20, 20), content_layer->visible_layer_rect());

  // Clear the external transform.
  external_transform = gfx::Transform();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_EQ(gfx::Rect(50, 50), content_layer->visible_layer_rect());
}

TEST_P(LayerTreeHostImplTest, ExternalTransformAffectsSublayerScaleFactor) {
  const gfx::Size viewport_size(50, 50);
  const gfx::Size layer_size(100, 100);
  SetupViewportLayersInnerScrolls(viewport_size, layer_size);
  LayerImpl* test_layer = AddContentLayer();
  gfx::Transform perspective_transform;
  perspective_transform.ApplyPerspectiveDepth(2);
  CreateTransformNode(test_layer).local = perspective_transform;
  CreateEffectNode(test_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  UpdateDrawProperties(host_impl_->active_tree());
  EffectNode* node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));

  gfx::Transform external_transform;
  external_transform.Translate(10, 10);
  external_transform.Scale(2, 2);
  gfx::Rect external_viewport;
  bool resourceless_software_draw = false;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  // Transform node's sublayer scale should include the device transform scale.
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2, 2));

  // Clear the external transform.
  external_transform = gfx::Transform();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  node = GetEffectNode(test_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));
}

TEST_P(LayerTreeHostImplTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  {
    // Creating the animation should set 'needs redraw'. This is required
    // for LatencyInfo's to be propagated along with the CompositorFrame
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(AtLeast(1));

    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));
  }

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::PointF(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  {
    // Updating the animation should set 'needs redraw'. This is required
    // for LatencyInfo's to be propagated along with the CompositorFrame
    StrictMock<MockLatencyInfoSwapPromiseMonitor> monitor(host_impl_.get());
    EXPECT_CALL(monitor, OnSetNeedsCommitOnMain()).Times(0);
    EXPECT_CALL(monitor, OnSetNeedsRedrawOnImpl()).Times(1);

    // Update target.
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_POINTF_EQ(gfx::PointF(0, 100), CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Tests latching behavior, in particular when a ScrollEnd is received but a
// new ScrollBegin is received before the animation from the previous gesture
// stream is finished.
TEST_P(LayerTreeHostImplTest, ScrollAnimatedLatching) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  AddScrollableLayer(OuterViewportScrollLayer(), gfx::Size(10, 10),
                     gfx::Size(20, 20));

  LayerImpl* outer_scroll = OuterViewportScrollLayer();

  DrawFrame();

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Start an animated scroll over the outer viewport.
  {
    gfx::Point position(40, 40);
    auto begin_state =
        BeginState(position, gfx::Vector2d(0, 50), ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollBegin(begin_state.get(),
                                  ui::ScrollInputType::kWheel);
    auto update_state = AnimatedUpdateState(position, gfx::Vector2d(0, 50));
    GetInputHandler().ScrollUpdate(update_state);
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_NE(gfx::PointF(), CurrentScrollOffset(outer_scroll));
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
  }

  // End the scroll gesture. We should still be latched to the scroll layer
  // since the animation is still in progress.
  {
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    begin_frame_args.frame_time += base::Milliseconds(5);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // Start a new animated scroll. We should remain latched to the outer
  // viewport, despite the fact this scroll began over the child scroller,
  // because we don't clear the latch until the animation is finished.
  {
    gfx::Point position(10, 10);
    auto begin_state =
        BeginState(position, gfx::Vector2d(0, 50), ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollBegin(begin_state.get(),
                                  ui::ScrollInputType::kWheel);
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // Run the animation to the end. Because we received a ScrollBegin, the
  // deferred ScrollEnd from the first gesture should have been cleared. That
  // is, we shouldn't clear the latch when the animation ends because we're
  // currently in an active scroll gesture.
  {
    begin_frame_args.frame_time += base::Milliseconds(200);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args);

    EXPECT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
    EXPECT_EQ(outer_scroll->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  // A ScrollEnd should now clear the latch.
  {
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(host_impl_->CurrentlyScrollingNode());
  }
}

// Test to ensure that animated scrolls correctly account for the page scale
// factor. That is, if you zoom into the page, a wheel scroll should scroll the
// content *less* than before so that it appears to move the same distance when
// zoomed in.
TEST_P(LayerTreeHostImplTest, ScrollAnimatedWhileZoomed) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scrolling_layer = InnerViewportScrollLayer();

  DrawFrame();

  // Zoom in to 2X
  {
    float min_page_scale = 1, max_page_scale = 4;
    float page_scale_factor = 2;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  }

  // Start an animated scroll then do another animated scroll immediately
  // afterwards. This will ensure we test both the starting animation and
  // animation update code.
  {
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 10),
                                    ui::ScrollInputType::kWheel)
                             .get(),
                         ui::ScrollInputType::kWheel)
            .thread);
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 10)));
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 20)));

    // Scrolling the inner viewport happens through the Viewport class which
    // uses the outer viewport to represent "latched to the viewport".
    EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Tick a frame to get the animation started.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_NE(0, CurrentScrollOffset(scrolling_layer).y());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Tick ahead to the end of the animation. We scrolled 30 viewport pixels but
  // since we're zoomed in to 2x we should have scrolled 15 content pixels.
  {
    begin_frame_args.frame_time = start_time + base::Milliseconds(1000);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_EQ(15, CurrentScrollOffset(scrolling_layer).y());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
}

// Ensures a scroll updating an in progress animation works correctly when the
// inner viewport is animating. Specifically this test makes sure the animation
// update doesn't get confused between the currently scrolling node and the
// currently animating node which are different. See https://crbug.com/1070561.
TEST_P(LayerTreeHostImplTest, ScrollAnimatedUpdateInnerViewport) {
  const gfx::Size content_size(210, 1000);
  const gfx::Size viewport_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  // Zoom in to 2X and ensure both viewports have scroll extent in every
  // direction.
  {
    float min_page_scale = 1, max_page_scale = 4;
    float page_scale_factor = 2;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

    SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d(50, 50));
    SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d(5, 400));
  }

  // Start an animated scroll on the inner viewport.
  {
    auto begin_state = BeginState(gfx::Point(10, 10), gfx::Vector2d(0, 60),
                                  ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollBegin(begin_state.get(),
                                  ui::ScrollInputType::kWheel);
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)));

    // Scrolling the inner viewport happens through the Viewport class which
    // uses the outer viewport to represent "latched to the viewport".
    EXPECT_EQ(OuterViewportScrollLayer()->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);

    // However, the animating scroll node should be the inner viewport.
    ASSERT_TRUE(InnerViewportScrollLayer()->element_id());
    EXPECT_TRUE(GetImplAnimationHost()->ElementHasImplOnlyScrollAnimation(
        InnerViewportScrollLayer()->element_id()));
  }

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Tick a frame to get the animation started.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    // The inner viewport should have scrolled a bit from its initial position;
    // the outer viewport should be still. Neither should move horizontally.
    EXPECT_LT(50, CurrentScrollOffset(InnerViewportScrollLayer()).y());
    EXPECT_EQ(400, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    EXPECT_EQ(50, CurrentScrollOffset(InnerViewportScrollLayer()).x());
    EXPECT_EQ(5, CurrentScrollOffset(OuterViewportScrollLayer()).x());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }

  // Do another scroll update which should update the existing animation curve.
  {
    // This scroll will cause the target to be at the max scroll offset. Inner
    // viewport max offset is 100px. Initial offset is 50px. We scrolled 60px +
    // 60px divided by scale factor 2 so 60px total.
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)));

    // While we're animating the curve, we don't distribute to the outer
    // viewport so this scroll update should be a no-op.
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(10, 10), gfx::Vector2d(0, 60)));
  }

  // Tick ahead to the end of the animation. The inner viewport should be
  // scrolled to the maximum offset. The outer viewport should not have
  // scrolled.
  {
    begin_frame_args.frame_time = start_time + base::Milliseconds(1000);
    begin_frame_args.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_EQ(100, CurrentScrollOffset(InnerViewportScrollLayer()).y());
    EXPECT_EQ(400, CurrentScrollOffset(OuterViewportScrollLayer()).y());

    // Ensure no horizontal delta is produced.
    EXPECT_EQ(50, CurrentScrollOffset(InnerViewportScrollLayer()).x());
    EXPECT_EQ(5, CurrentScrollOffset(OuterViewportScrollLayer()).x());
    host_impl_->DidFinishImplFrame(begin_frame_args);
  }
}

// Fluent Overlay Scrollbars track opacity is scaled depending on the thickness
// scale factor of the scrollbar's thumb. When the thumb's thickness is at its
// minimum the track should be invisible
// (`thickness_scale_factor_` == `kIdleThicknessScale`) => (`opacity_` == 0).
// When the thumb's thickness is at its maximum, the track should be fully
// visible.
// (`thickness_scale_factor_` == 1) => (`opacity_` == 1).
// For every thickness value in between `kIdleThicknessScale` and 1.f the
// opacity should be scaled appropriately. This test ensures the correlation
// between thickness of the thumb and opacity of the track.
TEST_P(FluentOverlayScrollbarOpacityLayerTreeHostImplTest,
       PaintedOverlayScrollbarTrackOpacityTest) {
  auto* scrollbar = CreateAndRegisterPaintedScrollbarLayer();
  // Make the scrollbar visible.
  scrollbar->SetOverlayScrollbarLayerOpacityAnimated(
      1.f, /*fade_out_animation=*/false);
  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_EQ(1.f, scrollbar->draw_properties().opacity);
  int const step = GetParam();
  float const thickness_scale_step =
      (1 - scrollbar->GetIdleThicknessScale()) / kParamSteps;
  VerifyCorrectOpacityForThickness(
      scrollbar,
      scrollbar->GetIdleThicknessScale() + thickness_scale_step * step,
      step / static_cast<float>(kParamSteps));
}

INSTANTIATE_TEST_SUITE_P(
    ScaleOpacity,
    FluentOverlayScrollbarOpacityLayerTreeHostImplTest,
    Range(0,
          FluentOverlayScrollbarOpacityLayerTreeHostImplTest::kParamSteps + 1));

TEST_F(FluentOverlayScrollbarLayerTreeHostImplTest,
       FluentScrollbarFlashAfterScrollUpdate) {
  auto* root_scrollbar = CreateAndRegisterPaintedScrollbarLayer();
  // Register child scrollable area layer and scrollbar.
  LayerImpl* root_scroll = OuterViewportScrollLayer();
  gfx::Size child_layer_size(250, 150);
  gfx::Size child_scrollbar_size(gfx::Size(15, child_layer_size.height()));
  auto* child =
      AddScrollableLayer(root_scroll, gfx::Size(100, 100), child_layer_size);
  GetTransformNode(child)->post_translation = gfx::Vector2dF(50, 50);

  auto* child_scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, false, true);
  // SetupScrollbarLayerCommon will register the scrollbar, which sets the
  // layer's opacity to 0. An effect node for the scrollbar layer object needs
  // to be registered in the EffectTree before this happens.
  auto& effect_node =
      CreateEffectNode(child_scrollbar, child->effect_tree_index());
  SetupScrollbarLayerCommon(child, child_scrollbar);
  // SetupScrollbarLayerCommon calls CopyProperties which overrides the effect
  // tree node registered to the scrollbar layer. We need to reset it to the
  // one we registered above.
  child_scrollbar->SetEffectTreeIndex(effect_node.id);
  child_scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  child_scrollbar->SetBounds(child_scrollbar_size);

  auto reset_scrollbars = [root_scrollbar, child_scrollbar](
                              LayerTreeImpl* active_tree,
                              base::OnceClosure& animation_task) {
    GetEffectNode(root_scrollbar)->opacity = 0;
    GetEffectNode(child_scrollbar)->opacity = 0;
    active_tree->UpdateAllScrollbarGeometriesForTesting();
    UpdateDrawProperties(active_tree);
    animation_task.Reset();
  };

  reset_scrollbars(host_impl_->active_tree(), animation_task_);
  host_impl_->active_tree()->DidBecomeActive();

  EXPECT_EQ(root_scrollbar->Opacity(), 0);
  EXPECT_EQ(child_scrollbar->Opacity(), 0);

  // Scroll on root should only flash root.
  GetInputHandler().RootScrollBegin(
      BeginState(gfx::Point(20, 20), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(UpdateState(
      gfx::Point(20, 20), gfx::Vector2d(0, 10), ui::ScrollInputType::kWheel));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_EQ(root_scrollbar->Opacity(), 1.f);
  EXPECT_EQ(child_scrollbar->Opacity(), 0);
  EXPECT_FALSE(animation_task_.is_null());

  reset_scrollbars(host_impl_->active_tree(), animation_task_);

  // Scrolling on child in a direction in which it can't scroll (upwards) should
  // flash the hit tested scrollbar and the one that ends up receiving the
  // scroll event (the root scrollbar in this case).
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(70, 70), gfx::Vector2dF(0, -100),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(70, 70), gfx::Vector2d(0, -100)));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  EXPECT_TRUE(root_scrollbar->Opacity());
  EXPECT_TRUE(child_scrollbar->Opacity());

  EXPECT_FALSE(animation_task_.is_null());
}

// Fluent Overlay Scrollbars opacity should be set to zero when creating
// the animation controller.
TEST_F(FluentOverlayScrollbarLayerTreeHostImplTest,
       PaintedOverlayLayerOnLoadOpacityTest) {
  EXPECT_FLOAT_EQ(CreateAndRegisterPaintedScrollbarLayer()->Opacity(), 0.f);
}

// Fluent Overlay Scrollbar should not be hit tested when its opacity is zero.
TEST_F(FluentOverlayScrollbarLayerTreeHostImplTest,
       DoesntGetHitTestedWhenInvisible) {
  EXPECT_FLOAT_EQ(CreateAndRegisterPaintedScrollbarLayer()->Opacity(), 0.f);
  // Per CreateAndRegisterPaintedScrollbarLayer the Scrollbar's rect is located
  // at (345,0)x(15,600). A point of 352 clicks it in the middle of the
  // track and would cause a scroll.
  InputHandlerPointerResult result =
      GetInputHandler().MouseMoveAt(gfx::Point(352, 300));
  EXPECT_FLOAT_EQ(result.scroll_delta.y(), 0u);
}

// This tests that faded-out Aura scrollbars can't be interacted with.
TEST_P(LayerTreeHostImplTest, FadedOutPaintedOverlayScrollbarHitTest) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<NinePatchThumbScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackStart(0);
  scrollbar->SetTrackLength(575);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  // Set up the scroll node and other state required for scrolling.
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                 ui::ScrollInputType::kScrollbar)
          .get(),
      ui::ScrollInputType::kScrollbar);

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // NinePatchThumbScrollbarLayerImpl(s) don't have a track, so we test thumb
  // drags instead. Start with 0.8 opacity. Scrolling is expected to occur in
  // this case.
  auto& scrollbar_effect_node = CreateEffectNode(scrollbar);
  scrollbar_effect_node.opacity = 0.8;

  GetInputHandler().MouseDown(gfx::PointF(350, 18),
                              /*jump_key_modifier*/ false);
  InputHandlerPointerResult result =
      GetInputHandler().MouseMoveAt(gfx::Point(350, 28));
  EXPECT_GT(result.scroll_delta.y(), 0u);
  GetInputHandler().MouseUp(gfx::PointF(350, 28));

  // Scrolling shouldn't occur at opacity = 0.
  scrollbar_effect_node.opacity = 0;

  GetInputHandler().MouseDown(gfx::PointF(350, 18),
                              /*jump_key_modifier*/ false);
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 28));
  EXPECT_EQ(result.scroll_delta.y(), 0u);
  GetInputHandler().MouseUp(gfx::PointF(350, 28));

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that no scrolls occur when thumb_len equals track_len.
TEST_P(LayerTreeHostImplTest, ScrollOnLargeThumb) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  layer_tree_impl->set_painted_device_scale_factor(2.5f);
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(575);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // PointerDown on the scrollbar should populate drag_state.
  GetInputHandler().MouseDown(gfx::PointF(350, 300),
                              /*jump_key_modifier*/ false);
  EXPECT_TRUE(GetInputHandler()
                  .scrollbar_controller_for_testing()
                  ->drag_state_.has_value());

  // Moving the mouse downwards should result in no scroll.
  InputHandlerPointerResult res =
      GetInputHandler().MouseMoveAt(gfx::Point(350, 600));
  EXPECT_EQ(res.scroll_delta.y(), 0);

  // Moving the mouse upwards should result in no scroll.
  res = GetInputHandler().MouseMoveAt(gfx::Point(350, 0));
  EXPECT_EQ(res.scroll_delta.y(), 0);

  // End the scroll.
  GetInputHandler().MouseUp(gfx::PointF(350, 0));
  EXPECT_TRUE(!GetInputHandler()
                   .scrollbar_controller_for_testing()
                   ->drag_state_.has_value());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that deleting a horizontal scrollbar doesn't affect the autoscroll task
// for the vertical scrollbar.
TEST_P(LayerTreeHostImplTest, AutoscrollOnDeletedScrollbar) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // PointerDown on the scrollbar schedules an autoscroll task.
  GetInputHandler().MouseDown(gfx::PointF(350, 580),
                              /*jump_key_modifier*/ false);
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // Now, unregister the horizontal scrollbar and test that the autoscroll task
  // that was scheduled for the vertical scrollbar still exists. (Note that
  // adding a horizontal scrollbar layer is not needed. This test merely checks
  // the logic inside ScrollbarController::DidUnregisterScrollbar)
  host_impl_->DidUnregisterScrollbarLayer(scroll_layer->element_id(),
                                          ScrollbarOrientation::kHorizontal);
  EXPECT_TRUE(GetInputHandler()
                  .scrollbar_controller_for_testing()
                  ->AutoscrollTaskIsScheduled());

  // If a call comes in to delete the scrollbar layer for which the autoscroll
  // was scheduled, the autoscroll task should set a waiting state instead of
  // initiating an autoscroll, in case the scrollbar comes back.
  layer_tree_impl->UnregisterScrollbar(scrollbar);

  EXPECT_TRUE(GetInputHandler()
                  .scrollbar_controller_for_testing()
                  ->AutoscrollTaskIsScheduled());

  GetInputHandler()
      .scrollbar_controller_for_testing()
      ->cancelable_autoscroll_task_->callback()
      .Run();
  GetInputHandler()
      .scrollbar_controller_for_testing()
      ->cancelable_autoscroll_task_.reset();
  EXPECT_EQ(GetInputHandler()
                .scrollbar_controller_for_testing()
                ->autoscroll_state_->status,
            ScrollbarController::AutoScrollStatus::kAutoscrollReady);

  // Re-register the scrollbar. An autoscroll task should be posted that
  // actually starts a scroll animation
  layer_tree_impl->RegisterScrollbar(scrollbar);

  EXPECT_TRUE(GetInputHandler()
                  .scrollbar_controller_for_testing()
                  ->AutoscrollTaskIsScheduled());

  GetInputHandler()
      .scrollbar_controller_for_testing()
      ->cancelable_autoscroll_task_->callback()
      .Run();
  GetInputHandler()
      .scrollbar_controller_for_testing()
      ->cancelable_autoscroll_task_.reset();
  EXPECT_EQ(GetInputHandler()
                .scrollbar_controller_for_testing()
                ->autoscroll_state_->status,
            ScrollbarController::AutoScrollStatus::kAutoscrollScrolling);

  // End the scroll.
  GetInputHandler().MouseUp(gfx::PointF(350, 580));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that a pointerdown followed by pointermove(s) produces
// InputHandlerPointerResult with scroll_offset > 0 even though the GSB might
// have been dispatched *after* the first pointermove was handled by the
// ScrollbarController.
TEST_P(LayerTreeHostImplTest, PointerMoveOutOfSequence) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // PointerDown sets up the state needed to initiate a drag. Although, the
  // resulting GSB won't be dispatched until the next VSync. Hence, the
  // CurrentlyScrollingNode is expected to be null.
  GetInputHandler().MouseDown(gfx::PointF(350, 18),
                              /*jump_key_modifier*/ false);
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // PointerMove arrives before the next VSync. This still needs to be handled
  // by the ScrollbarController even though the GSB has not yet been dispatched.
  // Note that this doesn't result in a scroll yet. It only prepares a GSU based
  // on the result that is returned by ScrollbarController::HandlePointerMove.
  InputHandlerPointerResult result =
      GetInputHandler().MouseMoveAt(gfx::Point(350, 19));
  EXPECT_GT(result.scroll_delta.y(), 0u);

  // GSB gets dispatched at VSync.
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = base::TimeTicks::Now();
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                 ui::ScrollInputType::kScrollbar)
          .get(),
      ui::ScrollInputType::kScrollbar);
  EXPECT_TRUE(host_impl_->CurrentlyScrollingNode());

  // The PointerMove(s) that follow should be handled and are expected to have a
  // scroll_offset > 0.
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 20));
  EXPECT_GT(result.scroll_delta.y(), 0u);

  // End the scroll.
  GetInputHandler().MouseUp(gfx::PointF(350, 20));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_TRUE(!host_impl_->CurrentlyScrollingNode());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// This tests that faded-out Mac scrollbars can't be interacted with.
TEST_P(LayerTreeHostImplTest, FadedOutPaintedScrollbarHitTest) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // MouseDown on the track of a scrollbar with opacity 0 should not produce a
  // scroll.
  scrollbar->SetScrollbarPaintedOpacity(0);
  InputHandlerPointerResult result = GetInputHandler().MouseDown(
      gfx::PointF(350, 100), /*jump_key_modifier*/ false);
  EXPECT_EQ(result.scroll_delta.y(), 0u);

  // MouseDown on the track of a scrollbar with opacity > 0 should produce a
  // scroll.
  scrollbar->SetScrollbarPaintedOpacity(1);
  result = GetInputHandler().MouseDown(gfx::PointF(350, 100),
                                       /*jump_key_modifier*/ false);
  EXPECT_GT(result.scroll_delta.y(), 0u);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_P(LayerTreeHostImplTest, SingleGSUForScrollbarThumbDragPerFrame) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));

  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 18), gfx::Vector2dF(),
                 ui::ScrollInputType::kScrollbar)
          .get(),
      ui::ScrollInputType::kScrollbar);
  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // ------------------------- Start frame 0 -------------------------
  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(200);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // MouseDown on the thumb should not produce a scroll.
  InputHandlerPointerResult result = GetInputHandler().MouseDown(
      gfx::PointF(350, 18), /*jump_key_modifier*/ false);
  EXPECT_EQ(result.scroll_delta.y(), 0u);

  // The first request for a GSU should be processed as expected.
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 19));
  EXPECT_GT(result.scroll_delta.y(), 0u);

  // A second request for a GSU within the same frame should be ignored as it
  // will cause the thumb drag to become jittery. The reason this happens is
  // because when the first GSU is processed, it gets queued in the compositor
  // thread event queue. So a second request within the same frame will end up
  // calculating an incorrect delta (as ComputeThumbQuadRect would not have
  // accounted for the delta in the first GSU that was not yet dispatched).
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 20));
  EXPECT_EQ(result.scroll_delta.y(), 0u);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ------------------------- Start frame 1 -------------------------
  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // MouseMove for a new frame gets processed as usual.
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 21));
  EXPECT_GT(result.scroll_delta.y(), 0u);

  // MouseUp is not expected to have a delta.
  result = GetInputHandler().MouseUp(gfx::PointF(350, 21));
  EXPECT_EQ(result.scroll_delta.y(), 0u);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Test if the AverageLagTrackingManager's pending frames list is cleared when
// the LayerTreeFrameSink loses context. It is necessary since the frames won't
// receive a presentation feedback if the context is lost, and the pending
// frames will never be removed from the list otherwise.
TEST_P(LayerTreeHostImplTest,
       ClearTrackingManagerOnLayerTreeFrameSinkLoseContext) {
  const gfx::Size content_size(1000, 10000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  DrawFrame();

  base::TimeTicks scroll_begin_arrival_timestamp = base::TimeTicks::Now();
  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(250, 250), gfx::Vector2dF(),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);

  // Draw 30 frames, each with 1 LatencyInfo object that will be added to the
  // AverageLagTrackingManager.
  for (int i = 0; i < 30; i++) {
    // Makes a scroll update so the next frame is set to be processed
    // (to force frame->has_no_damage = false).
    GetInputHandler().ScrollUpdate(UpdateState(
        gfx::Point(), gfx::Vector2d(0, 10), ui::ScrollInputType::kTouchscreen));

    // Add an `EventMetrics` object that will be accepted by
    // `AverageLagTrackingManager::CollectScrollEventsFromFrame()`.
    EventMetrics::List events_metrics;
    base::TimeTicks now = base::TimeTicks::Now();
    events_metrics.push_back(ScrollUpdateEventMetrics::Create(
        ui::EventType::kGestureScrollUpdate, ui::ScrollInputType::kTouchscreen,
        /*is_inertial=*/false,
        i == 0 ? ScrollUpdateEventMetrics::ScrollUpdateType::kStarted
               : ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
        /*delta=*/10.0f, /*timestamp=*/now,
        /*arrived_in_browser_main_timestamp=*/now + base::Milliseconds(1),
        /*blocking_touch_dispatched_to_renderer=*/base::TimeTicks(),
        /*trace_id=*/base::IdType64<class ui::LatencyInfo>(123),
        scroll_begin_arrival_timestamp));
    host_impl_->active_tree()->AppendEventsMetricsFromMainThread(
        std::move(events_metrics));

    DrawFrame();
  }

  // Make LayerTreeFrameSink lose context. It should clear
  // |lag_tracking_manager_|.
  host_impl_->DidLoseLayerTreeFrameSink();

  // Finish the test. |lag_tracking_manager_| will check in its destructor if
  // there is less than 20 frames in its pending frames list.
}

// Tests that the scheduled autoscroll task aborts if a 2nd mousedown occurs in
// the same frame.
TEST_P(LayerTreeHostImplTest, AutoscrollTaskAbort) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  {
    // An autoscroll task gets scheduled on mousedown.
    InputHandlerPointerResult result = GetInputHandler().MouseDown(
        gfx::PointF(350, 575), /*jump_key_modifier*/ false);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    auto begin_state = BeginState(gfx::Point(350, 575), gfx::Vector2d(0, 40),
                                  ui::ScrollInputType::kScrollbar);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(begin_state.get(), ui::ScrollInputType::kScrollbar)
            .thread);
    EXPECT_TRUE(GetInputHandler()
                    .scrollbar_controller_for_testing()
                    ->AutoscrollTaskIsScheduled());
  }

  {
    // Another mousedown occurs in the same frame. InputHandlerProxy calls
    // LayerTreeHostImpl::ScrollEnd and the autoscroll task should be cancelled.
    InputHandlerPointerResult result = GetInputHandler().MouseDown(
        gfx::PointF(350, 575), /*jump_key_modifier*/ false);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_FALSE(GetInputHandler()
                     .scrollbar_controller_for_testing()
                     ->AutoscrollTaskIsScheduled());
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that the ScrollbarController handles jump clicks.
TEST_P(LayerTreeHostImplTest, JumpOnScrollbarClick) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));

  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  const gfx::PointF click_pos(350, 400);

  // Verify all 4 combinations of JumpOnTrackClick and jump_key_modifier.
  {
    // Click on track when JumpOnTrackClick is false and jump_key_modifier is
    // false. Expected to perform a regular track scroll.
    scrollbar->SetJumpOnTrackClick(false);
    InputHandlerPointerResult result =
        GetInputHandler().MouseDown(click_pos, /*jump_key_modifier*/ false);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(),
              std::max(viewport_size.height() * kMinFractionToStepWhenPaging,
                       static_cast<float>(viewport_size.height() -
                                          kMaxOverlapBetweenPages)));
    result = GetInputHandler().MouseUp(click_pos);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(), 0);
  }

  const int thumb_length = scrollbar->ThumbLength();
  const float expected_jump_delta =
      round(click_pos.y() - thumb_length / 2.0f - scrollbar->track_rect().y()) *
      (scrollbar->scroll_layer_length() - scrollbar->clip_layer_length()) /
      (scrollbar->track_rect().height() - thumb_length);

  {
    // Click on track when JumpOnTrackClick is false and jump_key_modifier is
    // true. Expected to perform scroller jump to the clicked location.
    scrollbar->SetJumpOnTrackClick(false);
    InputHandlerPointerResult result =
        GetInputHandler().MouseDown(click_pos, /*jump_key_modifier*/ true);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_FLOAT_EQ(result.scroll_delta.y(), expected_jump_delta);
    result = GetInputHandler().MouseUp(click_pos);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(), 0);
  }

  {
    // Click on track when JumpOnTrackClick is true and jump_key_modifier is
    // false. Expected to perform scroller jump to the clicked location.
    scrollbar->SetJumpOnTrackClick(true);
    InputHandlerPointerResult result =
        GetInputHandler().MouseDown(click_pos, /*jump_key_modifier*/ false);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_FLOAT_EQ(result.scroll_delta.y(), expected_jump_delta);
    result = GetInputHandler().MouseUp(click_pos);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(), 0);
  }

  {
    // Click on track when JumpOnTrackClick is true and jump_key_modifier is
    // true. Expected to perform a regular track scroll.
    scrollbar->SetJumpOnTrackClick(true);
    InputHandlerPointerResult result =
        GetInputHandler().MouseDown(click_pos, /*jump_key_modifier*/ true);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(),
              std::max(viewport_size.height() * kMinFractionToStepWhenPaging,
                       static_cast<float>(viewport_size.height() -
                                          kMaxOverlapBetweenPages)));
    result = GetInputHandler().MouseUp(click_pos);
    EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
    EXPECT_EQ(result.scroll_delta.y(), 0);
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that a thumb drag continues to function as expected after a jump click
// or thumb click. The functionality of thumb drag itself is pretty well tested.
// So all that this test needs to verify is that the thumb drag_state_ is
// correctly populated.
TEST_P(LayerTreeHostImplTest, ThumbDragAfterJumpClickOrThumbClick) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));

  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  {
    // Test thumb drag after jump click

    scrollbar->SetJumpOnTrackClick(false);
    EXPECT_FALSE(GetInputHandler()
                     .scrollbar_controller_for_testing()
                     ->drag_state_.has_value());

    // Perform a pointerdown on the track part to induce a jump click.
    const InputHandlerPointerResult result =
        GetInputHandler().scrollbar_controller_for_testing()->HandlePointerDown(
            gfx::PointF(350, 560), /*jump_key_modifier*/ true);

    const int thumb_length = scrollbar->ThumbLength();
    const float expected_jump_delta =
        round(560 - thumb_length / 2.0f - scrollbar->track_rect().y()) *
        (scrollbar->scroll_layer_length() - scrollbar->clip_layer_length()) /
        (scrollbar->track_rect().height() - thumb_length);

    // This verifies that the jump click took place as expected.
    EXPECT_EQ(0, result.scroll_delta.x());
    EXPECT_FLOAT_EQ(result.scroll_delta.y(), expected_jump_delta);

    // This verifies that the drag_state_ was initialized when a jump click
    // occurred.
    EXPECT_TRUE(GetInputHandler()
                    .scrollbar_controller_for_testing()
                    ->drag_state_.has_value());

    // This verifies that the start/snap-back position is the scroll position
    // before any jump-click
    EXPECT_FLOAT_EQ(GetInputHandler()
                        .scrollbar_controller_for_testing()
                        ->drag_state_->scroll_position_at_start_,
                    0.0f);

    // Expect the drag origin to be at the center of the thumb.
    EXPECT_FLOAT_EQ(GetInputHandler()
                        .scrollbar_controller_for_testing()
                        ->drag_state_->drag_origin.y(),
                    15.0f + thumb_length / 2.0f);
    GetInputHandler().scrollbar_controller_for_testing()->HandlePointerUp(
        gfx::PointF(350, 560));
  }

  {
    // Test thumb drag after click on thumb

    scrollbar->SetJumpOnTrackClick(false);
    EXPECT_FALSE(GetInputHandler()
                     .scrollbar_controller_for_testing()
                     ->drag_state_.has_value());

    // Perform a pointerdown on the thumb part.
    const InputHandlerPointerResult result =
        GetInputHandler().scrollbar_controller_for_testing()->HandlePointerDown(
            gfx::PointF(350, 60), /*jump_key_modifier*/ true);

    // This verifies that the pointerdown on the thumb did not cause any jump
    EXPECT_EQ(0, result.scroll_delta.x());
    EXPECT_EQ(0, result.scroll_delta.y());

    // This verifies that the drag_state_ was initialized when click
    // on thumb.
    EXPECT_TRUE(GetInputHandler()
                    .scrollbar_controller_for_testing()
                    ->drag_state_.has_value());

    // This verifies that the start/snap-back position is the scroll position
    // before any jump-click
    EXPECT_FLOAT_EQ(GetInputHandler()
                        .scrollbar_controller_for_testing()
                        ->drag_state_->scroll_position_at_start_,
                    0.0f);

    // Expect the drag origin to be at the point of pointerdown.
    EXPECT_FLOAT_EQ(GetInputHandler()
                        .scrollbar_controller_for_testing()
                        ->drag_state_->drag_origin.y(),
                    60.0f);
    GetInputHandler().scrollbar_controller_for_testing()->HandlePointerUp(
        gfx::PointF(350, 60));
  }

  {
    // This test verifies that clicking on parts other than the track(jump
    // click) or thumb does not trigger a thumb drag.

    scrollbar->SetJumpOnTrackClick(false);
    EXPECT_FALSE(GetInputHandler()
                     .scrollbar_controller_for_testing()
                     ->drag_state_.has_value());

    // Perform a pointerdown on the button part to induce scroll.
    const InputHandlerPointerResult result =
        GetInputHandler().scrollbar_controller_for_testing()->HandlePointerDown(
            gfx::PointF(350, 580), /*jump_key_modifier*/ true);

    // This verifies that the scroll took place as expected, i.e. the click was
    // handled.
    EXPECT_EQ(0, result.scroll_delta.x());
    EXPECT_EQ(result.scroll_delta.y(),
              kPixelsPerLineStep * GetInputHandler()
                                       .scrollbar_controller_for_testing()
                                       ->ScreenSpaceScaleFactor());

    // This verifies that the drag_state_ was not initialized although the
    // click was handled.
    EXPECT_FALSE(GetInputHandler()
                     .scrollbar_controller_for_testing()
                     ->drag_state_.has_value());
    GetInputHandler().scrollbar_controller_for_testing()->HandlePointerUp(
        gfx::PointF(350, 580));
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that an existing scroll offset animation (for a scrollbar) is aborted
// before a new one is created.
TEST_P(LayerTreeHostImplTest, AbortAnimatedScrollBeforeStartingAutoscroll) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  {
    // Set up an animated scrollbar autoscroll.
    GetInputHandler().scrollbar_controller_for_testing()->HandlePointerDown(
        gfx::PointF(350, 560), /*jump_key_modifier*/ false);
    auto begin_state = BeginState(gfx::Point(350, 560), gfx::Vector2d(0, 40),
                                  ui::ScrollInputType::kScrollbar);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(begin_state.get(), ui::ScrollInputType::kScrollbar)
            .thread);
    auto update_state = UpdateState(gfx::Point(350, 560), gfx::Vector2dF(0, 40),
                                    ui::ScrollInputType::kScrollbar);
    update_state.data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;
    GetInputHandler().ScrollUpdate(update_state);

    // Autoscroll animations should be active.
    EXPECT_TRUE(GetInputHandler()
                    .scrollbar_controller_for_testing()
                    ->ScrollbarScrollIsActive());
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
  }

  {
    // When it's time to kick off the scrollbar autoscroll animation (i.e ~250ms
    // after pointerdown), the ScrollbarController should ensure that any
    // existing scroll offset animations are aborted and a new autoscroll
    // animation is created. Test passes if unit test doesn't hit any DCHECK
    // failures.
    GetInputHandler().scrollbar_controller_for_testing()->autoscroll_state_ =
        ScrollbarController::AutoScrollState();
    GetInputHandler()
        .scrollbar_controller_for_testing()
        ->autoscroll_state_->velocity = 800;
    GetInputHandler()
        .scrollbar_controller_for_testing()
        ->autoscroll_state_->pressed_scrollbar_part =
        ScrollbarPart::kForwardTrack;
    GetInputHandler().scrollbar_controller_for_testing()->StartAutoScroll();
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that an animated scrollbar scroll aborts when a different device (like
// a mousewheel) wants to animate the scroll offset.
TEST_P(LayerTreeHostImplTest, AnimatedScrollYielding) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  // TODO(crbug.com/40126196): Setting the dimensions for scrollbar parts
  // (like thumb, track etc) should be moved to SetupScrollbarLayer.
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  {
    // Set up an animated scrollbar autoscroll.
    GetInputHandler().scrollbar_controller_for_testing()->HandlePointerDown(
        gfx::PointF(350, 560), /*jump_key_modifier*/ false);
    auto begin_state = BeginState(gfx::Point(350, 560), gfx::Vector2d(0, 40),
                                  ui::ScrollInputType::kScrollbar);
    EXPECT_EQ(
        ScrollThread::kScrollOnImplThread,
        GetInputHandler()
            .ScrollBegin(begin_state.get(), ui::ScrollInputType::kScrollbar)
            .thread);
    auto update_state = UpdateState(gfx::Point(350, 560), gfx::Vector2dF(0, 40),
                                    ui::ScrollInputType::kScrollbar);
    update_state.data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;
    GetInputHandler().ScrollUpdate(update_state);

    // Autoscroll animations should be active.
    EXPECT_TRUE(GetInputHandler()
                    .scrollbar_controller_for_testing()
                    ->ScrollbarScrollIsActive());
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
  }

  {
    // While the cc autoscroll is in progress, a mousewheel tick takes place.
    auto begin_state = BeginState(gfx::Point(), gfx::Vector2d(350, 560),
                                  ui::ScrollInputType::kWheel);
    begin_state->data()->delta_granularity =
        ui::ScrollGranularity::kScrollByPixel;

    // InputHandlerProxy calls LayerTreeHostImpl::ScrollEnd to end the
    // autoscroll as it has detected a device change.
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    // This then leads to ScrollbarController::ResetState getting called which
    // clears ScrollbarController::autoscroll_state_,
    // captured_scrollbar_metadata_ etc. That means
    // ScrollbarController::ScrollbarLayer should return null.
    EXPECT_FALSE(
        GetInputHandler().scrollbar_controller_for_testing()->ScrollbarLayer());

    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
                  .thread);

    // Autoscroll animation should be aborted at this point.
    EXPECT_FALSE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());
    GetInputHandler().ScrollUpdate(
        AnimatedUpdateState(gfx::Point(350, 560), gfx::Vector2d(0, 40)));

    // Mousewheel animation should be active.
    EXPECT_TRUE(GetImplAnimationHost()->HasImplOnlyScrollAnimatingElement());

    // This should not trigger any DCHECKs and will be a no-op.
    GetInputHandler().scrollbar_controller_for_testing()->WillBeginImplFrame();
  }

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

// Tests that changing the scroller length in the middle of a thumb drag doesn't
// cause the scroller to jump.
TEST_P(LayerTreeHostImplTest, ThumbDragScrollerLengthIncrease) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical,
      /*is_left_side_vertical_scrollbar*/ false,
      /*is_overlay*/ false);

  // TODO(crbug.com/40126196): Setting the dimensions for scrollbar parts
  // (like thumb, track etc) should be moved to SetupScrollbarLayer.
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(0, 570), gfx::Size(15, 15)));

  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));
  layer_tree_impl->UpdateAllScrollbarGeometriesForTesting();

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // ----------------------------- Start frame 0 -----------------------------
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(200);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  InputHandlerPointerResult result = GetInputHandler().MouseDown(
      gfx::PointF(350, 18), /*jump_key_modifier*/ false);
  EXPECT_EQ(result.scroll_delta.y(), 0);
  EXPECT_EQ(result.type, PointerResultType::kScrollbarScroll);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 1 -----------------------------
  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);

  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 20));
  EXPECT_FLOAT_EQ(
      result.scroll_delta.y(),
      2 * (scrollbar->scroll_layer_length() - scrollbar->clip_layer_length()) /
          (scrollbar->track_rect().height() - scrollbar->ThumbLength()));

  // This is intentional. The thumb drags that follow will test the behavior
  // *after* the scroller length expansion.
  scrollbar->SetScrollLayerLength(7000);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 2 -----------------------------
  // The very first mousemove after the scroller length change will result in a
  // zero offset. This is done to ensure that the scroller doesn't jump ahead
  // when the length changes mid thumb drag. So, every time the scroller length
  // changes mid thumb drag, a GSU is lost (in the worst case).
  begin_frame_args.frame_time = start_time + base::Milliseconds(300);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 22));
  EXPECT_EQ(result.scroll_delta.y(), 0);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // ----------------------------- Start frame 3 -----------------------------
  // All subsequent mousemoves then produce deltas which are "displaced" by a
  // certain amount. The previous mousemove (to y = 22) would have calculated
  // the drag_state_->scroller_displacement value as 48 (based on the pointer
  // movement). The current pointermove (to y = 26) calculates the delta as 97.
  // Since delta -= drag_state_->scroller_displacement, the actual delta becomes
  // 97 - 48 which is 49. The math that calculates the deltas (i.e 97 and 48)
  // can be found in ScrollbarController::GetScrollDeltaForDragPosition.
  begin_frame_args.frame_time = start_time + base::Milliseconds(350);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  result = GetInputHandler().MouseMoveAt(gfx::Point(350, 26));
  EXPECT_FLOAT_EQ(result.scroll_delta.y(), 48.761906f);
  GetInputHandler().MouseUp(gfx::PointF(350, 26));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_P(LayerTreeHostImplTest, MainThreadFallback) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // Setup the viewport.
  const gfx::Size viewport_size = gfx::Size(360, 600);
  const gfx::Size content_size = gfx::Size(345, 3800);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  LayerImpl* scroll_layer = OuterViewportScrollLayer();

  // Set up the scrollbar and its dimensions.
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayer(scroll_layer, scrollbar);
  const gfx::Size scrollbar_size = gfx::Size(15, 600);
  scrollbar->SetBounds(scrollbar_size);

  // Set up the thumb dimensions.
  scrollbar->SetThumbThickness(15);
  scrollbar->SetMinimumThumbLength(50);
  scrollbar->SetTrackRect(gfx::Rect(0, 15, 15, 575));

  // Set up scrollbar arrows.
  scrollbar->SetBackButtonRect(
      gfx::Rect(gfx::Point(345, 0), gfx::Size(15, 15)));
  scrollbar->SetForwardButtonRect(
      gfx::Rect(gfx::Point(345, 570), gfx::Size(15, 15)));
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  TestInputHandlerClient input_handler_client;
  GetInputHandler().BindToClient(&input_handler_client);

  // Clicking on the track should produce a scroll on the compositor thread.
  InputHandlerPointerResult compositor_threaded_scrolling_result =
      GetInputHandler().MouseDown(gfx::PointF(350, 500),
                                  /*jump_key_modifier*/ false);
  GetInputHandler().MouseUp(gfx::PointF(350, 500));
  EXPECT_EQ(compositor_threaded_scrolling_result.scroll_delta.y(),
            std::max(viewport_size.height() * kMinFractionToStepWhenPaging,
                     static_cast<float>(viewport_size.height() -
                                        kMaxOverlapBetweenPages)));
  EXPECT_FALSE(GetScrollNode(scroll_layer)->main_thread_repaint_reasons);

  // Assign a main_thread_scrolling_reason to the scroll node.
  GetScrollNode(scroll_layer)->main_thread_repaint_reasons =
      MainThreadScrollingReason::kPreferNonCompositedScrolling;
  compositor_threaded_scrolling_result = GetInputHandler().MouseDown(
      gfx::PointF(350, 500), /*jump_key_modifier*/ false);
  GetInputHandler().MouseUp(gfx::PointF(350, 500));
  // A scrollbar layer track click applies the scroll on the compositor thread
  // even though it has a main thread scrolling reason.
  EXPECT_EQ(compositor_threaded_scrolling_result.scroll_delta.y(),
            std::max(viewport_size.height() * kMinFractionToStepWhenPaging,
                     static_cast<float>(viewport_size.height() -
                                        kMaxOverlapBetweenPages)));

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_P(LayerTreeHostImplTest, SecondScrollAnimatedBeginNotIgnored) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The second ScrollBegin should not get ignored.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
}

// Verfify that a smooth scroll animation doesn't jump when UpdateTarget gets
// called before the animation is started.
TEST_P(AnimationsLayerTreeHostImplTest,
       AnimatedScrollUpdateTargetBeforeStarting) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(200);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));
  // This will call ScrollOffsetAnimationCurve::UpdateTarget while the animation
  // created above is in state ANIMATION::WAITING_FOR_TARGET_AVAILABILITY and
  // doesn't have a start time.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)));

  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  // This is when the animation above gets promoted to STARTING.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(300);
  begin_frame_args.frame_id.sequence_number++;
  // This is when the animation above gets ticked.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify no jump.
  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);
}

TEST_P(AnimationsLayerTreeHostImplTest, ScrollAnimatedWithDelay) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Create animation with a 100ms delay.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 100),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)),
      base::Milliseconds(100));

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // First tick, animation is started.
  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_NE(gfx::PointF(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Second tick after 50ms, animation should be half way done since the
  // duration due to delay is 100ms. Subtract off the frame interval since we
  // progress a full frame on the first tick.
  base::TimeTicks half_way_time =
      start_time - begin_frame_args.interval + base::Milliseconds(50);
  begin_frame_args.frame_time = half_way_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(50, CurrentScrollOffset(scrolling_layer).y());
  host_impl_->DidFinishImplFrame(begin_frame_args);

  // Update target.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 100)),
      base::Milliseconds(150));

  // Third tick after 100ms, should be at the target position since update
  // target was called with a large value of jank.
  begin_frame_args.frame_time = start_time + base::Milliseconds(100);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_LT(100, CurrentScrollOffset(scrolling_layer).y());
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST_P(LayerTreeHostImplTimelinesTest, ScrollAnimatedAborted) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Perform animated scroll.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_TRUE(GetImplAnimationHost()->HasAnyAnimationTargetingProperty(
      scrolling_layer->element_id(), TargetProperty::SCROLL_OFFSET));

  EXPECT_NE(gfx::PointF(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // Perform instant scroll.
  // Use "precise pixel" granularity to avoid animating.
  auto begin_state = BeginState(gfx::Point(0, y), gfx::Vector2dF(0, 50),
                                ui::ScrollInputType::kWheel);
  begin_state->data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
                .thread);
  auto update_state = UpdateState(gfx::Point(0, y), gfx::Vector2d(0, 50),
                                  ui::ScrollInputType::kWheel);
  // Use "precise pixel" granularity to avoid animating.
  update_state.data()->delta_granularity =
      ui::ScrollGranularity::kScrollByPrecisePixel;
  GetInputHandler().ScrollUpdate(update_state);
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

  // The instant scroll should have marked the smooth scroll animation as
  // aborted.
  EXPECT_FALSE(GetImplAnimationHost()->HasTickingKeyframeModelForTesting(
      scrolling_layer->element_id()));

  EXPECT_POINTF_EQ(gfx::PointF(0, y + 50),
                   CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Evolved from LayerTreeHostImplTest.ScrollAnimated.
TEST_P(LayerTreeHostImplTimelinesTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(0, 50),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::PointF(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(0, 50)));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_POINTF_EQ(gfx::PointF(0, 100), CurrentScrollOffset(scrolling_layer));
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

// Test that the scroll delta for an animated scroll is distributed correctly
// between the inner and outer viewport.
TEST_P(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimated) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);
  UpdateDrawProperties(host_impl_->active_tree());

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport (but scrolling the viewport always sets the outer as the
  // currently scrolling node).
  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(250);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(10, 20),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(10, 20)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  EXPECT_POINTF_EQ(gfx::PointF(5, 10), CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(100, 100)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(350));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(5, 10), CurrentScrollOffset(outer_scroll_layer));

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(190, 180)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(850));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(100, 100),
                   CurrentScrollOffset(outer_scroll_layer));

  // Scroll upwards by the max scroll extent. The inner viewport should animate
  // and the remainder should bubble to the outer viewport.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(-110, -120)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(1200));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(95, 90),
                   CurrentScrollOffset(outer_scroll_layer));
}

// Test that the correct viewport scroll layer is updated when the target offset
// is updated.
TEST_P(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimatedUpdate) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll the inner viewport.
  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(50);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(90, 90),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(90, 90)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  // When either the inner or outer node is being scrolled, the outer node is
  // the one that's "latched".
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  float inner_x = CurrentScrollOffset(inner_scroll_layer).x();
  float inner_y = CurrentScrollOffset(inner_scroll_layer).y();
  EXPECT_TRUE(inner_x > 0 && inner_x < 45);
  EXPECT_TRUE(inner_y > 0 && inner_y < 45);
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));

  // Update target.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)));
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify that all the delta is applied to the inner viewport and nothing is
  // carried forward.
  begin_frame_args.frame_id.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(350));
  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(inner_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(outer_scroll_layer));
}

// Test that smooth scroll offset animation doesn't happen for non user
// scrollable layers.
TEST_P(LayerTreeHostImplTimelinesTest, ScrollAnimatedNotUserScrollable) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  GetScrollNode(scrolling_layer)->user_scrollable_vertical = true;
  GetScrollNode(scrolling_layer)->user_scrollable_horizontal = false;

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2d(50, 50),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)));

  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::PointF(), CurrentScrollOffset(scrolling_layer));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(50);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should not have scrolled horizontally.
  EXPECT_EQ(0, CurrentScrollOffset(scrolling_layer).x());
  float y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(50, 50)));
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = CurrentScrollOffset(scrolling_layer).y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  begin_frame_args.frame_time = start_time + base::Milliseconds(250);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_POINTF_EQ(gfx::PointF(0, 100), CurrentScrollOffset(scrolling_layer));
  // The CurrentlyScrollingNode shouldn't be cleared until a GestureScrollEnd.
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
}

// Test that smooth scrolls clamp correctly when bounds change mid-animation.
TEST_P(LayerTreeHostImplTimelinesTest, ScrollAnimatedChangingBounds) {
  const gfx::Size old_content_size(1000, 1000);
  const gfx::Size new_content_size(750, 750);
  const gfx::Size viewport_size(500, 500);

  SetupViewportLayersOuterScrolls(viewport_size, old_content_size);
  LayerImpl* scrolling_layer = OuterViewportScrollLayer();
  LayerImpl* content_layer = AddContentLayer();

  DrawFrame();

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(), gfx::Vector2d(500, 500),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(gfx::Point(), gfx::Vector2d(500, 500)));

  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  content_layer->SetBounds(new_content_size);
  scrolling_layer->SetBounds(new_content_size);
  GetScrollNode(scrolling_layer)->bounds = new_content_size;

  begin_frame_args.frame_time = start_time + base::Milliseconds(200);
  begin_frame_args.frame_id.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_EQ(gfx::PointF(250, 250), CurrentScrollOffset(scrolling_layer));
}

TEST_P(LayerTreeHostImplTest, WheelScrollWithPageScaleFactorOnInnerLayer) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* scroll_layer = InnerViewportScrollLayer();
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  DrawFrame();

  EXPECT_EQ(scroll_layer, InnerViewportScrollLayer());

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 1;

  // The scroll deltas should have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2;
    GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                             ui::ScrollInputType::kTouchscreen)
                                      .get(),
                                  ui::ScrollInputType::kTouchscreen);
    GetInputHandler().PinchGestureBegin(gfx::Point(),
                                        ui::ScrollInputType::kWheel);
    GetInputHandler().PinchGestureUpdate(page_scale_delta, gfx::Point());
    GetInputHandler().PinchGestureEnd(gfx::Point());
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);

    gfx::Vector2dF scroll_delta(0, 5);
    EXPECT_EQ(ScrollThread::kScrollOnImplThread,
              GetInputHandler()
                  .ScrollBegin(BeginState(gfx::Point(), scroll_delta,
                                          ui::ScrollInputType::kWheel)
                                   .get(),
                               ui::ScrollInputType::kWheel)
                  .thread);
    EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(scroll_layer));

    GetInputHandler().ScrollUpdate(
        UpdateState(gfx::Point(), scroll_delta, ui::ScrollInputType::kWheel));
    GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
    EXPECT_POINTF_EQ(gfx::PointF(0, 2.5), CurrentScrollOffset(scroll_layer));
  }
}

class LayerTreeHostImplCountingLostSurfaces : public LayerTreeHostImplTest {
 public:
  void DidLoseLayerTreeFrameSinkOnImplThread() override {
    num_lost_surfaces_++;
  }

 protected:
  int num_lost_surfaces_ = 0;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(LayerTreeHostImplCountingLostSurfaces);

// We do not want to reset context recovery state when we get repeated context
// loss notifications via different paths.
TEST_P(LayerTreeHostImplCountingLostSurfaces, TwiceLostSurface) {
  EXPECT_EQ(0, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
}

size_t CountRenderPassesWithId(const viz::CompositorRenderPassList& list,
                               viz::CompositorRenderPassId id) {
  return std::ranges::count(list, id, &viz::CompositorRenderPass::id);
}

TEST_P(LayerTreeHostImplTest, RemoveUnreferencedRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass2->SetNew(viz::CompositorRenderPassId{2}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass3->SetNew(viz::CompositorRenderPassId{3}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());

  // Add a quad to each pass so they aren't empty.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;
  color_quad = pass2->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;
  color_quad = pass3->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;

  // pass3 is referenced by pass2.
  auto* rpdq =
      pass2->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = pass3->id;

  // But pass2 is not referenced by pass1. So pass2 and pass3 should be culled.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{1u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{2u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{3u}));
  EXPECT_EQ(viz::CompositorRenderPassId{1u}, frame.render_passes[0]->id);
}

TEST_P(LayerTreeHostImplTest, RemoveEmptyRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass2->SetNew(viz::CompositorRenderPassId{2}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass3->SetNew(viz::CompositorRenderPassId{3}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());

  // pass1 is not empty, but pass2 and pass3 are.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::Material::kSolidColor;

  // pass3 is referenced by pass2.
  auto* rpdq =
      pass2->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{1u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{2u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{3u}));
  EXPECT_EQ(viz::CompositorRenderPassId{1u}, frame.render_passes[0]->id);
  // The viz::CompositorRenderPassDrawQuad should be removed from pass1.
  EXPECT_EQ(1u, pass1->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::Material::kSolidColor,
            pass1->quad_list.ElementAt(0)->material);
}

TEST_P(LayerTreeHostImplTest, DoNotRemoveEmptyRootRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::CompositorRenderPass::Create());
  viz::CompositorRenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass2->SetNew(viz::CompositorRenderPassId{2}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());
  pass3->SetNew(viz::CompositorRenderPassId{3}, gfx::Rect(), gfx::Rect(),
                gfx::Transform());

  // pass3 is referenced by pass2.
  auto* rpdq =
      pass2->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::CompositorRenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::Material::kCompositorRenderPass;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed. Then pass1 is empty too, but it's the root so it should
  // not be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{1u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{2u}));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes,
                                        viz::CompositorRenderPassId{3u}));
  EXPECT_EQ(viz::CompositorRenderPassId{1u}, frame.render_passes[0]->id);
  // The viz::CompositorRenderPassDrawQuad should be removed from pass1.
  EXPECT_EQ(0u, pass1->quad_list.size());
}

class FakeVideoFrameController : public VideoFrameController {
 public:
  void OnBeginFrame(const viz::BeginFrameArgs& args) override {
    begin_frame_args_ = args;
    did_draw_frame_ = false;
  }

  void DidDrawFrame() override { did_draw_frame_ = true; }

  const viz::BeginFrameArgs& begin_frame_args() const {
    return begin_frame_args_;
  }

  bool did_draw_frame() const { return did_draw_frame_; }

 private:
  viz::BeginFrameArgs begin_frame_args_;
  bool did_draw_frame_ = false;
};

TEST_P(LayerTreeHostImplTest, AddVideoFrameControllerInsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->AddVideoFrameController(&controller);
  EXPECT_TRUE(controller.begin_frame_args().IsValid());
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_FALSE(controller.did_draw_frame());
  TestFrameData frame;
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_TRUE(controller.did_draw_frame());

  controller.OnBeginFrame(begin_frame_args);
  EXPECT_FALSE(controller.did_draw_frame());
  host_impl_->RemoveVideoFrameController(&controller);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_FALSE(controller.did_draw_frame());
}

TEST_P(LayerTreeHostImplTest, AddVideoFrameControllerOutsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->DidFinishImplFrame(begin_frame_args);

  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->AddVideoFrameController(&controller);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());

  begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 3);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->WillBeginImplFrame(begin_frame_args);
  EXPECT_TRUE(controller.begin_frame_args().IsValid());

  EXPECT_FALSE(controller.did_draw_frame());
  TestFrameData frame;
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_TRUE(controller.did_draw_frame());

  controller.OnBeginFrame(begin_frame_args);
  EXPECT_FALSE(controller.did_draw_frame());
  host_impl_->RemoveVideoFrameController(&controller);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_FALSE(controller.did_draw_frame());
}

class GpuRasterizationDisabledLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

class MsaaIsSlowLayerTreeHostImplTest
    : public CommitToActiveTreeLayerTreeHostImplTest {
 public:
  void CreateHostImplWithCaps(bool msaa_is_slow, bool avoid_stencil_buffers) {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_msaa_sample_count = 4;
    auto frame_sink =
        FakeLayerTreeFrameSink::Builder()
            .AllContexts(&viz::TestRasterInterface::set_msaa_is_slow,
                         msaa_is_slow)
            .AllContexts(&viz::TestRasterInterface::set_avoid_stencil_buffers,
                         avoid_stencil_buffers)
            .EnableGpuTileRasterizationFeatureInWorkerContext()
            .Build();
    EXPECT_TRUE(CreateHostImpl(settings, std::move(frame_sink)));
  }
};

TEST_F(MsaaIsSlowLayerTreeHostImplTest, GpuRasterizationStatusMsaaIsSlow) {
  // Ensure that without the msaa_is_slow or avoid_stencil_buffers caps
  // we raster slow paths with msaa.
  CreateHostImplWithCaps(false, false);
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())->CommitComplete();
  }
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->can_use_msaa());

  // Ensure that with either msaa_is_slow or avoid_stencil_buffers caps
  // we don't raster slow paths with msaa (we'll still use GPU raster, though).
  // msaa_is_slow = true, avoid_stencil_buffers = false
  CreateHostImplWithCaps(true, false);
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())->CommitComplete();
  }
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());

  // msaa_is_slow = false, avoid_stencil_buffers = true
  CreateHostImplWithCaps(false, true);
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())->CommitComplete();
  }
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());

  // msaa_is_slow = true, avoid_stencil_buffers = true
  CreateHostImplWithCaps(true, true);
  // TODO(496580137): Move this to ClientLayerTreeHostImpl specific tests.
  if (!host_impl_->settings().trees_in_viz_in_viz_process) {
    static_cast<ClientLayerTreeHostImpl*>(host_impl_.get())->CommitComplete();
  }
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());
}

TEST_P(LayerTreeHostImplTest, SubLayerScaleForNodeInSubtreeOfPageScaleLayer) {
  // Checks that the sublayer scale of a transform node in the subtree of the
  // page scale layer is updated without a property tree rebuild.
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 3);
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  LayerImpl* in_subtree_of_page_scale_layer = AddLayerInActiveTree();
  CopyProperties(root_layer(), in_subtree_of_page_scale_layer);
  in_subtree_of_page_scale_layer->SetTransformTreeIndex(
      host_impl_->active_tree()->PageScaleTransformNode()->id);
  CreateEffectNode(in_subtree_of_page_scale_layer).render_surface_reason =
      RenderSurfaceReason::kTest;

  DrawFrame();

  EffectNode* node = GetEffectNode(in_subtree_of_page_scale_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1, 1));

  host_impl_->active_tree()->SetPageScaleOnActiveTree(2);

  DrawFrame();

  node = GetEffectNode(in_subtree_of_page_scale_layer);
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2, 2));
}

// Checks that if we lose a GPU raster enabled LayerTreeFrameSink and replace
// it with a software LayerTreeFrameSink, LayerTreeHostImpl correctly
// re-computes GPU rasterization status.
TEST_P(LayerTreeHostImplTest, RecomputeGpuRasterOnLayerTreeFrameSinkChange) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  host_impl_ = CreateLayerTreeHostImplForTesting(
      DefaultSettings(), this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
      nullptr, nullptr);
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl_));
  host_impl_->SetVisible(true);

  // InitializeFrameSink with a gpu-raster enabled output surface.
  auto gpu_raster_layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  host_impl_->InitializeFrameSink(gpu_raster_layer_tree_frame_sink.get());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Re-initialize with a software output surface.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
}



TEST_P(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(true);
}

TEST_P(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInNotMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(false);
}

TEST_P(LayerTreeHostImplTest, RasterColorSpace) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  // The default raster color space should be sRGB.
  auto wcg_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kWideColorGamut);
  EXPECT_EQ(wcg_params.color_space, gfx::ColorSpace::CreateSRGB());

  // The raster color space should update with tree activation.
  host_impl_->active_tree()->SetDisplayColorSpaces(
      gfx::DisplayColorSpaces(gfx::ColorSpace::CreateDisplayP3D65()));
  wcg_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kWideColorGamut);
  EXPECT_EQ(wcg_params.color_space, gfx::ColorSpace::CreateDisplayP3D65());
}

TEST_P(LayerTreeHostImplTest, RasterColorSpaceSoftware) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, FakeLayerTreeFrameSink::CreateSoftware());

  // Software composited resources should always use sRGB as their color space.
  auto wcg_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kWideColorGamut);
  EXPECT_EQ(wcg_params.color_space, gfx::ColorSpace::CreateSRGB());

  host_impl_->active_tree()->SetDisplayColorSpaces(
      gfx::DisplayColorSpaces(gfx::ColorSpace::CreateDisplayP3D65()));
  wcg_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kWideColorGamut);
  EXPECT_EQ(wcg_params.color_space, gfx::ColorSpace::CreateSRGB());
}

TEST_P(LayerTreeHostImplTest, RasterColorSpaceHDR) {
  constexpr float kCustomWhiteLevel = 200.f;
  constexpr float kHDRMaxLuminanceRelative = 2.f;
  auto hdr = gfx::ColorSpace::CreateHDR10();
  gfx::DisplayColorSpaces display_cs(hdr);
  display_cs.SetSDRMaxLuminanceNits(kCustomWhiteLevel);
  display_cs.SetHDRMaxLuminanceRelative(kHDRMaxLuminanceRelative);

  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDisplayColorSpaces(display_cs);

  const auto srgb_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kSRGB);
  const auto wcg_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kWideColorGamut);
  const auto hdr_params =
      host_impl_->GetTargetColorParams(gfx::ContentColorUsage::kHDR);

  // sRGB content is rastered as sRGB, WCG as P3.
  const auto srgb = gfx::ColorSpace::CreateSRGB();
  const auto p3 = gfx::ColorSpace::CreateDisplayP3D65();
  EXPECT_EQ(srgb_params.color_space, srgb);
  EXPECT_EQ(srgb_params.GetHdrHeadroom(), 0.f);
  EXPECT_EQ(wcg_params.color_space, p3);
  EXPECT_EQ(wcg_params.GetHdrHeadroom(), 0.f);
  EXPECT_EQ(hdr_params.color_space, p3.GetAsHDR());
  EXPECT_EQ(hdr_params.GetHdrHeadroom(), std::log2(kHDRMaxLuminanceRelative));
}

TEST_P(LayerTreeHostImplTest, AllowedTouchActionTest1) {
  AllowedTouchActionTestHelper(1.0f, 1.0f);
}

TEST_P(LayerTreeHostImplTest, AllowedTouchActionTest2) {
  AllowedTouchActionTestHelper(1.0f, 0.789f);
}

TEST_P(LayerTreeHostImplTest, AllowedTouchActionTest3) {
  AllowedTouchActionTestHelper(2.345f, 1.0f);
}

TEST_P(LayerTreeHostImplTest, AllowedTouchActionTest4) {
  AllowedTouchActionTestHelper(2.654f, 0.678f);
}

TEST_P(LayerTreeHostImplTest, RenderFrameMetadata) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 0.5f, 4);

  {
    // Check initial metadata is correct.
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::PointF(), metadata.root_scroll_offset);
    EXPECT_EQ(1, metadata.page_scale_factor);

#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(gfx::SizeF(50, 50), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(0, 10),
                                        ui::ScrollInputType::kWheel)
                                 .get(),
                             ui::ScrollInputType::kWheel)
                .thread);
  GetInputHandler().ScrollUpdate(UpdateState(gfx::Point(), gfx::Vector2d(0, 10),
                                             ui::ScrollInputType::kWheel));
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
  }
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
  }

#if BUILDFLAG(IS_ANDROID)
  // Root "overflow: hidden" properties should be reflected on the outer
  // viewport scroll layer.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
  }

  // Re-enable scrollability and verify that overflows are no longer
  // hidden.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = true;
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = true;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  // Root "overflow: hidden" properties should also be reflected on the
  // inner viewport scroll layer.
  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_horizontal = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
  }

  {
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->OuterViewportScrollNode()->user_scrollable_vertical = false;

    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
  }
#endif

  // Page scale should update metadata correctly (shrinking only the viewport).
  GetInputHandler().ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                           ui::ScrollInputType::kTouchscreen)
                                    .get(),
                                ui::ScrollInputType::kTouchscreen);
  GetInputHandler().PinchGestureBegin(gfx::Point(),
                                      ui::ScrollInputType::kWheel);
  GetInputHandler().PinchGestureUpdate(2, gfx::Point());
  GetInputHandler().PinchGestureEnd(gfx::Point());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(2, metadata.page_scale_factor);

#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(gfx::SizeF(25, 25), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
#endif
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessCompositorDeltas(/* main_thread_mutator_host */ nullptr);
  host_impl_->active_tree()->PushPageScaleFromMainThread(4, 0.5f, 4);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(4);
  {
    RenderFrameMetadata metadata = StartDrawAndProduceRenderFrameMetadata();

    EXPECT_EQ(gfx::PointF(0, 10), metadata.root_scroll_offset);
    EXPECT_EQ(4, metadata.page_scale_factor);

#if BUILDFLAG(IS_ANDROID)
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);
    EXPECT_EQ(4, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100, 100), metadata.root_layer_size);
#endif
  }
}

// Tests ScrollUpdate() to see if the method sets the scroll tree's currently
// scrolling node.
TEST_P(LayerTreeHostImplTest, ScrollUpdateDoesNotSetScrollingNode) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  UpdateDrawProperties(host_impl_->active_tree());

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree_mutable();

  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(BeginState(gfx::Point(), gfx::Vector2dF(),
                                        ui::ScrollInputType::kTouchscreen)
                                 .get(),
                             ui::ScrollInputType::kTouchscreen)
                .thread);

  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  EXPECT_TRUE(scroll_node);

  ScrollStateData scroll_state_data;
  ScrollState scroll_state(scroll_state_data);
  GetInputHandler().ScrollUpdate(scroll_state);

  // Check to see the scroll tree's currently scrolling node is
  // still the same.
  EXPECT_EQ(scroll_node, scroll_tree.CurrentlyScrollingNode());

  // Set the scroll tree's currently scrolling node to null.
  host_impl_->active_tree()->SetCurrentlyScrollingNode(nullptr);
  EXPECT_FALSE(scroll_tree.CurrentlyScrollingNode());

  GetInputHandler().ScrollUpdate(scroll_state);

  EXPECT_EQ(nullptr, scroll_tree.CurrentlyScrollingNode());
}

class HitTestRegionListGeneratingLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    // Enable hit test data generation with the CompositorFrame.
    LayerTreeSettings new_settings = settings;
    return LayerTreeHostImplTest::CreateHostImpl(
        new_settings, std::move(layer_tree_frame_sink));
  }
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(
    HitTestRegionListGeneratingLayerTreeHostImplTest);

// Test to ensure that hit test data is created correctly from the active layer
// tree.
TEST_P(HitTestRegionListGeneratingLayerTreeHostImplTest, BuildHitTestData) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---intermediate_layer (200, 300), 200x200
  // +-----surface_child1 (50, 50), 100x100, Rotate(45)
  // +---surface_child2 (450, 300), 100x100
  // +---overlapping_layer (500, 350), 200x200
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* intermediate_layer = AddLayerInActiveTree();
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* overlapping_layer = AddLayerInActiveTree();

  intermediate_layer->SetBounds(gfx::Size(200, 200));

  surface_child1->SetBounds(gfx::Size(100, 100));
  gfx::Transform rotate;
  rotate.Rotate(45);
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(true);

  overlapping_layer->SetBounds(gfx::Size(200, 200));
  overlapping_layer->SetDrawsContent(true);
  overlapping_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);
  surface_child2->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  CopyProperties(root, intermediate_layer);
  intermediate_layer->SetOffsetToTransformParent(gfx::Vector2dF(200, 300));
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(450, 300));
  CopyProperties(root, overlapping_layer);
  overlapping_layer->SetOffsetToTransformParent(gfx::Vector2dF(500, 350));

  CopyProperties(intermediate_layer, surface_child1);
  auto& surface_child1_transform_node = CreateTransformNode(surface_child1);
  // The post_translation includes offset of intermediate_layer.
  surface_child1_transform_node.post_translation = gfx::Vector2dF(250, 350);
  surface_child1_transform_node.local = rotate;

  UpdateDrawProperties(host_impl_->active_tree());
  draw_property_utils::ComputeEffects(
      &host_impl_->active_tree()->property_trees()->effect_tree_mutable());

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  std::optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  // Since surface_child2 draws in front of surface_child1, it should also be in
  // the front of the hit test region list.
  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(2u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[1].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[1].flags);
  gfx::Transform child1_transform;
  child1_transform.Rotate(-45);
  child1_transform.Translate(-250, -350);
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[1].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), hit_test_region_list->regions[1].rect);

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child2_transform;
  child2_transform.Translate(-450, -300);
  EXPECT_TRUE(child2_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), hit_test_region_list->regions[0].rect);
}

TEST_P(HitTestRegionListGeneratingLayerTreeHostImplTest, PointerEvents) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---overlapping_surface_child2 (50, 50), 100x100, pointer-events: none,
  // does not generate hit test region
  auto* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());
  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(false);
  surface_child2->SetHasPointerEventsNone(true);
  CopyProperties(root, surface_child2);
  surface_child2->SetOffsetToTransformParent(gfx::Vector2dF(50, 50));

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  // Since |surface_child2| is not |surface_hit_testable|, it does not
  // contribute to a hit test region. Although it overlaps |surface_child1|, it
  // does not make |surface_child1| kHitTestAsk because it has pointer-events
  // none property.
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), hit_test_region_list->regions[0].rect);
}

TEST_P(HitTestRegionListGeneratingLayerTreeHostImplTest, ComplexPage) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child (0, 0), 100x100
  // +---100x non overlapping layers (110, 110), 1x1
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child->SetBounds(gfx::Size(100, 100));
  surface_child->SetDrawsContent(true);
  surface_child->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child->SetSurfaceHitTestable(true);
  surface_child->SetHasPointerEventsNone(false);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                          std::nullopt);

  CopyProperties(root, surface_child);

  // Create 101 non overlapping layers.
  for (size_t i = 0; i <= 100; ++i) {
    LayerImpl* layer = AddLayerInActiveTree();
    layer->SetBounds(gfx::Size(1, 1));
    layer->SetDrawsContent(true);
    layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
    CopyProperties(root, layer);
  }

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // Since the layer count is greater than 100, in order to save time, we do not
  // check whether each layer overlaps the surface layer, instead, we are being
  // conservative and make the surface layer slow hit testing.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), hit_test_region_list->regions[0].rect);
}

TEST_P(HitTestRegionListGeneratingLayerTreeHostImplTest, InvalidFrameSinkId) {
  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---surface_child1 (0, 0), 100x100
  // +---surface_child2 (0, 0), 50x50, frame_sink_id = (0, 0)
  LayerImpl* root = SetupDefaultRootLayer(gfx::Size(1024, 768));
  auto* surface_child1 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(1024, 768));

  surface_child1->SetBounds(gfx::Size(100, 100));
  surface_child1->SetDrawsContent(true);
  surface_child1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child1->SetSurfaceHitTestable(true);
  surface_child1->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child1);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(std::nullopt, child_surface_id),
                           std::nullopt);

  auto* surface_child2 = AddLayer<SurfaceLayerImpl>(host_impl_->active_tree());

  surface_child2->SetBounds(gfx::Size(50, 50));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  surface_child2->SetSurfaceHitTestable(true);
  surface_child2->SetHasPointerEventsNone(false);
  CopyProperties(root, surface_child2);

  surface_child2->SetRange(viz::SurfaceRange(std::nullopt, viz::SurfaceId()),
                           std::nullopt);

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  UpdateDrawProperties(host_impl_->active_tree());
  std::optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  // Generating HitTestRegionList should have been enabled for this test.
  ASSERT_TRUE(hit_test_region_list);

  uint32_t expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                            viz::HitTestRegionFlags::kHitTestTouch |
                            viz::HitTestRegionFlags::kHitTestMine;
  EXPECT_EQ(expected_flags, hit_test_region_list->flags);
  EXPECT_EQ(kFrameRect, hit_test_region_list->bounds);
  EXPECT_EQ(1u, hit_test_region_list->regions.size());

  EXPECT_EQ(child_surface_id.frame_sink_id(),
            hit_test_region_list->regions[0].frame_sink_id);
  // We do not populate hit test region for a surface layer with invalid frame
  // sink id to avoid deserialization failure. Instead we make the overlapping
  // hit test region kHitTestAsk.
  expected_flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch |
                   viz::HitTestRegionFlags::kHitTestChildSurface |
                   viz::HitTestRegionFlags::kHitTestAsk;
  EXPECT_EQ(expected_flags, hit_test_region_list->regions[0].flags);
  gfx::Transform child1_transform;
  EXPECT_TRUE(child1_transform.ApproximatelyEqual(
      hit_test_region_list->regions[0].transform));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), hit_test_region_list->regions[0].rect);
}

TEST_P(LayerTreeHostImplTest, SkipOnDrawDoesNotUpdateDrawParams) {
  EXPECT_TRUE(CreateHostImpl(DefaultSettings(),
                             FakeLayerTreeFrameSink::CreateSoftware()));
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));
  auto* layer = InnerViewportScrollLayer();
  layer->SetDrawsContent(true);
  gfx::Transform transform;
  transform.Translate(20, 20);
  gfx::Rect viewport(0, 0, 50, 50);
  const bool resourceless_software_draw = false;

  bool skip_draw = false;
  host_impl_->OnDraw(transform, viewport, resourceless_software_draw,
                     skip_draw);
  EXPECT_EQ(transform, host_impl_->DrawTransform());
  EXPECT_EQ(viewport, host_impl_->active_tree()->GetDeviceViewport());

  skip_draw = true;
  gfx::Transform new_transform;
  gfx::Rect new_viewport;
  host_impl_->OnDraw(new_transform, new_viewport, resourceless_software_draw,
                     skip_draw);
  EXPECT_EQ(transform, host_impl_->DrawTransform());
  EXPECT_EQ(viewport, host_impl_->active_tree()->GetDeviceViewport());
}

// Test that a touch scroll over a SolidColorScrollbarLayer, the scrollbar used
// on Android, does not register as a scrollbar scroll and result in main
// threaded scrolling.
TEST_P(LayerTreeHostImplTest, TouchScrollOnAndroidScrollbar) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size viewport_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(360, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  SetupViewportLayersNoScrolls(viewport_size);
  LayerImpl* content = AddScrollableLayer(OuterViewportScrollLayer(),
                                          viewport_size, scroll_content_size);

  auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, 10, 0, false);
  SetupScrollbarLayer(content, scrollbar);
  scrollbar->SetBounds(scrollbar_size);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();

  // Do a scroll over the scrollbar layer as well as the content layer, which
  // should result in scrolling the scroll layer on the impl thread as the
  // scrollbar should not be hit.
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 50), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kTouchscreen)
          .get(),
      ui::ScrollInputType::kTouchscreen);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
}

// Verify that page based scrolling resolves to the correct amount of scroll
// delta.
TEST_P(LayerTreeHostImplTest, PageBasedScroll) {
  const gfx::Size kViewportSize(100, 100);
  const gfx::Size kContentSize(300, 300);
  SetupViewportLayersOuterScrolls(kViewportSize, kContentSize);
  DrawFrame();

  const gfx::Vector2dF kPageDelta(2, 1);

  auto begin_state =
      BeginState(gfx::Point(), kPageDelta, ui::ScrollInputType::kWheel);
  begin_state->data()->delta_granularity = ui::ScrollGranularity::kScrollByPage;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel)
                .thread);

  auto update_state =
      UpdateState(gfx::Point(), kPageDelta, ui::ScrollInputType::kWheel);
  update_state.data()->delta_granularity = ui::ScrollGranularity::kScrollByPage;
  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  GetInputHandler().ScrollUpdate(update_state);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(2000));

  const gfx::PointF kExpectedOffset(
      kPageDelta.x() * kViewportSize.width() * kMinFractionToStepWhenPaging,
      kPageDelta.y() * kViewportSize.height() * kMinFractionToStepWhenPaging);
  const gfx::PointF kCurrentOffset =
      host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree()
          .current_scroll_offset(
              host_impl_->OuterViewportScrollNode()->element_id);

  EXPECT_EQ(kExpectedOffset, kCurrentOffset);

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

TEST_P(LayerTreeHostImplTest, PageBasedScrollSnap) {
  gfx::Size view_size(100, 100);
  gfx::Size overflow_size(100, 300);
  gfx::RectF snap_area_1(0, 0, 100, 20);
  // This snap area should be skipped because it is too close.
  gfx::RectF snap_area_2(0, 20, 100, 40);
  // This snap area should be snapped to because scrolling to the next one
  // would skip over content.
  gfx::RectF snap_area_3(0, 60, 100, 60);
  // Scrolling to this snap area would skip over content.
  gfx::RectF snap_area_4(0, 120, 100, 100);
  // Regression test for https://crbug.com/41483533. This snap area
  // should not be selected by a page down.
  gfx::RectF snap_area_5(0, 220, 100, 80);

  SetupViewportLayersInnerScrolls(view_size, view_size);
  LayerImpl* overflow =
      AddScrollableLayer(OuterViewportScrollLayer(), view_size, overflow_size);

  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 100, 100), gfx::PointF(0, 200));
  ScrollSnapAlign start = ScrollSnapAlign(SnapAlignment::kStart);
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_1, false, false, ElementId(10)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_2, false, false, ElementId(20)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_3, false, false, ElementId(30)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_4, false, false, ElementId(40)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_5, false, false, ElementId(50)));
  GetScrollNode(overflow)->snap_container_data.emplace(container);
  DrawFrame();

  gfx::Point position(95, 75);
  gfx::Vector2dF kPageDelta(0, 1);

  auto begin_state = BeginState(
      position, kPageDelta, ui::ScrollInputType::kScrollbar);
  begin_state->data()->delta_granularity = ui::ScrollGranularity::kScrollByPage;
  EXPECT_EQ(ScrollThread::kScrollOnImplThread,
            GetInputHandler()
                .ScrollBegin(begin_state.get(), ui::ScrollInputType::kScrollbar)
                .thread);

  auto update_state = UpdateState(
      position, kPageDelta, ui::ScrollInputType::kScrollbar);
  update_state.data()->delta_granularity = ui::ScrollGranularity::kScrollByPage;
  GetInputHandler().ScrollUpdate(update_state);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  base::TimeTicks start_time = base::TimeTicks() + base::Milliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(50));
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::Milliseconds(2000));

  EXPECT_POINTF_EQ(gfx::PointF(0, 60), CurrentScrollOffset(overflow));
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

class UnifiedScrollingTest : public LayerTreeHostImplTest {
 public:
  using ScrollStatus = InputHandler::ScrollStatus;

  LayerTreeSettings DefaultSettings() override {
    auto settings = LayerTreeHostImplTest::DefaultSettings();
    settings.enable_hit_test_opaqueness = true;
    return settings;
  }

  void SetUp() override {
    LayerTreeHostImplTest::SetUp();

    cur_time_ = base::TimeTicks() + base::Milliseconds(100);
    begin_frame_args_ =
        viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

    SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  }

  void CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion() {
    // Create an non-composited scroll node that corresponds to a
    // MainThreadScrollHitTestRegion on the outer viewport scroll layer.
    gfx::Size scrollable_content_bounds(100, 100);
    gfx::Size container_bounds(50, 50);
    CreateScrollNodeForNonCompositedScroller(
        GetPropertyTrees(), host_impl_->OuterViewportScrollNode()->id,
        ScrollerElementId(), scrollable_content_bounds, container_bounds);
    OuterViewportScrollLayer()->SetMainThreadScrollHitTestRegion(
        gfx::Rect(50, 50));

    host_impl_->active_tree()->set_needs_update_draw_properties();
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();
    DrawFrame();
  }

  void CreateScroller(
      uint32_t main_thread_repaint_reasons,
      HitTestOpaqueness hit_test_opaqueness = HitTestOpaqueness::kOpaque) {
    // Creates a regular composited scroller that comes with a ScrollNode and
    // Layer.
    gfx::Size scrollable_content_bounds(100, 100);
    gfx::Size container_bounds(50, 50);

    LayerImpl* layer =
        AddScrollableLayer(OuterViewportScrollLayer(), container_bounds,
                           scrollable_content_bounds);
    layer->SetHitTestOpaqueness(hit_test_opaqueness);
    scroller_layer_ = layer;
    GetScrollNode(layer)->main_thread_repaint_reasons =
        main_thread_repaint_reasons;
    GetScrollNode(layer)->is_composited = true;

    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();
    DrawFrame();
  }

  void CreateLayerCoveringWholeViewport(const LayerImpl* parent_scroller,
                                        HitTestOpaqueness opaqueness) {
    LayerImpl* layer = AddLayerInActiveTree();
    layer->SetBounds(gfx::Size(100, 100));
    layer->SetDrawsContent(true);
    layer->SetHitTestOpaqueness(opaqueness);
    CopyProperties(parent_scroller, layer);
    UpdateDrawProperties(host_impl_->active_tree());
  }

  void CreateLayerCoveringWholeViewportEscapingScrollers(
      HitTestOpaqueness opaqueness) {
    // Add a layer with the outer viewport as its scroll parent but it covers
    // the entire viewport and any scrollers underneath it.
    CreateLayerCoveringWholeViewport(OuterViewportScrollLayer(), opaqueness);
  }

  void CreateLayerCoveringWholeViewportInScroller(
      HitTestOpaqueness opaqueness) {
    CreateLayerCoveringWholeViewport(scroller_layer_.get(), opaqueness);
  }

  ScrollStatus ScrollBegin(const gfx::Vector2d& delta) {
    auto scroll_state =
        BeginState(gfx::Point(25, 25), delta, ui::ScrollInputType::kWheel);
    ScrollStatus status = GetInputHandler().ScrollBegin(
        scroll_state.get(), ui::ScrollInputType::kWheel);

    if (status.main_thread_hit_test_reasons) {
      to_be_continued_scroll_begin_ = std::move(scroll_state);
    }

    return status;
  }

  ScrollStatus ContinuedScrollBegin(ElementId element_id) {
    DCHECK(to_be_continued_scroll_begin_)
        << "ContinuedScrollBegin needs to come after a ScrollBegin that "
           "requested a main frame";
    std::unique_ptr<ScrollState> scroll_state =
        std::move(to_be_continued_scroll_begin_);

    scroll_state->data()->set_current_native_scrolling_element(element_id);
    scroll_state->data()->main_thread_hit_tested_reasons =
        MainThreadScrollingReason::kFailedHitTest;

    return GetInputHandler().ScrollBegin(scroll_state.get(),
                                         ui::ScrollInputType::kWheel);
  }

  InputHandlerScrollResult ScrollUpdate(const gfx::Vector2d& delta) {
    auto scroll_state =
        UpdateState(gfx::Point(25, 25), delta, ui::ScrollInputType::kWheel);
    return GetInputHandler().ScrollUpdate(scroll_state);
  }

  InputHandlerScrollResult AnimatedScrollUpdate(const gfx::Vector2d& delta) {
    auto scroll_state = AnimatedUpdateState(gfx::Point(25, 25), delta);
    return GetInputHandler().ScrollUpdate(scroll_state);
  }

  InputHandlerScrollEndResult ScrollEnd() {
    return GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
  }

  // An animation is setup in the first BeginFrame so it won't actually update
  // in the first one. Use a named method for that so it's clear rather than
  // mysteriously calling BeginFrame twice.
  void StartAnimation() { BeginFrame(base::TimeDelta()); }

  void BeginFrame(base::TimeDelta forward) {
    cur_time_ += forward;
    begin_frame_args_.frame_time = cur_time_;
    begin_frame_args_.frame_id.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args_);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame(begin_frame_args_);
  }

  gfx::PointF GetScrollOffset(ScrollNode* node) {
    return GetPropertyTrees()->scroll_tree().current_scroll_offset(
        node->element_id);
  }

  gfx::PointF ScrollerOffset() {
    return GetPropertyTrees()->scroll_tree().current_scroll_offset(
        ScrollerElementId());
  }

  PropertyTrees* GetPropertyTrees() {
    return host_impl_->active_tree()->property_trees();
  }

  ScrollNode* CurrentlyScrollingNode() {
    return host_impl_->CurrentlyScrollingNode();
  }

  ScrollNode* ScrollerNode() {
    ScrollNode* node =
        GetPropertyTrees()->scroll_tree_mutable().MutableFindNodeFromElementId(
            ScrollerElementId());
    DCHECK(node);
    return node;
  }
  LayerImpl* ScrollerLayer() const { return scroller_layer_.get(); }
  ElementId ScrollerElementId() const {
    if (scroller_layer_)
      return scroller_layer_->element_id();

    return ElementId(1234);
  }

  base::TimeDelta kFrameInterval = base::Milliseconds(16);

  // Parameterized test body. Defined inline with tests.
  void TestNonCompositedScrollingState(bool mutates_transform_tree);

 private:
  raw_ptr<LayerImpl> scroller_layer_ = nullptr;

  base::TimeTicks cur_time_;
  viz::BeginFrameArgs begin_frame_args_;

  std::unique_ptr<ScrollState> to_be_continued_scroll_begin_;
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(UnifiedScrollingTest);

// A ScrollBegin that hits a MainThreadScrollHitTestRegion must return a request
// for a main thread hit test.
TEST_P(UnifiedScrollingTest, UnifiedScrollMainThreadScrollHitTestRegion) {
  CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion();

  // Scrolling inside a MainThreadScrollHitTestRegion should request a main
  // thread hit test. It's the client's responsibility to request a hit test
  // from Blink. It can then call ScrollBegin again, providing the element_id
  // to scroll.
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));

    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);

    // The scroll hasn't started yet though.
    EXPECT_FALSE(CurrentlyScrollingNode());
  }

  // Simulate the scroll hit test coming back from the main thread. This time
  // ScrollBegin will be called with an element id provided so that a hit test
  // is unnecessary.
  {
    ScrollStatus status = ContinuedScrollBegin(ScrollerElementId());

    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);

    EXPECT_TRUE(CurrentlyScrollingNode());
    EXPECT_EQ(ScrollerNode(), CurrentlyScrollingNode());
  }

  // Ensure ScrollUpdates can successfully scroll this node. They shouldn't
  // mutate the associated transform node.
  {
    EXPECT_TRUE(ScrollUpdate(gfx::Vector2d(0, 10)).did_scroll);
    EXPECT_EQ(gfx::PointF(0, 10), ScrollerOffset());
  }

  // Try to scroll past the end. Ensure the max scrolling bounds are respected.
  {
    EXPECT_TRUE(ScrollUpdate(gfx::Vector2d(0, 1000)).did_scroll);
    EXPECT_EQ(gfx::PointF(0, 50), ScrollerOffset());
  }

  // Overscrolling should cause the scroll update to be dropped.
  {
    EXPECT_FALSE(ScrollUpdate(gfx::Vector2d(0, 10)).did_scroll);
    EXPECT_EQ(gfx::PointF(0, 50), ScrollerOffset());
  }

  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// A main thread hit test should still go through latch bubbling. That is, if
// the hit tested scroller is fully scrolled and cannot consume the scroll, we
// should chain up to its ancestor.
TEST_P(UnifiedScrollingTest, MainThreadHitTestLatchBubbling) {
  CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion();

  // Start with the scroller fully scrolled.
  {
    ScrollBegin(gfx::Vector2d(0, 1000));
    ContinuedScrollBegin(ScrollerElementId());
    ScrollUpdate(gfx::Vector2d(0, 1000));
    ScrollEnd();
    ASSERT_EQ(gfx::PointF(0, 50), ScrollerOffset());
  }

  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    ASSERT_EQ(MainThreadScrollingReason::kMainThreadScrollHitTestRegion,
              status.main_thread_hit_test_reasons);
    status = ContinuedScrollBegin(ScrollerElementId());

    // Since the hit tested scroller in ContinuedScrollBegin was fully
    // scrolled, we should latch to the viewport instead.
    EXPECT_TRUE(CurrentlyScrollingNode());
    EXPECT_EQ(host_impl_->OuterViewportScrollNode(), CurrentlyScrollingNode());
  }
}

using UnifiedScrollingDeathTest = UnifiedScrollingTest;

INSTANTIATE_COMMIT_TO_TREE_TEST_P(UnifiedScrollingDeathTest);

// A main thread hit test that with an empty target id should be dropped. This
// test makes sure that that's enforced with a CHECK.
TEST_P(UnifiedScrollingDeathTest, EmptyMainThreadHitTest) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion();
  {
    ElementId kInvalidId;
    DCHECK(!kInvalidId);

    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));

    // Note, we have a CHECK in here to make sure this cannot happen.
    EXPECT_CHECK_DEATH({ status = ContinuedScrollBegin(kInvalidId); });
  }
}

// A main thread hit test that returns a scroll node we can't find should be
// dropped.
TEST_P(UnifiedScrollingTest, MainThreadHitTestScrollNodeNotFound) {
  CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion();

  {
    ElementId kMixed(42);
    DCHECK(!GetPropertyTrees()->scroll_tree().FindNodeFromElementId(kMixed));

    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    status = ContinuedScrollBegin(kMixed);
    EXPECT_EQ(ScrollThread::kScrollIgnored, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_repaint_reasons);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
  }
}

TEST_P(UnifiedScrollingTest, NonCompositedScrollOnCompositor) {
  // Create an non-composited scroll node that can start scroll on the
  // compositor, in the outer viewport scroll layer.
  gfx::Size scrollable_content_bounds(100, 100);
  gfx::Size container_bounds(50, 50);
  CreateScrollNodeForNonCompositedScroller(
      GetPropertyTrees(), host_impl_->OuterViewportScrollNode()->id,
      ScrollerElementId(), scrollable_content_bounds, container_bounds);
  OuterViewportScrollLayer()->SetNonCompositedScrollHitTestRects(
      {ScrollHitTestRect{ScrollerElementId(), gfx::Rect(container_bounds)}});

  host_impl_->active_tree()->set_needs_update_draw_properties();
  UpdateDrawProperties(host_impl_->active_tree());
  host_impl_->active_tree()->DidBecomeActive();
  DrawFrame();

  ScrollStatus status = ScrollBegin(gfx::Vector2d(10, 10));
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_hit_test_reasons);
  EXPECT_EQ(ScrollerNode(), CurrentlyScrollingNode());
  GetInputHandler().ScrollEnd(/*should_snap=*/false, std::nullopt);
}

// The presence of a layer with mixed hit test opaqueness causes "fail to hit
// test" if its scroll parent isn't the first hit scrollable layer. This
// requires falling back to the main thread (see InputHandler::
// IsInitialScrollHitTestReliable). However, we can still perform the scroll
// updates on the compositor.
TEST_P(UnifiedScrollingTest,
       LayerMixedHitTestOpaquenessCausesMainThreadHitTest) {
  // Create a scroller that should scroll on the compositor thread and the add
  // a layer with mixed hit test opaqueness over top it. This simulates the
  // case where a squashing layer obscuring a scroller makes the hit test
  // unreliable.
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportEscapingScrollers(HitTestOpaqueness::kMixed);

  // Scrolling over a squashing-like layer that cannot be reliably hit tested
  // on the compositor should request a main thread hit test.
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
  }

  // Resolving the hit test should allow the scroller underneath to scroll as
  // normal on the impl thread.
  {
    ScrollStatus status = ContinuedScrollBegin(ScrollerElementId());
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);

    EXPECT_TRUE(CurrentlyScrollingNode());
    EXPECT_EQ(ScrollerNode(), CurrentlyScrollingNode());
  }
}

// Similar to LayerMixedHitTestOpaquenessCausesMainThreadHitTest, with more
// layers that have mixed hit-test opaqueness but are reliable to hit test.
// These layers should not affect the unreliable hit test on the target layer.
TEST_P(UnifiedScrollingTest,
       LayerMixedHitTestOpaquenessCausesMainThreadHitTest2) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);
  CreateLayerCoveringWholeViewportEscapingScrollers(HitTestOpaqueness::kMixed);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);

  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
  }

  {
    ScrollStatus status = ContinuedScrollBegin(ScrollerElementId());
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);

    EXPECT_TRUE(CurrentlyScrollingNode());
    EXPECT_EQ(ScrollerNode(), CurrentlyScrollingNode());
  }
}

// Similar to LayerMixedHitTestOpaquenessCausesMainThreadHitTest, but the layer
// is opaque to hit test.
TEST_P(UnifiedScrollingTest, LayerOpaqueToHitTestScrollsOnCompositor) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportEscapingScrollers(HitTestOpaqueness::kOpaque);
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    // We can start scroll the scroll parent of the layer, which is the outer
    // viewport scroll layer.
    EXPECT_EQ(host_impl_->OuterViewportScrollNode(), CurrentlyScrollingNode());
  }
}

TEST_P(UnifiedScrollingTest, FixedLayerOpaqueToHitTestScrollsOnCompositor) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewport(InnerViewportScrollLayer(),
                                   HitTestOpaqueness::kOpaque);

  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    // See InputHandler::GetNodeToScroll() for why the scrolling node is the
    // outer viewport scroll node instead of the inner viewport scroll node.
    EXPECT_EQ(host_impl_->OuterViewportScrollNode(), CurrentlyScrollingNode());
  }
}

TEST_P(UnifiedScrollingTest,
       LayerOpaqueToHitTestEscapingScrollersWithMixedToHitTestLayers) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);
  CreateLayerCoveringWholeViewportEscapingScrollers(HitTestOpaqueness::kOpaque);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
  }
}

TEST_P(UnifiedScrollingTest,
       ReliableScrollHitTestWithOpaqueAndMixedToHitTestLayers) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kOpaque);
  CreateLayerCoveringWholeViewportInScroller(HitTestOpaqueness::kMixed);
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
    EXPECT_EQ(ScrollerNode(), CurrentlyScrollingNode());
  }
}

// Under unified scroling, a composited scroller with a main thread scrolling
// reason should be scrolled on the compositor. Ensure ScrollBegin returns
// success without needing a main thread hit test.
TEST_P(UnifiedScrollingTest, MainThreadScrollingReasonsScrollOnCompositor) {
  CreateScroller(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
              status.main_thread_hit_test_reasons);
  }
}

TEST_P(UnifiedScrollingTest, UnreliableHitTestOnNonOpaqueToHitTestScroller) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain,
                 HitTestOpaqueness::kMixed);

  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_hit_test_reasons);
  }
}

TEST_P(UnifiedScrollingTest, ScrollbarLayerClippedByRoundedCorner) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain,
                 HitTestOpaqueness::kMixed);
  auto* scrollbar_layer = AddLayer<PaintedScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical, false, true);
  CreateEffectNode(ScrollerLayer()).node_or_ancestor_has_fast_rounded_corner =
      true;
  SetupScrollbarLayer(ScrollerLayer(), scrollbar_layer);
  scrollbar_layer->SetBounds(gfx::Size(10, 50));
  scrollbar_layer->SetOffsetToTransformParent(gfx::Vector2dF(40, 0));

  // Hit test on the scrollbar layer is not reliable because of the rounded
  // corner.
  auto status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(45, 20), gfx::Vector2d(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  EXPECT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_repaint_reasons);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);
}

// This tests whether or not various kinds of scrolling mutates the transform
// tree or not. It is parameterized and used by tests below.
void UnifiedScrollingTest::TestNonCompositedScrollingState(
    bool mutates_transform_tree) {
  const ScrollTree& scroll_tree = GetPropertyTrees()->scroll_tree();
  TransformTree& transform_tree = GetPropertyTrees()->transform_tree_mutable();
  TransformNode& transform_node =
      transform_tree.MutableNode(ScrollerNode()->transform_id);

  // Ensure we're in a clean state to start.
  {
    ASSERT_EQ(transform_node.element_id, ScrollerElementId());
    ASSERT_TRUE(transform_node.scrolls);

    ASSERT_EQ(gfx::PointF(0, 0), transform_node.scroll_offset());
    ASSERT_FALSE(transform_node.transform_changed());
    ASSERT_FALSE(transform_node.needs_local_transform_update);
    ASSERT_FALSE(transform_tree.needs_update());
  }

  // Start a scroll, ensure the scroll tree was updated and a commit was
  // requested. Check that the transform tree mutation was as expected for the
  // test parameter.
  {
    ScrollStatus status = ScrollBegin(gfx::Vector2d(0, 10));
    if (status.main_thread_hit_test_reasons) {
      ContinuedScrollBegin(ScrollerElementId());
    }

    ASSERT_EQ(ScrollerNode(), CurrentlyScrollingNode());

    did_request_commit_ = false;

    ScrollUpdate(gfx::Vector2d(0, 10));
    ASSERT_EQ(gfx::PointF(0, 10), ScrollerOffset());
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);

    // Ensure the transform tree was updated only if expected.
    EXPECT_EQ(mutates_transform_tree, transform_node.transform_changed());
    EXPECT_EQ(mutates_transform_tree,
              transform_node.needs_local_transform_update);
    EXPECT_EQ(mutates_transform_tree, transform_tree.needs_update());
    if (mutates_transform_tree) {
      EXPECT_EQ(gfx::PointF(0, 10), transform_node.scroll_offset());
      EXPECT_EQ(gfx::PointF(0, 10),
                scroll_tree.GetScrollOffsetForScrollTimeline(*ScrollerNode()));
    } else {
      EXPECT_EQ(gfx::PointF(0, 0), transform_node.scroll_offset());
      EXPECT_EQ(gfx::PointF(0, 0),
                scroll_tree.GetScrollOffsetForScrollTimeline(*ScrollerNode()));
    }
  }

  // Perform animated scroll update. Ensure the same things.
  {
    did_request_commit_ = false;

    AnimatedScrollUpdate(gfx::Vector2d(0, 10));
    ASSERT_TRUE(
        host_impl_->mutator_host()->HasImplOnlyScrollAnimatingElement());
    ASSERT_EQ(gfx::PointF(0, 10), ScrollerOffset());

    StartAnimation();
    BeginFrame(kFrameInterval);
    BeginFrame(base::Milliseconds(500));
    BeginFrame(kFrameInterval);

    ASSERT_EQ(gfx::PointF(0, 20), ScrollerOffset());
    EXPECT_EQ(!host_impl_->settings().trees_in_viz_in_viz_process,
              did_request_commit_);

    EXPECT_EQ(mutates_transform_tree, transform_node.transform_changed());
    EXPECT_EQ(mutates_transform_tree,
              transform_node.needs_local_transform_update);
    EXPECT_EQ(mutates_transform_tree, transform_tree.needs_update());
    if (mutates_transform_tree) {
      EXPECT_EQ(gfx::PointF(0, 20), transform_node.scroll_offset());
      EXPECT_EQ(gfx::PointF(0, 20),
                scroll_tree.GetScrollOffsetForScrollTimeline(*ScrollerNode()));
    } else {
      EXPECT_EQ(gfx::PointF(0, 0), transform_node.scroll_offset());
      EXPECT_EQ(gfx::PointF(0, 0),
                scroll_tree.GetScrollOffsetForScrollTimeline(*ScrollerNode()));
    }
  }
}

// When scrolling a main-thread hit tested scroller with main thread reasons,
// we should update the scroll node but the transform tree shouldn't be
// mutated. Also ensure NeedsCommit is set. A nice additional benefit of scroll
// unification should be seamless upgrade to a full compositor scroll if a main
// thread reason is removed.
TEST_P(UnifiedScrollingTest, MainThreadReasonsScrollDoesntAffectTransform) {
  CreateScroller(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  TestNonCompositedScrollingState(/*mutates_transform_tree=*/false);

  ASSERT_EQ(ScrollerNode()->main_thread_repaint_reasons,
            MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  TransformTree& tree = GetPropertyTrees()->transform_tree_mutable();
  TransformNode& transform_node =
      tree.MutableNode(ScrollerNode()->transform_id);

  // Removing the main thread reason bit should start mutating the transform
  // tree.
  {
    ScrollerNode()->main_thread_repaint_reasons =
        MainThreadScrollingReason::kNotScrollingOnMain;
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();

    ScrollUpdate(gfx::Vector2d(0, 10));
    ASSERT_EQ(gfx::PointF(0, 30), ScrollerOffset());

    // The transform node should now be updated by the scroll.
    EXPECT_EQ(gfx::PointF(0, 30), transform_node.scroll_offset());
    EXPECT_TRUE(transform_node.transform_changed());
    EXPECT_TRUE(transform_node.needs_local_transform_update);
    EXPECT_TRUE(tree.needs_update());
  }

  ScrollEnd();
}

// When scrolling an non-composited scroller, we shouldn't modify the transform
// tree. If a scroller is promoted mid-scroll it should start mutating the
// transform tree.
TEST_P(UnifiedScrollingTest, NonCompositedScrollerDoesntAffectTransform) {
  CreateNonCompositedScrollerAndMainThreadScrollHitTestRegion();

  TestNonCompositedScrollingState(/*mutates_transform_tree=*/false);

  ASSERT_FALSE(ScrollerNode()->is_composited);
  TransformTree& tree = GetPropertyTrees()->transform_tree_mutable();
  TransformNode& transform_node =
      tree.MutableNode(ScrollerNode()->transform_id);

  // Marking the node as composited should start updating the transform tree.
  {
    ScrollerNode()->is_composited = true;
    UpdateDrawProperties(host_impl_->active_tree());
    host_impl_->active_tree()->DidBecomeActive();

    ScrollUpdate(gfx::Vector2d(0, 10));
    ASSERT_EQ(gfx::PointF(0, 30), ScrollerOffset());

    // The transform node should now be updated by the scroll.
    EXPECT_EQ(gfx::PointF(0, 30), transform_node.scroll_offset());
    EXPECT_TRUE(transform_node.transform_changed());
    EXPECT_TRUE(transform_node.needs_local_transform_update);
    EXPECT_TRUE(tree.needs_update());
  }

  ScrollEnd();
}

// When scrolling a composited scroller that just happens to have needed a main
// thread hit test first, we should modify the transform tree as usual.
TEST_P(UnifiedScrollingTest, CompositedWithSquashedLayerMutatesTransform) {
  CreateScroller(MainThreadScrollingReason::kNotScrollingOnMain);
  CreateLayerCoveringWholeViewportEscapingScrollers(HitTestOpaqueness::kMixed);

  TestNonCompositedScrollingState(/*mutates_transform_tree=*/true);

  ScrollEnd();
}

// Verifies that when a surface layer is occluded, its frame sink id will be
// marked as qualified for throttling.
TEST_P(OccludedSurfaceThrottlingLayerTreeHostImplTest,
       ThrottleOccludedSurface) {
  LayerTreeImpl* tree = host_impl_->active_tree();
  gfx::Rect viewport_rect(0, 0, 800, 600);
  auto* root = SetupRootLayer<LayerImpl>(tree, viewport_rect.size());

  auto* occluded = AddLayer<SurfaceLayerImpl>(tree);
  occluded->SetBounds(gfx::Size(400, 300));
  occluded->SetDrawsContent(true);
  viz::SurfaceId start = MakeSurfaceId(viz::FrameSinkId(1, 2), 1);
  viz::SurfaceId end = MakeSurfaceId(viz::FrameSinkId(3, 4), 1);
  occluded->SetRange(viz::SurfaceRange(start, end), 2u);
  CopyProperties(root, occluded);

  auto* occluder = AddLayer<SolidColorLayerImpl>(tree);
  occluder->SetBounds(gfx::Size(400, 400));
  occluder->SetDrawsContent(true);
  occluder->SetContentsOpaque(true);
  CopyProperties(root, occluder);

  DrawFrame();
  EXPECT_EQ(host_impl_->GetFrameSinksToThrottleForTesting(),
            base::flat_set<viz::FrameSinkId>{end.frame_sink_id()});
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestSimple) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetDrawsContent(true);
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer).visible_frame_element_id = ElementId(0x10);

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(10, 10)),
            ElementId(0x10));
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestInheritance) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer, root_layer()->transform_tree_index())
      .visible_frame_element_id = ElementId(0x20);

  // Create a child layer with no associated frame, but with the above frame
  // layer as a parent.
  LayerImpl* child_layer = AddLayerInActiveTree();
  child_layer->SetBounds(gfx::Size(50, 50));
  child_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), child_layer);
  auto& child_node =
      CreateTransformNode(child_layer, frame_layer->transform_tree_index());
  child_node.parent_frame_id = frame_layer->transform_tree_index();
  child_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  UpdateDrawProperties(host_impl_->active_tree());

  // Hit tests on the parent should return the parent's frame element ID.
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(15, 15)),
            ElementId(0x20));

  // Ensure that hit tests on the child (non-frame) layer returns the frame
  // element id of its parent.
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(60, 60)),
            ElementId(0x20));
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestOverlap) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer).visible_frame_element_id = ElementId(0x10);

  LayerImpl* occluding_frame_layer = AddLayerInActiveTree();
  occluding_frame_layer->SetBounds(gfx::Size(50, 50));
  occluding_frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), occluding_frame_layer);
  auto& occluding_frame_node = CreateTransformNode(
      occluding_frame_layer, frame_layer->transform_tree_index());
  occluding_frame_node.visible_frame_element_id = ElementId(0x20);
  occluding_frame_node.parent_frame_id = frame_layer->transform_tree_index();
  occluding_frame_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  UpdateDrawProperties(host_impl_->active_tree());

  // Both frame layers should return their own frame element IDs, despite
  // overlapping.
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(15, 15)),
            ElementId(0x10));
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(30, 30)),
            ElementId(0x20));
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestOverlapSimpleClip) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer).visible_frame_element_id = ElementId(0x10);

  LayerImpl* clipped_frame_layer = AddLayerInActiveTree();
  clipped_frame_layer->SetBounds(gfx::Size(50, 50));
  clipped_frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), clipped_frame_layer);
  CreateTransformNode(clipped_frame_layer).visible_frame_element_id =
      ElementId(0x20);
  clipped_frame_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // Create a clip excluding the overlapped region.
  auto& clip_node = CreateClipNode(clipped_frame_layer);
  clip_node.clip = gfx::RectF(40, 40, 10, 10);

  UpdateDrawProperties(host_impl_->active_tree());

  // Ensure that the overlapping (clipped) layer isn't targeted.
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(30, 30)),
            ElementId(0x10));
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestOverlapRoundedCorners) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer).visible_frame_element_id = ElementId(0x10);

  LayerImpl* rounded_frame_layer = AddLayerInActiveTree();
  rounded_frame_layer->SetBounds(gfx::Size(50, 50));
  rounded_frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), rounded_frame_layer);
  CreateTransformNode(rounded_frame_layer, frame_layer->transform_tree_index())
      .visible_frame_element_id = ElementId(0x20);
  rounded_frame_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  // Add rounded corners to the layer, which are unable to be hit tested by the
  // simple quad-based logic.
  CreateEffectNode(rounded_frame_layer).mask_filter_info =
      gfx::MaskFilterInfo(gfx::RRectF(25, 25, 50, 50, 5));

  UpdateDrawProperties(host_impl_->active_tree());

  // The lookup should bail out in the presence of a complex clip/mask on the
  // target chain.
  EXPECT_FALSE(
      GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(30, 30)));
}

TEST_P(LayerTreeHostImplTest, FrameElementIdHitTestOverlapSibling) {
  SetupDefaultRootLayer();

  LayerImpl* frame_layer = AddLayerInActiveTree();
  frame_layer->SetBounds(gfx::Size(50, 50));
  frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), frame_layer);
  CreateTransformNode(frame_layer, root_layer()->transform_tree_index())
      .visible_frame_element_id = ElementId(0x20);

  LayerImpl* sibling_frame_layer = AddLayerInActiveTree();
  sibling_frame_layer->SetBounds(gfx::Size(50, 50));
  sibling_frame_layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(root_layer(), sibling_frame_layer);
  CreateTransformNode(sibling_frame_layer, root_layer()->transform_tree_index())
      .visible_frame_element_id = ElementId(0x30);
  sibling_frame_layer->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));

  UpdateDrawProperties(host_impl_->active_tree());

  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(15, 15)),
            ElementId(0x20));
  EXPECT_EQ(GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(60, 60)),
            ElementId(0x30));

  // If we have a layer occluded by a layer from another document, attributions
  // should be discarded outside of the simple frame -> subframe case.
  EXPECT_FALSE(
      GetInputHandler().FindFrameElementIdAtPoint(gfx::PointF(30, 30)));
}

TEST_P(LayerTreeHostImplTest, ViewTransitionRequestCausesDamage) {
  const gfx::Size viewport_size(100, 100);
  SetupDefaultRootLayer(viewport_size);
  UpdateDrawProperties(host_impl_->active_tree());

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();
  did_request_redraw_ = false;

  // Ensure there is no damage.
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(last_on_draw_frame_->has_no_damage);
  last_on_draw_frame_.reset();
  did_request_redraw_ = false;

  // Adding a transition effect should cause us to redraw.
  host_impl_->active_tree()->AddViewTransitionRequest(
      ViewTransitionRequest::CreateAnimateRenderer(
          blink::ViewTransitionToken(), /*maybe_cross_frame_sink=*/false,
          /*delay_layer_tree_view_deletion=*/false));

  // Ensure there is damage and we requested a redraw.
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

TEST_P(LayerTreeHostImplTest, CollectRegionCaptureBounds) {
  const auto kFirstId = viz::RegionCaptureCropId::CreateRandom();
  const auto kSecondId = viz::RegionCaptureCropId::CreateRandom();
  const auto kThirdId = viz::RegionCaptureCropId::CreateRandom();
  const auto kFourthId = viz::RegionCaptureCropId::CreateRandom();

  const viz::RegionCaptureBounds kRootBounds{
      {{kFirstId, gfx::Rect{0, 0, 250, 250}}, {kSecondId, gfx::Rect{}}}};
  const viz::RegionCaptureBounds kChildBounds{
      {{kThirdId, gfx::Rect{5, 6, 300, 400}}}};
  const viz::RegionCaptureBounds kSecondChildBounds{
      {{kFourthId, gfx::Rect{20, 10, 400, 500}}}};

  // Set up the root layer.
  LayerImpl* root_layer = SetupDefaultRootLayer(gfx::Size(350, 360));
  root_layer->SetCaptureBounds(kRootBounds);

  // Set up a child layer, with a scaling transform.
  LayerImpl* child_layer = AddLayerInActiveTree();
  CopyProperties(root_layer, child_layer);
  child_layer->SetCaptureBounds(kChildBounds);
  gfx::Transform child_layer_transform;
  child_layer_transform.Scale(2.0, 3.0);
  child_layer->draw_properties().screen_space_transform =
      std::move(child_layer_transform);

  // Set up another child layer, with a rotation transform.
  LayerImpl* second_child_layer = AddLayerInActiveTree();
  CopyProperties(root_layer, second_child_layer);
  second_child_layer->SetCaptureBounds(kSecondChildBounds);
  gfx::Transform second_layer_transform;
  second_layer_transform.Rotate(45);
  second_child_layer->draw_properties().screen_space_transform =
      std::move(second_layer_transform);

  // Set up the drawable content rect and transforms for the root surface.
  // Drawing the frame sets up the RenderSurfaceImpl in the backend.
  UpdateDrawProperties(host_impl_->active_tree());
  DrawFrame();

  // NOTE: setting contribution has to be done after updating and drawing, and
  // causes the layers to use draw_properties().screen_screen_transform instead
  // of using values from the transform tree.
  child_layer->set_contributes_to_drawn_render_surface(true);
  second_child_layer->set_contributes_to_drawn_render_surface(true);

  // Actually collect the region capture bounds.
  const viz::RegionCaptureBounds collected_bounds =
      host_impl_->CollectRegionCaptureBounds();
  EXPECT_EQ(4u, collected_bounds.bounds().size());

  // Validate expectations.
  EXPECT_EQ((gfx::Rect{0, 0, 250, 250}),
            collected_bounds.bounds().find(kFirstId)->second);
  EXPECT_EQ((gfx::Rect{}), collected_bounds.bounds().find(kSecondId)->second);

  // The third case is more interesting: the bounds are scaled 2x and 3x,
  // which changes the origin but also causes both bounds to clip at the
  // content rect.
  EXPECT_EQ((gfx::Rect{10, 18, 340, 342}),
            collected_bounds.bounds().find(kThirdId)->second);

  // Finally, test a rotation instead of a simple scaling.
  EXPECT_EQ((gfx::Rect{0, 21, 290, 339}),
            collected_bounds.bounds().find(kFourthId)->second);
}

TEST_P(LayerTreeHostImplTest, RecomputeRasterCapsOnLayerTreeFrameSinkUpdate) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  host_impl_ = CreateLayerTreeHostImplForTesting(
      DefaultSettings(), this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
      nullptr, nullptr);
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl_));
  host_impl_->SetVisible(true);

  // InitializeFrameSink with a gpu-raster enabled output surface.
  auto gpu_raster_layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  host_impl_->InitializeFrameSink(gpu_raster_layer_tree_frame_sink.get());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->can_use_msaa());

  // Re-initialize with a software output surface.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->can_use_msaa());
}

TEST_P(LayerTreeHostImplTest, AnimatedScrollSnapStrategyCurrentOffset) {
  LayerImpl* snapping_layer = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(0, 10);

  GetInputHandler().ScrollBegin(
      BeginState(pointer_position, delta, ui::ScrollInputType::kWheel).get(),
      ui::ScrollInputType::kWheel);

  GetInputHandler().ScrollUpdate(AnimatedUpdateState(pointer_position, delta));

  EXPECT_NE(GetInputHandler().snap_strategy_for_testing(), nullptr);
  EXPECT_VECTOR2DF_EQ(GetInputHandler()
                          .snap_strategy_for_testing()
                          ->current_position()
                          .OffsetFromOrigin(),
                      delta);
  // Animated scroll offsets are not immediately reflected in the scroll tree.
  EXPECT_POINTF_EQ(gfx::PointF(0, 0), CurrentScrollOffset(snapping_layer));
}

TEST_P(LayerTreeHostImplTest, NonAnimatedScrollSnapStrategyCurrentOffset) {
  LayerImpl* snapping_layer = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(0, 10);

  GetInputHandler().ScrollBegin(
      BeginState(pointer_position, delta, ui::ScrollInputType::kWheel).get(),
      ui::ScrollInputType::kWheel);

  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, delta, ui::ScrollInputType::kWheel));

  EXPECT_NE(GetInputHandler().snap_strategy_for_testing(), nullptr);
  EXPECT_VECTOR2DF_EQ(GetInputHandler()
                          .snap_strategy_for_testing()
                          ->current_position()
                          .OffsetFromOrigin(),
                      delta);
  // Non-Animated scroll offsets are immediately reflected in the scroll tree.
  EXPECT_POINTF_EQ(gfx::PointF(0, 10), CurrentScrollOffset(snapping_layer));
}

TEST_P(LayerTreeHostImplTest, AnimatedViewportScrollSnapStrategyCurrentOffset) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();
  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d(50, 50));
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d(5, 400));

  const gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 60);
  const gfx::Point pointer_position = gfx::Point(10, 10);
  // Start an animated scroll on the inner viewport.
  auto begin_state = BeginState(pointer_position, gfx::Vector2d(0, 60),
                                ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      AnimatedUpdateState(pointer_position, scroll_delta));

  EXPECT_VECTOR2DF_EQ(
      GetInputHandler()
          .snap_strategy_for_testing()
          ->current_position()
          .OffsetFromOrigin(),
      CurrentScrollOffset(InnerViewportScrollLayer()).OffsetFromOrigin() +
          CurrentScrollOffset(OuterViewportScrollLayer()).OffsetFromOrigin() +
          scroll_delta);
  EXPECT_POINTF_EQ(gfx::PointF(50, 50),
                   CurrentScrollOffset(InnerViewportScrollLayer()));
  EXPECT_POINTF_EQ(gfx::PointF(5, 400),
                   CurrentScrollOffset(OuterViewportScrollLayer()));
}

TEST_P(LayerTreeHostImplTest,
       NonAnimatedViewportScrollSnapStrategyCurrentOffset) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(200, 200);
  SetupViewportLayersOuterScrolls(viewport_size, content_size);

  DrawFrame();

  float min_page_scale = 1, max_page_scale = 4;
  float page_scale_factor = 2;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  SetScrollOffsetDelta(InnerViewportScrollLayer(), gfx::Vector2d(50, 50));
  SetScrollOffsetDelta(OuterViewportScrollLayer(), gfx::Vector2d(5, 400));

  const gfx::Vector2dF scroll_delta = gfx::Vector2dF(0, 60);
  const gfx::Point pointer_position = gfx::Point(10, 10);

  // Start an animated scroll on the inner viewport.
  auto begin_state =
      BeginState(pointer_position, scroll_delta, ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollBegin(begin_state.get(), ui::ScrollInputType::kWheel);
  GetInputHandler().ScrollUpdate(
      UpdateState(pointer_position, scroll_delta, ui::ScrollInputType::kWheel));

  // NonAnimated scrolls are immediately reflected in the scroll tree.
  EXPECT_VECTOR2DF_EQ(
      GetInputHandler()
          .snap_strategy_for_testing()
          ->current_position()
          .OffsetFromOrigin(),
      CurrentScrollOffset(InnerViewportScrollLayer()).OffsetFromOrigin() +
          CurrentScrollOffset(OuterViewportScrollLayer()).OffsetFromOrigin());
  // Inner viewport's delta increases by 30 (60 / |page_scale_factor|).
  EXPECT_POINTF_EQ(gfx::PointF(50, 80),
                   CurrentScrollOffset(InnerViewportScrollLayer()));
  EXPECT_POINTF_EQ(gfx::PointF(5, 400),
                   CurrentScrollOffset(OuterViewportScrollLayer()));
}

TEST_P(LayerTreeHostImplTest, FlingSnapStrategyCurrentOffset) {
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(100, 5000);

  gfx::RectF snap_area_1(0, 0, 50, 1000);
  gfx::RectF snap_area_2(0, 1200, 50, 1000);

  SetupViewportLayersInnerScrolls(viewport_size, viewport_size);
  LayerImpl* snapping_layer = AddScrollableLayer(OuterViewportScrollLayer(),
                                                 viewport_size, content_size);

  SnapContainerData container(
      ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 100, 100), gfx::PointF(0, 4900));
  ScrollSnapAlign start = ScrollSnapAlign(SnapAlignment::kStart);
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_1, false, false, ElementId(10)));
  container.AddSnapAreaData(
      SnapAreaData(start, snap_area_2, false, false, ElementId(20)));
  GetScrollNode(snapping_layer)->snap_container_data.emplace(container);
  DrawFrame();

  auto& handler = GetInputHandler();
  gfx::PointF initial_offset, target_offset;
  gfx::Point position(50, 50);
  ui::ScrollInputType type = ui::ScrollInputType::kTouchscreen;

  // Scroll to the bottom of snap_area_1.
  handler.ScrollBegin(BeginState(position, gfx::Vector2dF(0, 950), type).get(),
                      type);
  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 950), type));

  // Fling phase.
  gfx::Vector2dF fling_displacement = gfx::Vector2dF(0, 400);
  handler.GetSnapFlingInfoAndSetAnimatingSnapTarget(
      gfx::Vector2dF(0, 100), fling_displacement, &initial_offset,
      &target_offset);
  EXPECT_EQ(handler.snap_strategy_for_testing()->current_position(),
            initial_offset);
  EXPECT_EQ(handler.snap_strategy_for_testing()->intended_position(),
            initial_offset);

  // Do an inertial phase scroll update.
  auto scroll_update_state =
      UpdateState(position, gfx::Vector2dF(0, 100), type);
  scroll_update_state.set_is_in_inertial_phase(true);
  handler.ScrollUpdate(scroll_update_state);

  // Be aware that the snap strategy should be based on the current position.
  EXPECT_EQ(handler.snap_strategy_for_testing()->current_position(),
            initial_offset + gfx::Vector2dF(0, 100));
  EXPECT_EQ(handler.snap_strategy_for_testing()->intended_position(),
            initial_offset + gfx::Vector2dF(0, 100));

  // Test that a new snap strategy is created at the end of an inertial scroll.
  const auto* old_snap_strategy = handler.snap_strategy_for_testing().get();
  handler.ScrollEndForSnapFling(true);
  EXPECT_TRUE(handler.snap_strategy_for_testing().get());
  EXPECT_NE(handler.snap_strategy_for_testing().get(), old_snap_strategy);
}

namespace {

class FakeLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<FakeLayerImpl> Create(LayerTreeImpl* tree_impl,
                                               int id) {
    return base::WrapUnique(new FakeLayerImpl(tree_impl, id));
  }

  ~FakeLayerImpl() override = default;
  void SetInInvisibleLayerTree() override {
    has_been_in_invisible_layer_tree_ = true;
  }
  bool has_been_in_invisible_layer_tree() const {
    return has_been_in_invisible_layer_tree_;
  }

 protected:
  FakeLayerImpl(LayerTreeImpl* tree_impl, int id) : LayerImpl(tree_impl, id) {}

  bool has_been_in_invisible_layer_tree_ = false;
};

}  // namespace

TEST_P(LayerTreeHostImplTest, VisbilityUpdateToLayers) {
  LayerTreeImpl* active_tree = host_impl_->active_tree();

  auto* layer = AddLayer<FakeLayerImpl>(active_tree);
  EXPECT_FALSE(layer->has_been_in_invisible_layer_tree());

  host_impl_->SetVisible(false);
  EXPECT_TRUE(layer->has_been_in_invisible_layer_tree());
}

struct PreservationTestCase {
  enum class Preserve {
    kAllMetrics,
    kScrollUpdatesAndEndsOnly,
  };
  std::string test_name;
  FrameSkippedReason reason;
  Preserve should_preserve;
  bool should_metrics_cause_frame_update;
};

class LayerTreeHostImplEventMetricPreservationTest
    : public LayerTreeHostImplTestBase,
      public testing::WithParamInterface<PreservationTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    LayerTreeHostImplEventMetricPreservationTest,
    LayerTreeHostImplEventMetricPreservationTest,
    testing::ValuesIn<PreservationTestCase>({
        // `LayerTreeHostImpl::DidNotProduceFrame()` should preserve all metrics
        // UNLESS the frame wasn't produced because there was no damage, in
        // which case it should preserve only GSUs/GSEs.
        {.test_name = "WaitingOnMain",
         .reason = FrameSkippedReason::kWaitingOnMain,
         .should_preserve = PreservationTestCase::Preserve::kAllMetrics,
         .should_metrics_cause_frame_update = true},
        {.test_name = "NoDamage",
         .reason = FrameSkippedReason::kNoDamage,
         .should_preserve =
             PreservationTestCase::Preserve::kScrollUpdatesAndEndsOnly,
         .should_metrics_cause_frame_update = false},
    }),
    [](const testing::TestParamInfo<
        LayerTreeHostImplEventMetricPreservationTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_P(LayerTreeHostImplEventMetricPreservationTest, PreserveMetrics) {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  std::vector<EventMetrics*> expected_preserved_metrics_ptrs;

  // Frame 1 which emits multiple metrics but doesn't end up being produced.
  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(16));
    host_impl_->WillBeginImplFrame(args);

    base::SimpleTestTickClock tick_clock;
    tick_clock.Advance(base::Milliseconds(18));
    auto metrics_array = std::to_array<std::unique_ptr<EventMetrics>>(
        {EventMetrics::CreateForTesting(
             ui::EventType::kTouchMoved,
             /* timestamp= */ base::TimeTicks() + base::Milliseconds(11),
             /* arrived_in_browser_main_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(12),
             &tick_clock,
             /* trace_id= */ std::nullopt),
         ScrollUpdateEventMetrics::CreateForTesting(
             ui::EventType::kGestureScrollUpdate,
             ui::ScrollInputType::kTouchscreen, /* is_inertial= */ false,
             ScrollUpdateEventMetrics::ScrollUpdateType::kContinued,
             /* delta= */ 4.2f,
             /* timestamp= */ base::TimeTicks() + base::Milliseconds(13),
             /* arrived_in_browser_main_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(14),
             &tick_clock,
             /* trace_id= */ std::nullopt,
             /* scroll_begin_arrival_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(10)),
         EventMetrics::CreateForTesting(
             ui::EventType::kTouchReleased,
             /* timestamp= */ base::TimeTicks() + base::Milliseconds(15),
             /* arrived_in_browser_main_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(16),
             &tick_clock,
             /* trace_id= */ std::nullopt),
         ScrollEventMetrics::CreateForTesting(
             ui::EventType::kGestureScrollEnd,
             ui::ScrollInputType::kTouchscreen,
             /* is_inertial= */ false,
             /* timestamp= */ base::TimeTicks() + base::Milliseconds(17),
             /* arrived_in_browser_main_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(18),
             &tick_clock,
             /* scroll_begin_arrival_timestamp= */ base::TimeTicks() +
                 base::Milliseconds(10))});
    switch (GetParam().should_preserve) {
      case PreservationTestCase::Preserve::kAllMetrics:
        std::transform(metrics_array.cbegin(), metrics_array.cend(),
                       std::back_inserter(expected_preserved_metrics_ptrs),
                       [](const auto& metrics) { return metrics.get(); });
        break;
      case PreservationTestCase::Preserve::kScrollUpdatesAndEndsOnly:
        expected_preserved_metrics_ptrs.push_back(metrics_array[1].get());
        expected_preserved_metrics_ptrs.push_back(metrics_array[3].get());
    }
    for (auto& metrics : metrics_array) {
      EXPECT_NE(metrics, nullptr);
      auto scoped_monitor =
          host_impl_->GetScopedEventMetricsMonitor(base::BindOnce(
              [](std::unique_ptr<EventMetrics> metrics, bool handled) {
                bool keep_metrics =
                    handled ||
                    EventMetrics::ShouldKeepEvenWithoutCausingFrameUpdate(
                        metrics->type());
                std::unique_ptr<EventMetrics> result =
                    keep_metrics ? std::move(metrics) : nullptr;
                return result;
              },
              std::move(metrics)));
      scoped_monitor->SetSaveMetrics();
    }

    host_impl_->DidFinishImplFrame(args);
    host_impl_->DidNotProduceFrame(viz::BeginFrameAck(), GetParam().reason);
  }

  // Frame 2 should submit metrics from frame 1.
  {
    TestFrameData frame;
    auto args = viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
        base::TimeTicks() + base::Milliseconds(32));
    host_impl_->WillBeginImplFrame(args);
    host_impl_->PrepareToDraw(&frame);
    std::optional<SubmitInfo> submit_info = host_impl_->DrawLayers(&frame);
    EXPECT_THAT(
        submit_info->events_metrics.impl_event_metrics,
        AllOf(Pointwise(UniquePtrMatches(), expected_preserved_metrics_ptrs),
              Each(Pointee(
                  Property(&EventMetrics::caused_frame_update,
                           GetParam().should_metrics_cause_frame_update)))));
  }
}

class ConcurrentImplOnlyScrollAnimationsTest : public LayerTreeHostImplTest {
 public:
  gfx::PointF CreateAndTickScrollAnimations();
  void CompleteScrollAnimations();

 protected:
  gfx::PointF current_offset1_ = gfx::PointF(0., 1.);
  gfx::PointF current_offset2_ = gfx::PointF(0., 3.);
  gfx::PointF target_offset1_ = gfx::PointF(0., 2.);
  gfx::PointF target_offset2_ = gfx::PointF(0., 4.);
  raw_ptr<LayerImpl> scroller1_;
  raw_ptr<LayerImpl> scroller2_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(ConcurrentImplOnlyScrollAnimationsTest);

gfx::PointF
ConcurrentImplOnlyScrollAnimationsTest::CreateAndTickScrollAnimations() {
  gfx::Size content_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(3600, 3800);

  SetupViewportLayersNoScrolls(content_size);
  scroller1_ = AddScrollableLayer(OuterViewportScrollLayer(), content_size,
                                  scroll_content_size);
  scroller2_ = AddScrollableLayer(OuterViewportScrollLayer(), content_size,
                                  scroll_content_size);

  AnimationHost* animation_host = GetImplAnimationHost();
  animation_host->ImplOnlyScrollAnimationCreate(
      scroller1_->element_id(), target_offset1_, current_offset1_,
      base::TimeDelta(), base::TimeDelta());
  animation_host->ImplOnlyScrollAnimationCreate(
      scroller2_->element_id(), target_offset2_, current_offset2_,
      base::TimeDelta(), base::TimeDelta());

  // Bring the animations to their initial values (current_offset).
  animation_host->TickAnimations(base::TimeTicks() + base::Milliseconds(200),
                                 host_impl_->GetScrollTree(), true, nullptr);
  EXPECT_EQ(host_impl_->GetScrollTree().current_scroll_offset(
                scroller1_->element_id()),
            current_offset1_);
  EXPECT_EQ(host_impl_->GetScrollTree().current_scroll_offset(
                scroller2_->element_id()),
            current_offset2_);

  // Promote the animations from STARTING to RUNNING.
  animation_host->UpdateAnimationState(true, nullptr);

  // Bring the animations to intermediate values.
  animation_host->TickAnimations(base::TimeTicks() + base::Milliseconds(300),
                                 host_impl_->GetScrollTree(), true, nullptr);
  EXPECT_GT(host_impl_->GetScrollTree()
                .current_scroll_offset(scroller1_->element_id())
                .y(),
            current_offset1_.y());
  // Store the scroller2's intermediate value which might be its final value.
  gfx::PointF intermediate_scroller2_offset =
      host_impl_->GetScrollTree().current_scroll_offset(
          scroller2_->element_id());
  EXPECT_GT(intermediate_scroller2_offset.y(), current_offset2_.y());
  return intermediate_scroller2_offset;
}

void ConcurrentImplOnlyScrollAnimationsTest::CompleteScrollAnimations() {
  AnimationHost* animation_host = GetImplAnimationHost();
  // Bring the animations to their final values.
  animation_host->TickAnimations(base::TimeTicks() + base::Milliseconds(600),
                                 host_impl_->GetScrollTree(), true, nullptr);
  // Bring the animations to a FINISHED state.
  animation_host->UpdateAnimationState(true, nullptr);

  EXPECT_FALSE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller1_->element_id()));
  EXPECT_FALSE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller2_->element_id()));

  EXPECT_EQ(host_impl_->GetScrollTree().current_scroll_offset(
                scroller1_->element_id()),
            target_offset1_);

  EXPECT_EQ(host_impl_->GetScrollTree().current_scroll_offset(
                scroller2_->element_id()),
            target_offset2_);
}

TEST_P(ConcurrentImplOnlyScrollAnimationsTest, Create) {
  CreateAndTickScrollAnimations();
  CompleteScrollAnimations();
}

TEST_P(ConcurrentImplOnlyScrollAnimationsTest, Update) {
  CreateAndTickScrollAnimations();
  AnimationHost* animation_host = GetImplAnimationHost();
  gfx::Vector2dF update_delta1(1., 1.);
  gfx::Vector2dF update_delta2(2., 2.);
  // Update the target offsets the test should verify against.
  target_offset1_ += update_delta1;
  target_offset2_ += update_delta2;
  // Update the animations' targets.
  animation_host->ImplOnlyScrollAnimationUpdateTarget(
      update_delta1, gfx::PointF(1000., 1000.),
      base::TimeTicks() + base::Milliseconds(400), base::TimeDelta(),
      scroller1_->element_id());
  animation_host->ImplOnlyScrollAnimationUpdateTarget(
      update_delta2, gfx::PointF(1000., 1000.),
      base::TimeTicks() + base::Milliseconds(400), base::TimeDelta(),
      scroller2_->element_id());
  EXPECT_TRUE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller1_->element_id()));
  EXPECT_TRUE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller2_->element_id()));
  CompleteScrollAnimations();
}

TEST_P(ConcurrentImplOnlyScrollAnimationsTest, Abort) {
  target_offset2_ = CreateAndTickScrollAnimations();
  AnimationHost* animation_host = GetImplAnimationHost();
  // Abort the animation on scroller2 targets.
  animation_host->ScrollAnimationAbort(scroller2_->element_id());
  EXPECT_TRUE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller1_->element_id()));
  EXPECT_FALSE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller2_->element_id()));
  CompleteScrollAnimations();
}

TEST_P(ConcurrentImplOnlyScrollAnimationsTest, RemovedByCommit) {
  target_offset2_ = CreateAndTickScrollAnimations();
  AnimationHost* animation_host = GetImplAnimationHost();
  // Remove scroller2 from the property trees.
  host_impl_->active_tree()
      ->property_trees()
      ->scroll_tree_mutable()
      .RemoveNodes(1);
  host_impl_->active_tree()
      ->property_trees()
      ->transform_tree_mutable()
      .RemoveNodes(1);
  animation_host->HandleRemovedScrollAnimatingElements(true);
  EXPECT_TRUE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller1_->element_id()));
  EXPECT_FALSE(animation_host->ElementHasImplOnlyScrollAnimation(
      scroller2_->element_id()));
  CompleteScrollAnimations();
}

class ConcurrentSnapAnimationsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeHostImplTest::SetUp();
    gfx::Size viewport_size(100, 100);
    gfx::Size content_size(100, 5000);

    gfx::RectF snap_area_1(0, 0, 50, 50);
    gfx::RectF snap_area_2(0, 1200, 50, 50);

    SetupViewportLayersInnerScrolls(viewport_size, viewport_size);

    // Create 2 scroll-snap containers.
    snapping_layer1_ = AddScrollableLayer(OuterViewportScrollLayer(),
                                          viewport_size, content_size);

    snapping_layer2_ = AddScrollableLayer(OuterViewportScrollLayer(),
                                          viewport_size, content_size);
    SnapContainerData container1(
        ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 100, 100), gfx::PointF(0, 4900));
    SnapContainerData container2(
        ScrollSnapType(false, SnapAxis::kY, SnapStrictness::kMandatory),
        gfx::RectF(100, 100, 100, 100), gfx::PointF(0, 4900));

    // Add snap areas to both snap containers.
    ScrollSnapAlign start_alignment = ScrollSnapAlign(SnapAlignment::kStart);
    container1.AddSnapAreaData(SnapAreaData(start_alignment, snap_area_1, false,
                                            false, ElementId(10)));
    container1.AddSnapAreaData(SnapAreaData(start_alignment, snap_area_2, false,
                                            false, ElementId(20)));
    container2.AddSnapAreaData(SnapAreaData(start_alignment, snap_area_1, false,
                                            false, ElementId(30)));
    container2.AddSnapAreaData(SnapAreaData(start_alignment, snap_area_2, false,
                                            false, ElementId(40)));

    scroll_node1_ = GetScrollNode(snapping_layer1_.get());
    scroll_node2_ = GetScrollNode(snapping_layer2_.get());
    container1_id_ = scroll_node1_->element_id;
    container2_id_ = scroll_node2_->element_id;
    scroll_node1_->snap_container_data.emplace(container1);
    scroll_node2_->snap_container_data.emplace(container2);

    DrawFrame();
  }

  raw_ptr<LayerImpl> snapping_layer1_ = nullptr;
  raw_ptr<LayerImpl> snapping_layer2_ = nullptr;
  raw_ptr<ScrollNode> scroll_node1_ = nullptr;
  raw_ptr<ScrollNode> scroll_node2_ = nullptr;
  ElementId container1_id_;
  ElementId container2_id_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(ConcurrentSnapAnimationsTest);

TEST_P(ConcurrentSnapAnimationsTest, TrackAnimatingSnapTargetIds) {
  auto& handler = GetInputHandler();

  gfx::Point position(50, 50);
  ui::ScrollInputType type = ui::ScrollInputType::kTouchscreen;
  base::flat_map<ElementId, InputHandler::SnapAnimationData>& snap_state_map =
      handler.get_snap_animation_data_map_for_testing();
  EXPECT_FALSE(snap_state_map.contains(container1_id_));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));

  handler.ScrollBegin(BeginState(position, gfx::Vector2dF(0, 150), type).get(),
                      type);
  // Manually latch onto scroll_node1.
  host_impl_->GetScrollTree().set_currently_scrolling_node(scroll_node1_->id);
  EXPECT_FALSE(snap_state_map.contains(container1_id_));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));

  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 150), type));
  EXPECT_FALSE(snap_state_map.contains(container1_id_));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));

  handler.ScrollEnd(/*should_snap=*/true, std::nullopt);
  EXPECT_TRUE(snap_state_map.contains(container1_id_));
  EXPECT_EQ(snap_state_map.at(container1_id_).animating_snap_target_ids_.y,
            ElementId(10));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));

  handler.ScrollBegin(BeginState(position, gfx::Vector2dF(0, 150), type).get(),
                      type);
  // Manually latch on to |scroll_node2|.
  host_impl_->GetScrollTree().set_currently_scrolling_node(scroll_node2_->id);

  handler.ScrollUpdate(UpdateState(position, gfx::Vector2dF(0, 150), type));
  EXPECT_TRUE(snap_state_map.contains(container1_id_));
  EXPECT_EQ(snap_state_map.at(container1_id_).animating_snap_target_ids_.y,
            ElementId(10));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));

  handler.ScrollEnd(/*should_snap=*/true, std::nullopt);
  EXPECT_TRUE(snap_state_map.contains(container1_id_));
  EXPECT_EQ(snap_state_map.at(container1_id_).animating_snap_target_ids_.y,
            ElementId(10));

  EXPECT_TRUE(snap_state_map.contains(container2_id_));
  EXPECT_EQ(snap_state_map.at(container2_id_).animating_snap_target_ids_.y,
            ElementId(30));

  // Tick snap animations to completion to avoid violating deferred_scroll_end_
  // DCHECK in InputHandler::ScrollEnd.
  AnimationHost* animation_host = GetImplAnimationHost();
  int t = 1;
  while (animation_host->HasImplOnlyScrollAnimatingElement()) {
    animation_host->TickAnimations(
        base::TimeTicks() + base::Milliseconds(t++ * 100),
        host_impl_->GetScrollTree(), true, nullptr);
    animation_host->UpdateAnimationState(true, nullptr);
  }

  // Finish the snap animation for scroll_node1.
  handler.ScrollOffsetAnimationFinished(container1_id_);
  EXPECT_FALSE(snap_state_map.contains(container1_id_));
  EXPECT_TRUE(snap_state_map.contains(container2_id_));
  EXPECT_EQ(snap_state_map.at(container2_id_).animating_snap_target_ids_.y,
            ElementId(30));

  // Finish the snap animation for scroll_node2.
  handler.ScrollOffsetAnimationFinished(container2_id_);
  EXPECT_FALSE(snap_state_map.contains(container1_id_));
  EXPECT_FALSE(snap_state_map.contains(container2_id_));
}

class ElasticOverscrollTest : public LayerTreeHostImplTest {
 public:
  LayerTreeSettings DefaultSettings() override {
    auto settings = LayerTreeHostImplTest::DefaultSettings();
    settings.enable_elastic_overscroll_on_root = true;
    return settings;
  }
};

// Verifies destroying the scroll elasticity helper without a viewport scroll
// node does not crash.
TEST_P(ElasticOverscrollTest, ElasticOverscrollWithoutViewport) {
  ASSERT_NE(nullptr,
            host_impl_->GetInputHandler().CreateScrollElasticityHelper());

  // Destroying the helper without a viewport should be a safe no-op.
  host_impl_->GetInputHandler().DestroyScrollElasticityHelper();
}
INSTANTIATE_COMMIT_TO_TREE_TEST_P(ElasticOverscrollTest);

class ElasticOverscrollInvalidationTest : public ElasticOverscrollTest {
 public:
  void SetupScroll(bool is_composited, uint32_t main_thread_repaint_reasons) {
    SetupViewportLayersOuterScrolls(gfx::Size(100, 100), gfx::Size(100, 100));
    layer = AddScrollableLayer(OuterViewportScrollLayer(), gfx::Size(100, 100),
                               gfx::Size(200, 200));

    scroll_node = &host_impl_->active_tree()
                       ->property_trees()
                       ->scroll_tree_mutable()
                       .MutableNode(layer->scroll_tree_index());
    scroll_node->is_composited = is_composited;
    scroll_node->main_thread_repaint_reasons = main_thread_repaint_reasons;
  }

  void CreateElasticityHelper() {
    helper = host_impl_->GetInputHandler().CreateScrollElasticityHelper();
  }

  void TearDown() override {
    if (helper) {
      helper = nullptr;
      host_impl_->GetInputHandler().DestroyScrollElasticityHelper();
    }
    scroll_node = nullptr;
    layer = nullptr;
    ElasticOverscrollTest::TearDown();
  }

 protected:
  raw_ptr<LayerImpl> layer = nullptr;
  raw_ptr<ScrollNode> scroll_node = nullptr;
  raw_ptr<ScrollElasticityHelper> helper = nullptr;
};

TEST_P(ElasticOverscrollInvalidationTest,
       ElasticOverscrollInvalidationComposited) {
  // Setup a composited scroller.
  SetupScroll(true /*is_composited*/,
              MainThreadScrollingReason::kNotScrollingOnMain);

  CreateElasticityHelper();

  // Apply stretch.
  const gfx::Vector2dF stretch(10, 10);
  helper->SetStretchAmount(layer->element_id(), stretch);
  EXPECT_EQ(stretch, helper->StretchAmount(layer->element_id()));

  EXPECT_FALSE(did_request_impl_side_invalidation_);
}

TEST_P(ElasticOverscrollInvalidationTest,
       ElasticOverscrollInvalidationThreadedOnly) {
  // Setup a non-composited, threaded scroller. (raster inducing)
  SetupScroll(false /*is_composited*/,
              MainThreadScrollingReason::kNotScrollingOnMain);

  CreateElasticityHelper();

  // Apply stretch.
  const gfx::Vector2dF stretch(10, 10);
  helper->SetStretchAmount(layer->element_id(), stretch);

  if (host_impl_->CommitsToActiveTree()) {
    EXPECT_EQ(gfx::Vector2dF(), helper->StretchAmount(layer->element_id()));
    EXPECT_FALSE(did_request_impl_side_invalidation_);
  } else {
    EXPECT_EQ(stretch, helper->StretchAmount(layer->element_id()));
    EXPECT_TRUE(did_request_impl_side_invalidation_);
  }
}

TEST_P(ElasticOverscrollInvalidationTest,
       ElasticOverscrollInvalidationMainOnly) {
  // Setup a main thread only scroller. (disables overscroll effect)
  SetupScroll(false /*is_composited*/,
              MainThreadScrollingReason::kPreferNonCompositedScrolling);

  CreateElasticityHelper();

  // Apply stretch.
  const gfx::Vector2dF stretch(10, 10);
  helper->SetStretchAmount(layer->element_id(), stretch);
  EXPECT_EQ(gfx::Vector2dF(), helper->StretchAmount(layer->element_id()));

  EXPECT_FALSE(did_request_impl_side_invalidation_);
}

TEST_P(ElasticOverscrollInvalidationTest, ElasticOverscrollSyncsToPendingTree) {
  // Configure as a threaded, non-composited scroller.
  SetupScroll(false /*is_composited*/,
              MainThreadScrollingReason::kNotScrollingOnMain);

  ElementId id = layer->element_id();
  EXPECT_EQ(id, scroll_node->element_id);

  // Ensure pending and active trees exist and are updated.
  UpdateDrawProperties(host_impl_->active_tree());

  CreateElasticityHelper();

  const gfx::Vector2dF stretch(10, 10);

  // Transform update is deferred on the active tree.
  auto transform_node_active = [&, transform_id = scroll_node->transform_id]() {
    return host_impl_->active_tree()->property_trees()->transform_tree().Node(
        transform_id);
  };
  EXPECT_TRUE(transform_node_active().local.IsIdentity());

  EnsureSyncTree();
  if (!host_impl_->CommitsToActiveTree()) {
    // Setup root layer to allow usage of `UpdateDrawProperties()`
    auto setup_root_layer = [&](LayerTreeImpl* tree) {
      std::unique_ptr<LayerImpl> layer_impl =
          LayerImpl::Create(tree, root_layer()->id());
      layer_impl->SetScrollTreeIndex(scroll_node->id);
      layer_impl->SetTransformTreeIndex(scroll_node->transform_id);
      layer_impl->SetClipTreeIndex(0);
      layer_impl->SetEffectTreeIndex(0);
      host_impl_->pending_tree()->SetRootLayerForTesting(std::move(layer_impl));
    };
    // Setup pending tree
    setup_root_layer(host_impl_->pending_tree());
    host_impl_->pending_tree()->SetPropertyTrees(
        *host_impl_->active_tree()->property_trees());

    auto transform_node_pending = [&, element_id = scroll_node->element_id]() {
      return host_impl_->pending_tree()
          ->property_trees()
          ->transform_tree()
          .FindNodeFromElementId(element_id);
    };

    ASSERT_TRUE(transform_node_pending());
    EXPECT_TRUE(transform_node_pending()->local.IsIdentity());

    // Set initial stretch.
    helper->SetStretchAmount(id, stretch);

    const ScrollTree& scroll_tree =
        host_impl_->active_tree()->property_trees()->scroll_tree();
    const TransformTree& transform_tree =
        host_impl_->active_tree()->property_trees()->transform_tree();
    const TransformTree& pending_transform_tree =
        host_impl_->pending_tree()->property_trees()->transform_tree();
    const ElementId scroll_element_id = scroll_node->element_id;

    // Stretch on `ScrollTree` is always kept up to date.
    EXPECT_EQ(stretch, scroll_tree.GetElasticOverscroll(*scroll_node));

    // Drawn stretch should not be set yet.
    EXPECT_EQ(gfx::Vector2dF(),
              transform_tree.GetDrawnElasticOverscroll(scroll_element_id));
    EXPECT_EQ(
        gfx::Vector2dF(),
        pending_transform_tree.GetDrawnElasticOverscroll(scroll_element_id));

    // Ensure transform invalidation has not happened yet.
    EXPECT_FALSE(transform_node_pending()->needs_local_transform_update);
    EXPECT_FALSE(transform_node_active().needs_local_transform_update);

    EXPECT_TRUE(transform_node_pending()->to_parent.IsIdentity());
    EXPECT_TRUE(transform_node_active().to_parent.IsIdentity());

    // Pending transform tree should be updated for all scrollers.
    helper->ApplyStretchAmountsToPending();

    // Drawn stretch should be set only on pending.
    EXPECT_EQ(gfx::Vector2dF(),
              transform_tree.GetDrawnElasticOverscroll(scroll_element_id));
    EXPECT_EQ(stretch, pending_transform_tree.GetDrawnElasticOverscroll(
                           scroll_element_id));

    // Ensure transforms have been invalidated, but not updated yet.
    EXPECT_TRUE(transform_node_pending()->needs_local_transform_update);
    EXPECT_FALSE(transform_node_active().needs_local_transform_update);

    EXPECT_TRUE(transform_node_pending()->to_parent.IsIdentity());
    EXPECT_TRUE(transform_node_active().to_parent.IsIdentity());

    // Activation should force the transform update on the active tree
    UpdateDrawProperties(host_impl_->pending_tree());

    // Drawn stretch should be set only on pending.
    EXPECT_EQ(gfx::Vector2dF(),
              transform_tree.GetDrawnElasticOverscroll(scroll_element_id));
    EXPECT_EQ(stretch, pending_transform_tree.GetDrawnElasticOverscroll(
                           scroll_element_id));

    // Ensure deferred update happens on pending tree.
    EXPECT_FALSE(transform_node_pending()->needs_local_transform_update);
    EXPECT_FALSE(transform_node_active().needs_local_transform_update);

    EXPECT_FALSE(transform_node_pending()->to_parent.IsIdentity());
    EXPECT_TRUE(transform_node_active().to_parent.IsIdentity());

    layer = nullptr;
    scroll_node = nullptr;
    host_impl_->ActivateSyncTree();

    // Drawn stretch should be propagated to active as well.
    EXPECT_EQ(stretch,
              transform_tree.GetDrawnElasticOverscroll(scroll_element_id));
    EXPECT_EQ(stretch, pending_transform_tree.GetDrawnElasticOverscroll(
                           scroll_element_id));

    // Ensure update is copied over to the active tree.
    EXPECT_FALSE(transform_node_active().needs_local_transform_update);

#if BUILDFLAG(IS_ANDROID)
    // On Android, elastic overscroll is implemented as a "stretch" effect.
    // This modifies the scale of the transform rather than applying a
    // translation.
    EXPECT_FALSE(transform_node_active().to_parent.IsIdentity());
    EXPECT_NE(transform_node_active().to_parent.To2dScale(),
              gfx::Vector2dF(1.0f, 1.0f));
#else
    // On non-Android, elastic overscroll translates the scroll container.
    // The transform is the inverse of the stretch vector (like scroll offset).
    EXPECT_EQ(transform_node_active().to_parent.To2dTranslation(), -stretch);
#endif
  } else {
    helper->SetStretchAmount(id, stretch);
    EXPECT_EQ(gfx::Vector2dF(), helper->StretchAmount(id));
  }
}

INSTANTIATE_COMMIT_TO_TREE_TEST_P(ElasticOverscrollInvalidationTest);

class OverscrollEffectTest : public LayerTreeHostImplTest {
 public:
  OverscrollEffectTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kOverscrollEffectOnNonRootScrollers);
  }

  void SetUp() override {
    LayerTreeHostImplTest::SetUp();
    // Common setup: 100x100 viewport, 200x200 content.
    SetupViewportLayers(host_impl_->active_tree(), gfx::Size(100, 100),
                        gfx::Size(100, 100), gfx::Size(200, 200));
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.1f, 10.f);
    host_impl_->active_tree()->UpdateDrawProperties(false, false);
  }

 protected:
  void VerifyOverscrollBehavior(LayerImpl* layer) {
    InputHandler& handler = GetInputHandler();
    ScrollNode* scroll_node = GetScrollNode(layer);
    bool is_root_scroller = layer == OuterViewportScrollLayer();

    // Scroll parameters
    const gfx::Vector2dF delta(10, 10);
    const gfx::Point hit_test_point(20, 20);

    // Helper to run a scroll sequence and return the unused delta.
    auto run_scroll = [&](const OverscrollBehavior& behavior) {
      scroll_node->overscroll_behavior = behavior;

      // Reset to the scroll boundary so any positive delta is "unused".
      layer->SetCurrentScrollOffset(gfx::PointF(100, 100));

      handler.ScrollBegin(BeginState(hit_test_point, gfx::Vector2dF(),
                                     ui::ScrollInputType::kTouchscreen)
                              .get(),
                          ui::ScrollInputType::kTouchscreen);

      auto result = handler.ScrollUpdate(
          UpdateState(hit_test_point, delta, ui::ScrollInputType::kTouchscreen),
          base::TimeDelta());

      handler.ScrollEnd(/*should_snap=*/false, std::nullopt);
      return result.unused_scroll_delta;
    };

    // Case 1: Auto (Default)
    // Expectation: Full delta is reported as unused (bubbling allowed).
    EXPECT_VECTOR2DF_EQ(
        delta, run_scroll(OverscrollBehavior(OverscrollBehavior::Type::kAuto)));

    // Case 2: None
    // Expectation: Unused delta is clamped to zero (bubbling suppressed) for
    // non-root scrollers. For the root scroller, it is preserved to allow
    // the browser to handle the overscroll event (e.g. navigation blocking).
    gfx::Vector2dF expected_none =
        is_root_scroller ? delta : gfx::Vector2dF(0, 0);
    EXPECT_VECTOR2DF_EQ(
        expected_none,
        run_scroll(OverscrollBehavior(OverscrollBehavior::Type::kNone)));

    // Case 3: Mixed (None on X, Auto on Y)
    // Expectation: X is clamped for non-root scrollers.
    gfx::Vector2dF expected_mixed =
        is_root_scroller ? delta : gfx::Vector2dF(0, delta.y());
    EXPECT_VECTOR2DF_EQ(expected_mixed, run_scroll(OverscrollBehavior(
                                            OverscrollBehavior::Type::kNone,
                                            OverscrollBehavior::Type::kAuto)));

    // Case 4: Chain
    // Expectation: Unused delta is clamped to zero (no local border effects)
    // but bubbling is allowed (tested separately).
    gfx::Vector2dF expected_chain =
        is_root_scroller ? delta : gfx::Vector2dF(0, 0);
    EXPECT_VECTOR2DF_EQ(
        expected_chain,
        run_scroll(OverscrollBehavior(OverscrollBehavior::Type::kChain)));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_COMMIT_TO_TREE_TEST_P(OverscrollEffectTest);

TEST_P(OverscrollEffectTest, RespectsOverscrollBehaviorOnChild) {
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* child = AddScrollableLayer(outer_scroll, gfx::Size(100, 100),
                                        gfx::Size(200, 200));

  // Position the child at (10, 10) so the hit_test_point (20, 20) targets it.
  child->SetOffsetToTransformParent(gfx::Vector2dF(10, 10));
  host_impl_->active_tree()->UpdateDrawProperties(false, false);

  VerifyOverscrollBehavior(child);
}

TEST_P(OverscrollEffectTest, RespectsOverscrollBehaviorOnRoot) {
  // The root layer covers the viewport, so it catches the hit test.
  VerifyOverscrollBehavior(OuterViewportScrollLayer());
}

TEST_P(LayerTreeHostImplTest, CollectTrackedElementRects) {
  auto* root_layer = SetupRootLayer<SolidColorLayerImpl>(
      host_impl_->active_tree(), gfx::Size(100, 100));

  const base::Token kId1 = base::Token(1, 2);
  const base::Token kId2 = base::Token(2, 3);
  const viz::TrackedElementFeature kFeature0 =
      static_cast<viz::TrackedElementFeature>(0);
  const viz::TrackedElementFeature kFeature1 =
      static_cast<viz::TrackedElementFeature>(1);

  viz::TrackedElementRects combined_rects;
  combined_rects[kFeature0] = {viz::TrackedElementRect(
      kId1, gfx::Rect(0, 0, 50, 50),
      /*should_add_to_compositor_frame_metadata=*/false)};
  combined_rects[kFeature1] = {viz::TrackedElementRect(
      kId2, gfx::Rect(0, 0, 10, 20),
      /*should_add_to_compositor_frame_metadata=*/true)};

  root_layer->SetTrackedElementRects(combined_rects);
  root_layer->SetDrawsContent(true);
  UpdateDrawProperties(host_impl_->active_tree());

  viz::TrackedElementRects compositor_rects =
      host_impl_->CollectTrackedElementRects(
          /*is_for_compositor_frame_metadata=*/true);
  EXPECT_EQ(1u, compositor_rects.size());
  EXPECT_TRUE(compositor_rects.contains(kFeature1));
  EXPECT_EQ(1u, compositor_rects.at(kFeature1).size());
  EXPECT_EQ(kId2, compositor_rects.at(kFeature1)[0].id);

  viz::TrackedElementRects render_frame_rects =
      host_impl_->CollectTrackedElementRects(
          /*is_for_compositor_frame_metadata=*/false);
  EXPECT_EQ(1u, render_frame_rects.size());
  EXPECT_TRUE(render_frame_rects.contains(kFeature0));
  EXPECT_EQ(1u, render_frame_rects.at(kFeature0).size());
  EXPECT_EQ(kId1, render_frame_rects.at(kFeature0)[0].id);
}

}  // namespace cc
