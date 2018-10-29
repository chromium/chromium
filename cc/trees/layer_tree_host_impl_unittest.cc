// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/animation/transform_operations.h"
#include "cc/base/histograms.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/input/page_scale_animation.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/video_layer_impl.h"
#include "cc/layers/viewport.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_mask_layer_impl.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/fake_video_frame_provider.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/layer_tree_test.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/render_frame_metadata_observer.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/single_thread_proxy.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_output_surface.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

#define EXPECT_SCOPED(statements) \
  {                               \
    SCOPED_TRACE("");             \
    statements;                   \
  }

using ::testing::Mock;
using ::testing::Return;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::_;
using media::VideoFrame;

namespace cc {
namespace {

viz::SurfaceId MakeSurfaceId(const viz::FrameSinkId& frame_sink_id,
                             uint32_t parent_id) {
  return viz::SurfaceId(
      frame_sink_id,
      viz::LocalSurfaceId(parent_id,
                          base::UnguessableToken::Deserialize(0, 1u)));
}

struct TestFrameData : public LayerTreeHostImpl::FrameData {
  TestFrameData() {
    // Set ack to something valid, so DCHECKs don't complain.
    begin_frame_ack = viz::BeginFrameAck::CreateManualAckWithDamage();
  }
};

class LayerTreeHostImplTest : public testing::Test,
                              public LayerTreeHostImplClient {
 public:
  LayerTreeHostImplTest()
      : task_runner_provider_(base::ThreadTaskRunnerHandle::Get()),
        always_main_thread_blocked_(&task_runner_provider_),
        on_can_draw_state_changed_called_(false),
        did_notify_ready_to_activate_(false),
        did_request_commit_(false),
        did_request_redraw_(false),
        did_request_next_frame_(false),
        did_request_prepare_tiles_(false),
        did_complete_page_scale_animation_(false),
        reduce_memory_result_(true),
        did_request_impl_side_invalidation_(false) {
    media::InitializeMediaLibrary();
  }

  LayerTreeSettings DefaultSettings() {
    LayerTreeSettings settings;
    settings.enable_surface_synchronization = true;
    settings.minimum_occlusion_tracking_size = gfx::Size();
    return settings;
  }

  void SetUp() override {
    CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());
  }

  void CreatePendingTree() {
    host_impl_->CreatePendingTree();
    LayerTreeImpl* pending_tree = host_impl_->pending_tree();
    pending_tree->SetDeviceViewportSize(
        host_impl_->active_tree()->GetDeviceViewport().size());
    pending_tree->SetDeviceScaleFactor(
        host_impl_->active_tree()->device_scale_factor());
  }

  void TearDown() override {
    if (host_impl_)
      host_impl_->ReleaseLayerTreeFrameSink();
  }

  void DidLoseLayerTreeFrameSinkOnImplThread() override {}
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void DidReceiveCompositorFrameAckOnImplThread() override {}
  void OnCanDrawStateChanged(bool can_draw) override {
    on_can_draw_state_changed_called_ = true;
  }
  void NotifyReadyToActivate() override {
    did_notify_ready_to_activate_ = true;
    host_impl_->ActivateSyncTree();
  }
  void NotifyReadyToDraw() override {}
  void SetNeedsRedrawOnImplThread() override { did_request_redraw_ = true; }
  void SetNeedsOneBeginImplFrameOnImplThread() override {
    did_request_next_frame_ = true;
  }
  void SetNeedsPrepareTilesOnImplThread() override {
    did_request_prepare_tiles_ = true;
  }
  void SetNeedsCommitOnImplThread() override { did_request_commit_ = true; }
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override {}
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) override {}
  bool IsInsideDraw() override { return false; }
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(const base::Closure& task,
                                            base::TimeDelta delay) override {
    animation_task_ = task;
    requested_animation_delay_ = delay;
  }
  void DidActivateSyncTree() override {
    // Make sure the active tree always has a valid LocalSurfaceId.
    host_impl_->active_tree()->SetLocalSurfaceIdFromParent(
        viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
        base::TimeTicks());
  }
  void WillPrepareTiles() override {}
  void DidPrepareTiles() override {}
  void DidCompletePageScaleAnimationOnImplThread() override {
    did_complete_page_scale_animation_ = true;
  }
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override {
    std::unique_ptr<TestFrameData> frame(new TestFrameData);
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(frame.get()));
    last_on_draw_render_passes_.clear();
    viz::RenderPass::CopyAll(frame->render_passes,
                             &last_on_draw_render_passes_);
    host_impl_->DrawLayers(frame.get());
    host_impl_->DidDrawAllLayers(*frame);
    last_on_draw_frame_ = std::move(frame);
  }
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation) override {
    did_request_impl_side_invalidation_ = true;
  }
  void NotifyImageDecodeRequestFinished() override {}
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const gfx::PresentationFeedback& feedback) override {}

  void set_reduce_memory_result(bool reduce_memory_result) {
    reduce_memory_result_ = reduce_memory_result;
  }

  virtual bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) {
    return CreateHostImplWithTaskRunnerProvider(
        settings, std::move(layer_tree_frame_sink), &task_runner_provider_);
  }

  AnimationHost* GetImplAnimationHost() const {
    return static_cast<AnimationHost*>(host_impl_->mutator_host());
  }

  virtual bool CreateHostImplWithTaskRunnerProvider(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink,
      TaskRunnerProvider* task_runner_provider) {
    if (host_impl_)
      host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_.reset();
    InitializeImageWorker(settings);
    host_impl_ = LayerTreeHostImpl::Create(
        settings, this, task_runner_provider, &stats_instrumentation_,
        &task_graph_runner_,
        AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0,
        image_worker_ ? image_worker_->task_runner() : nullptr);
    layer_tree_frame_sink_ = std::move(layer_tree_frame_sink);
    host_impl_->SetVisible(true);
    bool init = host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
    host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 10));
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    host_impl_->active_tree()->SetLocalSurfaceIdFromParent(
        viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
        base::TimeTicks());
    // Set the viz::BeginFrameArgs so that methods which use it are able to.
    host_impl_->WillBeginImplFrame(viz::CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1,
        base::TimeTicks() + base::TimeDelta::FromMilliseconds(1)));
    host_impl_->DidFinishImplFrame();

    timeline_ =
        AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
    GetImplAnimationHost()->AddAnimationTimeline(timeline_);

    return init;
  }

  void SetupRootLayerImpl(std::unique_ptr<LayerImpl> root) {
    root->SetPosition(gfx::PointF());
    root->SetBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    root->draw_properties().visible_layer_rect = gfx::Rect(0, 0, 10, 10);
    root->test_properties()->force_render_surface = true;
    host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  }

  static gfx::Vector2dF ScrollDelta(LayerImpl* layer_impl) {
    gfx::ScrollOffset delta = layer_impl->layer_tree_impl()
                                  ->property_trees()
                                  ->scroll_tree.GetScrollOffsetDeltaForTesting(
                                      layer_impl->element_id());
    return gfx::Vector2dF(delta.x(), delta.y());
  }

  static void ExpectClearedScrollDeltasRecursive(LayerImpl* root) {
    for (auto* layer : *root->layer_tree_impl())
      ASSERT_EQ(ScrollDelta(layer), gfx::Vector2d());
  }

  static ::testing::AssertionResult ScrollInfoContains(
      const ScrollAndScaleSet& scroll_info,
      ElementId id,
      const gfx::ScrollOffset& scroll_delta) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].element_id != id)
        continue;

      if (scroll_delta != scroll_info.scrolls[i].scroll_delta) {
        return ::testing::AssertionFailure()
               << "Expected " << scroll_delta.ToString() << ", not "
               << scroll_info.scrolls[i].scroll_delta.ToString();
      }
      times_encountered++;
    }

    if (id == scroll_info.inner_viewport_scroll.element_id) {
      if (scroll_delta != scroll_info.inner_viewport_scroll.scroll_delta) {
        return ::testing::AssertionFailure()
               << "Expected " << scroll_delta.ToString() << ", not "
               << scroll_info.inner_viewport_scroll.scroll_delta.ToString();
      }
      times_encountered++;
    }

    if (times_encountered != 1)
      return ::testing::AssertionFailure() << "No scroll found with id " << id;
    return ::testing::AssertionSuccess();
  }

  static void ExpectNone(const ScrollAndScaleSet& scroll_info, ElementId id) {
    int times_encountered = 0;

    for (size_t i = 0; i < scroll_info.scrolls.size(); ++i) {
      if (scroll_info.scrolls[i].element_id != id)
        continue;
      times_encountered++;
    }

    ASSERT_EQ(0, times_encountered);
  }

  LayerImpl* CreateScrollAndContentsLayers(LayerTreeImpl* layer_tree_impl,
                                           const gfx::Size& content_size) {
    // Clear any existing viewport layers that were setup so this function can
    // be called multiple times.
    layer_tree_impl->ClearViewportLayers();

    // Create both an inner viewport scroll layer and an outer viewport scroll
    // layer. The MaxScrollOffset of the outer viewport scroll layer will be
    // 0x0, so the scrolls will be applied directly to the inner viewport.
    const int kOuterViewportClipLayerId = 116;
    const int kOuterViewportScrollLayerId = 117;
    const int kContentLayerId = 118;
    const int kInnerViewportScrollLayerId = 2;
    const int kInnerViewportClipLayerId = 4;
    const int kPageScaleLayerId = 5;

    std::unique_ptr<LayerImpl> root = LayerImpl::Create(layer_tree_impl, 1);
    root->SetBounds(content_size);
    root->SetPosition(gfx::PointF());
    root->test_properties()->force_render_surface = true;

    std::unique_ptr<LayerImpl> inner_scroll =
        LayerImpl::Create(layer_tree_impl, kInnerViewportScrollLayerId);
    inner_scroll->test_properties()->is_container_for_fixed_position_layers =
        true;
    inner_scroll->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            inner_scroll->element_id(), gfx::ScrollOffset());

    std::unique_ptr<LayerImpl> inner_clip =
        LayerImpl::Create(layer_tree_impl, kInnerViewportClipLayerId);
    gfx::Size viewport_scroll_bounds =
        gfx::Size(content_size.width() / 2, content_size.height() / 2);
    inner_clip->SetBounds(viewport_scroll_bounds);

    std::unique_ptr<LayerImpl> page_scale =
        LayerImpl::Create(layer_tree_impl, kPageScaleLayerId);

    inner_scroll->SetScrollable(viewport_scroll_bounds);
    inner_scroll->SetElementId(
        LayerIdToElementIdForTesting(inner_scroll->id()));
    inner_scroll->SetBounds(content_size);
    inner_scroll->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> outer_clip =
        LayerImpl::Create(layer_tree_impl, kOuterViewportClipLayerId);
    outer_clip->SetBounds(content_size);
    outer_clip->test_properties()->is_container_for_fixed_position_layers =
        true;

    std::unique_ptr<LayerImpl> outer_scroll =
        LayerImpl::Create(layer_tree_impl, kOuterViewportScrollLayerId);
    outer_scroll->SetScrollable(content_size);
    outer_scroll->SetElementId(
        LayerIdToElementIdForTesting(outer_scroll->id()));
    outer_scroll->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            outer_scroll->element_id(), gfx::ScrollOffset());
    outer_scroll->SetBounds(content_size);
    outer_scroll->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> contents =
        LayerImpl::Create(layer_tree_impl, kContentLayerId);
    contents->SetDrawsContent(true);
    contents->SetBounds(content_size);
    contents->SetPosition(gfx::PointF());

    outer_scroll->test_properties()->AddChild(std::move(contents));
    outer_clip->test_properties()->AddChild(std::move(outer_scroll));
    inner_scroll->test_properties()->AddChild(std::move(outer_clip));
    page_scale->test_properties()->AddChild(std::move(inner_scroll));
    inner_clip->test_properties()->AddChild(std::move(page_scale));
    root->test_properties()->AddChild(std::move(inner_clip));

    layer_tree_impl->SetRootLayerForTesting(std::move(root));
    layer_tree_impl->BuildPropertyTreesForTesting();
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = kPageScaleLayerId;
    viewport_ids.inner_viewport_container = kInnerViewportClipLayerId;
    viewport_ids.outer_viewport_container = kOuterViewportClipLayerId;
    viewport_ids.inner_viewport_scroll = kInnerViewportScrollLayerId;
    viewport_ids.outer_viewport_scroll = kOuterViewportScrollLayerId;
    layer_tree_impl->SetViewportLayersFromIds(viewport_ids);

    layer_tree_impl->DidBecomeActive();
    return layer_tree_impl->InnerViewportScrollLayer();
  }

  LayerImpl* SetupScrollAndContentsLayers(const gfx::Size& content_size) {
    LayerImpl* scroll_layer =
        CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
    host_impl_->active_tree()->DidBecomeActive();
    return scroll_layer;
  }

  void CreateAndTestNonScrollableLayers(const bool& transparent_layer) {
    LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
    gfx::Size content_size = gfx::Size(360, 600);
    gfx::Size scroll_content_size = gfx::Size(345, 3800);
    gfx::Size scrollbar_size = gfx::Size(15, 600);

    host_impl_->active_tree()->SetDeviceViewportSize(content_size);
    std::unique_ptr<LayerImpl> root = LayerImpl::Create(layer_tree_impl, 1);
    root->SetBounds(content_size);
    root->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 3);
    scroll->SetBounds(scroll_content_size);
    scroll->SetScrollable(content_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
        SolidColorScrollbarLayerImpl::Create(layer_tree_impl, 4, VERTICAL, 10,
                                             0, false, true);
    scrollbar->SetBounds(scrollbar_size);
    scrollbar->SetPosition(gfx::PointF(345, 0));
    scrollbar->SetScrollElementId(scroll->element_id());
    scrollbar->SetDrawsContent(true);
    scrollbar->test_properties()->opacity = 1.f;

    std::unique_ptr<LayerImpl> squash1 = LayerImpl::Create(layer_tree_impl, 5);
    squash1->SetBounds(gfx::Size(140, 300));
    squash1->SetPosition(gfx::PointF(220, 0));
    if (transparent_layer) {
      squash1->test_properties()->opacity = 0.0f;
      // The transparent layer should still participate in hit testing even
      // through it does not draw content.
      squash1->SetHitTestableWithoutDrawsContent(true);
    } else {
      squash1->SetDrawsContent(true);
    }

    std::unique_ptr<LayerImpl> squash2 = LayerImpl::Create(layer_tree_impl, 6);
    squash2->SetBounds(gfx::Size(140, 300));
    squash2->SetPosition(gfx::PointF(220, 300));
    squash2->SetDrawsContent(true);

    scroll->test_properties()->AddChild(std::move(squash2));
    root->test_properties()->AddChild(std::move(scroll));
    root->test_properties()->AddChild(std::move(scrollbar));
    root->test_properties()->AddChild(std::move(squash1));

    layer_tree_impl->SetRootLayerForTesting(std::move(root));
    layer_tree_impl->BuildPropertyTreesForTesting();
    layer_tree_impl->DidBecomeActive();

    // The point hits squash1 layer and also scroll layer, because scroll layer
    // is not an ancestor of squash1 layer, we cannot scroll on impl thread.
    InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
        BeginState(gfx::Point(230, 150)).get(), InputHandler::WHEEL);
    ASSERT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_scrolling_reasons);

    // The point hits squash1 layer and also scrollbar layer.
    status = host_impl_->ScrollBegin(BeginState(gfx::Point(350, 150)).get(),
                                     InputHandler::WHEEL);
    ASSERT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
    ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_scrolling_reasons);

    // The point hits squash2 layer and also scroll layer, because scroll layer
    // is an ancestor of squash2 layer, we should scroll on impl.
    status = host_impl_->ScrollBegin(BeginState(gfx::Point(230, 450)).get(),
                                     InputHandler::WHEEL);
    ASSERT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  }

  // Sets up a typical virtual viewport setup with one child content layer.
  // Returns a pointer to the content layer.
  LayerImpl* CreateBasicVirtualViewportLayers(const gfx::Size& viewport_size,
                                              const gfx::Size& content_size) {
    // CreateScrollAndContentsLayers makes the outer viewport unscrollable and
    // the inner a different size from the outer. We'll reuse its layer
    // hierarchy but adjust the sizing to our needs.
    CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);

    LayerImpl* content_layer = host_impl_->OuterViewportScrollLayer()
                                   ->test_properties()
                                   ->children.back();
    content_layer->SetBounds(content_size);
    host_impl_->OuterViewportScrollLayer()->SetBounds(content_size);
    host_impl_->OuterViewportScrollLayer()->SetScrollable(viewport_size);

    LayerImpl* outer_clip =
        host_impl_->OuterViewportScrollLayer()->test_properties()->parent;
    outer_clip->SetBounds(viewport_size);

    LayerImpl* inner_clip_layer = host_impl_->InnerViewportScrollLayer()
                                      ->test_properties()
                                      ->parent->test_properties()
                                      ->parent;
    inner_clip_layer->SetBounds(viewport_size);
    host_impl_->InnerViewportScrollLayer()->SetBounds(viewport_size);
    host_impl_->InnerViewportScrollLayer()->SetScrollable(viewport_size);

    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);
    host_impl_->active_tree()->DidBecomeActive();

    return content_layer;
  }

  std::unique_ptr<LayerImpl> CreateScrollableLayer(int id,
                                                   const gfx::Size& size) {
    std::unique_ptr<LayerImpl> layer =
        LayerImpl::Create(host_impl_->active_tree(), id);
    layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
    layer->SetDrawsContent(true);
    layer->SetBounds(size);
    gfx::Size scroll_container_bounds =
        gfx::Size(size.width() / 2, size.height() / 2);
    layer->SetScrollable(scroll_container_bounds);
    return layer;
  }

  std::unique_ptr<ScrollState> BeginState(const gfx::Point& point) {
    ScrollStateData scroll_state_data;
    scroll_state_data.is_beginning = true;
    scroll_state_data.position_x = point.x();
    scroll_state_data.position_y = point.y();
    std::unique_ptr<ScrollState> scroll_state(
        new ScrollState(scroll_state_data));
    return scroll_state;
  }

  std::unique_ptr<ScrollState> UpdateState(const gfx::Point& point,
                                           const gfx::Vector2dF& delta) {
    ScrollStateData scroll_state_data;
    scroll_state_data.delta_x = delta.x();
    scroll_state_data.delta_y = delta.y();
    scroll_state_data.position_x = point.x();
    scroll_state_data.position_y = point.y();
    std::unique_ptr<ScrollState> scroll_state(
        new ScrollState(scroll_state_data));
    return scroll_state;
  }

  std::unique_ptr<ScrollState> EndState() {
    ScrollStateData scroll_state_data;
    scroll_state_data.is_ending = true;
    std::unique_ptr<ScrollState> scroll_state(
        new ScrollState(scroll_state_data));
    return scroll_state;
  }

  void DrawFrame() {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }

  void TestGPUMemoryForTilings(const gfx::Size& layer_size) {
    std::unique_ptr<FakeRecordingSource> recording_source =
        FakeRecordingSource::CreateFilledRecordingSource(layer_size);
    PaintImage checkerable_image =
        CreateDiscardablePaintImage(gfx::Size(500, 500));
    recording_source->add_draw_image(checkerable_image, gfx::Point(0, 0));

    recording_source->Rerecord();
    scoped_refptr<FakeRasterSource> raster_source =
        FakeRasterSource::CreateFromRecordingSource(recording_source.get());

    // Create the pending tree.
    host_impl_->BeginCommit();
    LayerTreeImpl* pending_tree = host_impl_->pending_tree();
    pending_tree->SetDeviceViewportSize(layer_size);
    pending_tree->SetRootLayerForTesting(
        FakePictureLayerImpl::CreateWithRasterSource(pending_tree, 1,
                                                     raster_source));
    auto* root = static_cast<FakePictureLayerImpl*>(*pending_tree->begin());
    root->SetBounds(layer_size);
    root->SetDrawsContent(true);
    pending_tree->BuildPropertyTreesForTesting();

    // CompleteCommit which should perform a PrepareTiles, adding tilings for
    // the root layer, each one having a raster task.
    host_impl_->CommitComplete();
    // Activate the pending tree and ensure that all tiles are rasterized.
    while (!did_notify_ready_to_activate_)
      base::RunLoop().RunUntilIdle();

    DrawFrame();

    host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_ = nullptr;
  }

  void WhiteListedTouchActionTestHelper(float device_scale_factor,
                                        float page_scale_factor) {
    LayerImpl* scroll = SetupScrollAndContentsLayers(gfx::Size(200, 200));
    host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));
    DrawFrame();
    LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();

    // Just hard code some random number, we care about the actual page scale
    // factor on the active tree.
    float min_page_scale_factor = 0.1f;
    float max_page_scale_factor = 5.0f;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale_factor, max_page_scale_factor);
    host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);

    std::unique_ptr<LayerImpl> child_layer =
        LayerImpl::Create(host_impl_->active_tree(), 6);
    LayerImpl* child = child_layer.get();
    child_layer->SetDrawsContent(true);
    child_layer->SetPosition(gfx::PointF(0, 0));
    child_layer->SetBounds(gfx::Size(25, 25));
    scroll->test_properties()->AddChild(std::move(child_layer));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    TouchActionRegion root_touch_action_region;
    root_touch_action_region.Union(kTouchActionPanX, gfx::Rect(0, 0, 50, 50));
    root->SetTouchActionRegion(root_touch_action_region);
    TouchActionRegion child_touch_action_region;
    child_touch_action_region.Union(kTouchActionPanLeft,
                                    gfx::Rect(0, 0, 25, 25));
    child->SetTouchActionRegion(child_touch_action_region);

    TouchAction touch_action = kTouchActionAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(10, 10),
                                                       &touch_action);
    EXPECT_EQ(kTouchActionPanLeft, touch_action);
    touch_action = kTouchActionAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(30, 30),
                                                       &touch_action);
    EXPECT_EQ(kTouchActionPanX, touch_action);

    TouchActionRegion new_child_region;
    new_child_region.Union(kTouchActionPanY, gfx::Rect(0, 0, 25, 25));
    child->SetTouchActionRegion(new_child_region);
    touch_action = kTouchActionAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(10, 10),
                                                       &touch_action);
    EXPECT_EQ(kTouchActionPanY, touch_action);
    touch_action = kTouchActionAuto;
    host_impl_->EventListenerTypeForTouchStartOrMoveAt(gfx::Point(30, 30),
                                                       &touch_action);
    EXPECT_EQ(kTouchActionPanX, touch_action);
  }

  LayerImpl* CreateLayerForSnapping() {
    LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(200, 200));
    host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

    gfx::Size overflow_size(400, 400);
    EXPECT_EQ(1u, scroll_layer->test_properties()->children.size());
    LayerImpl* overflow = scroll_layer->test_properties()->children[0];
    overflow->SetBounds(overflow_size);
    overflow->SetScrollable(gfx::Size(100, 100));
    overflow->SetElementId(LayerIdToElementIdForTesting(overflow->id()));
    overflow->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(overflow->element_id(),
                                                       gfx::ScrollOffset());
    overflow->SetPosition(gfx::PointF(0, 0));

    SnapContainerData container_data(
        ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
        gfx::RectF(0, 0, 200, 200), gfx::ScrollOffset(300, 300));
    SnapAreaData area_data(ScrollSnapAlign(SnapAlignment::kStart),
                           gfx::RectF(50, 50, 100, 100), false);
    container_data.AddSnapAreaData(area_data);
    overflow->test_properties()->snap_container_data.emplace(container_data);
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    DrawFrame();

    return overflow;
  }

  void pinch_zoom_pan_viewport_forces_commit_redraw(float device_scale_factor);
  void pinch_zoom_pan_viewport_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_boundary_test(
      float device_scale_factor);

  void SetupMouseMoveAtWithDeviceScale(float device_scale_factor);

  void SetupMouseMoveAtTestScrollbarStates(bool main_thread_scrolling);

  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }

 protected:
  virtual std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() {
    return FakeLayerTreeFrameSink::Create3dForGpuRasterization();
  }

  void DrawOneFrame() {
    TestFrameData frame_data;
    host_impl_->PrepareToDraw(&frame_data);
    host_impl_->DidDrawAllLayers(frame_data);
  }

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta) {
    if (layer_impl->layer_tree_impl()
            ->property_trees()
            ->scroll_tree.SetScrollOffsetDeltaForTesting(
                layer_impl->element_id(), delta))
      layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
          layer_impl->element_id());
  }

  void BeginImplFrameAndAnimate(viz::BeginFrameArgs begin_frame_args,
                                base::TimeTicks frame_time) {
    begin_frame_args.frame_time = frame_time;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame();
  }

  void InitializeImageWorker(const LayerTreeSettings& settings) {
    if (settings.enable_checker_imaging) {
      image_worker_ = std::make_unique<base::Thread>("ImageWorker");
      ASSERT_TRUE(image_worker_->Start());
    } else {
      image_worker_.reset();
    }
  }

  FakeImplTaskRunnerProvider task_runner_provider_;
  DebugScopedSetMainThreadBlocked always_main_thread_blocked_;

  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink_;
  std::unique_ptr<LayerTreeHostImpl> host_impl_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  bool on_can_draw_state_changed_called_;
  bool did_notify_ready_to_activate_;
  bool did_request_commit_;
  bool did_request_redraw_;
  bool did_request_next_frame_;
  bool did_request_prepare_tiles_;
  bool did_complete_page_scale_animation_;
  bool reduce_memory_result_;
  bool did_request_impl_side_invalidation_;
  base::Closure animation_task_;
  base::TimeDelta requested_animation_delay_;
  std::unique_ptr<TestFrameData> last_on_draw_frame_;
  viz::RenderPassList last_on_draw_render_passes_;
  scoped_refptr<AnimationTimeline> timeline_;
  std::unique_ptr<base::Thread> image_worker_;
};

class CommitToPendingTreeLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.commit_to_active_tree = false;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
  }
};

// A test fixture for new animation timelines tests.
class LayerTreeHostImplTimelinesTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());
  }
};

class TestInputHandlerClient : public InputHandlerClient {
 public:
  TestInputHandlerClient()
      : page_scale_factor_(0.f),
        min_page_scale_factor_(-1.f),
        max_page_scale_factor_(-1.f) {}
  ~TestInputHandlerClient() override = default;

  // InputHandlerClient implementation.
  void WillShutdown() override {}
  void Animate(base::TimeTicks time) override {}
  void ReconcileElasticOverscrollAndRootScroll() override {}
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::ScrollOffset& total_scroll_offset,
      const gfx::ScrollOffset& max_scroll_offset,
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
  void DeliverInputForBeginFrame() override {}

  gfx::ScrollOffset last_set_scroll_offset() { return last_set_scroll_offset_; }

  gfx::ScrollOffset max_scroll_offset() const { return max_scroll_offset_; }

  gfx::SizeF scrollable_size() const { return scrollable_size_; }

  float page_scale_factor() const { return page_scale_factor_; }

  float min_page_scale_factor() const { return min_page_scale_factor_; }

  float max_page_scale_factor() const { return max_page_scale_factor_; }

 private:
  gfx::ScrollOffset last_set_scroll_offset_;
  gfx::ScrollOffset max_scroll_offset_;
  gfx::SizeF scrollable_size_;
  float page_scale_factor_;
  float min_page_scale_factor_;
  float max_page_scale_factor_;
};

TEST_F(LayerTreeHostImplTest, NotifyIfCanDrawChanged) {
  // Note: It is not possible to disable the renderer once it has been set,
  // so we do not need to test that disabling the renderer notifies us
  // that can_draw changed.
  EXPECT_FALSE(host_impl_->CanDraw());
  on_can_draw_state_changed_called_ = false;

  // Set up the root layer, which allows us to draw.
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the root layer to make sure it toggles can_draw
  host_impl_->active_tree()->SetRootLayerForTesting(nullptr);
  host_impl_->active_tree()->DetachLayers();
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  // Toggle the device viewport size to make sure it toggles can_draw.
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size());
  EXPECT_FALSE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));
  EXPECT_TRUE(host_impl_->CanDraw());
  EXPECT_TRUE(on_can_draw_state_changed_called_);
  on_can_draw_state_changed_called_ = false;
}

TEST_F(LayerTreeHostImplTest, ResourcelessDrawWithEmptyViewport) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_TRUE(host_impl_->CanDraw());
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size());
  EXPECT_FALSE(host_impl_->CanDraw());

  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());
  EXPECT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 0u);
  gfx::Transform identity;
  gfx::Rect viewport(100, 100);
  const bool resourceless_software_draw = true;
  host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
  ASSERT_EQ(fake_layer_tree_frame_sink->num_sent_frames(), 1u);
#if defined(OS_ANDROID)
  EXPECT_EQ(
      gfx::SizeF(100.f, 100.f),
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.root_layer_size);
#endif
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaNoLayers) {
  ASSERT_FALSE(host_impl_->active_tree()->root_layer_for_testing());

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaTreeButNoChanges) {
  {
    std::unique_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->test_properties()->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 2));
    root->test_properties()->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 3));
    root->test_properties()->children[1]->test_properties()->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 4));
    root->test_properties()->children[1]->test_properties()->AddChild(
        LayerImpl::Create(host_impl_->active_tree(), 5));
    root->test_properties()
        ->children[1]
        ->test_properties()
        ->children[0]
        ->test_properties()
        ->AddChild(LayerImpl::Create(host_impl_->active_tree(), 6));
    host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  }
  LayerImpl* root = *host_impl_->active_tree()->begin();

  ExpectClearedScrollDeltasRecursive(root);

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);

  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 0u);
  ExpectClearedScrollDeltasRecursive(root);
}

TEST_F(LayerTreeHostImplTest, ScrollDeltaRepeatedScrolls) {
  gfx::ScrollOffset scroll_offset(20, 30);
  gfx::ScrollOffset scroll_delta(11, -15);

  auto root_owned = LayerImpl::Create(host_impl_->active_tree(), 1);
  auto* root = root_owned.get();

  root->SetBounds(gfx::Size(110, 110));
  root->SetScrollable(gfx::Size(10, 10));
  root->SetElementId(LayerIdToElementIdForTesting(root->id()));
  root->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(root->element_id(),
                                                     scroll_offset);
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_owned));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  root->ScrollBy(gfx::ScrollOffsetToVector2dF(scroll_delta));
  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info, root->element_id(), scroll_delta));

  gfx::ScrollOffset scroll_delta2(-5, 27);
  root->ScrollBy(gfx::ScrollOffsetToVector2dF(scroll_delta2));
  scroll_info = host_impl_->ProcessScrollDeltas();
  ASSERT_EQ(scroll_info->scrolls.size(), 1u);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, root->element_id(),
                                 scroll_delta + scroll_delta2));

  root->ScrollBy(gfx::Vector2d());
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, root->element_id(),
                                 scroll_delta + scroll_delta2));
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       GPUMemoryForSmallLayerHistogramTest) {
  base::HistogramTester histogram_tester;
  SetClientNameForMetrics("Renderer");
  // With default tile size being set to 256 * 256, the following layer needs
  // one tile only which costs 256 * 256 * 4 / 1024 = 256KB memory.
  TestGPUMemoryForTilings(gfx::Size(200, 200));
  histogram_tester.ExpectBucketCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 256, 1);
  histogram_tester.ExpectTotalCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       GPUMemoryForLargeLayerHistogramTest) {
  base::HistogramTester histogram_tester;
  SetClientNameForMetrics("Renderer");
  // With default tile size being set to 256 * 256, the following layer needs
  // 4 tiles which cost 256 * 256 * 4 * 4 / 1024 = 1024KB memory.
  TestGPUMemoryForTilings(gfx::Size(500, 500));
  histogram_tester.ExpectBucketCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1024, 1);
  histogram_tester.ExpectTotalCount(
      "Compositing.Renderer.GPUMemoryForTilingsInKb", 1);
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeRootLayerAttached) {
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  status = host_impl_->RootScrollBegin(BeginState(gfx::Point()).get(),
                                       InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollRootCallsCommitAndRedraw) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(),
                                                      InputHandler::WHEEL));
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(0, 10),
                                                      InputHandler::WHEEL));
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(),
                                                       InputHandler::WHEEL));
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollActiveOnlyAfterScrollMovement) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  EXPECT_FALSE(host_impl_->IsActivelyScrolling());
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_TRUE(host_impl_->IsActivelyScrolling());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->IsActivelyScrolling());
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRootLayer) {
  // We should not crash when trying to scroll an empty layer tree.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutRenderer) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  gl_owned->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                GL_INNOCENT_CONTEXT_RESET_ARB);

  // Initialization will fail.
  EXPECT_FALSE(
      CreateHostImpl(DefaultSettings(),
                     FakeLayerTreeFrameSink::Create3d(std::move(gl_owned))));

  SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should not crash when trying to scroll after the renderer initialization
  // fails.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ReplaceTreeWhileScrolling) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  // We should not crash if the tree is replaced while we are scrolling.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->active_tree()->DetachLayers();

  scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // We should still be scrolling, because the scrolled layer also exists in the
  // new tree.
  gfx::ScrollOffset scroll_delta(0, 10);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(scroll_delta))
          .get());
  host_impl_->ScrollEnd(EndState().get());
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ScrollBlocksOnWheelEventHandlers) {
  LayerImpl* scroll = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  scroll->SetWheelEventHandlerRegion(Region(gfx::Rect(20, 20)));
  DrawFrame();

  // Wheel handlers determine whether mouse events block scroll.
  host_impl_->active_tree()->set_event_listener_properties(
      EventListenerClass::kMouseWheel, EventListenerProperties::kBlocking);
  EXPECT_EQ(
      EventListenerProperties::kBlocking,
      host_impl_->GetEventListenerProperties(EventListenerClass::kMouseWheel));

  // LTHI should know the wheel event handler region and only block mouse events
  // in that region.
  EXPECT_TRUE(host_impl_->HasBlockingWheelEventHandlerAt(gfx::Point(10, 10)));
  EXPECT_FALSE(host_impl_->HasBlockingWheelEventHandlerAt(gfx::Point(30, 30)));

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplTest, ScrollBlocksOnTouchEventHandlers) {
  LayerImpl* scroll = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();
  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();

  LayerImpl* child = nullptr;
  {
    std::unique_ptr<LayerImpl> child_layer =
        LayerImpl::Create(host_impl_->active_tree(), 6);
    child = child_layer.get();
    child_layer->SetDrawsContent(true);
    child_layer->SetPosition(gfx::PointF(0, 20));
    child_layer->SetBounds(gfx::Size(50, 50));
    scroll->test_properties()->AddChild(std::move(child_layer));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
  }

  // Touch handler regions determine whether touch events block scroll.
  TouchAction touch_action;
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionPanLeft, gfx::Rect(0, 0, 100, 100));
  touch_action_region.Union(kTouchActionPanRight, gfx::Rect(25, 25, 100, 100));
  root->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 10), &touch_action));
  EXPECT_EQ(kTouchActionPanLeft, touch_action);

  // But they don't influence the actual handling of the scroll gestures.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::TOUCHSCREEN);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  root->SetTouchActionRegion(TouchActionRegion());
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  EXPECT_EQ(kTouchActionAuto, touch_action);
  touch_action_region = TouchActionRegion();
  touch_action_region.Union(kTouchActionPanX, gfx::Rect(0, 0, 50, 50));
  child->SetTouchActionRegion(std::move(touch_action_region));
  EXPECT_EQ(InputHandler::TouchStartOrMoveEventListenerType::HANDLER,
            host_impl_->EventListenerTypeForTouchStartOrMoveAt(
                gfx::Point(10, 30), &touch_action));
  EXPECT_EQ(kTouchActionPanX, touch_action);
}

TEST_F(LayerTreeHostImplTest, ShouldScrollOnMainThread) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();

  root->set_main_thread_scrolling_reasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);

  status = host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                                   InputHandler::TOUCHSCREEN);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollWithOverlappingNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(false);
}

TEST_F(LayerTreeHostImplTest,
       ScrollWithOverlappingTransparentNonScrollableLayer) {
  CreateAndTestNonScrollableLayers(true);
}

TEST_F(LayerTreeHostImplTest, ScrolledOverlappingDrawnScrollbarLayer) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size content_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(345, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  host_impl_->active_tree()->SetDeviceViewportSize(content_size);
  std::unique_ptr<LayerImpl> root = LayerImpl::Create(layer_tree_impl, 1);
  root->SetBounds(content_size);
  root->SetPosition(gfx::PointF());

  std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 3);
  scroll->SetBounds(scroll_content_size);
  scroll->SetScrollable(content_size);
  scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
  scroll->SetDrawsContent(true);

  std::unique_ptr<SolidColorScrollbarLayerImpl> drawn_scrollbar =
      SolidColorScrollbarLayerImpl::Create(layer_tree_impl, 4, VERTICAL, 10, 0,
                                           false, true);
  drawn_scrollbar->SetBounds(scrollbar_size);
  drawn_scrollbar->SetPosition(gfx::PointF(345, 0));
  drawn_scrollbar->SetScrollElementId(scroll->element_id());
  drawn_scrollbar->SetDrawsContent(true);
  drawn_scrollbar->test_properties()->opacity = 1.f;

  std::unique_ptr<LayerImpl> squash = LayerImpl::Create(layer_tree_impl, 5);
  squash->SetBounds(gfx::Size(140, 300));
  squash->SetPosition(gfx::PointF(220, 0));
  squash->SetDrawsContent(true);

  scroll->test_properties()->AddChild(std::move(drawn_scrollbar));
  scroll->test_properties()->AddChild(std::move(squash));
  root->test_properties()->AddChild(std::move(scroll));

  layer_tree_impl->SetRootLayerForTesting(std::move(root));
  layer_tree_impl->BuildPropertyTreesForTesting();
  layer_tree_impl->DidBecomeActive();

  // The point hits squash layer and also scrollbar layer, but because the
  // scrollbar layer is a drawn scrollbar, we cannot scroll on the impl thread.
  auto status = host_impl_->ScrollBegin(BeginState(gfx::Point(350, 150)).get(),
                                        InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);

  // The point hits the drawn scrollbar layer completely and should not scroll
  // on the impl thread.
  status = host_impl_->ScrollBegin(BeginState(gfx::Point(350, 500)).get(),
                                   InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionBasic) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  outer_scroll->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  // All scroll types inside the non-fast scrollable region should fail.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(25, 25)).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(25, 25),
                                                       InputHandler::WHEEL));

  status = host_impl_->ScrollBegin(BeginState(gfx::Point(25, 25)).get(),
                                   InputHandler::TOUCHSCREEN);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(25, 25), InputHandler::TOUCHSCREEN));

  // All scroll types outside this region should succeed.
  status = host_impl_->ScrollBegin(BeginState(gfx::Point(75, 75)).get(),
                                   InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(75, 75), InputHandler::TOUCHSCREEN));
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(25, 25), InputHandler::TOUCHSCREEN));
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(75, 75), InputHandler::TOUCHSCREEN));

  status = host_impl_->ScrollBegin(BeginState(gfx::Point(75, 75)).get(),
                                   InputHandler::TOUCHSCREEN);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);
  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(75, 75), InputHandler::TOUCHSCREEN));
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(
      gfx::Point(75, 75), InputHandler::TOUCHSCREEN));
}

TEST_F(LayerTreeHostImplTest, NonFastScrollableRegionWithOffset) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  outer_scroll->SetNonFastScrollableRegion(gfx::Rect(0, 0, 50, 50));
  outer_scroll->SetPosition(gfx::PointF(-25.f, 0.f));
  outer_scroll->SetDrawsContent(true);

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  // This point would fall into the non-fast scrollable region except that we've
  // moved the layer left by 25 pixels.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(40, 10)).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(40, 10),
                                                      InputHandler::WHEEL));
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 1)).get());
  host_impl_->ScrollEnd(EndState().get());

  // This point is still inside the non-fast region.
  status = host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10)).get(),
                                   InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNonFastScrollableRegion,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollHandlerNotPresent) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  EXPECT_FALSE(host_impl_->active_tree()->have_scroll_event_handlers());
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
}

TEST_F(LayerTreeHostImplTest, ScrollHandlerPresent) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->set_have_scroll_event_handlers(true);
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  EXPECT_TRUE(host_impl_->scroll_affects_scroll_handler());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(host_impl_->scroll_affects_scroll_handler());
}

TEST_F(LayerTreeHostImplTest, ScrollByReturnsCorrectValue) {
  SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::TOUCHSCREEN);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            status.main_thread_scrolling_reasons);

  // Trying to scroll to the left/top will not succeed.
  EXPECT_FALSE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0)).get())
          .did_scroll);
  EXPECT_FALSE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, -10)).get())
          .did_scroll);
  EXPECT_FALSE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10)).get())
          .did_scroll);

  // Scrolling to the right/bottom will succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(10, 0)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(10, 10)).get())
          .did_scroll);

  // Scrolling to left/top will now succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, -10)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, -10)).get())
          .did_scroll);

  // Scrolling diagonally against an edge will succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(10, -10)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, 0)).get())
          .did_scroll);
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(-10, 10)).get())
          .did_scroll);

  // Trying to scroll more than the available space will also succeed.
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(5000, 5000)).get())
          .did_scroll);
}

// TODO(sunyunjia): Move scroll snap tests to a separate file.
// https://crbug.com/851690
TEST_F(LayerTreeHostImplTest, ScrollSnapOnX) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  gfx::Vector2dF x_delta(20, 0);
  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(EndState().get(), true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 0), overflow->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ScrollSnapOnY) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  gfx::Vector2dF y_delta(0, 20);
  host_impl_->ScrollBy(UpdateState(pointer_position, y_delta).get());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(EndState().get(), true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 50), overflow->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ScrollSnapOnBoth) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  gfx::Vector2dF delta(20, 20);
  host_impl_->ScrollBy(UpdateState(pointer_position, delta).get());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(EndState().get(), true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50), overflow->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ScrollSnapAfterAnimatedScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  gfx::Vector2dF delta(20, 20);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_->ScrollAnimated(pointer_position, delta).thread);

  EXPECT_EQ(overflow->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the wheel scroll.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(50));
  EXPECT_FALSE(host_impl_->is_animating_for_snap_for_testing());
  gfx::ScrollOffset current_offset = overflow->CurrentScrollOffset();
  EXPECT_LT(0, current_offset.x());
  EXPECT_GT(20, current_offset.x());
  EXPECT_LT(0, current_offset.y());
  EXPECT_GT(20, current_offset.y());

  // Animating for the snap.
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1000));
  EXPECT_TRUE(host_impl_->is_animating_for_snap_for_testing());

  // Finish the animation.
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1500));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50), overflow->CurrentScrollOffset());
  EXPECT_FALSE(host_impl_->is_animating_for_snap_for_testing());
}

TEST_F(LayerTreeHostImplTest, SnapAnimationCancelledByScroll) {
  LayerImpl* overflow = CreateLayerForSnapping();

  gfx::Point pointer_position(10, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  gfx::Vector2dF x_delta(20, 0);
  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());
  EXPECT_FALSE(host_impl_->is_animating_for_snap_for_testing());

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->ScrollEnd(EndState().get(), true);
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  BeginImplFrameAndAnimate(begin_frame_args, start_time);

  // Animating for the snap.
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(100));
  EXPECT_TRUE(host_impl_->is_animating_for_snap_for_testing());
  gfx::ScrollOffset current_offset = overflow->CurrentScrollOffset();
  EXPECT_GT(50, current_offset.x());
  EXPECT_LT(20, current_offset.x());
  EXPECT_EQ(0, current_offset.y());

  // Interrup the snap animation with ScrollBegin.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_FALSE(host_impl_->is_animating_for_snap_for_testing());
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(150));
  EXPECT_VECTOR_EQ(gfx::ScrollOffsetToVector2dF(current_offset),
                   overflow->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, GetSnapFlingInfoWhenZoomed) {
  LayerImpl* overflow = CreateLayerForSnapping();
  // Scales the page to its 1/5.
  host_impl_->active_tree()->PushPageScaleFromMainThread(0.2f, 0.1f, 5.f);

  // Should be (10, 10) in the scroller's coordinate.
  gfx::Point pointer_position(2, 2);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // Should be (20, 20) in the scroller's coordinate.
  gfx::Vector2dF delta(4, 4);
  InputHandlerScrollResult result =
      host_impl_->ScrollBy(UpdateState(pointer_position, delta).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), overflow->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), result.current_visual_offset);

  gfx::Vector2dF initial_offset, target_offset;
  EXPECT_TRUE(host_impl_->GetSnapFlingInfo(gfx::Vector2dF(10, 10),
                                           &initial_offset, &target_offset));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(4, 4), initial_offset);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), target_offset);
}

TEST_F(LayerTreeHostImplTest, OverscrollBehaviorPreventsPropagation) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  gfx::Size overflow_size(400, 400);
  ASSERT_EQ(1u, scroll_layer->test_properties()->children.size());
  LayerImpl* overflow = scroll_layer->test_properties()->children[0];
  overflow->SetBounds(overflow_size);
  overflow->SetScrollable(gfx::Size(100, 100));
  overflow->SetElementId(LayerIdToElementIdForTesting(overflow->id()));
  overflow->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(overflow->element_id(),
                                                     gfx::ScrollOffset());
  overflow->SetPosition(gfx::PointF(40, 40));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset(30, 30));

  DrawFrame();
  gfx::Point pointer_position(50, 50);

  // OverscrollBehaviorTypeAuto shouldn't prevent scroll propagation.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(30, 30), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), overflow->CurrentScrollOffset());

  gfx::Vector2dF x_delta(-10, 0);
  gfx::Vector2dF y_delta(0, -10);
  gfx::Vector2dF diagonal_delta(-10, -10);
  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  overflow->test_properties()->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeContain,
                         OverscrollBehavior::kOverscrollBehaviorTypeAuto);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // OverscrollBehaviorContain on x should prevent propagations of scroll
  // on x.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // OverscrollBehaviorContain on x shouldn't prevent propagations of
  // scroll on y.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 30), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, y_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, diagonal_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // Changing scroll-boundary-behavior to y axis.
  overflow->test_properties()->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeAuto,
                         OverscrollBehavior::kOverscrollBehaviorTypeContain);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // OverscrollBehaviorContain on y shouldn't prevent propagations of
  // scroll on x.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(20, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // OverscrollBehaviorContain on y should prevent propagations of scroll
  // on y.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, y_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // A scroll update with both x & y delta will adhere to the most restrictive
  // case.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, diagonal_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  // Gesture scroll should latch to the first scroller that has non-auto
  // overscroll-behavior.
  overflow->test_properties()->overscroll_behavior =
      OverscrollBehavior(OverscrollBehavior::kOverscrollBehaviorTypeContain,
                         OverscrollBehavior::kOverscrollBehaviorTypeContain);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(pointer_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(pointer_position, x_delta).get());
  host_impl_->ScrollBy(UpdateState(pointer_position, -x_delta).get());
  host_impl_->ScrollBy(UpdateState(pointer_position, y_delta).get());
  host_impl_->ScrollBy(UpdateState(pointer_position, -y_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), overflow->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ScrollWithUserUnscrollableLayers) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(200, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  gfx::Size overflow_size(400, 400);
  ASSERT_EQ(1u, scroll_layer->test_properties()->children.size());
  LayerImpl* overflow = scroll_layer->test_properties()->children[0];
  overflow->SetBounds(overflow_size);
  overflow->SetScrollable(gfx::Size(100, 100));
  overflow->SetElementId(LayerIdToElementIdForTesting(overflow->id()));
  overflow->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(overflow->element_id(),
                                                     gfx::ScrollOffset());
  overflow->SetPosition(gfx::PointF());
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();
  gfx::Point scroll_position(10, 10);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(scroll_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), overflow->CurrentScrollOffset());

  gfx::Vector2dF scroll_delta(10, 10);
  host_impl_->ScrollBy(UpdateState(scroll_position, scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), overflow->CurrentScrollOffset());

  overflow->test_properties()->user_scrollable_horizontal = false;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(scroll_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(scroll_position, scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->CurrentScrollOffset());

  overflow->test_properties()->user_scrollable_vertical = false;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(scroll_position).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->CurrentScrollOffset());

  host_impl_->ScrollBy(UpdateState(scroll_position, scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 10), scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(10, 20), overflow->CurrentScrollOffset());
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       AnimationSchedulingPendingTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));

  CreatePendingTree();
  auto root_owned = LayerImpl::Create(host_impl_->pending_tree(), 1);
  auto* root = root_owned.get();
  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(root_owned));
  root->SetBounds(gfx::Size(50, 50));
  root->test_properties()->force_render_surface = true;
  root->SetNeedsPushProperties();

  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->pending_tree(), 2));
  LayerImpl* child = root->test_properties()->children[0];
  child->SetBounds(gfx::Size(10, 10));
  child->draw_properties().visible_layer_rect = gfx::Rect(10, 10);
  child->SetDrawsContent(true);
  child->SetNeedsPushProperties();

  host_impl_->pending_tree()->SetElementIdsForTesting();

  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 3, 0);
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();

  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  host_impl_->AnimatePendingTreeAfterCommit();

  // An animation exists on the pending layer. Doing
  // AnimatePendingTreeAfterCommit() requests another frame.
  // In reality, animations without has_set_start_time() == true do not need to
  // be continuously ticked on the pending tree, so it should not request
  // another animation frame here. But we currently do so blindly if any
  // animation exists.
  EXPECT_TRUE(did_request_next_frame_);
  // The pending tree with an animation does not need to draw after animating.
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  host_impl_->ActivateSyncTree();

  // When the animation activates, we should request another animation frame
  // to keep the animation moving.
  EXPECT_TRUE(did_request_next_frame_);
  // On activation we don't need to request a redraw for the animation,
  // activating will draw on its own when it's ready.
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       AnimationSchedulingActiveTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));

  host_impl_->active_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl_->active_tree(), 1));
  LayerImpl* root = *host_impl_->active_tree()->begin();
  root->SetBounds(gfx::Size(50, 50));
  root->test_properties()->force_render_surface = true;

  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->active_tree(), 2));
  LayerImpl* child = root->test_properties()->children[0];
  child->SetBounds(gfx::Size(10, 10));
  child->draw_properties().visible_layer_rect = gfx::Rect(10, 10);
  child->SetDrawsContent(true);
  host_impl_->active_tree()->SetElementIdsForTesting();

  // Add a translate from 6,7 to 8,9.
  TransformOperations start;
  start.AppendTranslate(6.f, 7.f, 0.f);
  TransformOperations end;
  end.AppendTranslate(8.f, 9.f, 0.f);
  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             4.0, start, end);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  base::TimeTicks now = base::TimeTicks::Now();
  host_impl_->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, now));

  // TODO(crbug.com/551134): We always request a new frame and a draw for
  // animations that are on the pending tree, but we don't need to do that
  // unless they are waiting for some future time to start.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  host_impl_->ActivateAnimations();

  // On activating an animation, we should request another frame so that we'll
  // continue ticking the animation.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
  did_request_next_frame_ = false;
  did_request_redraw_ = false;
  did_request_commit_ = false;

  // The next frame after activating, we'll tick the animation again.
  host_impl_->Animate();

  // An animation exists on the active layer. Doing Animate() requests another
  // frame after the current one.
  EXPECT_TRUE(did_request_next_frame_);
  // The animation should cause us to draw at the frame's deadline.
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, AnimationSchedulingCommitToActiveTree) {
  FakeImplTaskRunnerProvider provider(nullptr);
  CreateHostImplWithTaskRunnerProvider(DefaultSettings(),
                                       CreateLayerTreeFrameSink(), &provider);
  EXPECT_TRUE(host_impl_->CommitToActiveTree());

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));

  auto root_owned = LayerImpl::Create(host_impl_->active_tree(), 1);
  auto* root = root_owned.get();
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_owned));
  root->SetBounds(gfx::Size(50, 50));

  auto child_owned = LayerImpl::Create(host_impl_->active_tree(), 2);
  auto* child = child_owned.get();
  root->test_properties()->AddChild(std::move(child_owned));
  child->SetBounds(gfx::Size(10, 10));
  child->draw_properties().visible_layer_rect = gfx::Rect(10, 10);
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();

  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             10.0, 3, 0);

  // Set up the property trees so that UpdateDrawProperties will work in
  // CommitComplete below.
  RenderSurfaceList list;
  LayerTreeHostCommon::CalcDrawPropsImplInputsForTesting inputs(
      root, gfx::Size(50, 50), &list);
  LayerTreeHostCommon::CalculateDrawPropertiesForTesting(&inputs);

  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  host_impl_->CommitComplete();

  // Animations on the active tree should be started and ticked, and a new frame
  // should be requested to continue ticking them.
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  // Delete the LayerTreeHostImpl before the TaskRunnerProvider goes away.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, AnimationSchedulingOnLayerDestruction) {
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));

  host_impl_->active_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl_->active_tree(), 1));
  LayerImpl* root = *host_impl_->active_tree()->begin();
  root->SetBounds(gfx::Size(50, 50));

  root->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->active_tree(), 2));
  LayerImpl* child = root->test_properties()->children[0];
  child->SetBounds(gfx::Size(10, 10));
  child->draw_properties().visible_layer_rect = gfx::Rect(10, 10);
  child->SetDrawsContent(true);

  host_impl_->active_tree()->SetElementIdsForTesting();

  // Add a translate animation.
  TransformOperations start;
  start.AppendTranslate(6.f, 7.f, 0.f);
  TransformOperations end;
  end.AppendTranslate(8.f, 9.f, 0.f);
  AddAnimatedTransformToElementWithAnimation(child->element_id(), timeline(),
                                             4.0, start, end);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

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

  // Destroy layer, unregister animation target (element).
  child->test_properties()->parent = nullptr;
  root->test_properties()->RemoveChild(child);
  child = nullptr;

  // Doing Animate() doesn't request another frame after the current one.
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);

  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
}

class MissingTilesLayer : public LayerImpl {
 public:
  MissingTilesLayer(LayerTreeImpl* layer_tree_impl, int id)
      : LayerImpl(layer_tree_impl, id), has_missing_tiles_(true) {}

  void set_has_missing_tiles(bool has_missing_tiles) {
    has_missing_tiles_ = has_missing_tiles;
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    append_quads_data->num_missing_tiles += has_missing_tiles_;
  }

 private:
  bool has_missing_tiles_;
};

TEST_F(LayerTreeHostImplTest, ImplPinchZoom) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  EXPECT_EQ(scroll_layer, host_impl_->InnerViewportScrollLayer());
  LayerImpl* container_layer = host_impl_->InnerViewportContainerLayer();
  EXPECT_EQ(gfx::Size(50, 50), container_layer->bounds());

  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 1.f;

  // The impl-based pinch zoom should adjust the max scroll position.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2.f;

    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);
    EXPECT_EQ(gfx::Size(50, 50), container_layer->bounds());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);

    EXPECT_EQ(gfx::ScrollOffset(75.0, 75.0).ToString(),
              scroll_layer->MaxScrollOffset().ToString());
  }

  // Scrolling after a pinch gesture should always be in local space.  The
  // scroll deltas have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd(EndState().get());

    gfx::Vector2d scroll_delta(0, 10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::WHEEL)
                  .thread);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(
        *scroll_info.get(), scroll_layer->element_id(),
        gfx::ScrollOffset(0, scroll_delta.y() / page_scale_delta)));
  }
}

TEST_F(LayerTreeHostImplTest, ViewportScrollbarGeometry) {
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

  SolidColorScrollbarLayerImpl* v_scrollbar;
  SolidColorScrollbarLayerImpl* h_scrollbar;

  // Setup
  {
    LayerTreeSettings settings = DefaultSettings();
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    LayerTreeImpl* active_tree = host_impl_->active_tree();
    active_tree->PushPageScaleFromMainThread(1.f, minimum_scale, 4.f);

    CreateBasicVirtualViewportLayers(viewport_size, content_size);

    // When Chrome on Android loads a non-mobile page, it resizes the main
    // frame (outer viewport) such that it matches the width of the content,
    // preventing horizontal scrolling. Replicate that behavior here.
    host_impl_->OuterViewportScrollLayer()->SetScrollable(outer_viewport_size);
    LayerImpl* outer_clip =
        host_impl_->OuterViewportScrollLayer()->test_properties()->parent;
    outer_clip->SetBounds(outer_viewport_size);

    // Add scrollbars. They will always exist - even if unscrollable - but their
    // visibility will be determined by whether the content can be scrolled.
    {
      std::unique_ptr<SolidColorScrollbarLayerImpl> v_scrollbar_unique =
          SolidColorScrollbarLayerImpl::Create(active_tree, 400, VERTICAL, 10,
                                               0, false, true);
      std::unique_ptr<SolidColorScrollbarLayerImpl> h_scrollbar_unique =
          SolidColorScrollbarLayerImpl::Create(active_tree, 401, HORIZONTAL, 10,
                                               0, false, true);
      v_scrollbar = v_scrollbar_unique.get();
      h_scrollbar = h_scrollbar_unique.get();

      LayerImpl* scroll = active_tree->OuterViewportScrollLayer();
      LayerImpl* root = active_tree->InnerViewportContainerLayer();
      v_scrollbar_unique->SetScrollElementId(scroll->element_id());
      h_scrollbar_unique->SetScrollElementId(scroll->element_id());
      root->test_properties()->AddChild(std::move(v_scrollbar_unique));
      root->test_properties()->AddChild(std::move(h_scrollbar_unique));
    }

    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    host_impl_->active_tree()->DidBecomeActive();
  }

  // Zoom out to the minimum scale. The scrollbars shoud not be scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(0.f);
  EXPECT_FALSE(v_scrollbar->CanScrollOrientation());
  EXPECT_FALSE(h_scrollbar->CanScrollOrientation());

  // Zoom in a little and confirm that they're now scrollable.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(minimum_scale * 1.05f);
  EXPECT_TRUE(v_scrollbar->CanScrollOrientation());
  EXPECT_TRUE(h_scrollbar->CanScrollOrientation());
}

TEST_F(LayerTreeHostImplTest, ViewportScrollOrder) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.25f, 4.f);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   outer_scroll_layer->MaxScrollOffset());

  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2.f, gfx::Point(0, 0));
  host_impl_->PinchGestureEnd(gfx::Point(0, 0), true);
  host_impl_->ScrollEnd(EndState().get());

  // Sanity check - we're zoomed in, starting from the origin.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   inner_scroll_layer->CurrentScrollOffset());

  // Scroll down - only the inner viewport should scroll.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());

  // Scroll down - outer viewport should start scrolling after the inner is at
  // its maximum.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000.f, 1000.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(250, 250),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(300, 300),
                   outer_scroll_layer->CurrentScrollOffset());
}

// Make sure scrolls smaller than a unit applied to the viewport don't get
// dropped. crbug.com/539334.
TEST_F(LayerTreeHostImplTest, ScrollViewportWithFractionalAmounts) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Sanity checks.
  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   outer_scroll_layer->MaxScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), inner_scroll_layer->CurrentScrollOffset());

  // Scroll only the layout viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(0.125f, 0.125f)).get());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.125f, 0.125f),
                      outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0, 0),
                      inner_scroll_layer->CurrentScrollOffset());
  host_impl_->ScrollEnd(EndState().get());

  host_impl_->active_tree()->PushPageScaleFromMainThread(2.f, 1.f, 2.f);

  // Now that we zoomed in, the scroll should be applied to the inner viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(0.5f, 0.5f)).get());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.125f, 0.125f),
                      outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.25f, 0.25f),
                      inner_scroll_layer->CurrentScrollOffset());
  host_impl_->ScrollEnd(EndState().get());
}

// Tests that scrolls during a pinch gesture (i.e. "two-finger" scrolls) work
// as expected. That is, scrolling during a pinch should bubble from the inner
// to the outer viewport.
TEST_F(LayerTreeHostImplTest, ScrollDuringPinchGesture) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  outer_scroll_layer->SetDrawsContent(true);
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();
  inner_scroll_layer->SetDrawsContent(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_VECTOR_EQ(gfx::Vector2dF(500, 500),
                   outer_scroll_layer->MaxScrollOffset());

  host_impl_->ScrollBegin(BeginState(gfx::Point(250, 250)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();

  host_impl_->PinchGestureUpdate(2, gfx::Point(250, 250));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(125, 125),
                   inner_scroll_layer->CurrentScrollOffset());

  // Needed so that the pinch is accounted for in draw properties.
  DrawFrame();

  host_impl_->ScrollBy(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(10.f, 10.f)).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(130, 130),
                   inner_scroll_layer->CurrentScrollOffset());

  DrawFrame();

  host_impl_->ScrollBy(
      UpdateState(gfx::Point(250, 250), gfx::Vector2dF(400.f, 400.f)).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(80, 80),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(250, 250),
                   inner_scroll_layer->CurrentScrollOffset());

  host_impl_->PinchGestureEnd(gfx::Point(250, 250), true);
  host_impl_->ScrollEnd(EndState().get());
}

// Tests the "snapping" of pinch-zoom gestures to the screen edge. That is, when
// a pinch zoom is anchored within a certain margin of the screen edge, we
// should assume the user means to scroll into the edge of the screen.
TEST_F(LayerTreeHostImplTest, PinchZoomSnapsToScreenEdge) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);

  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  int offsetFromEdge = Viewport::kPinchZoomSnapMarginDips - 5;
  gfx::Point anchor(viewport_size.width() - offsetFromEdge,
                    viewport_size.height() - offsetFromEdge);

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // bottom and right.
  host_impl_->ScrollBegin(BeginState(anchor).get(), InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(250, 250),
      host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1.f);
  SetScrollOffsetDelta(host_impl_->InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(host_impl_->OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in within the margins. The scroll should stay exactly locked to the
  // top and left.
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  host_impl_->ScrollBegin(BeginState(anchor).get(), InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(0, 0),
      host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1.f);
  SetScrollOffsetDelta(host_impl_->InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(host_impl_->OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(offsetFromEdge, offsetFromEdge);
  host_impl_->ScrollBegin(BeginState(anchor).get(), InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(50, 50),
      host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());

  // Reset.
  host_impl_->active_tree()->SetPageScaleOnActiveTree(1.f);
  SetScrollOffsetDelta(host_impl_->InnerViewportScrollLayer(), gfx::Vector2d());
  SetScrollOffsetDelta(host_impl_->OuterViewportScrollLayer(), gfx::Vector2d());

  // Pinch in just outside the margin. There should be no snapping.
  offsetFromEdge = Viewport::kPinchZoomSnapMarginDips;
  anchor = gfx::Point(viewport_size.width() - offsetFromEdge,
                      viewport_size.height() - offsetFromEdge);
  host_impl_->ScrollBegin(BeginState(anchor).get(), InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2, anchor);
  host_impl_->PinchGestureEnd(anchor, true);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(200, 200),
      host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ImplPinchZoomWheelBubbleBetweenViewports) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 2.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                          InputHandler::WHEEL);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(10.f, 20.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), outer_scroll_layer->CurrentScrollOffset());

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                          InputHandler::WHEEL);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   outer_scroll_layer->CurrentScrollOffset());

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                          InputHandler::WHEEL);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, 0), gfx::Vector2dF(190.f, 180.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 100),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, ScrollWithSwapPromises) {
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));

  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  host_impl_->QueueSwapPromiseForMainThreadScrollUpdate(
      std::move(swap_promise));
  host_impl_->ScrollEnd(EndState().get());

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(1u, scroll_info->swap_promises.size());
  EXPECT_EQ(latency_info.trace_id(), scroll_info->swap_promises[0]->TraceId());
}

// Test that scrolls targeting a layer with a non-null scroll_parent() don't
// bubble up.
TEST_F(LayerTreeHostImplTest, ScrollDoesntBubble) {
  LayerImpl* viewport_scroll =
      SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));

  // Set up two scrolling children of the root, one of which is a scroll parent
  // to the other. Scrolls shouldn't bubbling from the child.
  LayerImpl* parent;
  LayerImpl* child;
  LayerImpl* child_clip;

  std::unique_ptr<LayerImpl> scroll_parent =
      CreateScrollableLayer(7, gfx::Size(10, 10));
  parent = scroll_parent.get();
  viewport_scroll->test_properties()->AddChild(std::move(scroll_parent));

  std::unique_ptr<LayerImpl> scroll_child_clip =
      LayerImpl::Create(host_impl_->active_tree(), 8);
  std::unique_ptr<LayerImpl> scroll_child =
      CreateScrollableLayer(9, gfx::Size(10, 10));
  child = scroll_child.get();
  scroll_child->SetPosition(gfx::PointF(20.f, 20.f));
  scroll_child_clip->test_properties()->AddChild(std::move(scroll_child));

  child_clip = scroll_child_clip.get();
  viewport_scroll->test_properties()->AddChild(std::move(scroll_child_clip));

  child_clip->test_properties()->scroll_parent = parent;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(21, 21)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(5, 5)).get());
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(100, 100)).get());
    host_impl_->ScrollEnd(EndState().get());

    // The child should be fully scrolled by the first ScrollBy.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 5), child->CurrentScrollOffset());

    // The scroll_parent shouldn't receive the second ScrollBy.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), parent->CurrentScrollOffset());

    // The viewport shouldn't have been scrolled at all.
    EXPECT_VECTOR_EQ(
        gfx::Vector2dF(0, 0),
        host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(
        gfx::Vector2dF(0, 0),
        host_impl_->OuterViewportScrollLayer()->CurrentScrollOffset());
  }

  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(21, 21)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(3, 4)).get());
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1)).get());
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1)).get());
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(21, 21), gfx::Vector2d(2, 1)).get());
    host_impl_->ScrollEnd(EndState().get());

    // The ScrollBy's should scroll the parent to its extent.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 5), parent->CurrentScrollOffset());

    // The viewport shouldn't receive any scroll delta.
    EXPECT_VECTOR_EQ(
        gfx::Vector2dF(0, 0),
        host_impl_->InnerViewportScrollLayer()->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(
        gfx::Vector2dF(0, 0),
        host_impl_->OuterViewportScrollLayer()->CurrentScrollOffset());
  }
}

TEST_F(LayerTreeHostImplTest, PinchGesture) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1.f;
  float max_page_scale = 4.f;

  // Basic pinch zoom in gesture
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  }

  // Zoom-in clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    float page_scale_delta = 10.f;

    host_impl_->ScrollBegin(BeginState(gfx::Point(50, 50)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, max_page_scale);
  }

  // Zoom-out clamping
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    float page_scale_delta = 0.1f;
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);

    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should not happen based on pinch events only
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd(gfx::Point(20, 20), true);
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(scroll_info->scrolls.empty());
  }

  // Two-finger panning should work with interleaved scroll events
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(20, 20));

    float page_scale_delta = 1.f;
    host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(10, 10), gfx::Vector2d(-10, -10)).get());
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(20, 20));
    host_impl_->PinchGestureEnd(gfx::Point(20, 20), true);
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-10, -10)));
  }

  // Two-finger panning should work when starting fully zoomed out.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(0.5f, 0.5f, 4.f);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.CollectScrollDeltasForTesting();
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(0, 0));

    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(2.f, gfx::Point(0, 0));
    host_impl_->PinchGestureUpdate(1.f, gfx::Point(0, 0));

    // Needed so layer transform includes page scale.
    DrawFrame();

    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2d(10, 10)).get());
    host_impl_->PinchGestureUpdate(1.f, gfx::Point(10, 10));
    host_impl_->PinchGestureEnd(gfx::Point(10, 10), true);
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2.f);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(10, 10)));
  }
}

TEST_F(LayerTreeHostImplTest, SyncSubpixelScrollDelta) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 1.f;
  float max_page_scale = 4.f;

  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                         max_page_scale);
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.CollectScrollDeltasForTesting();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 20));

  float page_scale_delta = 1.f;
  host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10)).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 10));
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(10, 10), gfx::Vector2dF(0, -1.001f)).get());
  host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(10, 9));
  host_impl_->PinchGestureEnd(gfx::Point(10, 9), true);
  host_impl_->ScrollEnd(EndState().get());

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 gfx::ScrollOffset(0, -1)));

  // Verify this scroll delta is consistent with the snapped position of the
  // scroll layer.
  draw_property_utils::ComputeTransforms(
      &scroll_layer->layer_tree_impl()->property_trees()->transform_tree);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, -19.f),
                   scroll_layer->ScreenSpaceTransform().To2dTranslation());
}

TEST_F(LayerTreeHostImplTest, SyncSubpixelScrollFromFractionalActiveBase) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.CollectScrollDeltasForTesting();
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scroll_layer->element_id(), gfx::ScrollOffset(0, 20.5f));

  host_impl_->ScrollBegin(BeginState(gfx::Point(10, 10)).get(),
                          InputHandler::WHEEL);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(10, 10), gfx::Vector2dF(0, -1)).get());
  host_impl_->ScrollEnd(EndState().get());

  gfx::ScrollOffset active_base =
      host_impl_->active_tree()
          ->property_trees()
          ->scroll_tree.GetScrollOffsetBaseForTesting(
              scroll_layer->element_id());
  EXPECT_VECTOR_EQ(active_base, gfx::Vector2dF(0, 20.5));
  // Fractional active base should not affect the scroll delta.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_VECTOR_EQ(scroll_info->inner_viewport_scroll.scroll_delta,
                   gfx::Vector2dF(0, -1));
}

TEST_F(LayerTreeHostImplTest, PinchZoomTriggersPageScaleAnimation) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  float min_page_scale = 1.f;
  float max_page_scale = 4.f;
  float page_scale_delta = 1.04f;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(200);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Zoom animation if page_scale is < 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 1.f);
  }

  start_time += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);
  page_scale_delta = 1.06f;

  // No zoom animation if page_scale is >= 1.05 * min_page_scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point(50, 50));
    host_impl_->PinchGestureEnd(gfx::Point(50, 50), true);
    host_impl_->ActivateSyncTree();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_FALSE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, page_scale_delta);
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimation) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4.f;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Non-anchor zoom-in
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(), false, 2.f,
                                          duration)));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    EXPECT_FALSE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 2);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-50, -50)));
  }

  start_time += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);

  // Anchor zoom-out
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(25, 25), true,
                                          min_page_scale, duration)));
    host_impl_->ActivateSyncTree();
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);

    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_next_frame_);
    host_impl_->DidFinishImplFrame();

    did_request_redraw_ = false;
    did_request_commit_ = false;
    did_request_next_frame_ = false;
    begin_frame_args.frame_time = end_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_commit_);
    host_impl_->DidFinishImplFrame();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, min_page_scale);
    // Pushed to (0,0) via clamping against contents layer size.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                   gfx::ScrollOffset(-50, -50)));
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationNoOp) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4.f;
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Anchor zoom with unchanged page scale should not change scroll or scale.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                           max_page_scale);
    scroll_layer->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            scroll_layer->element_id(), gfx::ScrollOffset(50, 50));

    host_impl_->active_tree()->SetPendingPageScaleAnimation(
        std::unique_ptr<PendingPageScaleAnimation>(
            new PendingPageScaleAnimation(gfx::Vector2d(), true, 1.f,
                                          duration)));
    host_impl_->ActivateSyncTree();
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->DidFinishImplFrame();

    begin_frame_args.frame_time = halfway_through_animation;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_redraw_);
    host_impl_->DidFinishImplFrame();

    begin_frame_args.frame_time = end_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_TRUE(did_request_commit_);
    host_impl_->DidFinishImplFrame();

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_EQ(scroll_info->page_scale_delta, 1);
    ExpectNone(*scroll_info, scroll_layer->element_id());
  }
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationTransferedOnSyncTreeActivate) {
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  CreateScrollAndContentsLayers(host_impl_->pending_tree(),
                                gfx::Size(100, 100));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  float min_page_scale = 0.5f;
  float max_page_scale = 4.f;
  host_impl_->sync_tree()->PushPageScaleFromMainThread(1.f, min_page_scale,
                                                       max_page_scale);
  host_impl_->ActivateSyncTree();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks third_through_animation = start_time + duration / 3;
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;
  float target_scale = 2.f;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(50, 50));

  // Make sure TakePageScaleAnimation works properly.

  host_impl_->sync_tree()->SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation>(new PendingPageScaleAnimation(
          gfx::Vector2d(), false, target_scale, duration)));
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
      std::unique_ptr<PendingPageScaleAnimation>(new PendingPageScaleAnimation(
          gfx::Vector2d(), false, target_scale, duration)));
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_next_frame_);
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame();

  // Activate the sync tree. This should cause the animation to become enabled.
  // It should also clear the pointer on the sync tree.
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(nullptr,
            host_impl_->sync_tree()->TakePendingPageScaleAnimation().get());
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);

  start_time += base::TimeDelta::FromSeconds(10);
  third_through_animation += base::TimeDelta::FromSeconds(10);
  halfway_through_animation += base::TimeDelta::FromSeconds(10);
  end_time += base::TimeDelta::FromSeconds(10);

  // From here on, make sure the animation runs as normal.
  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame();

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = third_through_animation;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame();

  // Another activation shouldn't have any effect on the animation.
  host_impl_->ActivateSyncTree();

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame();

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;
  begin_frame_args.frame_time = end_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_request_commit_);
  EXPECT_FALSE(did_request_next_frame_);
  host_impl_->DidFinishImplFrame();

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_EQ(scroll_info->page_scale_delta, target_scale);
  EXPECT_TRUE(ScrollInfoContains(*scroll_info, scroll_layer->element_id(),
                                 gfx::ScrollOffset(-50, -50)));
}

TEST_F(LayerTreeHostImplTest, PageScaleAnimationCompletedNotification) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  LayerImpl* scroll_layer = host_impl_->InnerViewportScrollLayer();
  DCHECK(scroll_layer);

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(100);
  base::TimeTicks halfway_through_animation = start_time + duration / 2;
  base::TimeTicks end_time = start_time + duration;

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(50, 50));

  did_complete_page_scale_animation_ = false;
  host_impl_->active_tree()->SetPendingPageScaleAnimation(
      std::unique_ptr<PendingPageScaleAnimation>(new PendingPageScaleAnimation(
          gfx::Vector2d(), false, 2.f, duration)));
  host_impl_->ActivateSyncTree();
  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time = halfway_through_animation;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time = end_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_TRUE(did_complete_page_scale_animation_);
  host_impl_->DidFinishImplFrame();
}

TEST_F(LayerTreeHostImplTest, MaxScrollOffsetAffectedByViewportBoundsDelta) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();
  LayerImpl* inner_container = host_impl_->InnerViewportContainerLayer();
  DCHECK(inner_scroll);
  DCHECK(inner_container);
  EXPECT_EQ(gfx::ScrollOffset(50, 50), inner_scroll->MaxScrollOffset());

  inner_container->SetViewportBoundsDelta(gfx::Vector2dF(15.f, 15.f));
  inner_scroll->SetViewportBoundsDelta(gfx::Vector2dF(7.f, 7.f));
  EXPECT_EQ(gfx::ScrollOffset(42, 42), inner_scroll->MaxScrollOffset());

  inner_container->SetViewportBoundsDelta(gfx::Vector2dF());
  inner_scroll->SetViewportBoundsDelta(gfx::Vector2dF());
  inner_scroll->SetBounds(gfx::Size());
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  DrawFrame();

  inner_scroll->SetViewportBoundsDelta(gfx::Vector2dF(60.f, 60.f));
  EXPECT_EQ(gfx::ScrollOffset(10, 10), inner_scroll->MaxScrollOffset());
}

class LayerTreeHostImplOverridePhysicalTime : public LayerTreeHostImpl {
 public:
  LayerTreeHostImplOverridePhysicalTime(
      const LayerTreeSettings& settings,
      LayerTreeHostImplClient* client,
      TaskRunnerProvider* task_runner_provider,
      TaskGraphRunner* task_graph_runner,
      RenderingStatsInstrumentation* rendering_stats_instrumentation)
      : LayerTreeHostImpl(settings,
                          client,
                          task_runner_provider,
                          rendering_stats_instrumentation,
                          task_graph_runner,
                          AnimationHost::CreateForTesting(ThreadInstance::IMPL),
                          0,
                          nullptr) {}

  viz::BeginFrameArgs CurrentBeginFrameArgs() const override {
    return viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1,
                                               fake_current_physical_time_);
  }

  void SetCurrentPhysicalTimeTicksForTest(base::TimeTicks fake_now) {
    fake_current_physical_time_ = fake_now;
  }

 private:
  base::TimeTicks fake_current_physical_time_;
};

class LayerTreeHostImplTestScrollbarAnimation : public LayerTreeHostImplTest {
 protected:
  void SetupLayers(LayerTreeSettings settings) {
    host_impl_->ReleaseLayerTreeFrameSink();
    host_impl_ = nullptr;

    gfx::Size content_size(100, 100);

    LayerTreeHostImplOverridePhysicalTime* host_impl_override_time =
        new LayerTreeHostImplOverridePhysicalTime(
            settings, this, &task_runner_provider_, &task_graph_runner_,
            &stats_instrumentation_);
    host_impl_ = base::WrapUnique(host_impl_override_time);
    layer_tree_frame_sink_ = CreateLayerTreeFrameSink();
    host_impl_->SetVisible(true);
    host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());

    SetupScrollAndContentsLayers(content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 4.f);
    host_impl_->active_tree()->SetDeviceViewportSize(
        gfx::Size(content_size.width() / 2, content_size.height() / 2));

    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
        SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(), 400,
                                             VERTICAL, 10, 0, false, true);
    scrollbar->test_properties()->opacity = 0.f;
    EXPECT_FLOAT_EQ(0.f, scrollbar->test_properties()->opacity);

    LayerImpl* scroll = host_impl_->active_tree()->OuterViewportScrollLayer();
    LayerImpl* root = host_impl_->active_tree()->InnerViewportContainerLayer();
    scrollbar->SetScrollElementId(scroll->element_id());
    root->test_properties()->AddChild(std::move(scrollbar));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    host_impl_->active_tree()->DidBecomeActive();
    host_impl_->active_tree()->HandleScrollbarShowRequestsFromMain();
    host_impl_->active_tree()->SetLocalSurfaceIdFromParent(
        viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
        base::TimeTicks());
    DrawFrame();

    // SetScrollElementId will initialize the scrollbar which will cause it to
    // show and request a redraw.
    did_request_redraw_ = false;
  }

  void RunTest(LayerTreeSettings::ScrollbarAnimator animator) {
    LayerTreeSettings settings = DefaultSettings();
    settings.scrollbar_animator = animator;
    settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
    settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);

    // If no animator is set, scrollbar won't show and no animation is expected.
    bool expecting_animations = animator != LayerTreeSettings::NO_ANIMATOR;

    SetupLayers(settings);

    base::TimeTicks fake_now = base::TimeTicks::Now();

    // Android Overlay Scrollbar does not have a initial show and fade out.
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      // A task will be posted to fade the initial scrollbar.
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_FALSE(animation_task_.Equals(base::Closure()));
      requested_animation_delay_ = base::TimeDelta();
      animation_task_ = base::Closure();
    } else {
      EXPECT_FALSE(did_request_next_frame_);
      EXPECT_FALSE(did_request_redraw_);
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    }

    // If no scroll happened during a scroll gesture, it should have no effect.
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::WHEEL);
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.Equals(base::Closure()));

    // For Aura Overlay Scrollbar, if no scroll happened during a scroll
    // gesture, shows scrollbars and schedules a delay fade out.
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::WHEEL);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2dF(0, 0)).get());
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (animator == LayerTreeSettings::AURA_OVERLAY) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.Equals(base::Closure()));
      requested_animation_delay_ = base::TimeDelta();
      animation_task_ = base::Closure();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
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
    EXPECT_TRUE(animation_task_.Equals(base::Closure()));
    host_impl_->DidFinishImplFrame();

    // After a scroll, a scrollbar animation should be scheduled about 20ms from
    // now.
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::WHEEL);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2dF(0, 5)).get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_TRUE(did_request_redraw_);
    did_request_redraw_ = false;
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.Equals(base::Closure()));
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
    }

    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.Equals(base::Closure()));
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
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
      host_impl_->DidFinishImplFrame();

      // Start the scrollbar animation.
      fake_now += requested_animation_delay_;
      requested_animation_delay_ = base::TimeDelta();
      animation_task_.Run();
      animation_task_ = base::Closure();
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
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
      host_impl_->DidFinishImplFrame();
    }

    // Setting the scroll offset outside a scroll should not cause the
    // scrollbar to appear or schedule a scrollbar animation.
    if (host_impl_->active_tree()
            ->property_trees()
            ->scroll_tree.UpdateScrollOffsetBaseForTesting(
                host_impl_->InnerViewportScrollLayer()->element_id(),
                gfx::ScrollOffset(5, 5)))
      host_impl_->active_tree()->DidUpdateScrollOffset(
          host_impl_->InnerViewportScrollLayer()->element_id());
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
    EXPECT_TRUE(animation_task_.Equals(base::Closure()));

    // Changing page scale triggers scrollbar animation.
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 4.f);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(1.1f);
    EXPECT_FALSE(did_request_next_frame_);
    EXPECT_FALSE(did_request_redraw_);
    if (expecting_animations) {
      EXPECT_EQ(base::TimeDelta::FromMilliseconds(20),
                requested_animation_delay_);
      EXPECT_FALSE(animation_task_.Equals(base::Closure()));
      requested_animation_delay_ = base::TimeDelta();
      animation_task_ = base::Closure();
    } else {
      EXPECT_EQ(base::TimeDelta(), requested_animation_delay_);
      EXPECT_TRUE(animation_task_.Equals(base::Closure()));
    }
  }
};

TEST_F(LayerTreeHostImplTestScrollbarAnimation, Android) {
  RunTest(LayerTreeSettings::ANDROID_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarAnimation, AuraOverlay) {
  RunTest(LayerTreeSettings::AURA_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarAnimation, NoAnimator) {
  RunTest(LayerTreeSettings::NO_ANIMATOR);
}

class LayerTreeHostImplTestScrollbarOpacity : public LayerTreeHostImplTest {
 protected:
  void RunTest(LayerTreeSettings::ScrollbarAnimator animator) {
    LayerTreeSettings settings = DefaultSettings();
    settings.scrollbar_animator = animator;
    settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
    settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
    gfx::Size content_size(100, 100);

    // If no animator is set, scrollbar won't show and no animation is expected.
    bool expecting_animations = animator != LayerTreeSettings::NO_ANIMATOR;

    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    CreatePendingTree();
    CreateScrollAndContentsLayers(host_impl_->pending_tree(), content_size);
    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
        SolidColorScrollbarLayerImpl::Create(host_impl_->pending_tree(), 400,
                                             VERTICAL, 10, 0, false, true);
    scrollbar->test_properties()->opacity = 0.f;
    LayerImpl* scroll = host_impl_->pending_tree()->OuterViewportScrollLayer();
    LayerImpl* container =
        host_impl_->pending_tree()->InnerViewportContainerLayer();
    scrollbar->SetScrollElementId(scroll->element_id());
    scrollbar->SetBounds(gfx::Size(10, 100));
    scrollbar->SetPosition(gfx::PointF(90, 0));
    scrollbar->SetNeedsPushProperties();
    container->test_properties()->AddChild(std::move(scrollbar));

    host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    host_impl_->pending_tree()->BuildPropertyTreesForTesting();
    host_impl_->ActivateSyncTree();

    LayerImpl* active_scrollbar_layer =
        host_impl_->active_tree()->LayerById(400);

    EffectNode* active_tree_node =
        host_impl_->active_tree()->property_trees()->effect_tree.Node(
            active_scrollbar_layer->effect_tree_index());
    EXPECT_FLOAT_EQ(active_scrollbar_layer->Opacity(),
                    active_tree_node->opacity);

    if (expecting_animations) {
      host_impl_->ScrollbarAnimationControllerForElementId(scroll->element_id())
          ->DidMouseMove(gfx::PointF(0, 90));
    } else {
      EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                             scroll->element_id()));
    }
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::WHEEL);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2dF(0, 5)).get());
    host_impl_->ScrollEnd(EndState().get());
    CreatePendingTree();
    // To test the case where the effect tree index of scrollbar layer changes,
    // we force the container layer to create a render surface.
    container = host_impl_->pending_tree()->InnerViewportContainerLayer();
    container->test_properties()->force_render_surface = true;
    container->SetBounds(gfx::Size(10, 10));
    container->SetNeedsPushProperties();

    host_impl_->pending_tree()->BuildPropertyTreesForTesting();

    LayerImpl* pending_scrollbar_layer =
        host_impl_->pending_tree()->LayerById(400);
    pending_scrollbar_layer->SetNeedsPushProperties();
    EffectNode* pending_tree_node =
        host_impl_->pending_tree()->property_trees()->effect_tree.Node(
            pending_scrollbar_layer->effect_tree_index());
    if (expecting_animations) {
      EXPECT_FLOAT_EQ(1.f, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(1.f, active_scrollbar_layer->Opacity());
    } else {
      EXPECT_FLOAT_EQ(0.f, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(0.f, active_scrollbar_layer->Opacity());
    }
    EXPECT_FLOAT_EQ(0.f, pending_tree_node->opacity);
    host_impl_->ActivateSyncTree();
    active_tree_node =
        host_impl_->active_tree()->property_trees()->effect_tree.Node(
            active_scrollbar_layer->effect_tree_index());
    if (expecting_animations) {
      EXPECT_FLOAT_EQ(1.f, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(1.f, active_scrollbar_layer->Opacity());
    } else {
      EXPECT_FLOAT_EQ(0.f, active_tree_node->opacity);
      EXPECT_FLOAT_EQ(0.f, active_scrollbar_layer->Opacity());
    }
  }
};

TEST_F(LayerTreeHostImplTestScrollbarOpacity, Android) {
  RunTest(LayerTreeSettings::ANDROID_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarOpacity, AuraOverlay) {
  RunTest(LayerTreeSettings::AURA_OVERLAY);
}

TEST_F(LayerTreeHostImplTestScrollbarOpacity, NoAnimator) {
  RunTest(LayerTreeSettings::NO_ANIMATOR);
}

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

    const int scrollbar_1_id = 10;
    const int scrollbar_2_id = 11;
    const int child_scroll_id = 13;

    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetDeviceScaleFactor(1);
    host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);
    CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
    host_impl_->active_tree()->InnerViewportContainerLayer()->SetBounds(
        viewport_size);
    LayerImpl* root_scroll =
        host_impl_->active_tree()->OuterViewportScrollLayer();

    // scrollbar_1 on root scroll.
    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar_1 =
        SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                             scrollbar_1_id, VERTICAL, 15, 0,
                                             true, true);
    scrollbar_1_ = scrollbar_1.get();
    scrollbar_1->SetScrollElementId(root_scroll->element_id());
    scrollbar_1->SetDrawsContent(true);
    scrollbar_1->SetBounds(scrollbar_size_1);
    TouchActionRegion touch_action_region;
    touch_action_region.Union(kTouchActionNone, gfx::Rect(scrollbar_size_1));
    scrollbar_1->SetTouchActionRegion(touch_action_region);
    scrollbar_1->SetCurrentPos(0);
    scrollbar_1->SetPosition(gfx::PointF(0, 0));
    host_impl_->active_tree()
        ->InnerViewportContainerLayer()
        ->test_properties()
        ->AddChild(std::move(scrollbar_1));

    // scrollbar_2 on child.
    std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar_2 =
        SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                             scrollbar_2_id, VERTICAL, 15, 0,
                                             true, true);
    scrollbar_2_ = scrollbar_2.get();
    std::unique_ptr<LayerImpl> child =
        LayerImpl::Create(host_impl_->active_tree(), child_scroll_id);
    child->SetPosition(gfx::PointF(50, 50));
    child->SetBounds(child_layer_size);
    child->SetDrawsContent(true);
    child->SetScrollable(gfx::Size(100, 100));
    child->SetElementId(LayerIdToElementIdForTesting(child->id()));
    ElementId child_element_id = child->element_id();

    scrollbar_2->SetScrollElementId(child_element_id);
    scrollbar_2->SetDrawsContent(true);
    scrollbar_2->SetBounds(scrollbar_size_2);
    scrollbar_2->SetCurrentPos(0);
    scrollbar_2->SetPosition(gfx::PointF(0, 0));

    child->test_properties()->AddChild(std::move(scrollbar_2));
    root_scroll->test_properties()->AddChild(std::move(child));

    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    host_impl_->active_tree()->DidBecomeActive();

    ResetScrollbars();
  }

  void ResetScrollbars() {
    scrollbar_1_->test_properties()->opacity = 0.f;
    scrollbar_2_->test_properties()->opacity = 0.f;

    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    if (is_aura_scrollbar_)
      animation_task_ = base::Closure();
  }

  bool is_aura_scrollbar_;
  SolidColorScrollbarLayerImpl* scrollbar_1_;
  SolidColorScrollbarLayerImpl* scrollbar_2_;
};

TEST_F(LayerTreeHostImplTestMultiScrollable,
       ScrollbarFlashAfterAnyScrollUpdate) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_after_any_scroll_update = true;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0.f);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0.f);

  // Scroll on root should flash all scrollbars.
  host_impl_->RootScrollBegin(BeginState(gfx::Point(20, 20)).get(),
                              InputHandler::WHEEL);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(20, 20), gfx::Vector2d(0, 10)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.Equals(base::Closure()));
  ResetScrollbars();

  // Scroll on child should flash all scrollbars.
  host_impl_->ScrollAnimatedBegin(BeginState(gfx::Point(70, 70)).get());
  host_impl_->ScrollAnimated(gfx::Point(70, 70), gfx::Vector2d(0, 100));
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());

  EXPECT_FALSE(animation_task_.Equals(base::Closure()));
}

TEST_F(LayerTreeHostImplTestMultiScrollable, ScrollbarFlashWhenMouseEnter) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_flash_when_mouse_enter = true;

  SetUpLayers(settings);

  EXPECT_EQ(scrollbar_1_->Opacity(), 0.f);
  EXPECT_EQ(scrollbar_2_->Opacity(), 0.f);

  // Scroll should flash when mouse enter.
  host_impl_->MouseMoveAt(gfx::Point(1, 1));

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_FALSE(scrollbar_2_->Opacity());
  EXPECT_FALSE(animation_task_.Equals(base::Closure()));

  host_impl_->MouseMoveAt(gfx::Point(51, 51));

  EXPECT_TRUE(scrollbar_1_->Opacity());
  EXPECT_TRUE(scrollbar_2_->Opacity());
  EXPECT_FALSE(animation_task_.Equals(base::Closure()));
}

TEST_F(LayerTreeHostImplTestMultiScrollable, ScrollHitTestOnScrollbar) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::NO_ANIMATOR;

  SetUpLayers(settings);

  // Wheel scroll on root scrollbar should process on impl thread.
  {
    InputHandler::ScrollStatus status = host_impl_->RootScrollBegin(
        BeginState(gfx::Point(1, 1)).get(), InputHandler::WHEEL);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD, status.thread);
  }

  // Touch scroll on root scrollbar should process on main thread.
  {
    InputHandler::ScrollStatus status = host_impl_->RootScrollBegin(
        BeginState(gfx::Point(1, 1)).get(), InputHandler::TOUCHSCREEN);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kScrollbarScrolling,
              status.main_thread_scrolling_reasons);
  }

  // Wheel scroll on scrollbar should fallback to main thread.
  {
    InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
        BeginState(gfx::Point(51, 51)).get(), InputHandler::WHEEL);
    EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
              status.main_thread_scrolling_reasons);
  }

  // Touch scroll on scrollbar should process on main thread.
  {
    InputHandler::ScrollStatus status = host_impl_->RootScrollBegin(
        BeginState(gfx::Point(51, 51)).get(), InputHandler::TOUCHSCREEN);
    EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
    EXPECT_EQ(MainThreadScrollingReason::kScrollbarScrolling,
              status.main_thread_scrolling_reasons);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollbarVisibilityChangeCausesRedrawAndCommit) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  gfx::Size content_size(100, 100);

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  CreatePendingTree();
  CreateScrollAndContentsLayers(host_impl_->pending_tree(), content_size);
  std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
      SolidColorScrollbarLayerImpl::Create(host_impl_->pending_tree(), 400,
                                           VERTICAL, 10, 0, false, true);
  scrollbar->test_properties()->opacity = 0.f;
  LayerImpl* scroll = host_impl_->pending_tree()->OuterViewportScrollLayer();
  LayerImpl* container =
      host_impl_->pending_tree()->InnerViewportContainerLayer();
  scrollbar->SetScrollElementId(scroll->element_id());
  scrollbar->SetBounds(gfx::Size(10, 100));
  scrollbar->SetPosition(gfx::PointF(90, 0));
  scrollbar->SetNeedsPushProperties();
  container->test_properties()->AddChild(std::move(scrollbar));

  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();

  ScrollbarAnimationController* scrollbar_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          scroll->element_id());

  // Scrollbars will flash shown but we should have a fade out animation
  // queued. Run it and fade out the scrollbars.
  {
    ASSERT_FALSE(animation_task_.Equals(base::Closure()));
    ASSERT_FALSE(animation_task_.IsCancelled());
    animation_task_.Run();

    base::TimeTicks fake_now = base::TimeTicks::Now();
    scrollbar_controller->Animate(fake_now);
    fake_now += settings.scrollbar_fade_delay;
    scrollbar_controller->Animate(fake_now);

    ASSERT_TRUE(scrollbar_controller->ScrollbarsHidden());
  }

  // Move the mouse over the scrollbar region. This should post a delayed fade
  // in task. Execute it to fade in the scrollbars.
  {
    animation_task_ = base::Closure();
    scrollbar_controller->DidMouseMove(gfx::PointF(90, 0));
    ASSERT_FALSE(animation_task_.Equals(base::Closure()));
    ASSERT_FALSE(animation_task_.IsCancelled());
  }

  // The fade in task should cause the scrollbars to show. Ensure that we
  // requested a redraw and a commit.
  {
    did_request_redraw_ = false;
    did_request_commit_ = false;
    ASSERT_TRUE(scrollbar_controller->ScrollbarsHidden());
    animation_task_.Run();

    base::TimeTicks fake_now = base::TimeTicks::Now();
    scrollbar_controller->Animate(fake_now);
    fake_now += settings.scrollbar_fade_duration;
    scrollbar_controller->Animate(fake_now);

    ASSERT_FALSE(scrollbar_controller->ScrollbarsHidden());
    EXPECT_TRUE(did_request_redraw_);
    EXPECT_TRUE(did_request_commit_);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollbarInnerLargerThanOuter) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size inner_viewport_size(315, 200);
  gfx::Size outer_viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  const int horiz_id = 11;
  const int child_scroll_id = 15;

  CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
  host_impl_->active_tree()->InnerViewportContainerLayer()->SetBounds(
      inner_viewport_size);
  host_impl_->active_tree()->InnerViewportScrollLayer()->SetScrollable(
      inner_viewport_size);
  host_impl_->active_tree()->OuterViewportContainerLayer()->SetBounds(
      outer_viewport_size);
  host_impl_->active_tree()->OuterViewportScrollLayer()->SetScrollable(
      outer_viewport_size);
  LayerImpl* root_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  std::unique_ptr<SolidColorScrollbarLayerImpl> horiz_scrollbar =
      SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(), horiz_id,
                                           HORIZONTAL, 5, 5, true, true);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl_->active_tree(), child_scroll_id);
  child->SetBounds(content_size);
  child->SetBounds(inner_viewport_size);

  horiz_scrollbar->SetScrollElementId(root_scroll->element_id());

  host_impl_->active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateScrollbarGeometries();

  EXPECT_EQ(300, horiz_scrollbar->clip_layer_length());
}

TEST_F(LayerTreeHostImplTest, ScrollbarRegistration) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::ANDROID_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  const int vert_1_id = 10;
  const int horiz_1_id = 11;
  const int vert_2_id = 12;
  const int horiz_2_id = 13;
  const int child_scroll_id = 15;

  CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
  LayerImpl* container =
      host_impl_->active_tree()->InnerViewportContainerLayer();
  container->SetBounds(viewport_size);
  LayerImpl* root_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();

  container->test_properties()->AddChild(SolidColorScrollbarLayerImpl::Create(
      host_impl_->active_tree(), vert_1_id, VERTICAL, 5, 5, true, true));
  auto* vert_1_scrollbar = static_cast<SolidColorScrollbarLayerImpl*>(
      container->test_properties()->children[1]);

  container->test_properties()->AddChild(SolidColorScrollbarLayerImpl::Create(
      host_impl_->active_tree(), horiz_1_id, HORIZONTAL, 5, 5, true, true));
  auto* horiz_1_scrollbar = static_cast<SolidColorScrollbarLayerImpl*>(
      container->test_properties()->children[2]);

  container->test_properties()->AddChild(SolidColorScrollbarLayerImpl::Create(
      host_impl_->active_tree(), vert_2_id, VERTICAL, 5, 5, true, true));
  auto* vert_2_scrollbar = static_cast<SolidColorScrollbarLayerImpl*>(
      container->test_properties()->children[3]);

  container->test_properties()->AddChild(SolidColorScrollbarLayerImpl::Create(
      host_impl_->active_tree(), horiz_2_id, HORIZONTAL, 5, 5, true, true));
  auto* horiz_2_scrollbar = static_cast<SolidColorScrollbarLayerImpl*>(
      container->test_properties()->children[4]);

  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl_->active_tree(), child_scroll_id);
  child->SetBounds(viewport_size);
  LayerImpl* child_ptr = child.get();

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

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
  animation_task_ = base::Closure();
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(10, 10)).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_FALSE(animation_task_.Equals(base::Closure()));
  animation_task_ = base::Closure();

  // Check scrollbar registration on a sublayer.
  child->SetScrollable(viewport_size);
  child->SetElementId(LayerIdToElementIdForTesting(child->id()));
  ElementId child_scroll_element_id = child->element_id();
  root_scroll->test_properties()->AddChild(std::move(child));
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
  animation_task_ = base::Closure();
  child_ptr->SetBounds(gfx::Size(200, 200));
  child_ptr->set_needs_show_scrollbars(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->HandleScrollbarShowRequestsFromMain();
  EXPECT_FALSE(animation_task_.Equals(base::Closure()));
  animation_task_ = base::Closure();

  // Check scrollbar unregistration.
  container->test_properties()->RemoveChild(vert_1_scrollbar);
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      root_scroll->element_id()));
  container->test_properties()->RemoveChild(horiz_1_scrollbar);
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll->element_id()));

  EXPECT_EQ(2ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  container->test_properties()->RemoveChild(vert_2_scrollbar);
  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_TRUE(host_impl_->ScrollbarAnimationControllerForElementId(
      child_scroll_element_id));
  container->test_properties()->RemoveChild(horiz_2_scrollbar);
  EXPECT_EQ(0ul, host_impl_->ScrollbarsFor(child_scroll_element_id).size());
  EXPECT_EQ(nullptr, host_impl_->ScrollbarAnimationControllerForElementId(
                         root_scroll->element_id()));

  // Changing scroll offset should no longer trigger any animation.
  host_impl_->active_tree()->InnerViewportScrollLayer()->SetCurrentScrollOffset(
      gfx::ScrollOffset(20, 20));
  EXPECT_TRUE(animation_task_.Equals(base::Closure()));
  child_ptr->SetCurrentScrollOffset(gfx::ScrollOffset(20, 20));
  EXPECT_TRUE(animation_task_.Equals(base::Closure()));
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeMouseMove) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(20);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(20);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);

  CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
  auto* container = host_impl_->active_tree()->InnerViewportContainerLayer();
  container->SetBounds(viewport_size);
  auto* root_scroll = host_impl_->active_tree()->OuterViewportScrollLayer();

  container->test_properties()->AddChild(SolidColorScrollbarLayerImpl::Create(
      host_impl_->active_tree(), 10, VERTICAL, 5, 0, false, true));
  auto* vert_scrollbar = static_cast<SolidColorScrollbarLayerImpl*>(
      container->test_properties()->children[1]);

  vert_scrollbar->SetScrollElementId(root_scroll->element_id());
  vert_scrollbar->SetBounds(gfx::Size(10, 200));
  vert_scrollbar->SetPosition(gfx::PointF(300, 0));
  vert_scrollbar->test_properties()->opacity_can_animate = true;
  vert_scrollbar->SetCurrentPos(0);

  host_impl_->active_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateScrollbarGeometries();

  EXPECT_EQ(1ul, host_impl_->ScrollbarsFor(root_scroll->element_id()).size());
  auto* scrollbar_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kDistanceToTriggerThumb =
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  // Move the mouse near the thumb in the top position.
  auto near_thumb_at_top = gfx::Point(300, -kDistanceToTriggerThumb + 1);
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Move the mouse away from the thumb.
  host_impl_->MouseMoveAt(gfx::Point(300, -kDistanceToTriggerThumb - 1));
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Scroll the page down which moves the thumb down.
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 100)).get());
  host_impl_->ScrollEnd(EndState().get());

  // Move the mouse near the thumb in the top position.
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_FALSE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));

  // Scroll the page up which moves the thumb back up.
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, -100)).get());
  host_impl_->ScrollEnd(EndState().get());

  // Move the mouse near the thumb in the top position.
  host_impl_->MouseMoveAt(near_thumb_at_top);
  EXPECT_TRUE(scrollbar_controller->MouseIsNearScrollbarThumb(VERTICAL));
}

void LayerTreeHostImplTest::SetupMouseMoveAtWithDeviceScale(
    float device_scale_factor) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  gfx::Size viewport_size(300, 200);
  gfx::Size device_viewport_size =
      gfx::ScaleToFlooredSize(viewport_size, device_scale_factor);
  gfx::Size content_size(1000, 1000);
  gfx::Size scrollbar_size(gfx::Size(15, viewport_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->active_tree()->SetDeviceViewportSize(device_viewport_size);

  CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
  host_impl_->active_tree()->InnerViewportContainerLayer()->SetBounds(
      viewport_size);
  LayerImpl* root_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  // The scrollbar is on the left side.
  std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
      SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(), 6,
                                           VERTICAL, 15, 0, true, true);
  scrollbar->SetScrollElementId(root_scroll->element_id());
  scrollbar->SetDrawsContent(true);
  scrollbar->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(scrollbar_size));
  scrollbar->SetTouchActionRegion(touch_action_region);
  host_impl_->active_tree()
      ->InnerViewportContainerLayer()
      ->test_properties()
      ->AddChild(std::move(scrollbar));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();
  host_impl_->active_tree()->UpdateDrawProperties();

  ScrollbarAnimationController* scrollbar_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kMouseMoveDistanceToTriggerFadeIn =
      ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

  const float kMouseMoveDistanceToTriggerExpand =
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 1));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand - 1, 10));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 100));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));

  did_request_redraw_ = false;
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 10));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(150, 120));
  EXPECT_FALSE(
      scrollbar_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf1) {
  SetupMouseMoveAtWithDeviceScale(1.f);
}

TEST_F(LayerTreeHostImplTest, MouseMoveAtWithDeviceScaleOf2) {
  SetupMouseMoveAtWithDeviceScale(2.f);
}

// This test verifies that only SurfaceLayers in the viewport and have fallbacks
// that are different are included in viz::CompositorFrameMetadata's
// |activation_dependencies|.
TEST_F(LayerTreeHostImplTest, ActivationDependenciesInMetadata) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();

  std::vector<viz::SurfaceId> primary_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(1, 1), 1),
      MakeSurfaceId(viz::FrameSinkId(2, 2), 2),
      MakeSurfaceId(viz::FrameSinkId(3, 3), 3)};

  std::vector<viz::SurfaceId> fallback_surfaces = {
      MakeSurfaceId(viz::FrameSinkId(4, 4), 1),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 2),
      MakeSurfaceId(viz::FrameSinkId(4, 4), 3)};

  for (size_t i = 0; i < primary_surfaces.size(); ++i) {
    std::unique_ptr<SurfaceLayerImpl> child =
        SurfaceLayerImpl::Create(host_impl_->active_tree(), i + 6);
    child->SetPosition(gfx::PointF(25.f * i, 0.f));
    child->SetBounds(gfx::Size(1, 1));
    child->SetDrawsContent(true);
    child->SetRange(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]), 2u);
    root->test_properties()->AddChild(std::move(child));
  }

  base::flat_set<viz::SurfaceRange> surfaces_set;
  // |fallback_surfaces| and |primary_surfaces| should have same size
  for (size_t i = 0; i < fallback_surfaces.size(); ++i) {
    surfaces_set.insert(
        viz::SurfaceRange(fallback_surfaces[i], primary_surfaces[i]));
  }

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
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
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
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
TEST_F(LayerTreeHostImplTest, SurfaceReferencesChangeCausesDamage) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // Submit an initial CompositorFrame with an empty set of referenced surfaces.
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
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
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->SetSurfaceRanges({viz::SurfaceRange(surface_id)});
  DrawFrame();

  {
    const viz::CompositorFrameMetadata& metadata =
        fake_layer_tree_frame_sink->last_sent_frame()->metadata;
    EXPECT_THAT(metadata.referenced_surfaces,
                testing::UnorderedElementsAre(viz::SurfaceRange(surface_id)));
  }
}

TEST_F(LayerTreeHostImplTest, CompositorFrameMetadata) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);
  DrawFrame();
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(), metadata.root_scroll_offset);
    EXPECT_EQ(1.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(50.f, 50.f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif
  }

  // Scrolling should update metadata immediately.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
  }
  host_impl_->ScrollEnd(EndState().get());
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
  }

  // Root "overflow: hidden" properties should be reflected on the outer
  // viewport scroll layer.
  {
    host_impl_->active_tree()
        ->OuterViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_horizontal = false;
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
#if defined(OS_ANDROID)
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif

    host_impl_->active_tree()
        ->OuterViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_vertical = false;
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    metadata = host_impl_->MakeCompositorFrameMetadata();
#if defined(OS_ANDROID)
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
#endif
  }

  // Re-enable scrollability and verify that overflows are no longer hidden.
  {
    host_impl_->active_tree()
        ->OuterViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_horizontal = true;
    host_impl_->active_tree()
        ->OuterViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_vertical = true;
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
#if defined(OS_ANDROID)
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif
  }

  // Root "overflow: hidden" properties should also be reflected on the
  // inner viewport scroll layer.
  {
    host_impl_->active_tree()
        ->InnerViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_horizontal = false;
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
#if defined(OS_ANDROID)
    EXPECT_FALSE(metadata.root_overflow_y_hidden);
#endif

    host_impl_->active_tree()
        ->InnerViewportScrollLayer()
        ->test_properties()
        ->user_scrollable_vertical = false;
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    metadata = host_impl_->MakeCompositorFrameMetadata();
#if defined(OS_ANDROID)
    EXPECT_TRUE(metadata.root_overflow_y_hidden);
#endif
  }

  // Page scale should update metadata correctly (shrinking only the viewport).
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2.f, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd(EndState().get());
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
    EXPECT_EQ(2.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(25.f, 25.f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
#endif
  }

  // Likewise if set from the main thread.
  host_impl_->ProcessScrollDeltas();
  host_impl_->active_tree()->PushPageScaleFromMainThread(4.f, 0.5f, 4.f);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(4.f);
  {
    viz::CompositorFrameMetadata metadata =
        host_impl_->MakeCompositorFrameMetadata();
    EXPECT_EQ(gfx::Vector2dF(0.f, 10.f), metadata.root_scroll_offset);
    EXPECT_EQ(4.f, metadata.page_scale_factor);
    EXPECT_EQ(gfx::SizeF(12.5f, 12.5f), metadata.scrollable_viewport_size);
    EXPECT_EQ(0.5f, metadata.min_page_scale_factor);

#if defined(OS_ANDROID)
    EXPECT_EQ(4.f, metadata.max_page_scale_factor);
    EXPECT_EQ(gfx::SizeF(100.f, 100.f), metadata.root_layer_size);
#endif
  }
}

class DidDrawCheckLayer : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new DidDrawCheckLayer(tree_impl, id));
  }

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* provider) override {
    if (!LayerImpl::WillDraw(draw_mode, provider))
      return false;
    if (will_draw_returns_false_)
      return false;
    will_draw_returned_true_ = true;
    return true;
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    append_quads_called_ = true;
    LayerImpl::AppendQuads(render_pass, append_quads_data);
  }

  void DidDraw(viz::ClientResourceProvider* provider) override {
    did_draw_called_ = true;
    LayerImpl::DidDraw(provider);
  }

  bool will_draw_returned_true() const { return will_draw_returned_true_; }
  bool append_quads_called() const { return append_quads_called_; }
  bool did_draw_called() const { return did_draw_called_; }

  void set_will_draw_returns_false() { will_draw_returns_false_ = true; }

  void ClearDidDrawCheck() {
    will_draw_returned_true_ = false;
    append_quads_called_ = false;
    did_draw_called_ = false;
  }

  void AddCopyRequest() {
    test_properties()->copy_requests.push_back(
        viz::CopyOutputRequest::CreateStubForTesting());
  }

 protected:
  DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id),
        will_draw_returns_false_(false),
        will_draw_returned_true_(false),
        append_quads_called_(false),
        did_draw_called_(false) {
    SetBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
    draw_properties().visible_layer_rect = gfx::Rect(0, 0, 10, 10);
  }

 private:
  bool will_draw_returns_false_;
  bool will_draw_returned_true_;
  bool append_quads_called_;
  bool did_draw_called_;
};

TEST_F(LayerTreeHostImplTest, DamageShouldNotCareAboutContributingLayers) {
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 10));
  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());

  // Make a child layer that draws.
  root->test_properties()->AddChild(
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 2));
  auto* layer =
      static_cast<SolidColorLayerImpl*>(root->test_properties()->children[0]);
  layer->SetBounds(gfx::Size(10, 10));
  layer->SetDrawsContent(true);
  layer->SetBackgroundColor(SK_ColorRED);

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_NE(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Stops the child layer from drawing. We should have damage from this but
  // should not have any quads. This should clear the damaged area.
  layer->SetDrawsContent(false);
  root->test_properties()->opacity = 0.f;

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  // The background is default to transparent. If the background is opaque, we
  // would fill the frame with background colour when no layers are contributing
  // quads. This means we would end up with 0 quad.
  EXPECT_EQ(host_impl_->active_tree()->background_color(), SK_ColorTRANSPARENT);

  {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_FALSE(frame.has_no_damage);
    EXPECT_NE(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Now tries to draw again. Nothing changes, so should have no damage, no
  // render pass, and no quad.
  {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    EXPECT_TRUE(frame.has_no_damage);
    EXPECT_EQ(frame.render_passes.size(), 0u);
    size_t total_quad_count = 0;
    for (const auto& pass : frame.render_passes)
      total_quad_count += pass->quad_list.size();
    EXPECT_EQ(total_quad_count, 0u);
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest, WillDrawReturningFalseDoesNotCall) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());

  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  root->test_properties()->force_render_surface = true;
  auto* layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children[0]);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);

    EXPECT_TRUE(layer->will_draw_returned_true());
    EXPECT_TRUE(layer->append_quads_called());
    EXPECT_TRUE(layer->did_draw_called());
  }

  host_impl_->SetViewportDamage(gfx::Rect(10, 10));

  {
    TestFrameData frame;

    layer->set_will_draw_returns_false();
    layer->ClearDidDrawCheck();

    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);

    EXPECT_FALSE(layer->will_draw_returned_true());
    EXPECT_FALSE(layer->append_quads_called());
    EXPECT_FALSE(layer->did_draw_called());
  }
}

TEST_F(LayerTreeHostImplTest, DidDrawNotCalledOnHiddenLayer) {
  // The root layer is always drawn, so run this test on a child layer that
  // will be masked out by the root layer's bounds.
  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());
  root->SetMasksToBounds(true);
  root->test_properties()->force_render_surface = true;
  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  auto* layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children[0]);
  // Ensure visible_layer_rect for layer is empty.
  layer->SetPosition(gfx::PointF(100.f, 100.f));
  layer->SetBounds(gfx::Size(10, 10));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_TRUE(layer->visible_layer_rect().IsEmpty());

  // Ensure visible_layer_rect for layer is not empty
  layer->SetPosition(gfx::PointF());
  layer->NoteLayerPropertyChanged();
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_FALSE(layer->will_draw_returned_true());
  EXPECT_FALSE(layer->did_draw_called());

  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_TRUE(layer->will_draw_returned_true());
  EXPECT_TRUE(layer->did_draw_called());

  EXPECT_FALSE(layer->visible_layer_rect().IsEmpty());
}

TEST_F(LayerTreeHostImplTest, WillDrawNotCalledOnOccludedLayer) {
  gfx::Size big_size(1000, 1000);
  host_impl_->active_tree()->SetDeviceViewportSize(big_size);

  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());

  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  auto* occluded_layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children[0]);

  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 3));
  root->test_properties()->force_render_surface = true;
  auto* top_layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children[1]);
  // This layer covers the occluded_layer above. Make this layer large so it can
  // occlude.
  top_layer->SetBounds(big_size);
  top_layer->SetContentsOpaque(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_FALSE(top_layer->will_draw_returned_true());
  EXPECT_FALSE(top_layer->did_draw_called());

  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_FALSE(occluded_layer->will_draw_returned_true());
  EXPECT_FALSE(occluded_layer->did_draw_called());
  EXPECT_TRUE(top_layer->will_draw_returned_true());
  EXPECT_TRUE(top_layer->did_draw_called());
}

TEST_F(LayerTreeHostImplTest, DidDrawCalledOnAllLayers) {
  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());

  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 2));
  root->test_properties()->force_render_surface = true;
  auto* layer1 =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children[0]);

  layer1->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 3));
  auto* layer2 =
      static_cast<DidDrawCheckLayer*>(layer1->test_properties()->children[0]);

  layer1->test_properties()->force_render_surface = true;
  layer1->test_properties()->should_flatten_transform = true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_FALSE(root->did_draw_called());
  EXPECT_FALSE(layer1->did_draw_called());
  EXPECT_FALSE(layer2->did_draw_called());

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_TRUE(root->did_draw_called());
  EXPECT_TRUE(layer1->did_draw_called());
  EXPECT_TRUE(layer2->did_draw_called());

  EXPECT_NE(GetRenderSurface(root), GetRenderSurface(layer1));
  EXPECT_TRUE(GetRenderSurface(layer1));
}

class MissingTextureAnimatingLayer : public DidDrawCheckLayer {
 public:
  static std::unique_ptr<LayerImpl> Create(
      LayerTreeImpl* tree_impl,
      int id,
      bool tile_missing,
      bool had_incomplete_tile,
      bool animating,
      scoped_refptr<AnimationTimeline> timeline) {
    return base::WrapUnique(new MissingTextureAnimatingLayer(
        tree_impl, id, tile_missing, had_incomplete_tile, animating, timeline));
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    LayerImpl::AppendQuads(render_pass, append_quads_data);
    if (had_incomplete_tile_)
      append_quads_data->num_incomplete_tiles++;
    if (tile_missing_)
      append_quads_data->num_missing_tiles++;
  }

 private:
  MissingTextureAnimatingLayer(LayerTreeImpl* tree_impl,
                               int id,
                               bool tile_missing,
                               bool had_incomplete_tile,
                               bool animating,
                               scoped_refptr<AnimationTimeline> timeline)
      : DidDrawCheckLayer(tree_impl, id),
        tile_missing_(tile_missing),
        had_incomplete_tile_(had_incomplete_tile) {
    if (animating) {
      this->SetElementId(LayerIdToElementIdForTesting(id));
      AddAnimatedTransformToElementWithAnimation(this->element_id(), timeline,
                                                 10.0, 3, 0);
    }
  }

  bool tile_missing_;
  bool had_incomplete_tile_;
};

struct PrepareToDrawSuccessTestCase {
  struct State {
    bool has_missing_tile = false;
    bool has_incomplete_tile = false;
    bool is_animating = false;
    bool has_copy_request = false;
  };
  bool high_res_required = false;
  State layer_before;
  State layer_between;
  State layer_after;
  DrawResult expected_result;

  explicit PrepareToDrawSuccessTestCase(DrawResult result)
      : expected_result(result) {}
};

static void CreateLayerFromState(
    DidDrawCheckLayer* root,
    const scoped_refptr<AnimationTimeline>& timeline,
    const PrepareToDrawSuccessTestCase::State& state) {
  static int layer_id = 2;
  root->test_properties()->AddChild(MissingTextureAnimatingLayer::Create(
      root->layer_tree_impl(), layer_id++, state.has_missing_tile,
      state.has_incomplete_tile, state.is_animating, timeline));
  auto* layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children.back());
  if (state.has_copy_request)
    layer->AddCopyRequest();
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       PrepareToDrawSucceedsAndFails) {
  std::vector<PrepareToDrawSuccessTestCase> cases;

  // 0. Default case.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  // 1. Animated layer first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.is_animating = true;
  // 2. Animated layer between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.is_animating = true;
  // 3. Animated layer last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.is_animating = true;
  // 4. Missing tile first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.has_missing_tile = true;
  // 5. Missing tile between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_missing_tile = true;
  // 6. Missing tile last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.has_missing_tile = true;
  // 7. Incomplete tile first.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_before.has_incomplete_tile = true;
  // 8. Incomplete tile between.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_incomplete_tile = true;
  // 9. Incomplete tile last.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_after.has_incomplete_tile = true;
  // 10. Animation with missing tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_CHECKERBOARD_ANIMATIONS));
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 11. Animation with incomplete tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_incomplete_tile = true;
  cases.back().layer_between.is_animating = true;

  // 12. High res required.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  // 13. High res required with incomplete tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_incomplete_tile = true;
  // 14. High res required with missing tile.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;

  // 15. High res required is higher priority than animating missing tiles.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_after.has_missing_tile = true;
  cases.back().layer_after.is_animating = true;
  // 16. High res required is higher priority than animating missing tiles.
  cases.push_back(
      PrepareToDrawSuccessTestCase(DRAW_ABORTED_MISSING_HIGH_RES_CONTENT));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_before.has_missing_tile = true;
  cases.back().layer_before.is_animating = true;

  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());
  root->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  for (size_t i = 0; i < cases.size(); ++i) {
    // Clean up host_impl_ state.
    const auto& testcase = cases[i];
    std::vector<LayerImpl*> to_remove;
    for (auto* child : root->test_properties()->children)
      to_remove.push_back(child);
    for (auto* child : to_remove)
      root->test_properties()->RemoveChild(child);
    timeline()->ClearAnimations();

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);
    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    TestFrameData frame;
    EXPECT_EQ(testcase.expected_result, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }
}

TEST_F(LayerTreeHostImplTest,
       PrepareToDrawWhenDrawAndSwapFullViewportEveryFrame) {
  CreateHostImpl(DefaultSettings(), FakeLayerTreeFrameSink::CreateSoftware());

  const gfx::Transform external_transform;
  const gfx::Rect external_viewport;
  const bool resourceless_software_draw = true;
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  std::vector<PrepareToDrawSuccessTestCase> cases;

  // 0. Default case.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  // 1. Animation with missing tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().layer_between.has_missing_tile = true;
  cases.back().layer_between.is_animating = true;
  // 2. High res required with incomplete tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_incomplete_tile = true;
  // 3. High res required with missing tile.
  cases.push_back(PrepareToDrawSuccessTestCase(DRAW_SUCCESS));
  cases.back().high_res_required = true;
  cases.back().layer_between.has_missing_tile = true;

  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), 1));
  auto* root = static_cast<DidDrawCheckLayer*>(
      host_impl_->active_tree()->root_layer_for_testing());
  root->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  for (size_t i = 0; i < cases.size(); ++i) {
    const auto& testcase = cases[i];
    std::vector<LayerImpl*> to_remove;
    for (auto* child : root->test_properties()->children)
      to_remove.push_back(child);
    for (auto* child : to_remove)
      root->test_properties()->RemoveChild(child);

    std::ostringstream scope;
    scope << "Test case: " << i;
    SCOPED_TRACE(scope.str());

    CreateLayerFromState(root, timeline(), testcase.layer_before);
    CreateLayerFromState(root, timeline(), testcase.layer_between);
    CreateLayerFromState(root, timeline(), testcase.layer_after);
    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    if (testcase.high_res_required)
      host_impl_->SetRequiresHighResToDraw();

    host_impl_->OnDraw(external_transform, external_viewport,
                       resourceless_software_draw, false);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollRootIgnored) {
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // Scroll event is ignored because layer is not scrollable.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);
  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ClampingAfterActivation) {
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  CreateScrollAndContentsLayers(host_impl_->pending_tree(),
                                gfx::Size(100, 100));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();

  CreatePendingTree();
  const gfx::ScrollOffset pending_scroll = gfx::ScrollOffset(-100, -100);
  LayerImpl* active_outer_layer =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  LayerImpl* pending_outer_layer =
      host_impl_->pending_tree()->OuterViewportScrollLayer();
  pending_outer_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          pending_outer_layer->element_id(), pending_scroll);

  host_impl_->ActivateSyncTree();
  // Scrolloffsets on the active tree will be clamped after activation.
  EXPECT_EQ(active_outer_layer->CurrentScrollOffset(), gfx::ScrollOffset(0, 0));
}

class LayerTreeHostImplBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  LayerTreeHostImplBrowserControlsTest()
      // Make the clip size the same as the layer (content) size so the layer is
      // non-scrollable.
      : layer_size_(10, 10),
        clip_size_(layer_size_),
        top_controls_height_(50) {
    viewport_size_ = gfx::Size(clip_size_.width(),
                               clip_size_.height() + top_controls_height_);
  }

  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    bool init = LayerTreeHostImplTest::CreateHostImpl(
        settings, std::move(layer_tree_frame_sink));
    if (init) {
      host_impl_->active_tree()->SetTopControlsHeight(top_controls_height_);
      host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f);
      host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    }
    return init;
  }

  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    settings_ = DefaultSettings();
    CreateHostImpl(settings_, CreateLayerTreeFrameSink());
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->active_tree(), inner_viewport_size, outer_viewport_size,
        scroll_layer_size);
  }

  void SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      LayerTreeImpl* tree_impl,
      const gfx::Size& inner_viewport_size,
      const gfx::Size& outer_viewport_size,
      const gfx::Size& scroll_layer_size) {
    tree_impl->set_browser_controls_shrink_blink_size(true);
    tree_impl->SetTopControlsHeight(top_controls_height_);
    tree_impl->SetCurrentBrowserControlsShownRatio(1.f);
    tree_impl->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    host_impl_->DidChangeBrowserControlsPosition();

    std::unique_ptr<LayerImpl> root = LayerImpl::Create(tree_impl, 1);
    std::unique_ptr<LayerImpl> root_clip = LayerImpl::Create(tree_impl, 2);
    std::unique_ptr<LayerImpl> page_scale = LayerImpl::Create(tree_impl, 3);

    std::unique_ptr<LayerImpl> outer_scroll = LayerImpl::Create(tree_impl, 4);
    std::unique_ptr<LayerImpl> outer_clip = LayerImpl::Create(tree_impl, 5);

    root_clip->SetBounds(inner_viewport_size);
    root->SetScrollable(inner_viewport_size);
    root->SetElementId(LayerIdToElementIdForTesting(root->id()));
    root->SetBounds(outer_viewport_size);
    root->SetPosition(gfx::PointF());
    root->SetDrawsContent(false);
    root_clip->test_properties()->force_render_surface = true;
    root->test_properties()->is_container_for_fixed_position_layers = true;
    outer_clip->SetBounds(outer_viewport_size);
    outer_scroll->SetScrollable(outer_viewport_size);
    outer_scroll->SetElementId(
        LayerIdToElementIdForTesting(outer_scroll->id()));
    outer_scroll->SetBounds(scroll_layer_size);
    outer_scroll->SetPosition(gfx::PointF());
    outer_scroll->SetDrawsContent(false);
    outer_scroll->test_properties()->is_container_for_fixed_position_layers =
        true;

    int inner_viewport_container_layer_id = root_clip->id();
    int outer_viewport_container_layer_id = outer_clip->id();
    int inner_viewport_scroll_layer_id = root->id();
    int outer_viewport_scroll_layer_id = outer_scroll->id();
    int page_scale_layer_id = page_scale->id();

    outer_clip->test_properties()->AddChild(std::move(outer_scroll));
    root->test_properties()->AddChild(std::move(outer_clip));
    page_scale->test_properties()->AddChild(std::move(root));
    root_clip->test_properties()->AddChild(std::move(page_scale));

    tree_impl->SetRootLayerForTesting(std::move(root_clip));
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = page_scale_layer_id;
    viewport_ids.inner_viewport_container = inner_viewport_container_layer_id;
    viewport_ids.outer_viewport_container = outer_viewport_container_layer_id;
    viewport_ids.inner_viewport_scroll = inner_viewport_scroll_layer_id;
    viewport_ids.outer_viewport_scroll = outer_viewport_scroll_layer_id;
    tree_impl->SetViewportLayersFromIds(viewport_ids);
    tree_impl->BuildPropertyTreesForTesting();

    host_impl_->active_tree()->SetDeviceViewportSize(inner_viewport_size);
    LayerImpl* root_clip_ptr = tree_impl->root_layer_for_testing();
    EXPECT_EQ(inner_viewport_size, root_clip_ptr->bounds());
  }

 protected:
  gfx::Size layer_size_;
  gfx::Size clip_size_;
  gfx::Size viewport_size_;
  float top_controls_height_;

  LayerTreeSettings settings_;
};  // class LayerTreeHostImplBrowserControlsTest

// Tests that, on a page with content the same size as the viewport, hiding
// the browser controls also increases the ScrollableSize (i.e. the content
// size). Since the viewport got larger, the effective scrollable "content" also
// did. This ensures, for one thing, that the overscroll glow is shown in the
// right place.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsExpandsScrollableSize) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a content layer beneath the outer viewport scroll layer.
  int id = host_impl_->OuterViewportScrollLayer()->id();
  host_impl_->OuterViewportScrollLayer()->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->active_tree(), id + 2));
  LayerImpl* content =
      active_tree->OuterViewportScrollLayer()->test_properties()->children[0];
  content->SetBounds(gfx::Size(50, 50));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  LayerImpl* inner_container = active_tree->InnerViewportContainerLayer();
  LayerImpl* outer_container = active_tree->OuterViewportContainerLayer();

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  ASSERT_EQ(gfx::Size(50, 50), inner_container->bounds());
  ASSERT_EQ(gfx::Size(50, 50), outer_container->bounds());

  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    ASSERT_EQ(gfx::Size(50, 75), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 75), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::SizeF(50, 50), content->BoundsForScrolling());
  }

  // Fully hide the browser controls.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    ASSERT_EQ(gfx::Size(50, 100), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 100), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::SizeF(50, 50), content->BoundsForScrolling());
  }

  // Scrolling additionally shouldn't have any effect.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    ASSERT_EQ(gfx::Size(50, 100), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 100), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::SizeF(50, 50), content->BoundsForScrolling());
  }

  host_impl_->browser_controls_manager()->ScrollEnd();
  host_impl_->ScrollEnd(EndState().get());
}

// Tests that browser controls affect the position of horizontal scrollbars.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       HidingBrowserControlsAdjustsScrollbarPosition) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(50, 50), gfx::Size(50, 50), gfx::Size(50, 50));

  LayerTreeImpl* active_tree = host_impl_->active_tree();

  // Create a horizontal scrollbar.
  const int scrollbar_id = 23;
  gfx::Size scrollbar_size(gfx::Size(50, 15));
  std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar =
      SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                           scrollbar_id, HORIZONTAL, 3, 20,
                                           false, true);
  scrollbar->SetScrollElementId(
      host_impl_->OuterViewportScrollLayer()->element_id());
  scrollbar->SetDrawsContent(true);
  scrollbar->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(scrollbar_size));
  scrollbar->SetTouchActionRegion(touch_action_region);
  scrollbar->SetCurrentPos(0);
  scrollbar->SetPosition(gfx::PointF(0, 35));
  host_impl_->active_tree()
      ->InnerViewportContainerLayer()
      ->test_properties()
      ->AddChild(std::move(scrollbar));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateScrollbarGeometries();

  DrawFrame();

  LayerImpl* inner_container = active_tree->InnerViewportContainerLayer();
  LayerImpl* outer_container = active_tree->OuterViewportContainerLayer();
  auto* scrollbar_layer = static_cast<SolidColorScrollbarLayerImpl*>(
      active_tree->LayerById(scrollbar_id));

  // The browser controls should start off showing so the viewport should be
  // shrunk.
  EXPECT_EQ(gfx::Size(50, 50), inner_container->bounds());
  EXPECT_EQ(gfx::Size(50, 50), outer_container->bounds());
  EXPECT_EQ(gfx::SizeF(50, 50), active_tree->ScrollableSize());
  EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
  EXPECT_EQ(gfx::Rect(20, 0, 10, 3), scrollbar_layer->ComputeThumbQuadRect());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();

  // Hide the browser controls by a bit, the scrollable size should increase but
  // the actual content bounds shouldn't.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    ASSERT_EQ(gfx::Size(50, 75), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 75), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 75), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 25, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Fully hide the browser controls.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    host_impl_->active_tree()->UpdateScrollbarGeometries();
    ASSERT_EQ(gfx::Size(50, 100), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 100), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  // Additional scrolling shouldn't have any effect.
  {
    host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 25.f));
    ASSERT_EQ(gfx::Size(50, 100), inner_container->bounds());
    ASSERT_EQ(gfx::Size(50, 100), outer_container->bounds());
    EXPECT_EQ(gfx::SizeF(50, 100), active_tree->ScrollableSize());
    EXPECT_EQ(gfx::Size(50, 15), scrollbar_layer->bounds());
    EXPECT_EQ(gfx::Rect(20, 50, 10, 3),
              scrollbar_layer->ComputeThumbQuadRect());
  }

  host_impl_->browser_controls_manager()->ScrollEnd();
  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplBrowserControlsTest,
       ScrollBrowserControlsByFractionalAmount) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 10), gfx::Size(10, 10), gfx::Size(10, 10));
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  // Make the test scroll delta a fractional amount, to verify that the
  // fixed container size delta is (1) non-zero, and (2) fractional, and
  // (3) matches the movement of the browser controls.
  gfx::Vector2dF top_controls_scroll_delta(0.f, 5.25f);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollEnd();

  host_impl_->ScrollEnd(EndState().get());
  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->inner_viewport_container_bounds_delta().y());
}

// In this test, the outer viewport is initially unscrollable. We test that a
// scroll initiated on the inner viewport, causing the browser controls to show
// and thus making the outer viewport scrollable, still scrolls the outer
// viewport.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsOuterViewportBecomesScrollable) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  LayerImpl* inner_scroll =
      host_impl_->active_tree()->InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* inner_container =
      host_impl_->active_tree()->InnerViewportContainerLayer();
  LayerImpl* outer_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);
  LayerImpl* outer_container =
      host_impl_->active_tree()->OuterViewportContainerLayer();

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  outer_scroll->SetDrawsContent(true);
  host_impl_->active_tree()->PushPageScaleFromMainThread(2.f, 1.f, 2.f);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, 50.f)).get());

  // The entire scroll delta should have been used to hide the browser controls.
  // The viewport layers should be resized back to their full sizes.
  EXPECT_EQ(0.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
  EXPECT_EQ(0.f, inner_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(100.f, inner_container->BoundsForScrolling().height());
  EXPECT_EQ(100.f, outer_container->BoundsForScrolling().height());

  // The inner viewport should be scrollable by 50px * page_scale.
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, 100.f)).get());
  EXPECT_EQ(50.f, inner_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(0.f, outer_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(gfx::ScrollOffset(), outer_scroll->MaxScrollOffset());

  host_impl_->ScrollEnd(EndState().get());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            outer_scroll->scroll_tree_index());

  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -50.f)).get());

  // The entire scroll delta should have been used to show the browser controls.
  // The outer viewport should be resized to accomodate and scrolled to the
  // bottom of the document to keep the viewport in place.
  EXPECT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
  EXPECT_EQ(50.f, outer_container->BoundsForScrolling().height());
  EXPECT_EQ(50.f, inner_container->BoundsForScrolling().height());
  EXPECT_EQ(25.f, outer_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(25.f, inner_scroll->CurrentScrollOffset().y());

  // Now when we continue scrolling, make sure the outer viewport gets scrolled
  // since it wasn't scrollable when the scroll began.
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -20.f)).get());
  EXPECT_EQ(25.f, outer_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(15.f, inner_scroll->CurrentScrollOffset().y());

  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -30.f)).get());
  EXPECT_EQ(25.f, outer_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(0.f, inner_scroll->CurrentScrollOffset().y());

  host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0.f, -50.f)).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_EQ(0.f, outer_scroll->CurrentScrollOffset().y());
  EXPECT_EQ(0.f, inner_scroll->CurrentScrollOffset().y());
}

// Test that the fixed position container delta is appropriately adjusted
// by the browser controls showing/hiding and page scale doesn't affect it.
TEST_F(LayerTreeHostImplBrowserControlsTest, FixedContainerDelta) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);

  float page_scale = 1.5f;
  // Zoom in, since the fixed container is the outer viewport, the delta should
  // not be scaled.
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1.f, 2.f);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  // Scroll down, the browser controls hiding should expand the viewport size so
  // the delta should be equal to the scroll distance.
  gfx::Vector2dF top_controls_scroll_delta(0.f, 20.f);
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_FLOAT_EQ(top_controls_height_ - top_controls_scroll_delta.y(),
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  auto* property_trees = host_impl_->active_tree()->property_trees();
  EXPECT_FLOAT_EQ(top_controls_scroll_delta.y(),
                  property_trees->outer_viewport_container_bounds_delta().y());
  host_impl_->ScrollEnd(EndState().get());

  // Scroll past the maximum extent. The delta shouldn't be greater than the
  // browser controls height.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  host_impl_->browser_controls_manager()->ScrollBy(top_controls_scroll_delta);
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, top_controls_height_),
                   property_trees->outer_viewport_container_bounds_delta());
  host_impl_->ScrollEnd(EndState().get());

  // Scroll in the direction to make the browser controls show.
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(-top_controls_scroll_delta);
  EXPECT_EQ(top_controls_scroll_delta.y(),
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR_EQ(
      gfx::Vector2dF(0, top_controls_height_ - top_controls_scroll_delta.y()),
      property_trees->outer_viewport_container_bounds_delta());
  host_impl_->browser_controls_manager()->ScrollEnd();
}

// Push a browser controls ratio from the main thread that we didn't send as a
// delta and make sure that the ratio is clamped to the [0, 1] range.
TEST_F(LayerTreeHostImplBrowserControlsTest, BrowserControlsPushUnsentRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(10, 50), gfx::Size(10, 50), gfx::Size(10, 100));
  DrawFrame();

  // Need SetDrawsContent so ScrollBegin's hit test finds an actual layer.
  LayerImpl* inner_scroll =
      host_impl_->active_tree()->InnerViewportScrollLayer();
  inner_scroll->SetDrawsContent(true);
  LayerImpl* outer_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  outer_scroll->SetDrawsContent(true);

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(1);
  ASSERT_EQ(1.0f,
            host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.5f);
  ASSERT_EQ(0.5f,
            host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  host_impl_->active_tree()->PushBrowserControlsFromMainThread(0);

  ASSERT_EQ(0, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
}

// Test that if only the browser controls are scrolled, we shouldn't request a
// commit.
TEST_F(LayerTreeHostImplBrowserControlsTest, BrowserControlsDontTriggerCommit) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 50), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();

  // Show browser controls
  EXPECT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  // Scroll 25px to hide browser controls
  gfx::Vector2dF scroll_delta(0.f, 25.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  EXPECT_FALSE(did_request_commit_);
}

// Test that if a scrollable sublayer doesn't consume the scroll,
// browser controls should hide when scrolling down.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollableSublayer) {
  gfx::Size sub_content_size(100, 400);
  gfx::Size sub_content_layer_size(100, 300);
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 50), gfx::Size(100, 100), gfx::Size(100, 100));
  DrawFrame();

  // Show browser controls
  EXPECT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  LayerImpl* outer_viewport_scroll_layer =
      host_impl_->active_tree()->OuterViewportScrollLayer();
  int id = outer_viewport_scroll_layer->id();
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl_->active_tree(), id + 2);

  child->SetScrollable(sub_content_layer_size);
  child->SetElementId(LayerIdToElementIdForTesting(child->id()));
  child->SetBounds(sub_content_size);
  child->SetPosition(gfx::PointF());
  child->SetDrawsContent(true);
  child->test_properties()->is_container_for_fixed_position_layers = true;

  LayerImpl* child_ptr = child.get();
  outer_viewport_scroll_layer->test_properties()->AddChild(std::move(child));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Scroll child to the limit.
  SetScrollOffsetDelta(child_ptr, gfx::Vector2dF(0, 100.f));

  // Scroll 25px to hide browser controls
  gfx::Vector2dF scroll_delta(0.f, 25.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // Browser controls should be hidden
  EXPECT_EQ(scroll_delta.y(),
            top_controls_height_ -
                host_impl_->browser_controls_manager()->ContentTopOffset());
}

// Ensure setting the browser controls position explicitly using the setters on
// the TreeImpl correctly affects the browser controls manager and viewport
// bounds.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       PositionBrowserControlsExplicitly) {
  settings_ = DefaultSettings();
  CreateHostImpl(settings_, CreateLayerTreeFrameSink());
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.f);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      30.f / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  EXPECT_FLOAT_EQ(30.f,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-20.f,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.f);
  EXPECT_FLOAT_EQ(0.f,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(-50.f,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->DidChangeBrowserControlsPosition();

  // Now that browser controls have moved, expect the clip to resize.
  LayerImpl* inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                                  ->test_properties()
                                  ->parent->test_properties()
                                  ->parent;
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());
}

// Test that the top_controls delta and sent delta are appropriately
// applied on sync tree activation. The total browser controls offset shouldn't
// change after the activation.
TEST_F(LayerTreeHostImplBrowserControlsTest, ApplyDeltaOnTreeActivation) {
  settings_ = DefaultSettings();
  CreateHostImpl(settings_, CreateLayerTreeFrameSink());
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
      20.f / top_controls_height_);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(
      15.f / top_controls_height_);
  host_impl_->active_tree()
      ->top_controls_shown_ratio()
      ->PullDeltaForMainThread();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.f);
  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(
      15.f / top_controls_height_);

  host_impl_->DidChangeBrowserControlsPosition();
  LayerImpl* inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                                  ->test_properties()
                                  ->parent->test_properties()
                                  ->parent;
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->ActivateSyncTree();

  inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                       ->test_properties()
                       ->parent->test_properties()
                       ->parent;
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());

  EXPECT_FLOAT_EQ(
      -15.f, host_impl_->active_tree()->top_controls_shown_ratio()->Delta() *
                 top_controls_height_);
  EXPECT_FLOAT_EQ(
      15.f,
      host_impl_->active_tree()->top_controls_shown_ratio()->ActiveBase() *
          top_controls_height_);
}

// Test that changing the browser controls layout height is correctly applied to
// the inner viewport container bounds. That is, the browser controls layout
// height is the amount that the inner viewport container was shrunk outside
// the compositor to accommodate the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsLayoutHeightChanged) {
  settings_ = DefaultSettings();
  CreateHostImpl(settings_, CreateLayerTreeFrameSink());
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  host_impl_->sync_tree()->PushBrowserControlsFromMainThread(1.f);
  host_impl_->sync_tree()->set_browser_controls_shrink_blink_size(true);

  host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(1.f);
  host_impl_->active_tree()->top_controls_shown_ratio()->PushPendingToActive();
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(0.f);

  host_impl_->DidChangeBrowserControlsPosition();
  LayerImpl* inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                                  ->test_properties()
                                  ->parent->test_properties()
                                  ->parent;
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());

  host_impl_->sync_tree()->root_layer_for_testing()->SetBounds(
      gfx::Size(inner_clip_ptr->bounds().width(),
                inner_clip_ptr->bounds().height() - 50.f));

  host_impl_->ActivateSyncTree();

  inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                       ->test_properties()
                       ->parent->test_properties()
                       ->parent;
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());

  // The total bounds should remain unchanged since the bounds delta should
  // account for the difference between the layout height and the current
  // browser controls offset.
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 50.f),
                   inner_clip_ptr->ViewportBoundsDelta());

  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f);
  host_impl_->DidChangeBrowserControlsPosition();

  EXPECT_EQ(1.f,
            host_impl_->browser_controls_manager()->TopControlsShownRatio());
  EXPECT_EQ(50.f, host_impl_->browser_controls_manager()->TopControlsHeight());
  EXPECT_EQ(50.f, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                   inner_clip_ptr->ViewportBoundsDelta());
  EXPECT_EQ(gfx::Size(viewport_size_.width(), viewport_size_.height() - 50.f),
            inner_clip_ptr->bounds());
}

// Test that showing/hiding the browser controls when the viewport is fully
// scrolled doesn't incorrectly change the viewport offset due to clamping from
// changing viewport bounds.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsViewportOffsetClamping) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200.f));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100.f));

  gfx::ScrollOffset viewport_offset =
      host_impl_->active_tree()->TotalScrollOffset();
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(), viewport_offset);

  // Hide the browser controls by 25px.
  gfx::Vector2dF scroll_delta(0.f, 25.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());

  // scrolling down at the max extents no longer hides the browser controls
  EXPECT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  // forcefully hide the browser controls by 25px
  host_impl_->browser_controls_manager()->ScrollBy(scroll_delta);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  inner_scroll->ClampScrollToMaxScrollOffset();
  outer_scroll->ClampScrollToMaxScrollOffset();

  // We should still be fully scrolled.
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());

  viewport_offset = host_impl_->active_tree()->TotalScrollOffset();

  // Bring the browser controls down by 25px.
  scroll_delta = gfx::Vector2dF(0.f, -25.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // The viewport offset shouldn't have changed.
  EXPECT_EQ(viewport_offset, host_impl_->active_tree()->TotalScrollOffset());

  // Scroll the viewports to max scroll offset.
  SetScrollOffsetDelta(outer_scroll, gfx::Vector2dF(0, 200.f));
  SetScrollOffsetDelta(inner_scroll, gfx::Vector2dF(100, 100.f));
  EXPECT_EQ(host_impl_->active_tree()->TotalMaxScrollOffset(),
            host_impl_->active_tree()->TotalScrollOffset());
}

// Test that the browser controls coming in and out maintains the same aspect
// ratio between the inner and outer viewports.
TEST_F(LayerTreeHostImplBrowserControlsTest, BrowserControlsAspectRatio) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 2.f);
  DrawFrame();

  EXPECT_FLOAT_EQ(top_controls_height_,
                  host_impl_->browser_controls_manager()->ContentTopOffset());

  gfx::Vector2dF scroll_delta(0.f, 25.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  // Browser controls were hidden by 25px so the inner viewport should have
  // expanded by that much.
  LayerImpl* outer_container =
      host_impl_->active_tree()->OuterViewportContainerLayer();
  LayerImpl* inner_container =
      host_impl_->active_tree()->InnerViewportContainerLayer();
  EXPECT_EQ(gfx::SizeF(100.f, 100.f + 25.f),
            inner_container->BoundsForScrolling());

  // Outer viewport should match inner's aspect ratio. The bounds are ceiled.
  float aspect_ratio = inner_container->BoundsForScrolling().width() /
                       inner_container->BoundsForScrolling().height();
  gfx::SizeF expected =
      gfx::SizeF(gfx::ToCeiledSize(gfx::SizeF(200, 200 / aspect_ratio)));
  EXPECT_EQ(expected, outer_container->BoundsForScrolling());
  EXPECT_EQ(expected,
            host_impl_->InnerViewportScrollLayer()->BoundsForScrolling());
}

// Test that scrolling the outer viewport affects the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       BrowserControlsScrollOuterViewport) {
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      gfx::Size(100, 100), gfx::Size(200, 200), gfx::Size(200, 400));
  DrawFrame();

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());

  // Send a gesture scroll that will scroll the outer viewport, make sure the
  // browser controls get scrolled.
  gfx::Vector2dF scroll_delta(0.f, 15.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());

  EXPECT_EQ(host_impl_->OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->ScrollEnd(EndState().get());

  EXPECT_FLOAT_EQ(
      scroll_delta.y(),
      top_controls_height_ -
          host_impl_->browser_controls_manager()->ContentTopOffset());

  scroll_delta = gfx::Vector2dF(0.f, 50.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());

  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_EQ(host_impl_->OuterViewportScrollLayer()->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  host_impl_->ScrollEnd(EndState().get());

  // Position the viewports such that the inner viewport will be scrolled.
  gfx::Vector2dF inner_viewport_offset(0.f, 25.f);
  SetScrollOffsetDelta(host_impl_->OuterViewportScrollLayer(),
                       gfx::Vector2dF());
  SetScrollOffsetDelta(host_impl_->InnerViewportScrollLayer(),
                       inner_viewport_offset);

  scroll_delta = gfx::Vector2dF(0.f, -65.f);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());

  EXPECT_EQ(top_controls_height_,
            host_impl_->browser_controls_manager()->ContentTopOffset());
  EXPECT_FLOAT_EQ(
      inner_viewport_offset.y() + (scroll_delta.y() + top_controls_height_),
      ScrollDelta(host_impl_->InnerViewportScrollLayer()).y());

  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplBrowserControlsTest,
       ScrollNonScrollableRootWithBrowserControls) {
  settings_ = DefaultSettings();
  CreateHostImpl(settings_, CreateLayerTreeFrameSink());
  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      layer_size_, layer_size_, layer_size_);
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(gfx::Vector2dF(0.f, 50.f));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  LayerImpl* inner_clip_ptr = host_impl_->InnerViewportScrollLayer()
                                  ->test_properties()
                                  ->parent->test_properties()
                                  ->parent;
  EXPECT_EQ(viewport_size_, inner_clip_ptr->bounds());

  host_impl_->ScrollEnd(EndState().get());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  float scroll_increment_y = -25.f;
  host_impl_->browser_controls_manager()->ScrollBegin();
  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0.f, scroll_increment_y));
  EXPECT_FLOAT_EQ(-scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(gfx::Size(viewport_size_.width(),
                      viewport_size_.height() + scroll_increment_y),
            inner_clip_ptr->bounds());

  host_impl_->browser_controls_manager()->ScrollBy(
      gfx::Vector2dF(0.f, scroll_increment_y));
  host_impl_->browser_controls_manager()->ScrollEnd();
  EXPECT_FLOAT_EQ(-2 * scroll_increment_y,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  // Now that browser controls have moved, expect the clip to resize.
  EXPECT_EQ(clip_size_, inner_clip_ptr->bounds());

  host_impl_->ScrollEnd(EndState().get());

  // Verify the layer is once-again non-scrollable.
  EXPECT_EQ(
      gfx::ScrollOffset(),
      host_impl_->active_tree()->InnerViewportScrollLayer()->MaxScrollOffset());

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
}

// Tests that activating a pending tree while there's a bounds_delta on the
// viewport layers from browser controls doesn't cause a scroll jump. This bug
// was occurring because the UpdateViewportContainerSizes was being called
// before the property trees were updated with the bounds_delta.
// crbug.com/597266.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       ViewportBoundsDeltaOnTreeActivation) {
  const gfx::Size inner_viewport_size(1000, 1000);
  const gfx::Size outer_viewport_size(1000, 1000);
  const gfx::Size content_size(2000, 2000);

  // Initialization
  {
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        inner_viewport_size, outer_viewport_size, content_size);
    host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);

    // Start off with the browser controls hidden on both main and impl.
    host_impl_->active_tree()->set_browser_controls_shrink_blink_size(false);
    host_impl_->active_tree()->PushBrowserControlsFromMainThread(0);

    CreatePendingTree();
    SetupBrowserControlsAndScrollLayerWithVirtualViewport(
        host_impl_->pending_tree(), inner_viewport_size, outer_viewport_size,
        content_size);
    host_impl_->pending_tree()->set_browser_controls_shrink_blink_size(false);

    // Fully scroll the viewport.
    host_impl_->ScrollBegin(BeginState(gfx::Point(75, 75)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2d(0, 2000)).get());
    host_impl_->ScrollEnd(EndState().get());
  }

  LayerImpl* outer_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();

  ASSERT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ContentTopOffset());
  ASSERT_EQ(1000, outer_scroll->MaxScrollOffset().y());
  ASSERT_EQ(1000, outer_scroll->CurrentScrollOffset().y());

  // Kick off an animation to show the browser controls.
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, true);
  base::TimeTicks start_time = base::TimeTicks::Now();
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // The first animation frame will not produce any delta, it will establish
  // the animation.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame();
    float delta =
        host_impl_->active_tree()->top_controls_shown_ratio()->Delta();
    ASSERT_EQ(delta, 0);
  }

  // Pump an animation frame to put some delta in the browser controls.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(50);
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame();
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
        ->PullDeltaForMainThread();
    host_impl_->active_tree()->top_controls_shown_ratio()->PushMainToPending(
        delta);
  }

  // 200 is the kShowHideMaxDurationMs value from browser_controls_manager.cc so
  // the browser controls should be fully animated in this frame.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(200);
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);
    host_impl_->DidFinishImplFrame();

    ASSERT_EQ(50, host_impl_->browser_controls_manager()->ContentTopOffset());
    ASSERT_EQ(1050, outer_scroll->MaxScrollOffset().y());
    // NEAR because clip layer bounds are truncated in MaxScrollOffset so we
    // lose some precision in the intermediate animation steps.
    ASSERT_NEAR(1050, outer_scroll->CurrentScrollOffset().y(), 1.f);
  }

  // Activate the pending tree which should have the same scroll value as the
  // active tree.
  {
    host_impl_->pending_tree()
        ->property_trees()
        ->scroll_tree.SetScrollOffsetDeltaForTesting(outer_scroll->element_id(),
                                                     gfx::Vector2dF(0, 1050));
    host_impl_->ActivateSyncTree();

    // Make sure we don't accidentally clamp the outer offset based on a bounds
    // delta that hasn't yet been updated.
    EXPECT_NEAR(1050, outer_scroll->CurrentScrollOffset().y(), 1.f);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollNonCompositedRoot) {
  // Test the configuration where a non-composited root layer is embedded in a
  // scrollable outer layer.
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  std::unique_ptr<LayerImpl> content_layer =
      LayerImpl::Create(host_impl_->active_tree(), 11);
  content_layer->SetDrawsContent(true);
  content_layer->SetPosition(gfx::PointF());
  content_layer->SetBounds(contents_size);

  LayerImpl* scroll_container_layer =
      CreateBasicVirtualViewportLayers(surface_size, surface_size);

  std::unique_ptr<LayerImpl> scroll_layer =
      LayerImpl::Create(host_impl_->active_tree(), 12);
  scroll_layer->SetScrollable(surface_size);
  scroll_layer->SetElementId(LayerIdToElementIdForTesting(scroll_layer->id()));
  scroll_layer->SetBounds(contents_size);
  scroll_layer->SetPosition(gfx::PointF());
  scroll_layer->test_properties()->AddChild(std::move(content_layer));
  scroll_container_layer->test_properties()->AddChild(std::move(scroll_layer));

  scroll_container_layer->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollChildCallsCommitAndRedraw) {
  gfx::Size surface_size(10, 10);
  gfx::Size contents_size(20, 20);

  LayerImpl* root =
      CreateBasicVirtualViewportLayers(surface_size, surface_size);

  root->test_properties()->AddChild(CreateScrollableLayer(12, contents_size));
  root->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_TRUE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesChild) {
  gfx::Size surface_size(10, 10);
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root->test_properties()->AddChild(CreateScrollableLayer(2, surface_size));
  root->test_properties()->force_render_surface = true;
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  // Scroll event is ignored because the input coordinate is outside the layer
  // boundaries.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(15, 5)).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollMissesBackfacingChild) {
  gfx::Size surface_size(10, 10);
  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);

  gfx::Transform matrix;
  matrix.RotateAboutXAxis(180.0);
  child->test_properties()->transform = matrix;
  child->test_properties()->double_sided = false;

  root->test_properties()->AddChild(std::move(child));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  // Scroll event is ignored because the scrollable layer is not facing the
  // viewer and there is nothing scrollable behind it.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_IGNORED, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kNoScrollingLayer,
            status.main_thread_scrolling_reasons);

  EXPECT_FALSE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);
}

TEST_F(LayerTreeHostImplTest, ScrollBlockedByContentLayer) {
  gfx::Size surface_size(10, 10);
  std::unique_ptr<LayerImpl> content_layer =
      CreateScrollableLayer(1, surface_size);
  content_layer->set_main_thread_scrolling_reasons(
      MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Note: we can use the same clip layer for both since both calls to
  // CreateScrollableLayer() use the same surface size.
  std::unique_ptr<LayerImpl> scroll_layer =
      CreateScrollableLayer(2, surface_size);
  scroll_layer->test_properties()->AddChild(std::move(content_layer));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(scroll_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  // Scrolling fails because the content layer is asking to be scrolled on the
  // main thread.
  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnMainThread) {
  gfx::Size viewport_size(20, 20);
  float page_scale = 2.f;

  SetupScrollAndContentsLayers(viewport_size);

  // Setup the layers so that the outer viewport is scrollable.
  host_impl_->InnerViewportScrollLayer()->test_properties()->parent->SetBounds(
      viewport_size);
  host_impl_->OuterViewportScrollLayer()->SetBounds(gfx::Size(40, 40));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);
  DrawFrame();

  LayerImpl* root_container = host_impl_->OuterViewportContainerLayer();
  EXPECT_EQ(viewport_size, root_container->bounds());

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  LayerImpl* root_scroll = host_impl_->OuterViewportScrollLayer();
  gfx::ScrollOffset expected_max_scroll = root_scroll->MaxScrollOffset();
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // Set new page scale from main thread.
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1.f, 2.f);

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, root_scroll->MaxScrollOffset());

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1.f, host_impl_->active_tree()->page_scale_delta());
}

TEST_F(LayerTreeHostImplTest, ScrollRootAndChangePageScaleOnImplThread) {
  gfx::Size viewport_size(20, 20);
  float page_scale = 2.f;

  SetupScrollAndContentsLayers(viewport_size);

  // Setup the layers so that the outer viewport is scrollable.
  host_impl_->InnerViewportScrollLayer()->test_properties()->parent->SetBounds(
      viewport_size);
  host_impl_->OuterViewportScrollLayer()->SetBounds(gfx::Size(40, 40));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  LayerImpl* root_container = host_impl_->OuterViewportContainerLayer();
  EXPECT_EQ(viewport_size, root_container->bounds());

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  LayerImpl* root_scroll = host_impl_->OuterViewportScrollLayer();
  gfx::ScrollOffset expected_max_scroll = root_scroll->MaxScrollOffset();
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // Set new page scale on impl thread by pinching.
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(page_scale, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd(EndState().get());

  DrawOneFrame();

  // The scroll delta is not scaled because the main thread did not scale.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should also have been updated.
  EXPECT_EQ(expected_max_scroll, root_scroll->MaxScrollOffset());

  // The page scale delta should match the new scale on the impl side.
  EXPECT_EQ(page_scale, host_impl_->active_tree()->current_page_scale_factor());
}

TEST_F(LayerTreeHostImplTest, PageScaleDeltaAppliedToRootScrollLayerOnly) {
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 2.f);
  gfx::Size surface_size(10, 10);
  float default_page_scale = 1.f;
  gfx::Transform default_page_scale_matrix;
  default_page_scale_matrix.Scale(default_page_scale, default_page_scale);

  float new_page_scale = 2.f;
  gfx::Transform new_page_scale_matrix;
  new_page_scale_matrix.Scale(new_page_scale, new_page_scale);

  // Create a normal scrollable root layer and another scrollable child layer.
  LayerImpl* scroll = SetupScrollAndContentsLayers(surface_size);
  scroll->SetDrawsContent(true);
  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();
  LayerImpl* child = scroll->test_properties()->children[0];
  child->SetDrawsContent(true);

  std::unique_ptr<LayerImpl> scrollable_child_clip =
      LayerImpl::Create(host_impl_->active_tree(), 6);
  std::unique_ptr<LayerImpl> scrollable_child =
      CreateScrollableLayer(7, surface_size);
  scrollable_child_clip->test_properties()->AddChild(
      std::move(scrollable_child));
  child->test_properties()->AddChild(std::move(scrollable_child_clip));
  LayerImpl* grand_child = child->test_properties()->children[0];
  grand_child->SetDrawsContent(true);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Set new page scale on impl thread by pinching.
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(new_page_scale, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd(EndState().get());
  DrawOneFrame();

  // Make sure all the layers are drawn with the page scale delta applied, i.e.,
  // the page scale delta on the root layer is applied hierarchically.
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_EQ(1.f, root->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(1.f, root->DrawTransform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale, scroll->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale, scroll->DrawTransform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale, child->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale, child->DrawTransform().matrix().getDouble(1, 1));
  EXPECT_EQ(new_page_scale,
            grand_child->DrawTransform().matrix().getDouble(0, 0));
  EXPECT_EQ(new_page_scale,
            grand_child->DrawTransform().matrix().getDouble(1, 1));
}

TEST_F(LayerTreeHostImplTest, ScrollChildAndChangePageScaleOnMainThread) {
  SetupScrollAndContentsLayers(gfx::Size(30, 30));

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();

  // Make the outer scroll layer scrollable.
  outer_scroll->SetBounds(gfx::Size(50, 50));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  gfx::Vector2d scroll_delta(0, 10);
  gfx::ScrollOffset expected_scroll_delta(scroll_delta);
  gfx::ScrollOffset expected_max_scroll(outer_scroll->MaxScrollOffset());
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  float page_scale = 2.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(page_scale, 1.f,
                                                         page_scale);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawOneFrame();

  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), inner_scroll->element_id(),
                                 expected_scroll_delta));

  // The scroll range should not have changed.
  EXPECT_EQ(outer_scroll->MaxScrollOffset(), expected_max_scroll);

  // The page scale delta remains constant because the impl thread did not
  // scale.
  EXPECT_EQ(1.f, host_impl_->active_tree()->page_scale_delta());
}

TEST_F(LayerTreeHostImplTest, ScrollChildBeyondLimit) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);

  LayerImpl* root =
      CreateBasicVirtualViewportLayers(surface_size, surface_size);

  root->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> grand_child =
      CreateScrollableLayer(13, content_size);

  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(12, content_size);
  LayerImpl* grand_child_layer = grand_child.get();
  child->test_properties()->AddChild(std::move(grand_child));

  LayerImpl* child_layer = child.get();
  root->test_properties()->AddChild(std::move(child));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 5));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(3, 0));

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();
  {
    gfx::Vector2d scroll_delta(-8, -7);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
            .thread);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    LayerImpl* child = host_impl_->active_tree()
                           ->root_layer_for_testing()
                           ->test_properties()
                           ->children[0];
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child_layer->element_id(),
                                   gfx::ScrollOffset(0, -5)));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child->element_id());
  }
}

TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedLatchToChild) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // parent layer isn't scrolled.
  gfx::Size surface_size(100, 100);
  gfx::Size content_size(150, 150);

  LayerImpl* root =
      CreateBasicVirtualViewportLayers(surface_size, surface_size);
  root->test_properties()->force_render_surface = true;

  root->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> grand_child =
      CreateScrollableLayer(13, content_size);

  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(12, content_size);
  LayerImpl* grand_child_layer = grand_child.get();
  child->test_properties()->AddChild(std::move(grand_child));

  LayerImpl* child_layer = child.get();
  root->test_properties()->AddChild(std::move(child));
  host_impl_->active_tree()->SetElementIdsForTesting();
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 30));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 50));

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(10);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, -100)).thread);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should have started scrolling.
  EXPECT_NE(gfx::ScrollOffset(0, 30), grand_child_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::ScrollOffset(0, 0), grand_child_layer->CurrentScrollOffset());
  EXPECT_EQ(gfx::ScrollOffset(0, 50), child_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  // Second ScrollAnimated should still latch to the grand_child_layer. Since it
  // is already at its extent and no scrolling happens, the scroll result must
  // be ignored.
  EXPECT_EQ(
      InputHandler::SCROLL_IGNORED,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, -100)).thread);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(450);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_EQ(gfx::ScrollOffset(0, 0), grand_child_layer->CurrentScrollOffset());
  EXPECT_EQ(gfx::ScrollOffset(0, 50), child_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

TEST_F(LayerTreeHostImplTest, ScrollWithoutBubbling) {
  // Scroll a child layer beyond its maximum scroll range and make sure the
  // the scroll doesn't bubble up to the parent layer.
  gfx::Size surface_size(20, 20);
  gfx::Size viewport_size(10, 10);
  const int kPageScaleLayerId = 1;
  const int kViewportClipLayerId = 2;
  const int kViewportScrollLayerId = 3;
  std::unique_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl_->active_tree(), kPageScaleLayerId);
  std::unique_ptr<LayerImpl> root_clip =
      LayerImpl::Create(host_impl_->active_tree(), kViewportClipLayerId);
  root_clip->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> root_scrolling =
      CreateScrollableLayer(kViewportScrollLayerId, surface_size);
  root_scrolling->test_properties()->is_container_for_fixed_position_layers =
      true;

  std::unique_ptr<LayerImpl> grand_child =
      CreateScrollableLayer(5, surface_size);

  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(4, surface_size);
  LayerImpl* grand_child_layer = grand_child.get();
  child->test_properties()->AddChild(std::move(grand_child));

  LayerImpl* child_layer = child.get();
  root_scrolling->test_properties()->AddChild(std::move(child));
  root_clip->test_properties()->AddChild(std::move(root_scrolling));
  root_ptr->test_properties()->AddChild(std::move(root_clip));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_ptr));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.page_scale = kPageScaleLayerId;
  viewport_ids.inner_viewport_container = kViewportClipLayerId;
  viewport_ids.inner_viewport_scroll = kViewportScrollLayerId;
  host_impl_->active_tree()->SetViewportLayersFromIds(viewport_ids);
  host_impl_->active_tree()->DidBecomeActive();
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 2));
  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 3));

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point()).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled up to its limit.
    LayerImpl* child = host_impl_->active_tree()
                           ->root_layer_for_testing()
                           ->test_properties()
                           ->children[0]
                           ->test_properties()
                           ->children[0]
                           ->test_properties()
                           ->children[0];
    LayerImpl* grand_child = child->test_properties()->children[0];
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child->element_id(),
                                   gfx::ScrollOffset(0, -2)));

    // The child should not have scrolled.
    ExpectNone(*scroll_info.get(), child->element_id());

    // The next time we scroll we should only scroll the parent.
    scroll_delta = gfx::Vector2d(0, -3);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child->scroll_tree_index());
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child->scroll_tree_index());
    host_impl_->ScrollEnd(EndState().get());

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The child should have scrolled up to its limit.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child->element_id(),
                                   gfx::ScrollOffset(0, -3)));

    // The grand child should not have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child->element_id(),
                                   gfx::ScrollOffset(0, -2)));

    // After scrolling the parent, another scroll on the opposite direction
    // should still scroll the child.
    scroll_delta = gfx::Vector2d(0, 7);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child->scroll_tree_index());
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child->scroll_tree_index());
    host_impl_->ScrollEnd(EndState().get());

    scroll_info = host_impl_->ProcessScrollDeltas();

    // The grand child should have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child->element_id(),
                                   gfx::ScrollOffset(0, 5)));

    // The child should not have scrolled.
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child->element_id(),
                                   gfx::ScrollOffset(0, -3)));

    // Scrolling should be adjusted from viewport space.
    host_impl_->active_tree()->PushPageScaleFromMainThread(2.f, 2.f, 2.f);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(2.f);

    scroll_delta = gfx::Vector2d(0, -2);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(1, 1)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_EQ(grand_child->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    scroll_info = host_impl_->ProcessScrollDeltas();

    // Should have scrolled by half the amount in layer space (5 - 2/2)
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(),
                                   grand_child->element_id(),
                                   gfx::ScrollOffset(0, 4)));
  }
}
TEST_F(LayerTreeHostImplTest, ScrollEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible.
  gfx::Size surface_size(10, 10);
  gfx::Size content_size(20, 20);
  const int kPageScaleLayerId = 4;
  const int kViewportClipLayerId = 1;
  const int kViewportScrollLayerId = 2;
  std::unique_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl_->active_tree(), kPageScaleLayerId);
  std::unique_ptr<LayerImpl> root_clip =
      LayerImpl::Create(host_impl_->active_tree(), 3);
  root_clip->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> root_scroll =
      CreateScrollableLayer(kViewportClipLayerId, content_size);
  // Make 'root' the clip layer for child: since they have the same sizes the
  // child will have zero max_scroll_offset and scrolls will bubble.
  std::unique_ptr<LayerImpl> child =
      CreateScrollableLayer(kViewportScrollLayerId, content_size);
  child->test_properties()->is_container_for_fixed_position_layers = true;
  root_scroll->SetBounds(content_size);
  child->SetScrollable(content_size);

  ElementId root_scroll_id = root_scroll->element_id();
  root_scroll->test_properties()->AddChild(std::move(child));
  root_clip->test_properties()->AddChild(std::move(root_scroll));
  root_ptr->test_properties()->AddChild(std::move(root_clip));

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_ptr));
  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.page_scale = kPageScaleLayerId;
  viewport_ids.inner_viewport_container = kViewportClipLayerId;
  viewport_ids.inner_viewport_scroll = kViewportScrollLayerId;
  host_impl_->active_tree()->SetViewportLayersFromIds(viewport_ids);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();
  {
    gfx::ScrollOffset scroll_delta(0, 4);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::WHEEL)
                  .thread);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(scroll_delta))
            .get());
    host_impl_->ScrollEnd(EndState().get());

    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();

    // Only the root scroll should have scrolled.
    ASSERT_EQ(scroll_info->scrolls.size(), 1u);
    EXPECT_TRUE(
        ScrollInfoContains(*scroll_info.get(), root_scroll_id, scroll_delta));
  }
}

TEST_F(LayerTreeHostImplTest, ScrollBeforeRedraw) {
  const int kPageScaleLayerId = 1;
  const int kInnerViewportClipLayerId = 2;
  const int kOuterViewportClipLayerId = 7;
  const int kInnerViewportScrollLayerId = 3;
  const int kOuterViewportScrollLayerId = 8;
  gfx::Size surface_size(10, 10);
  std::unique_ptr<LayerImpl> root_ptr =
      LayerImpl::Create(host_impl_->active_tree(), kPageScaleLayerId);
  std::unique_ptr<LayerImpl> inner_clip =
      LayerImpl::Create(host_impl_->active_tree(), kInnerViewportClipLayerId);
  std::unique_ptr<LayerImpl> inner_scroll =
      CreateScrollableLayer(kInnerViewportScrollLayerId, surface_size);
  std::unique_ptr<LayerImpl> outer_clip =
      LayerImpl::Create(host_impl_->active_tree(), kOuterViewportClipLayerId);
  std::unique_ptr<LayerImpl> outer_scroll =
      CreateScrollableLayer(kOuterViewportScrollLayerId, surface_size);
  inner_clip->test_properties()->force_render_surface = true;
  inner_scroll->test_properties()->is_container_for_fixed_position_layers =
      true;
  outer_scroll->test_properties()->is_container_for_fixed_position_layers =
      true;
  outer_clip->test_properties()->AddChild(std::move(outer_scroll));
  inner_scroll->test_properties()->AddChild(std::move(outer_clip));
  inner_clip->test_properties()->AddChild(std::move(inner_scroll));
  root_ptr->test_properties()->AddChild(std::move(inner_clip));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_ptr));
  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.page_scale = kPageScaleLayerId;
  viewport_ids.inner_viewport_container = kInnerViewportClipLayerId;
  viewport_ids.outer_viewport_container = kOuterViewportClipLayerId;
  viewport_ids.inner_viewport_scroll = kInnerViewportScrollLayerId;
  viewport_ids.outer_viewport_scroll = kOuterViewportScrollLayerId;
  host_impl_->active_tree()->SetViewportLayersFromIds(viewport_ids);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);

  // Draw one frame and then immediately rebuild the layer tree to mimic a tree
  // synchronization.
  DrawFrame();

  const int kPageScaleLayerId2 = 4;
  const int kInnerViewportClipLayerId2 = 5;
  const int kOuterViewportClipLayerId2 = 9;
  const int kInnerViewportScrollLayerId2 = 6;
  const int kOuterViewportScrollLayerId2 = 10;
  host_impl_->active_tree()->DetachLayers();
  std::unique_ptr<LayerImpl> root_ptr2 =
      LayerImpl::Create(host_impl_->active_tree(), 4);
  std::unique_ptr<LayerImpl> inner_clip2 =
      LayerImpl::Create(host_impl_->active_tree(), kInnerViewportClipLayerId2);
  std::unique_ptr<LayerImpl> inner_scroll2 =
      CreateScrollableLayer(kInnerViewportScrollLayerId2, surface_size);
  std::unique_ptr<LayerImpl> outer_clip2 =
      LayerImpl::Create(host_impl_->active_tree(), kOuterViewportClipLayerId2);
  std::unique_ptr<LayerImpl> outer_scroll2 =
      CreateScrollableLayer(kOuterViewportScrollLayerId2, surface_size);
  inner_scroll2->test_properties()->is_container_for_fixed_position_layers =
      true;
  outer_scroll2->test_properties()->is_container_for_fixed_position_layers =
      true;
  outer_clip2->test_properties()->AddChild(std::move(outer_scroll2));
  inner_scroll2->test_properties()->AddChild(std::move(outer_clip2));
  inner_clip2->test_properties()->AddChild(std::move(inner_scroll2));
  inner_clip2->test_properties()->force_render_surface = true;
  root_ptr2->test_properties()->AddChild(std::move(inner_clip2));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_ptr2));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  LayerTreeImpl::ViewportLayerIds viewport_ids2;
  viewport_ids2.page_scale = kPageScaleLayerId2;
  viewport_ids2.inner_viewport_container = kInnerViewportClipLayerId2;
  viewport_ids2.outer_viewport_container = kOuterViewportClipLayerId2;
  viewport_ids2.inner_viewport_scroll = kInnerViewportScrollLayerId2;
  viewport_ids2.outer_viewport_scroll = kOuterViewportScrollLayerId2;
  host_impl_->active_tree()->SetViewportLayersFromIds(viewport_ids2);
  host_impl_->active_tree()->DidBecomeActive();

  // Scrolling should still work even though we did not draw yet.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(), InputHandler::WHEEL)
          .thread);
}

TEST_F(LayerTreeHostImplTest, ScrollAxisAlignedRotatedLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  scroll_layer->SetDrawsContent(true);

  // Rotate the root layer 90 degrees counter-clockwise about its center.
  gfx::Transform rotate_transform;
  rotate_transform.Rotate(-90.0);
  host_impl_->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->transform = rotate_transform;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  gfx::Size surface_size(50, 50);
  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  // Scroll to the right in screen coordinates with a gesture.
  gfx::Vector2d gesture_scroll_delta(10, 0);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gesture_scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // The layer should have scrolled down in its local coordinates.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                         gfx::ScrollOffset(0, gesture_scroll_delta.x())));

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::ScrollOffset wheel_scroll_delta(0, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(
                                                     wheel_scroll_delta))
                           .get());
  host_impl_->ScrollEnd(EndState().get());

  // The layer should have scrolled down in its local coordinates.
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ScrollNonAxisAlignedRotatedLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  int child_clip_layer_id = 6;
  int child_layer_id = 7;
  float child_layer_angle = -20.f;

  // Create a child layer that is rotated to a non-axis-aligned angle.
  std::unique_ptr<LayerImpl> clip_layer =
      LayerImpl::Create(host_impl_->active_tree(), child_clip_layer_id);
  std::unique_ptr<LayerImpl> child =
      CreateScrollableLayer(child_layer_id, scroll_layer->bounds());
  gfx::Transform rotate_transform;
  rotate_transform.Translate(-50.0, -50.0);
  rotate_transform.Rotate(child_layer_angle);
  rotate_transform.Translate(50.0, 50.0);
  clip_layer->test_properties()->transform = rotate_transform;

  // Only allow vertical scrolling.
  gfx::Size scroll_container_bounds =
      gfx::Size(child->bounds().width(), child->bounds().height() / 2);
  clip_layer->SetBounds(scroll_container_bounds);
  child->SetScrollable(scroll_container_bounds);
  // The rotation depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer->test_properties()->transform_origin = gfx::Point3F(
      clip_layer->bounds().width() * 0.5f, clip_layer->bounds().height(), 0.f);
  LayerImpl* child_ptr = child.get();
  clip_layer->test_properties()->AddChild(std::move(child));
  // TODO(pdr): Shouldn't clip_layer be scroll_layer's parent?
  scroll_layer->test_properties()->AddChild(std::move(clip_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  ElementId child_scroll_id = LayerIdToElementIdForTesting(child_layer_id);

  gfx::Size surface_size(50, 50);
  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();
  {
    // Scroll down in screen coordinates with a gesture.
    gfx::Vector2d gesture_scroll_delta(0, 10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(1, 1)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), gesture_scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::ScrollOffset expected_scroll_delta(
        0, std::floor(gesture_scroll_delta.y() *
                      std::cos(gfx::DegToRad(child_layer_angle))));
    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
  {
    // Now reset and scroll the same amount horizontally.
    SetScrollOffsetDelta(child_ptr, gfx::Vector2dF());
    gfx::Vector2d gesture_scroll_delta(10, 0);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(1, 1)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), gesture_scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());

    // The child layer should have scrolled down in its local coordinates an
    // amount proportional to the angle between it and the input scroll delta.
    gfx::ScrollOffset expected_scroll_delta(
        0, std::floor(-gesture_scroll_delta.x() *
                      std::sin(gfx::DegToRad(child_layer_angle))));
    std::unique_ptr<ScrollAndScaleSet> scroll_info =
        host_impl_->ProcessScrollDeltas();
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child_scroll_id,
                                   expected_scroll_delta));

    // The root scroll layer shouldn't have scrolled.
    ExpectNone(*scroll_info.get(), scroll_layer->element_id());
  }
}

TEST_F(LayerTreeHostImplTest, ScrollPerspectiveTransformedLayer) {
  // When scrolling an element with perspective, the distance scrolled
  // depends on the point at which the scroll begins.
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  int child_clip_layer_id = 6;
  int child_layer_id = 7;

  // Create a child layer that is rotated on its x axis, with perspective.
  std::unique_ptr<LayerImpl> clip_layer =
      LayerImpl::Create(host_impl_->active_tree(), child_clip_layer_id);
  std::unique_ptr<LayerImpl> child =
      CreateScrollableLayer(child_layer_id, scroll_layer->bounds());
  LayerImpl* child_ptr = child.get();
  gfx::Transform perspective_transform;
  perspective_transform.Translate(-50.0, -50.0);
  perspective_transform.ApplyPerspectiveDepth(20);
  perspective_transform.RotateAboutXAxis(45);
  perspective_transform.Translate(50.0, 50.0);
  clip_layer->test_properties()->transform = perspective_transform;

  clip_layer->SetBounds(gfx::Size(child_ptr->bounds().width() / 2,
                                  child_ptr->bounds().height() / 2));
  // The transform depends on the layer's transform origin, and the child layer
  // is a different size than the clip, so make sure the clip layer's origin
  // lines up over the child.
  clip_layer->test_properties()->transform_origin = gfx::Point3F(
      clip_layer->bounds().width(), clip_layer->bounds().height(), 0.f);
  clip_layer->test_properties()->AddChild(std::move(child));
  scroll_layer->test_properties()->AddChild(std::move(clip_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  gfx::Size surface_size(50, 50);
  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);

  std::unique_ptr<ScrollAndScaleSet> scroll_info;

  gfx::ScrollOffset gesture_scroll_deltas[4];
  gesture_scroll_deltas[0] = gfx::ScrollOffset(4, 10);
  gesture_scroll_deltas[1] = gfx::ScrollOffset(4, 10);
  gesture_scroll_deltas[2] = gfx::ScrollOffset(10, 0);
  gesture_scroll_deltas[3] = gfx::ScrollOffset(10, 0);

  gfx::ScrollOffset expected_scroll_deltas[4];
  // Perspective affects the vertical delta by a different
  // amount depending on the vertical position of the |viewport_point|.
  expected_scroll_deltas[0] = gfx::ScrollOffset(2, 9);
  expected_scroll_deltas[1] = gfx::ScrollOffset(1, 4);
  // Deltas which start with the same vertical position of the
  // |viewport_point| are subject to identical perspective effects.
  expected_scroll_deltas[2] = gfx::ScrollOffset(5, 0);
  expected_scroll_deltas[3] = gfx::ScrollOffset(5, 0);

  gfx::Point viewport_point(1, 1);

  // Scroll in screen coordinates with a gesture. Each scroll starts
  // where the previous scroll ended, but the scroll position is reset
  // for each scroll.
  for (int i = 0; i < 4; ++i) {
    SetScrollOffsetDelta(child_ptr, gfx::Vector2dF());
    DrawFrame();
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(viewport_point).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    host_impl_->ScrollBy(
        UpdateState(viewport_point,
                    gfx::ScrollOffsetToVector2dF(gesture_scroll_deltas[i]))
            .get());
    viewport_point +=
        gfx::ScrollOffsetToFlooredVector2d(gesture_scroll_deltas[i]);
    host_impl_->ScrollEnd(EndState().get());

    scroll_info = host_impl_->ProcessScrollDeltas();
    ElementId child_scroll_id = LayerIdToElementIdForTesting(child_layer_id);
    EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), child_scroll_id,
                                   expected_scroll_deltas[i]));

    // The root scroll layer should not have scrolled, because the input delta
    // was close to the layer's axis of movement.
    EXPECT_EQ(scroll_info->scrolls.size(), 1u);
  }
}

TEST_F(LayerTreeHostImplTest, ScrollScaledLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));

  // Scale the layer to twice its normal size.
  int scale = 2;
  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  scroll_layer->test_properties()->parent->test_properties()->transform =
      scale_transform;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  gfx::Size surface_size(50, 50);
  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();

  // Scroll down in screen coordinates with a gesture.
  gfx::Vector2d scroll_delta(0, 10);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  host_impl_->ScrollEnd(EndState().get());

  // The layer should have scrolled down in its local coordinates, but half the
  // amount.
  std::unique_ptr<ScrollAndScaleSet> scroll_info =
      host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(
      ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                         gfx::ScrollOffset(0, scroll_delta.y() / scale)));

  // Reset and scroll down with the wheel.
  SetScrollOffsetDelta(scroll_layer, gfx::Vector2dF());
  gfx::ScrollOffset wheel_scroll_delta(0, 10);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), gfx::ScrollOffsetToVector2dF(
                                                     wheel_scroll_delta))
                           .get());
  host_impl_->ScrollEnd(EndState().get());

  // It should apply the scale factor to the scroll delta for the wheel event.
  scroll_info = host_impl_->ProcessScrollDeltas();
  EXPECT_TRUE(ScrollInfoContains(*scroll_info.get(), scroll_layer->element_id(),
                                 wheel_scroll_delta));
}

TEST_F(LayerTreeHostImplTest, ScrollViewportRounding) {
  int width = 332;
  int height = 20;
  int scale = 3;
  SetupScrollAndContentsLayers(gfx::Size(width, height));
  gfx::Size container_bounds = gfx::Size(width * scale - 1, height * scale);
  host_impl_->active_tree()->InnerViewportContainerLayer()->SetBounds(
      container_bounds);
  host_impl_->active_tree()->InnerViewportScrollLayer()->SetScrollable(
      container_bounds);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceScaleFactor(scale);
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);

  LayerImpl* inner_viewport_scroll_layer =
      host_impl_->active_tree()->InnerViewportScrollLayer();
  EXPECT_EQ(gfx::ScrollOffset(0, 0),
            inner_viewport_scroll_layer->MaxScrollOffset());
}

TEST_F(LayerTreeHostImplTest, RootLayerScrollOffsetDelegation) {
  TestInputHandlerClient scroll_watcher;
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 20));
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  LayerImpl* clip_layer =
      scroll_layer->test_properties()->parent->test_properties()->parent;
  clip_layer->SetBounds(gfx::Size(10, 20));
  scroll_layer->SetScrollable(gfx::Size(10, 20));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->BindToClient(&scroll_watcher);

  gfx::Vector2dF initial_scroll_delta(10.f, 10.f);
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset());
  SetScrollOffsetDelta(scroll_layer, initial_scroll_delta);

  EXPECT_EQ(gfx::ScrollOffset(), scroll_watcher.last_set_scroll_offset());

  // Requesting an update results in the current scroll offset being set.
  host_impl_->RequestUpdateForSynchronousInputHandler();
  EXPECT_EQ(gfx::ScrollOffset(initial_scroll_delta),
            scroll_watcher.last_set_scroll_offset());

  // Setting the delegate results in the scrollable_size, max_scroll_offset,
  // page_scale_factor and {min|max}_page_scale_factor being set.
  EXPECT_EQ(gfx::SizeF(100, 100), scroll_watcher.scrollable_size());
  EXPECT_EQ(gfx::ScrollOffset(90, 80), scroll_watcher.max_scroll_offset());
  EXPECT_EQ(1.f, scroll_watcher.page_scale_factor());
  EXPECT_EQ(1.f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(1.f, scroll_watcher.max_page_scale_factor());

  // Put a page scale on the tree.
  host_impl_->active_tree()->PushPageScaleFromMainThread(2.f, 0.5f, 4.f);
  EXPECT_EQ(1.f, scroll_watcher.page_scale_factor());
  EXPECT_EQ(1.f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(1.f, scroll_watcher.max_page_scale_factor());
  // Activation will update the delegate.
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(2.f, scroll_watcher.page_scale_factor());
  EXPECT_EQ(.5f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(4.f, scroll_watcher.max_page_scale_factor());

  // Animating page scale can change the root offset, so it should update the
  // delegate. Also resets the page scale to 1 for the rest of the test.
  host_impl_->LayerTreeHostImpl::StartPageScaleAnimation(
      gfx::Vector2d(0, 0), false, 1.f, base::TimeDelta());
  host_impl_->Animate();
  EXPECT_EQ(1.f, scroll_watcher.page_scale_factor());
  EXPECT_EQ(.5f, scroll_watcher.min_page_scale_factor());
  EXPECT_EQ(4.f, scroll_watcher.max_page_scale_factor());

  // The pinch gesture doesn't put the delegate into a state where the scroll
  // offset is outside of the scroll range.  (this is verified by DCHECKs in the
  // delegate).
  host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                          InputHandler::TOUCHSCREEN);
  host_impl_->PinchGestureBegin();
  host_impl_->PinchGestureUpdate(2.f, gfx::Point());
  host_impl_->PinchGestureUpdate(.5f, gfx::Point());
  host_impl_->PinchGestureEnd(gfx::Point(), true);
  host_impl_->ScrollEnd(EndState().get());

  // Scrolling should be relative to the offset as given by the delegate.
  gfx::Vector2dF scroll_delta(0.f, 10.f);
  gfx::ScrollOffset current_offset(7.f, 8.f);

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);

  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  EXPECT_EQ(ScrollOffsetWithDelta(current_offset, scroll_delta),
            scroll_watcher.last_set_scroll_offset());

  current_offset = gfx::ScrollOffset(42.f, 41.f);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);
  host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
  EXPECT_EQ(current_offset + gfx::ScrollOffset(scroll_delta),
            scroll_watcher.last_set_scroll_offset());
  host_impl_->ScrollEnd(EndState().get());
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(gfx::ScrollOffset());

  // Forces a full tree synchronization and ensures that the scroll delegate
  // sees the correct size of the new tree.
  gfx::Size new_size(42, 24);
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  CreateScrollAndContentsLayers(host_impl_->pending_tree(), new_size);
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(gfx::SizeF(new_size), scroll_watcher.scrollable_size());

  // Tear down the LayerTreeHostImpl before the InputHandlerClient.
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;
}

void CheckLayerScrollDelta(LayerImpl* layer, gfx::Vector2dF scroll_delta) {
  const gfx::Transform target_space_transform =
      layer->draw_properties().target_space_transform;
  EXPECT_TRUE(target_space_transform.IsScaleOrTranslation());
  gfx::Point translated_point;
  target_space_transform.TransformPoint(&translated_point);
  gfx::Point expected_point = gfx::Point() - ToRoundedVector2d(scroll_delta);
  EXPECT_EQ(expected_point.ToString(), translated_point.ToString());
}

TEST_F(LayerTreeHostImplTest,
       ExternalRootLayerScrollOffsetDelegationReflectedInNextDraw) {
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 20));
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  LayerImpl* clip_layer =
      scroll_layer->test_properties()->parent->test_properties()->parent;
  clip_layer->SetBounds(gfx::Size(10, 20));
  scroll_layer->SetScrollable(gfx::Size(10, 20));
  scroll_layer->SetDrawsContent(true);

  // Draw first frame to clear any pending draws and check scroll.
  DrawFrame();
  CheckLayerScrollDelta(scroll_layer, gfx::Vector2dF(0.f, 0.f));
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Set external scroll delta on delegate and notify LayerTreeHost.
  gfx::ScrollOffset scroll_offset(10.f, 10.f);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Check scroll delta reflected in layer.
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_FALSE(frame.has_no_damage);
  CheckLayerScrollDelta(scroll_layer,
                        gfx::ScrollOffsetToVector2dF(scroll_offset));
}

TEST_F(LayerTreeHostImplTest,
       ExternalRootLayerScrollOffsetPreventedByUserNotScrollable) {
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 20));
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  LayerImpl* clip_layer =
      scroll_layer->test_properties()->parent->test_properties()->parent;
  clip_layer->SetBounds(gfx::Size(10, 20));
  scroll_layer->SetScrollable(gfx::Size(10, 20));
  scroll_layer->SetDrawsContent(true);
  host_impl_->active_tree()
      ->InnerViewportScrollLayer()
      ->test_properties()
      ->user_scrollable_vertical = false;
  host_impl_->active_tree()
      ->InnerViewportScrollLayer()
      ->test_properties()
      ->user_scrollable_horizontal = false;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Draw first frame to clear any pending draws and check scroll.
  DrawFrame();
  CheckLayerScrollDelta(scroll_layer, gfx::Vector2dF(0.f, 0.f));
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Set external scroll delta on delegate and notify LayerTreeHost.
  gfx::ScrollOffset scroll_offset(10.f, 10.f);
  host_impl_->SetSynchronousInputHandlerRootScrollOffset(scroll_offset);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  EXPECT_TRUE(frame.has_no_damage);
  CheckLayerScrollDelta(scroll_layer,
                        gfx::ScrollOffsetToVector2dF(gfx::ScrollOffset()));
}

TEST_F(LayerTreeHostImplTest, OverscrollRoot) {
  InputHandlerScrollResult scroll_result;
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // In-bounds scrolling does not affect overscroll.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // Overscroll events are reflected immediately.
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, 50)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // In-bounds scrolling resets accumulated overscroll for the scrolled axes.
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, -50)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, -10)).get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(10, 0)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(-15, 0)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(-5, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, 60)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(-5, 10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(10, -60)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // Overscroll accumulates within the scope of ScrollBegin/ScrollEnd as long
  // as no scroll occurs.
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, -20)).get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -30), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, -20)).get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -20), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -50), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  // Overscroll resets on valid scroll.
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 0), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, 0), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, -20)).get());
  EXPECT_TRUE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, -10), scroll_result.unused_scroll_delta);
  EXPECT_EQ(gfx::Vector2dF(0, -10), host_impl_->accumulated_root_overscroll());
  EXPECT_EQ(scroll_result.accumulated_root_overscroll,
            host_impl_->accumulated_root_overscroll());

  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplTest, OverscrollChildWithoutBubbling) {
  // Scroll child layers beyond their maximum scroll range and make sure root
  // overscroll does not accumulate.
  InputHandlerScrollResult scroll_result;
  gfx::Size surface_size(10, 10);
  const int kInnerViewportClipLayerId = 4;
  const int kInnerViewportScrollLayerId = 1;
  std::unique_ptr<LayerImpl> root_clip =
      LayerImpl::Create(host_impl_->active_tree(), kInnerViewportClipLayerId);
  root_clip->test_properties()->force_render_surface = true;

  std::unique_ptr<LayerImpl> root =
      CreateScrollableLayer(kInnerViewportScrollLayerId, surface_size);

  std::unique_ptr<LayerImpl> grand_child =
      CreateScrollableLayer(3, surface_size);

  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(2, surface_size);
  LayerImpl* grand_child_layer = grand_child.get();
  child->test_properties()->AddChild(std::move(grand_child));

  LayerTreeImpl::ViewportLayerIds viewport_ids;
  viewport_ids.inner_viewport_container = kInnerViewportClipLayerId;
  viewport_ids.inner_viewport_scroll = kInnerViewportScrollLayerId;
  host_impl_->active_tree()->SetViewportLayersFromIds(viewport_ids);

  LayerImpl* child_layer = child.get();
  root->test_properties()->AddChild(std::move(child));
  root_clip->test_properties()->AddChild(std::move(root));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_clip));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(child_layer->element_id(),
                                                     gfx::ScrollOffset(0, 3));
  grand_child_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          grand_child_layer->element_id(), gfx::ScrollOffset(0, 2));

  host_impl_->active_tree()->SetDeviceViewportSize(surface_size);
  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, -10);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point()).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());

    // The next time we scroll we should only scroll the parent, but overscroll
    // should still not reach the root layer.
    scroll_delta = gfx::Vector2d(0, -30);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());

    // After scrolling the parent, another scroll on the opposite direction
    // should scroll the child.
    scroll_delta = gfx::Vector2d(0, 70);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
              grand_child_layer->scroll_tree_index());
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollChildEventBubbling) {
  // When we try to scroll a non-scrollable child layer, the scroll delta
  // should be applied to one of its ancestors if possible. Overscroll should
  // be reflected only when it has bubbled up to the root scrolling layer.
  InputHandlerScrollResult scroll_result;
  SetupScrollAndContentsLayers(gfx::Size(20, 20));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();
  {
    gfx::Vector2d scroll_delta(0, 8);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                                InputHandler::WHEEL)
                  .thread);
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 6), host_impl_->accumulated_root_overscroll());
    scroll_result =
        host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF(0, 14), host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollAlways) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(50, 50));
  LayerImpl* clip_layer =
      scroll_layer->test_properties()->parent->test_properties()->parent;

  clip_layer->SetBounds(gfx::Size(50, 50));
  scroll_layer->SetScrollable(gfx::Size(50, 50));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 0.5f, 4.f);
  DrawFrame();
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());

  // Even though the layer can't scroll the overscroll still happens.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get());
  EXPECT_FALSE(scroll_result.did_scroll);
  EXPECT_TRUE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(0, 10), host_impl_->accumulated_root_overscroll());
}

TEST_F(LayerTreeHostImplTest, NoOverscrollWhenNotAtEdge) {
  InputHandlerScrollResult scroll_result;
  gfx::Size viewport_size(100, 100);
  gfx::Size content_size(200, 200);
  LayerImpl* root_scroll_layer =
      CreateBasicVirtualViewportLayers(viewport_size, viewport_size);
  host_impl_->active_tree()->OuterViewportScrollLayer()->SetBounds(
      content_size);
  root_scroll_layer->SetBounds(content_size);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();
  {
    // Edge glow effect should be applicable only upon reaching Edges
    // of the content. unnecessary glow effect calls shouldn't be
    // called while scrolling up without reaching the edge of the content.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                                InputHandler::WHEEL)
                  .thread);
    scroll_result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, 100)).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    scroll_result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, -2.30f)).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    host_impl_->ScrollEnd(EndState().get());
    // unusedrootDelta should be subtracted from applied delta so that
    // unwanted glow effect calls are not called.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    scroll_result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, 20)).get());
    EXPECT_TRUE(scroll_result.did_scroll);
    EXPECT_TRUE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.000000f, 17.699997f),
                        host_impl_->accumulated_root_overscroll());

    scroll_result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(0.02f, -0.01f)).get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_VECTOR2DF_EQ(gfx::Vector2dF(0.000000f, 17.699997f),
                        host_impl_->accumulated_root_overscroll());
    host_impl_->ScrollEnd(EndState().get());
    // TestCase to check  kEpsilon, which prevents minute values to trigger
    // gloweffect without reaching edge.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                                InputHandler::WHEEL)
                  .thread);
    scroll_result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(-0.12f, 0.1f)).get());
    EXPECT_FALSE(scroll_result.did_scroll);
    EXPECT_FALSE(scroll_result.did_overscroll_root);
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              host_impl_->accumulated_root_overscroll().ToString());
    host_impl_->ScrollEnd(EndState().get());
  }
}

TEST_F(LayerTreeHostImplTest, NoOverscrollOnNonViewportLayers) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  LayerImpl* content_layer =
      CreateBasicVirtualViewportLayers(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* scroll_layer = nullptr;

  // Initialization: Add a nested scrolling layer, simulating a scrolling div.
  {
    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 11);
    scroll->SetBounds(gfx::Size(400, 400));
    scroll->SetScrollable(content_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    scroll_layer = scroll.get();

    content_layer->test_properties()->AddChild(std::move(scroll));
    layer_tree_impl->BuildPropertyTreesForTesting();
  }

  InputHandlerScrollResult scroll_result;
  DrawFrame();

  // Start a scroll gesture, ensure it's scrolling the subscroller.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(100.f, 100.f),
                     scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                     outer_scroll_layer->CurrentScrollOffset());
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(120.f, 140.f)).get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200.f, 200.f),
                     scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                     outer_scroll_layer->CurrentScrollOffset());
    EXPECT_FALSE(result.did_overscroll_root);
  }

  // Continue the scroll. Ensure that scrolling beyond the child's extent
  // doesn't consume the delta but it isn't counted as overscroll.
  {
    InputHandlerScrollResult result = host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(20.f, 40.f)).get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200.f, 200.f),
                     scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                     outer_scroll_layer->CurrentScrollOffset());
    EXPECT_FALSE(result.did_overscroll_root);
  }

  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplTest, OverscrollOnMainThread) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size content_size(50, 50);
  const gfx::Size viewport_size(50, 50);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  host_impl_->active_tree()
      ->InnerViewportScrollLayer()
      ->set_main_thread_scrolling_reasons(
          MainThreadScrollingReason::kThreadedScrollingDisabled);
  host_impl_->active_tree()
      ->OuterViewportScrollLayer()
      ->set_main_thread_scrolling_reasons(
          MainThreadScrollingReason::kThreadedScrollingDisabled);

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // Overscroll initiated outside layers will be handled by the main thread.
  EXPECT_EQ(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 60)));
  EXPECT_EQ(InputHandler::SCROLL_ON_MAIN_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(0, 60)).get(),
                              InputHandler::WHEEL)
                .thread);

  // Overscroll initiated inside layers will be handled by the main thread.
  EXPECT_NE(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 0)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_MAIN_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 0)).get(), InputHandler::WHEEL)
          .thread);
}

// Test that scrolling the inner viewport directly works, as can happen when the
// scroll chains up to it from an sibling of the outer viewport.
TEST_F(LayerTreeHostImplTest, ScrollFromOuterViewportSibling) {
  const gfx::Size viewport_size(100, 100);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  CreateBasicVirtualViewportLayers(viewport_size, viewport_size);
  host_impl_->active_tree()->SetTopControlsHeight(10);
  host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  LayerImpl* scroll_layer = nullptr;

  // Create a scrolling layer that's parented directly to the inner viewport.
  // This will test that scrolls that chain up to the inner viewport without
  // passing through the outer viewport still scroll correctly and affect
  // browser controls.
  {
    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 11);
    scroll->SetBounds(gfx::Size(400, 400));
    scroll->SetScrollable(viewport_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    scroll_layer = scroll.get();

    inner_scroll_layer->test_properties()->AddChild(std::move(scroll));

    // Move the outer viewport layer away so that scrolls won't target it.
    host_impl_->active_tree()->OuterViewportContainerLayer()->SetPosition(
        gfx::PointF(400, 400));

    layer_tree_impl->BuildPropertyTreesForTesting();

    float min_page_scale = 1.f, max_page_scale = 4.f;
    float page_scale_factor = 2.f;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  }

  // Fully scroll the child.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000.f, 1000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_EQ(1.f,
              host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(300.f, 300.f),
                     scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     inner_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());
  }

  // Scrolling on the child now should chain up directly to the inner viewport.
  // Scrolling it should cause browser controls to hide. The outer viewport
  // should not be affected.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    gfx::Vector2d scroll_delta(0, 10);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    EXPECT_EQ(0.f,
              host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     inner_scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 10.f),
                     inner_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());
    host_impl_->ScrollEnd(EndState().get());
  }
}

// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport. This happens in the
// rootScroller proposal.
TEST_F(LayerTreeHostImplTest, ScrollChainingWithReplacedOuterViewport) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  LayerImpl* content_layer =
      CreateBasicVirtualViewportLayers(viewport_size, content_size);
  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  LayerImpl* scroll_layer = nullptr;
  LayerImpl* child_scroll_layer = nullptr;

  // Initialization: Add two nested scrolling layers, simulating a scrolling div
  // with another scrolling div inside it. Set the outer "div" to be the outer
  // viewport.
  {
    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 11);
    scroll->SetBounds(gfx::Size(400, 400));
    scroll->SetScrollable(content_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    std::unique_ptr<LayerImpl> scroll2 = LayerImpl::Create(layer_tree_impl, 13);
    scroll2->SetBounds(gfx::Size(500, 500));
    scroll2->SetScrollable(gfx::Size(300, 300));
    scroll2->SetElementId(LayerIdToElementIdForTesting(scroll2->id()));
    scroll2->SetDrawsContent(true);

    scroll_layer = scroll.get();
    child_scroll_layer = scroll2.get();

    scroll->test_properties()->AddChild(std::move(scroll2));
    content_layer->test_properties()->AddChild(std::move(scroll));
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = layer_tree_impl->PageScaleLayer()->id();
    viewport_ids.inner_viewport_scroll = inner_scroll_layer->id();
    viewport_ids.outer_viewport_scroll = scroll_layer->id();
    layer_tree_impl->SetViewportLayersFromIds(viewport_ids);
    layer_tree_impl->BuildPropertyTreesForTesting();
  }

  // Scroll should target the nested scrolling layer in the content and then
  // chain to the parent scrolling layer which is now set as the outer
  // viewport. The original outer viewport layer shouldn't get any scroll here.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(200.f, 200.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200.f, 200.f),
                     child_scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(200.f, 200.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                     outer_scroll_layer->CurrentScrollOffset());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(200.f, 200.f),
                     scroll_layer->CurrentScrollOffset());
  }

  // Now that the nested scrolling layers are fully scrolled, further scrolls
  // would normally chain up to the "outer viewport" but since we've set the
  // scrolling content as the outer viewport, it should stop chaining there.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());
  }

  // Zoom into the page by a 2X factor so that the inner viewport becomes
  // scrollable.
  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 2.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Reset the parent scrolling layer (i.e. the current outer viewport) so that
  // we can ensure viewport scrolling works correctly.
  scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset(0, 0));

  // Scrolling the content layer should now scroll the inner viewport first,
  // and then chain up to the current outer viewport (i.e. the parent scroll
  // layer).
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(50.f, 50.f),
                     inner_scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(0.f, 0.f),
                     outer_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(50.f, 50.f),
                     scroll_layer->CurrentScrollOffset());
  }
}
// Test that scrolls chain correctly when a child scroller on the page (e.g. a
// scrolling div) is set as the outer viewport but scrolls start from a layer
// that's not a descendant of the outer viewport. This happens in the
// rootScroller proposal.
TEST_F(LayerTreeHostImplTest, RootScrollerScrollNonDescendant) {
  const gfx::Size content_size(300, 300);
  const gfx::Size viewport_size(300, 300);

  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();

  LayerImpl* content_layer =
      CreateBasicVirtualViewportLayers(viewport_size, content_size);
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  LayerImpl* outer_scroll_layer = nullptr;
  LayerImpl* sibling_scroll_layer = nullptr;

  // Initialization: Add a scrolling layer, simulating an ordinary DIV, to be
  // set as the outer viewport. Add a sibling scrolling layer that isn't a child
  // of the outer viewport scroll layer.
  {
    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 11);
    scroll->SetBounds(gfx::Size(1200, 1200));
    scroll->SetScrollable(content_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    outer_scroll_layer = scroll.get();

    content_layer->test_properties()->AddChild(std::move(scroll));

    // Create the non-descendant.
    std::unique_ptr<LayerImpl> scroll2 = LayerImpl::Create(layer_tree_impl, 15);
    scroll2->SetBounds(gfx::Size(1200, 1200));
    scroll2->SetScrollable(gfx::Size(600, 600));
    scroll2->SetElementId(LayerIdToElementIdForTesting(scroll2->id()));
    scroll2->SetDrawsContent(true);

    sibling_scroll_layer = scroll2.get();

    content_layer->test_properties()->AddChild(std::move(scroll2));

    LayerImpl* inner_container =
        host_impl_->active_tree()->InnerViewportContainerLayer();
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = layer_tree_impl->PageScaleLayer()->id();
    viewport_ids.inner_viewport_container = inner_container->id();
    viewport_ids.inner_viewport_scroll = inner_scroll_layer->id();
    viewport_ids.outer_viewport_scroll = outer_scroll_layer->id();
    layer_tree_impl->SetViewportLayersFromIds(viewport_ids);
    layer_tree_impl->BuildPropertyTreesForTesting();

    ASSERT_EQ(outer_scroll_layer, layer_tree_impl->OuterViewportScrollLayer());
  }

  // Scrolls should target the non-descendant scroller. Chaining should not
  // propagate to the outer viewport scroll layer.
  {
    // This should fully scroll the layer.
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000.f, 1000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600.f, 600.f),
                     sibling_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());

    // Scrolling now should chain up but, since the outer viewport is a sibling
    // rather than an ancestor, we shouldn't chain to it.
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(1000.f, 1000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600.f, 600.f),
                     sibling_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());
  }

  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 1.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);

  gfx::Vector2dF viewport_size_vec(viewport_size.width(),
                                   viewport_size.height());

  // Reset the scroll offset.
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());

  // Now pinch-zoom in. Anchoring should cause scrolling only on the inner
  // viewport layer.
  {
    // Pinch in to the middle of the screen. The inner viewport should scroll
    // to keep the gesture anchored but not the outer or the sibling scroller.
    page_scale_factor = 2.f;
    gfx::Point anchor(viewport_size.width() / 2, viewport_size.height() / 2);
    host_impl_->ScrollBegin(BeginState(anchor).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_factor, anchor);
    host_impl_->PinchGestureEnd(anchor, true);

    EXPECT_VECTOR_EQ(gfx::Vector2dF(anchor.x() / 2, anchor.y() / 2),
                     inner_scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollBy(UpdateState(anchor, viewport_size_vec).get());

    EXPECT_VECTOR_EQ(ScaleVector2d(viewport_size_vec, 1.f / page_scale_factor),
                     inner_scroll_layer->CurrentScrollOffset());
    // TODO(bokan): This doesn't yet work but we'll probably want to fix this
    // at some point.
    // EXPECT_VECTOR_EQ(
    //     gfx::Vector2dF(),
    //     outer_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     sibling_scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollEnd(EndState().get());
  }

  // Reset the scroll offsets
  sibling_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());
  inner_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());
  outer_scroll_layer->SetCurrentScrollOffset(gfx::ScrollOffset());

  // Scrolls over the sibling while pinched in should scroll the sibling first,
  // but then chain up to the inner viewport so that the user can still pan
  // around. The outer viewport should be unaffected.
  {
    // This should fully scroll the sibling but, because we latch to the
    // scroller, it shouldn't chain up to the inner viewport yet.
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000.f, 2000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(600.f, 600.f),
                     sibling_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     inner_scroll_layer->CurrentScrollOffset());

    // Scrolling now should chain up to the inner viewport.
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000.f, 2000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(ScaleVector2d(viewport_size_vec, 1 / page_scale_factor),
                     inner_scroll_layer->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());

    // No more scrolling should be possible.
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(2000.f, 2000.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(),
                     outer_scroll_layer->CurrentScrollOffset());
  }
}

TEST_F(LayerTreeHostImplTest, OverscrollOnImplThread) {
  InputHandlerScrollResult scroll_result;
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());

  const gfx::Size content_size(50, 50);
  const gfx::Size viewport_size(50, 50);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  // By default, no main thread scrolling reasons should exist.
  LayerImpl* scroll_layer =
      host_impl_->active_tree()->InnerViewportScrollLayer();
  EXPECT_EQ(MainThreadScrollingReason::kNotScrollingOnMain,
            scroll_layer->main_thread_scrolling_reasons());

  DrawFrame();

  // Overscroll initiated outside layers will be handled by the impl thread.
  EXPECT_EQ(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 60)));
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point(0, 60)).get(),
                              InputHandler::WHEEL)
                .thread);

  // Overscroll initiated inside layers will be handled by the impl thread.
  EXPECT_NE(nullptr, host_impl_->active_tree()->FindLayerThatIsHitByPoint(
                         gfx::PointF(0, 0)));
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, 0)).get(), InputHandler::WHEEL)
          .thread);
}

class BlendStateCheckLayer : public LayerImpl {
 public:
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
        quad_visible_rect_(5, 5, 5, 5) {
    resource_id_ = resource_provider_->ImportResource(
        viz::TransferableResource::MakeSoftware(
            viz::SharedBitmap::GenerateId(), gfx::Size(1, 1), viz::RGBA_8888),
        viz::SingleReleaseCallback::Create(base::DoNothing()));
    SetBounds(gfx::Size(10, 10));
    SetDrawsContent(true);
  }

  void ReleaseResources() override {
    resource_provider_->RemoveImportedResource(resource_id_);
  }

  void AppendQuads(viz::RenderPass* render_pass,
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
        resource_id_, gfx::RectF(0.f, 0.f, 1.f, 1.f), gfx::Size(1, 1), false,
        false, false, false);

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
  viz::ClientResourceProvider* resource_provider_;
  bool blend_;
  bool has_render_surface_;
  LayerImpl* comparison_layer_;
  bool quads_appended_;
  gfx::Rect quad_rect_;
  gfx::Rect opaque_content_rect_;
  gfx::Rect quad_visible_rect_;
  viz::ResourceId resource_id_;
};

TEST_F(LayerTreeHostImplTest, BlendingOffWhenDrawingOpaqueLayers) {
  {
    std::unique_ptr<LayerImpl> root =
        LayerImpl::Create(host_impl_->active_tree(), 1);
    root->SetBounds(gfx::Size(10, 10));
    root->SetDrawsContent(false);
    root->test_properties()->force_render_surface = true;
    host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  }
  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();

  root->test_properties()->AddChild(std::make_unique<BlendStateCheckLayer>(
      host_impl_->active_tree(), 2, host_impl_->resource_provider()));
  auto* layer1 =
      static_cast<BlendStateCheckLayer*>(root->test_properties()->children[0]);
  layer1->SetPosition(gfx::PointF(2.f, 2.f));

  TestFrameData frame;

  // Opaque layer, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent content and painting, so drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 0.5f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with translucent opacity and painting, drawn with blending.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 0.5f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  layer1->test_properties()->AddChild(std::make_unique<BlendStateCheckLayer>(
      host_impl_->active_tree(), 3, host_impl_->resource_provider()));
  auto* layer2 = static_cast<BlendStateCheckLayer*>(
      layer1->test_properties()->children[0]);
  layer2->SetPosition(gfx::PointF(4.f, 4.f));

  // 2 opaque layers, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 1.f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  layer2->test_properties()->opacity = 1.f;
  layer2->NoteLayerPropertyChanged();
  layer2->SetExpectation(false, false, root);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content, drawn with blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, root);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent content but opaque painting, drawn without
  // blending.
  // Child layer with opaque content, drawn without blending.
  layer1->SetContentsOpaque(true);
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, root);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Parent layer with translucent opacity and opaque content. Since it has a
  // drawing child, it's drawn to a render surface which carries the opacity,
  // so it's itself drawn without blending.
  // Child layer with opaque content, drawn without blending (parent surface
  // carries the inherited opacity).
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 0.5f;
  layer1->NoteLayerPropertyChanged();
  layer1->test_properties()->force_render_surface = true;
  layer1->SetExpectation(false, true, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetExpectation(false, false, layer1);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);
  layer1->test_properties()->force_render_surface = false;

  // Draw again, but with child non-opaque, to make sure
  // layer1 not culled.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 1.f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  layer2->test_properties()->opacity = 0.5f;
  layer2->NoteLayerPropertyChanged();
  layer2->SetExpectation(true, false, layer1);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // A second way of making the child non-opaque.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 1.f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(false);
  layer2->test_properties()->opacity = 1.f;
  layer2->NoteLayerPropertyChanged();
  layer2->SetExpectation(true, false, root);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // And when the layer says its not opaque but is painted opaque, it is not
  // blended.
  layer1->SetContentsOpaque(true);
  layer1->test_properties()->opacity = 1.f;
  layer1->NoteLayerPropertyChanged();
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  layer2->SetContentsOpaque(true);
  layer2->test_properties()->opacity = 1.f;
  layer2->NoteLayerPropertyChanged();
  layer2->SetExpectation(false, false, root);
  layer2->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  EXPECT_TRUE(layer2->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents partially culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 5, 2));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents culled, drawn with blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(7, 5, 3, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(true, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);

  // Layer with partially opaque contents and translucent contents culled, drawn
  // without blending.
  layer1->SetContentsOpaque(false);
  layer1->SetQuadRect(gfx::Rect(5, 5, 5, 5));
  layer1->SetQuadVisibleRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetOpaqueContentRect(gfx::Rect(5, 5, 2, 5));
  layer1->SetExpectation(false, false, root);
  layer1->SetUpdateRect(gfx::Rect(layer1->bounds()));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->set_needs_update_draw_properties();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  EXPECT_TRUE(layer1->quads_appended());
  host_impl_->DidDrawAllLayers(frame);
}

static bool MayContainVideoBitSetOnFrameData(LayerTreeHostImpl* host_impl) {
  host_impl->active_tree()->BuildPropertyTreesForTesting();
  host_impl->active_tree()->set_needs_update_draw_properties();
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl->PrepareToDraw(&frame));
  host_impl->DrawLayers(&frame);
  host_impl->DidDrawAllLayers(frame);
  return frame.may_contain_video;
}

TEST_F(LayerTreeHostImplTest, MayContainVideo) {
  gfx::Size big_size(1000, 1000);
  host_impl_->active_tree()->SetDeviceViewportSize(big_size);

  int layer_id = 1;
  host_impl_->active_tree()->SetRootLayerForTesting(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), layer_id++));
  auto* root =
      static_cast<DidDrawCheckLayer*>(*host_impl_->active_tree()->begin());

  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), layer_id++));
  auto* video_layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children.back());
  video_layer->set_may_contain_video(true);
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  // Test with the video layer occluded.
  root->test_properties()->AddChild(
      DidDrawCheckLayer::Create(host_impl_->active_tree(), layer_id++));
  auto* large_layer =
      static_cast<DidDrawCheckLayer*>(root->test_properties()->children.back());
  large_layer->SetBounds(big_size);
  large_layer->SetContentsOpaque(true);
  EXPECT_FALSE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  // Remove the large layer.
  root->test_properties()->RemoveChild(large_layer);
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  // Move the video layer so it goes beyond the root.
  video_layer->SetPosition(gfx::PointF(100.f, 100.f));
  EXPECT_FALSE(MayContainVideoBitSetOnFrameData(host_impl_.get()));

  video_layer->SetPosition(gfx::PointF(0.f, 0.f));
  video_layer->NoteLayerPropertyChanged();
  EXPECT_TRUE(MayContainVideoBitSetOnFrameData(host_impl_.get()));
}

class LayerTreeHostImplViewportCoveredTest : public LayerTreeHostImplTest {
 protected:
  LayerTreeHostImplViewportCoveredTest()
      : gutter_quad_material_(viz::DrawQuad::SOLID_COLOR),
        child_(nullptr),
        did_activate_pending_tree_(false) {}

  std::unique_ptr<LayerTreeFrameSink> CreateFakeLayerTreeFrameSink(
      bool software) {
    if (software)
      return FakeLayerTreeFrameSink::CreateSoftware();
    return FakeLayerTreeFrameSink::Create3d();
  }

  void SetupActiveTreeLayers() {
    host_impl_->active_tree()->set_background_color(SK_ColorGRAY);
    host_impl_->active_tree()->SetRootLayerForTesting(
        LayerImpl::Create(host_impl_->active_tree(), 1));
    host_impl_->active_tree()
        ->root_layer_for_testing()
        ->test_properties()
        ->force_render_surface = true;
    host_impl_->active_tree()
        ->root_layer_for_testing()
        ->test_properties()
        ->AddChild(std::make_unique<BlendStateCheckLayer>(
            host_impl_->active_tree(), 2, host_impl_->resource_provider()));
    child_ = static_cast<BlendStateCheckLayer*>(host_impl_->active_tree()
                                                    ->root_layer_for_testing()
                                                    ->test_properties()
                                                    ->children[0]);
    child_->SetExpectation(false, false,
                           host_impl_->active_tree()->root_layer_for_testing());
    child_->SetContentsOpaque(true);
  }

  // Expect no gutter rects.
  void TestLayerCoversFullViewport() {
    gfx::Rect layer_rect(viewport_size_);
    child_->SetPosition(gfx::PointF(layer_rect.origin()));
    child_->SetBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();

    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    ASSERT_EQ(1u, frame.render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(frame.render_passes[0]->quad_list));
    EXPECT_EQ(1u, frame.render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(frame.render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(frame.render_passes[0]->quad_list);
    host_impl_->DidDrawAllLayers(frame);
  }

  // Expect fullscreen gutter rect.
  void SetUpEmptylayer() {
    gfx::Rect layer_rect(0, 0, 0, 0);
    child_->SetPosition(gfx::PointF(layer_rect.origin()));
    child_->SetBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
  }

  void VerifyEmptyLayerRenderPasses(const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(1u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(1u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(render_passes[0]->quad_list);
  }

  void TestEmptyLayer() {
    SetUpEmptylayer();
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    VerifyEmptyLayerRenderPasses(frame.render_passes);
    host_impl_->DidDrawAllLayers(frame);
  }

  void TestEmptyLayerWithOnDraw() {
    SetUpEmptylayer();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyEmptyLayerRenderPasses(last_on_draw_render_passes_);
  }

  // Expect four surrounding gutter rects.
  void SetUpLayerInMiddleOfViewport() {
    gfx::Rect layer_rect(500, 500, 200, 200);
    child_->SetPosition(gfx::PointF(layer_rect.origin()));
    child_->SetBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
  }

  void VerifyLayerInMiddleOfViewport(const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(4u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(5u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);

    VerifyQuadsExactlyCoverViewport(render_passes[0]->quad_list);
  }

  void TestLayerInMiddleOfViewport() {
    SetUpLayerInMiddleOfViewport();
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    VerifyLayerInMiddleOfViewport(frame.render_passes);
    host_impl_->DidDrawAllLayers(frame);
  }

  void TestLayerInMiddleOfViewportWithOnDraw() {
    SetUpLayerInMiddleOfViewport();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyLayerInMiddleOfViewport(last_on_draw_render_passes_);
  }

  // Expect no gutter rects.
  void SetUpLayerIsLargerThanViewport() {
    gfx::Rect layer_rect(viewport_size_.width() + 10,
                         viewport_size_.height() + 10);
    child_->SetPosition(gfx::PointF(layer_rect.origin()));
    child_->SetBounds(layer_rect.size());
    child_->SetQuadRect(gfx::Rect(layer_rect.size()));
    child_->SetQuadVisibleRect(gfx::Rect(layer_rect.size()));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
  }

  void VerifyLayerIsLargerThanViewport(
      const viz::RenderPassList& render_passes) {
    ASSERT_EQ(1u, render_passes.size());

    EXPECT_EQ(0u, CountGutterQuads(render_passes[0]->quad_list));
    EXPECT_EQ(1u, render_passes[0]->quad_list.size());
    ValidateTextureDrawQuads(render_passes[0]->quad_list);
  }

  void TestLayerIsLargerThanViewport() {
    SetUpLayerIsLargerThanViewport();
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    VerifyLayerIsLargerThanViewport(frame.render_passes);
    host_impl_->DidDrawAllLayers(frame);
  }

  void TestLayerIsLargerThanViewportWithOnDraw() {
    SetUpLayerIsLargerThanViewport();
    gfx::Transform identity;
    gfx::Rect viewport(viewport_size_);
    bool resourceless_software_draw = true;
    host_impl_->OnDraw(identity, viewport, resourceless_software_draw, false);
    VerifyLayerIsLargerThanViewport(last_on_draw_render_passes_);
  }

  void DidActivateSyncTree() override {
    LayerTreeHostImplTest::DidActivateSyncTree();
    did_activate_pending_tree_ = true;
  }

  void set_gutter_quad_material(viz::DrawQuad::Material material) {
    gutter_quad_material_ = material;
  }
  void set_gutter_texture_size(const gfx::Size& gutter_texture_size) {
    gutter_texture_size_ = gutter_texture_size;
  }

 protected:
  size_t CountGutterQuads(const viz::QuadList& quad_list) {
    size_t num_gutter_quads = 0;
    for (auto* quad : quad_list) {
      num_gutter_quads += (quad->material == gutter_quad_material_) ? 1 : 0;
    }
    return num_gutter_quads;
  }

  void VerifyQuadsExactlyCoverViewport(const viz::QuadList& quad_list) {
    LayerTestCommon::VerifyQuadsExactlyCoverRect(
        quad_list, gfx::Rect(DipSizeToPixelSize(viewport_size_)));
  }

  // Make sure that the texture coordinates match their expectations.
  void ValidateTextureDrawQuads(const viz::QuadList& quad_list) {
    for (auto* quad : quad_list) {
      if (quad->material != viz::DrawQuad::TEXTURE_CONTENT)
        continue;
      const viz::TextureDrawQuad* texture_quad =
          viz::TextureDrawQuad::MaterialCast(quad);
      gfx::SizeF gutter_texture_size_pixels =
          gfx::ScaleSize(gfx::SizeF(gutter_texture_size_),
                         host_impl_->active_tree()->device_scale_factor());
      EXPECT_EQ(texture_quad->uv_top_left.x(),
                texture_quad->rect.x() / gutter_texture_size_pixels.width());
      EXPECT_EQ(texture_quad->uv_top_left.y(),
                texture_quad->rect.y() / gutter_texture_size_pixels.height());
      EXPECT_EQ(
          texture_quad->uv_bottom_right.x(),
          texture_quad->rect.right() / gutter_texture_size_pixels.width());
      EXPECT_EQ(
          texture_quad->uv_bottom_right.y(),
          texture_quad->rect.bottom() / gutter_texture_size_pixels.height());
    }
  }

  gfx::Size DipSizeToPixelSize(const gfx::Size& size) {
    return gfx::ScaleToRoundedSize(
        size, host_impl_->active_tree()->device_scale_factor());
  }

  viz::DrawQuad::Material gutter_quad_material_;
  gfx::Size gutter_texture_size_;
  gfx::Size viewport_size_;
  BlendStateCheckLayer* child_;
  bool did_activate_pending_tree_;
};

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCovered) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = false;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  host_impl_->active_tree()->SetDeviceViewportSize(
      DipSizeToPixelSize(viewport_size_));
  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestLayerCoversFullViewport());
  EXPECT_SCOPED(TestEmptyLayer());
  EXPECT_SCOPED(TestLayerInMiddleOfViewport());
  EXPECT_SCOPED(TestLayerIsLargerThanViewport());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ViewportCoveredScaled) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = false;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  host_impl_->active_tree()->SetDeviceScaleFactor(2.f);
  host_impl_->active_tree()->SetDeviceViewportSize(
      DipSizeToPixelSize(viewport_size_));
  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestLayerCoversFullViewport());
  EXPECT_SCOPED(TestEmptyLayer());
  EXPECT_SCOPED(TestLayerInMiddleOfViewport());
  EXPECT_SCOPED(TestLayerIsLargerThanViewport());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeGrowViewportInvalid) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = true;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  // Pending tree to force active_tree size invalid. Not used otherwise.
  CreatePendingTree();
  host_impl_->active_tree()->SetDeviceViewportSize(
      DipSizeToPixelSize(viewport_size_));

  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestEmptyLayerWithOnDraw());
  EXPECT_SCOPED(TestLayerInMiddleOfViewportWithOnDraw());
  EXPECT_SCOPED(TestLayerIsLargerThanViewportWithOnDraw());
}

TEST_F(LayerTreeHostImplViewportCoveredTest, ActiveTreeShrinkViewportInvalid) {
  viewport_size_ = gfx::Size(1000, 1000);

  bool software = true;
  CreateHostImpl(DefaultSettings(), CreateFakeLayerTreeFrameSink(software));

  // Set larger viewport and activate it to active tree.
  CreatePendingTree();
  gfx::Size larger_viewport(viewport_size_.width() + 100,
                            viewport_size_.height() + 100);
  host_impl_->active_tree()->SetDeviceViewportSize(
      DipSizeToPixelSize(larger_viewport));
  host_impl_->ActivateSyncTree();
  EXPECT_TRUE(did_activate_pending_tree_);

  // Shrink pending tree viewport without activating.
  CreatePendingTree();
  host_impl_->active_tree()->SetDeviceViewportSize(
      DipSizeToPixelSize(viewport_size_));

  SetupActiveTreeLayers();
  EXPECT_SCOPED(TestEmptyLayerWithOnDraw());
  EXPECT_SCOPED(TestLayerInMiddleOfViewportWithOnDraw());
  EXPECT_SCOPED(TestLayerIsLargerThanViewportWithOnDraw());
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
TEST_F(LayerTreeHostImplTest, PartialSwapReceivesDamageRect) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  gl_owned->set_have_post_sub_buffer(true);
  scoped_refptr<viz::TestContextProvider> context_provider(
      viz::TestContextProvider::Create(std::move(gl_owned)));
  context_provider->BindToCurrentThread();

  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d(context_provider));
  FakeLayerTreeFrameSink* fake_layer_tree_frame_sink =
      layer_tree_frame_sink.get();

  // This test creates its own LayerTreeHostImpl, so
  // that we can force partial swap enabled.
  LayerTreeSettings settings = DefaultSettings();
  std::unique_ptr<LayerTreeHostImpl> layer_tree_host_impl =
      LayerTreeHostImpl::Create(
          settings, this, &task_runner_provider_, &stats_instrumentation_,
          &task_graph_runner_,
          AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr);
  layer_tree_host_impl->SetVisible(true);
  layer_tree_host_impl->InitializeFrameSink(layer_tree_frame_sink.get());
  layer_tree_host_impl->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2));
  layer_tree_host_impl->active_tree()->SetDeviceViewportSize(
      gfx::Size(500, 500));

  std::unique_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(layer_tree_host_impl->active_tree(), 1);
  root->test_properties()->force_render_surface = true;
  std::unique_ptr<LayerImpl> child =
      FakeDrawableLayerImpl::Create(layer_tree_host_impl->active_tree(), 2);
  child->SetPosition(gfx::PointF(12.f, 13.f));
  child->SetBounds(gfx::Size(14, 15));
  child->SetDrawsContent(true);
  root->SetBounds(gfx::Size(500, 500));
  root->SetDrawsContent(true);
  root->test_properties()->AddChild(std::move(child));
  layer_tree_host_impl->active_tree()->SetRootLayerForTesting(std::move(root));
  layer_tree_host_impl->active_tree()->BuildPropertyTreesForTesting();
  layer_tree_host_impl->active_tree()->SetLocalSurfaceIdFromParent(
      viz::LocalSurfaceId(1, base::UnguessableToken::Deserialize(2u, 3u)),
      base::TimeTicks());

  TestFrameData frame;

  // First frame, the entire screen should get swapped.
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  layer_tree_host_impl->DidDrawAllLayers(frame);
  gfx::Rect expected_swap_rect(500, 500);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  // Second frame, only the damaged area should get swapped. Damage should be
  // the union of old and new child rects: gfx::Rect(26, 28).
  layer_tree_host_impl->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->children[0]
      ->SetPosition(gfx::PointF());
  layer_tree_host_impl->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->children[0]
      ->NoteLayerPropertyChanged();
  layer_tree_host_impl->active_tree()->BuildPropertyTreesForTesting();
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  expected_swap_rect = gfx::Rect(26, 28);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  layer_tree_host_impl->active_tree()->SetDeviceViewportSize(gfx::Size(10, 10));
  // This will damage everything.
  layer_tree_host_impl->active_tree()
      ->root_layer_for_testing()
      ->SetBackgroundColor(SK_ColorBLACK);
  EXPECT_EQ(DRAW_SUCCESS, layer_tree_host_impl->PrepareToDraw(&frame));
  layer_tree_host_impl->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  expected_swap_rect = gfx::Rect(10, 10);
  EXPECT_EQ(expected_swap_rect.ToString(),
            fake_layer_tree_frame_sink->last_swap_rect().ToString());

  layer_tree_host_impl->ReleaseLayerTreeFrameSink();
}

TEST_F(LayerTreeHostImplTest, RootLayerDoesntCreateExtraSurface) {
  std::unique_ptr<LayerImpl> root =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 1);
  std::unique_ptr<LayerImpl> child =
      FakeDrawableLayerImpl::Create(host_impl_->active_tree(), 2);
  child->SetBounds(gfx::Size(10, 10));
  child->SetDrawsContent(true);
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->test_properties()->force_render_surface = true;
  root->test_properties()->AddChild(std::move(child));

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;

  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_EQ(1u, frame.render_surface_list->size());
  EXPECT_EQ(1u, frame.render_passes.size());
  host_impl_->DidDrawAllLayers(frame);
}

class FakeLayerWithQuads : public LayerImpl {
 public:
  static std::unique_ptr<LayerImpl> Create(LayerTreeImpl* tree_impl, int id) {
    return base::WrapUnique(new FakeLayerWithQuads(tree_impl, id));
  }

  void AppendQuads(viz::RenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override {
    viz::SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    PopulateSharedQuadState(shared_quad_state, contents_opaque());

    SkColor gray = SkColorSetRGB(100, 100, 100);
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

TEST_F(LayerTreeHostImplTest, LayersFreeTextures) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  viz::TestGLES2Interface* gl = gl_owned.get();
  std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink(
      FakeLayerTreeFrameSink::Create3d(std::move(gl_owned)));
  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  std::unique_ptr<LayerImpl> root_layer =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  root_layer->SetBounds(gfx::Size(10, 10));
  root_layer->test_properties()->force_render_surface = true;

  scoped_refptr<VideoFrame> softwareFrame = media::VideoFrame::CreateColorFrame(
      gfx::Size(4, 4), 0x80, 0x80, 0x80, base::TimeDelta());
  FakeVideoFrameProvider provider;
  provider.set_frame(softwareFrame);
  std::unique_ptr<VideoLayerImpl> video_layer = VideoLayerImpl::Create(
      host_impl_->active_tree(), 4, &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  root_layer->test_properties()->AddChild(std::move(video_layer));

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  EXPECT_EQ(0u, gl->NumTextures());

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  EXPECT_GT(gl->NumTextures(), 0u);

  // Kill the layer tree.
  host_impl_->active_tree()->DetachLayers();
  // There should be no textures left in use after.
  EXPECT_EQ(0u, gl->NumTextures());
}


TEST_F(LayerTreeHostImplTest, HasTransparentBackground) {
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->set_background_color(SK_ColorWHITE);

  // Verify one quad is drawn when transparent background set is not set.
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(1u, root_pass->quad_list.size());
    EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
              root_pass->quad_list.front()->material);
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when transparent background is set.
  host_impl_->active_tree()->set_background_color(SK_ColorTRANSPARENT);
  host_impl_->SetFullViewportDamage();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  // Cause damage so we would draw something if possible.
  host_impl_->SetFullViewportDamage();

  // Verify no quads are drawn when semi-transparent background is set.
  host_impl_->active_tree()->set_background_color(SkColorSetARGB(5, 255, 0, 0));
  host_impl_->SetFullViewportDamage();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  {
    const auto& root_pass = frame.render_passes.back();
    ASSERT_EQ(0u, root_pass->quad_list.size());
  }
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
}

class LayerTreeHostImplTestDrawAndTestDamage : public LayerTreeHostImplTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }

  void DrawFrameAndTestDamage(const gfx::Rect& expected_damage) {
    bool expect_to_draw = !expected_damage.IsEmpty();

    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

    if (!expect_to_draw) {
      // With no damage, we don't draw, and no quads are created.
      ASSERT_EQ(0u, frame.render_passes.size());
    } else {
      ASSERT_EQ(1u, frame.render_passes.size());

      // Verify the damage rect for the root render pass.
      const viz::RenderPass* root_render_pass =
          frame.render_passes.back().get();
      EXPECT_EQ(expected_damage, root_render_pass->damage_rect);

      // Verify the root and child layers' quads are generated and not being
      // culled.
      ASSERT_EQ(2u, root_render_pass->quad_list.size());

      LayerImpl* child = host_impl_->active_tree()
                             ->root_layer_for_testing()
                             ->test_properties()
                             ->children[0];
      gfx::Rect expected_child_visible_rect(child->bounds());
      EXPECT_EQ(expected_child_visible_rect,
                root_render_pass->quad_list.front()->visible_rect);

      LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();
      gfx::Rect expected_root_visible_rect(root->bounds());
      EXPECT_EQ(expected_root_visible_rect,
                root_render_pass->quad_list.ElementAt(1)->visible_rect);
    }

    EXPECT_EQ(expect_to_draw, host_impl_->DrawLayers(&frame));
    host_impl_->DidDrawAllLayers(frame);
  }
};

TEST_F(LayerTreeHostImplTestDrawAndTestDamage, FrameIncludesDamageRect) {
  std::unique_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetPosition(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->SetBackgroundColor(SK_ColorRED);
  root->test_properties()->force_render_surface = true;

  // Child layer is in the bottom right corner.
  std::unique_ptr<SolidColorLayerImpl> child =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 2);
  child->SetPosition(gfx::PointF(9.f, 9.f));
  child->SetBounds(gfx::Size(1, 1));
  child->SetDrawsContent(true);
  child->SetBackgroundColor(SK_ColorRED);
  root->test_properties()->AddChild(std::move(child));

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Draw a frame. In the first frame, the entire viewport should be damaged.
  gfx::Rect full_frame_damage(
      host_impl_->active_tree()->GetDeviceViewport().size());
  DrawFrameAndTestDamage(full_frame_damage);

  // The second frame has damage that doesn't touch the child layer. Its quads
  // should still be generated.
  gfx::Rect small_damage = gfx::Rect(0, 0, 1, 1);
  host_impl_->active_tree()->root_layer_for_testing()->SetUpdateRect(
      small_damage);
  DrawFrameAndTestDamage(small_damage);

  // The third frame should have no damage, so no quads should be generated.
  gfx::Rect no_damage;
  DrawFrameAndTestDamage(no_damage);
}

class GLRendererWithSetupQuadForAntialiasing : public viz::GLRenderer {
 public:
  using viz::GLRenderer::ShouldAntialiasQuad;
};

TEST_F(LayerTreeHostImplTest, FarAwayQuadsDontNeedAA) {
  // Due to precision issues (especially on Android), sometimes far
  // away quads can end up thinking they need AA.
  float device_scale_factor = 4.f / 3.f;
  gfx::Size root_size(2000, 1000);
  gfx::Size device_viewport_size =
      gfx::ScaleToCeiledSize(root_size, device_scale_factor);
  host_impl_->active_tree()->SetDeviceViewportSize(device_viewport_size);

  CreatePendingTree();
  host_impl_->pending_tree()->SetDeviceScaleFactor(device_scale_factor);
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f / 16.f,
                                                          16.f);

  std::unique_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->pending_tree(), 1);
  LayerImpl* root = scoped_root.get();
  root->test_properties()->force_render_surface = true;
  root->SetNeedsPushProperties();

  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(scoped_root));

  std::unique_ptr<LayerImpl> scoped_scrolling_layer =
      LayerImpl::Create(host_impl_->pending_tree(), 2);
  LayerImpl* scrolling_layer = scoped_scrolling_layer.get();
  root->test_properties()->AddChild(std::move(scoped_scrolling_layer));
  scrolling_layer->SetNeedsPushProperties();

  gfx::Size content_layer_bounds(100001, 100);
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(content_layer_bounds));

  std::unique_ptr<FakePictureLayerImpl> scoped_content_layer =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                   3, raster_source);
  LayerImpl* content_layer = scoped_content_layer.get();
  scrolling_layer->test_properties()->AddChild(std::move(scoped_content_layer));
  content_layer->SetBounds(content_layer_bounds);
  content_layer->SetDrawsContent(true);
  content_layer->SetNeedsPushProperties();

  root->SetBounds(root_size);

  gfx::ScrollOffset scroll_offset(100000, 0);
  scrolling_layer->SetScrollable(content_layer_bounds);
  scrolling_layer->SetElementId(
      LayerIdToElementIdForTesting(scrolling_layer->id()));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();

  scrolling_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scrolling_layer->element_id(), scroll_offset);
  host_impl_->ActivateSyncTree();

  host_impl_->active_tree()->UpdateDrawProperties();
  ASSERT_EQ(1u, host_impl_->active_tree()->GetRenderSurfaceList().size());

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));

  ASSERT_EQ(1u, frame.render_passes.size());
  ASSERT_LE(1u, frame.render_passes[0]->quad_list.size());
  const viz::DrawQuad* quad = frame.render_passes[0]->quad_list.front();

  bool clipped = false, force_aa = false;
  gfx::QuadF device_layer_quad = MathUtil::MapQuad(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::QuadF(gfx::RectF(quad->shared_quad_state->visible_quad_layer_rect)),
      &clipped);
  EXPECT_FALSE(clipped);
  bool antialiased =
      GLRendererWithSetupQuadForAntialiasing::ShouldAntialiasQuad(
          device_layer_quad, clipped, force_aa);
  EXPECT_FALSE(antialiased);

  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
}

class CompositorFrameMetadataTest : public LayerTreeHostImplTest {
 public:
  CompositorFrameMetadataTest() = default;

  void DidReceiveCompositorFrameAckOnImplThread() override { acks_received_++; }

  int acks_received_ = 0;
};

TEST_F(CompositorFrameMetadataTest, CompositorFrameAckCountsAsSwapComplete) {
  SetupRootLayerImpl(FakeLayerWithQuads::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  {
    TestFrameData frame;
    EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
    host_impl_->DrawLayers(&frame);
    host_impl_->DidDrawAllLayers(frame);
  }
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

TEST_F(LayerTreeHostImplTest,
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
  std::unique_ptr<SolidColorLayerImpl> root_layer =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);

  // VideoLayerImpl will not be drawn.
  FakeVideoFrameProvider provider;
  std::unique_ptr<VideoLayerImpl> video_layer = VideoLayerImpl::Create(
      host_impl_->active_tree(), 2, &provider, media::VIDEO_ROTATION_0);
  video_layer->SetBounds(gfx::Size(10, 10));
  video_layer->SetDrawsContent(true);
  root_layer->test_properties()->AddChild(std::move(video_layer));
  SetupRootLayerImpl(std::move(root_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);

  EXPECT_EQ(1u, last_on_draw_frame_->will_draw_layers.size());
  EXPECT_EQ(host_impl_->active_tree()->root_layer_for_testing(),
            last_on_draw_frame_->will_draw_layers[0]);
}

// Checks that we use the memory limits provided.
TEST_F(LayerTreeHostImplTest, MemoryLimits) {
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
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr);

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

namespace {
void ExpectFullDamageAndDraw(LayerTreeHostImpl* host_impl) {
  gfx::Rect full_frame_damage(
      host_impl->active_tree()->GetDeviceViewport().size());
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl->PrepareToDraw(&frame));
  ASSERT_EQ(1u, frame.render_passes.size());
  const viz::RenderPass* root_render_pass = frame.render_passes.back().get();
  EXPECT_EQ(full_frame_damage, root_render_pass->damage_rect);
  EXPECT_TRUE(host_impl->DrawLayers(&frame));
  host_impl->DidDrawAllLayers(frame);
}
}  // namespace

TEST_F(LayerTreeHostImplTestDrawAndTestDamage,
       RequireHighResAndRedrawWhenVisible) {
  ASSERT_TRUE(host_impl_->active_tree());

  std::unique_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);
  root->SetBackgroundColor(SK_ColorRED);
  SetupRootLayerImpl(std::move(root));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

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

TEST_F(LayerTreeHostImplTest, RequireHighResAfterGpuRasterizationToggles) {
  ASSERT_TRUE(host_impl_->active_tree());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());

  // RequiresHighResToDraw is set when new output surface is used.
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());

  host_impl_->ResetRequiresHighResToDraw();

  host_impl_->SetContentHasSlowPaths(false);
  host_impl_->SetHasGpuRasterizationTrigger(false);
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  host_impl_->NotifyReadyToActivate();
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  host_impl_->NotifyReadyToActivate();
  host_impl_->SetHasGpuRasterizationTrigger(false);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  host_impl_->NotifyReadyToActivate();

  host_impl_->ResetRequiresHighResToDraw();

  EXPECT_FALSE(host_impl_->RequiresHighResToDraw());
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->CommitComplete();
  EXPECT_TRUE(host_impl_->RequiresHighResToDraw());
  host_impl_->NotifyReadyToActivate();
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
    host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(10, 10));
  }

  FakeLayerTreeHostImpl* fake_host_impl_;
};

TEST_F(LayerTreeHostImplTestPrepareTiles, PrepareTilesWhenInvisible) {
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(false);
  EXPECT_FALSE(fake_host_impl_->prepare_tiles_needed());
  host_impl_->SetVisible(true);
  EXPECT_TRUE(fake_host_impl_->prepare_tiles_needed());
}

TEST_F(LayerTreeHostImplTest, UIResourceManagement) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  viz::TestGLES2Interface* gl = gl_owned.get();

  std::unique_ptr<FakeLayerTreeFrameSink> layer_tree_frame_sink =
      FakeLayerTreeFrameSink::Create3d(std::move(gl_owned));
  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  EXPECT_EQ(0u, gl->NumTextures());

  UIResourceId ui_resource_id = 1;
  bool is_opaque = false;
  UIResourceBitmap bitmap(gfx::Size(1, 1), is_opaque);
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, gl->NumTextures());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id1);

  // Multiple requests with the same id is allowed.  The previous texture is
  // deleted.
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, gl->NumTextures());
  viz::ResourceId id2 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id2);
  EXPECT_NE(id1, id2);

  // Deleting invalid UIResourceId is allowed and does not change state.
  host_impl_->DeleteUIResource(-1);
  EXPECT_EQ(1u, gl->NumTextures());

  // Should return zero for invalid UIResourceId.  Number of textures should
  // not change.
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(-1));
  EXPECT_EQ(1u, gl->NumTextures());

  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, host_impl_->ResourceIdForUIResource(ui_resource_id));
  EXPECT_EQ(0u, gl->NumTextures());

  // Should not change state for multiple deletion on one UIResourceId
  host_impl_->DeleteUIResource(ui_resource_id);
  EXPECT_EQ(0u, gl->NumTextures());
}

TEST_F(LayerTreeHostImplTest, CreateETC1UIResource) {
  auto gl_owned = std::make_unique<viz::TestGLES2Interface>();
  gl_owned->set_support_compressed_texture_etc1(true);
  viz::TestGLES2Interface* gl = gl_owned.get();

  CreateHostImpl(DefaultSettings(),
                 FakeLayerTreeFrameSink::Create3d(std::move(gl_owned)));

  EXPECT_EQ(0u, gl->NumTextures());

  gfx::Size size(4, 4);
  // SkImageInfo has no support for ETC1.  The |info| below contains the right
  // total pixel size for the bitmap but not the right height and width.  The
  // correct width/height are passed directly to UIResourceBitmap.
  SkImageInfo info =
      SkImageInfo::Make(4, 2, kAlpha_8_SkColorType, kPremul_SkAlphaType);
  sk_sp<SkPixelRef> pixel_ref(SkMallocPixelRef::MakeAllocate(info, 0));
  pixel_ref->setImmutable();
  UIResourceBitmap bitmap(std::move(pixel_ref), size);
  UIResourceId ui_resource_id = 1;
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  EXPECT_EQ(1u, gl->NumTextures());
  viz::ResourceId id1 = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(0u, id1);
}

TEST_F(LayerTreeHostImplTest,
       GpuRasterizationStatusChangeDoesNotEvictUIResources) {
  // Create a host impl with MSAA support and a forced sample count of 4.
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = 4;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(
                        msaaSettings.gpu_rasterization_msaa_sample_count)));

  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());

  UIResourceId ui_resource_id = 1;
  UIResourceBitmap bitmap(gfx::Size(1, 1), false /* is_opaque */);
  host_impl_->CreateUIResource(ui_resource_id, bitmap);
  viz::ResourceId resource_id =
      host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(viz::kInvalidResourceId, resource_id);
  EXPECT_FALSE(host_impl_->EvictedUIResourcesExist());

  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->use_msaa());

  resource_id = host_impl_->ResourceIdForUIResource(ui_resource_id);
  EXPECT_NE(viz::kInvalidResourceId, resource_id);
  EXPECT_FALSE(host_impl_->EvictedUIResourcesExist());
}

class FrameSinkClient : public viz::TestLayerTreeFrameSinkClient {
 public:
  explicit FrameSinkClient(
      scoped_refptr<viz::ContextProvider> display_context_provider)
      : display_context_provider_(std::move(display_context_provider)) {}

  std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider)
      override {
    return viz::FakeOutputSurface::Create3d(
        std::move(display_context_provider_));
  }

  void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) override {}
  void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) override {}
  void DisplayWillDrawAndSwap(
      bool will_draw_and_swap,
      const viz::RenderPassList& render_passes) override {}
  void DisplayDidDrawAndSwap() override {}

 private:
  scoped_refptr<viz::ContextProvider> display_context_provider_;
};

TEST_F(LayerTreeHostImplTest, ShutdownReleasesContext) {
  scoped_refptr<viz::TestContextProvider> context_provider =
      viz::TestContextProvider::Create();
  FrameSinkClient test_client(context_provider);

  constexpr bool synchronous_composite = true;
  constexpr bool disable_display_vsync = false;
  constexpr double refresh_rate = 60.0;
  auto layer_tree_frame_sink = std::make_unique<viz::TestLayerTreeFrameSink>(
      context_provider, viz::TestContextProvider::CreateWorker(), nullptr,
      viz::RendererSettings(), base::ThreadTaskRunnerHandle::Get().get(),
      synchronous_composite, disable_display_vsync, refresh_rate);
  layer_tree_frame_sink->SetClient(&test_client);

  CreateHostImpl(DefaultSettings(), std::move(layer_tree_frame_sink));

  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));

  LayerImpl* root = host_impl_->active_tree()->root_layer_for_testing();
  struct Helper {
    std::unique_ptr<viz::CopyOutputResult> unprocessed_result;
    void OnResult(std::unique_ptr<viz::CopyOutputResult> result) {
      unprocessed_result = std::move(result);
    }
  } helper;
  root->test_properties()->copy_requests.push_back(
      std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(&Helper::OnResult, base::Unretained(&helper))));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);

  // The CopyOutputResult has a ref on the viz::ContextProvider and a texture
  // in a texture mailbox.
  ASSERT_TRUE(helper.unprocessed_result);
  EXPECT_FALSE(context_provider->HasOneRef());
  EXPECT_EQ(1u, context_provider->TestContextGL()->NumTextures());

  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  // The texture release callback that was given to the CopyOutputResult has
  // been canceled, and the texture deleted.
  EXPECT_TRUE(context_provider->HasOneRef());
  EXPECT_EQ(0u, context_provider->TestContextGL()->NumTextures());

  // When resetting the CopyOutputResult, it will run its texture release
  // callback. This should not cause a crash, etc.
  helper.unprocessed_result.reset();
}

TEST_F(LayerTreeHostImplTest, ScrollUnknownNotOnAncestorChain) {
  // If we ray cast a scroller that is not on the first layer's ancestor chain,
  // we should return SCROLL_UNKNOWN.
  gfx::Size content_size(100, 100);
  SetupScrollAndContentsLayers(content_size);

  int scroll_layer_id = 2;
  LayerImpl* scroll_layer =
      host_impl_->active_tree()->LayerById(scroll_layer_id);
  scroll_layer->SetDrawsContent(true);

  int page_scale_layer_id = 5;
  LayerImpl* page_scale_layer =
      host_impl_->active_tree()->LayerById(page_scale_layer_id);

  int occluder_layer_id = 6;
  std::unique_ptr<LayerImpl> occluder_layer =
      LayerImpl::Create(host_impl_->active_tree(), occluder_layer_id);
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetBounds(content_size);
  occluder_layer->SetPosition(gfx::PointF());

  // The parent of the occluder is *above* the scroller.
  page_scale_layer->test_properties()->AddChild(std::move(occluder_layer));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollUnknownScrollAncestorMismatch) {
  // If we ray cast a scroller this is on the first layer's ancestor chain, but
  // is not the first scroller we encounter when walking up from the layer, we
  // should also return SCROLL_UNKNOWN.
  gfx::Size content_size(100, 100);
  SetupScrollAndContentsLayers(content_size);

  int scroll_layer_id = 2;
  LayerImpl* scroll_layer =
      host_impl_->active_tree()->LayerById(scroll_layer_id);
  scroll_layer->SetDrawsContent(true);

  int occluder_layer_id = 6;
  std::unique_ptr<LayerImpl> occluder_layer =
      LayerImpl::Create(host_impl_->active_tree(), occluder_layer_id);
  occluder_layer->SetDrawsContent(true);
  occluder_layer->SetBounds(content_size);
  occluder_layer->SetPosition(gfx::PointF(-10.f, -10.f));

  int child_scroll_clip_layer_id = 7;
  std::unique_ptr<LayerImpl> child_scroll_clip =
      LayerImpl::Create(host_impl_->active_tree(), child_scroll_clip_layer_id);

  int child_scroll_layer_id = 8;
  std::unique_ptr<LayerImpl> child_scroll =
      CreateScrollableLayer(child_scroll_layer_id, content_size);

  child_scroll->SetPosition(gfx::PointF(10.f, 10.f));

  child_scroll->test_properties()->AddChild(std::move(occluder_layer));
  child_scroll_clip->test_properties()->AddChild(std::move(child_scroll));
  scroll_layer->test_properties()->AddChild(std::move(child_scroll_clip));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  InputHandler::ScrollStatus status = host_impl_->ScrollBegin(
      BeginState(gfx::Point()).get(), InputHandler::WHEEL);
  EXPECT_EQ(InputHandler::SCROLL_UNKNOWN, status.thread);
  EXPECT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_scrolling_reasons);
}

TEST_F(LayerTreeHostImplTest, ScrollInvisibleScroller) {
  gfx::Size content_size(100, 100);
  SetupScrollAndContentsLayers(content_size);

  int scroll_layer_id = 2;
  LayerImpl* scroll_layer =
      host_impl_->active_tree()->LayerById(scroll_layer_id);

  int child_scroll_layer_id = 7;
  std::unique_ptr<LayerImpl> child_scroll =
      CreateScrollableLayer(child_scroll_layer_id, content_size);
  child_scroll->SetDrawsContent(false);

  scroll_layer->test_properties()->AddChild(std::move(child_scroll));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // We should have scrolled |child_scroll| even though it does not move
  // any layer that is a drawn RSLL member.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);

  EXPECT_EQ(host_impl_->active_tree()->LayerById(7)->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
}

template <bool commit_to_active_tree>
class LayerTreeHostImplLatencyInfoTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.commit_to_active_tree = commit_to_active_tree;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());

    std::unique_ptr<SolidColorLayerImpl> root =
        SolidColorLayerImpl::Create(host_impl_->active_tree(), 1);
    root->SetPosition(gfx::PointF());
    root->SetBounds(gfx::Size(10, 10));
    root->SetDrawsContent(true);
    root->test_properties()->force_render_surface = true;

    host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
    host_impl_->active_tree()->BuildPropertyTreesForTesting();
  }
};

// Make sure LatencyInfo are passed in viz::CompositorFrameMetadata properly in
// the Renderer. This includes components added by LatencyInfoSwapPromise and
// the default LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT.
using LayerTreeHostImplLatencyInfoRendererTest =
    LayerTreeHostImplLatencyInfoTest<false>;
TEST_F(LayerTreeHostImplLatencyInfoRendererTest,
       LatencyInfoPassedToCompositorFrameMetadataRenderer) {
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // The first frame should only have the default BeginFrame component.
  TestFrameData frame1;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame1));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame1));
  host_impl_->DidDrawAllLayers(frame1);

  const std::vector<ui::LatencyInfo>& metadata_latency_after1 =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.latency_info;
  EXPECT_EQ(1u, metadata_latency_after1.size());
  EXPECT_TRUE(metadata_latency_after1[0].FindLatency(
      ui::LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT, nullptr));
  EXPECT_TRUE(metadata_latency_after1[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));

  // The second frame should have the default BeginFrame component and the
  // component attached via LatencyInfoSwapPromise.
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));
  host_impl_->active_tree()->QueuePinnedSwapPromise(std::move(swap_promise));

  TestFrameData frame2;
  host_impl_->SetFullViewportDamage();
  host_impl_->SetNeedsRedraw();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame2));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame2));
  host_impl_->DidDrawAllLayers(frame2);

  const std::vector<ui::LatencyInfo>& metadata_latency_after2 =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.latency_info;
  EXPECT_EQ(2u, metadata_latency_after2.size());
  EXPECT_TRUE(metadata_latency_after2[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_TRUE(metadata_latency_after2[1].FindLatency(
      ui::LATENCY_BEGIN_FRAME_RENDERER_COMPOSITOR_COMPONENT, nullptr));

  // Renderer should also record INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT.
  EXPECT_TRUE(metadata_latency_after2[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));
  EXPECT_TRUE(metadata_latency_after2[1].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));
}

// Make sure LatencyInfo are passed in viz::CompositorFrameMetadata properly in
// the UI. This includes components added by LatencyInfoSwapPromise and
// the default LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT.
using LayerTreeHostImplLatencyInfoUITest =
    LayerTreeHostImplLatencyInfoTest<true>;
TEST_F(LayerTreeHostImplLatencyInfoUITest,
       LatencyInfoPassedToCompositorFrameMetadataUI) {
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // The first frame should only have the default BeginFrame component.
  TestFrameData frame1;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame1));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame1));
  host_impl_->DidDrawAllLayers(frame1);

  const std::vector<ui::LatencyInfo>& metadata_latency_after1 =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.latency_info;
  EXPECT_EQ(1u, metadata_latency_after1.size());
  EXPECT_TRUE(metadata_latency_after1[0].FindLatency(
      ui::LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT, nullptr));
  EXPECT_FALSE(metadata_latency_after1[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));

  // The second frame should have the default BeginFrame component and the
  // component attached via LatencyInfoSwapPromise.
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT);
  std::unique_ptr<SwapPromise> swap_promise(
      new LatencyInfoSwapPromise(latency_info));
  host_impl_->active_tree()->QueuePinnedSwapPromise(std::move(swap_promise));

  TestFrameData frame2;
  host_impl_->SetFullViewportDamage();
  host_impl_->SetNeedsRedraw();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame2));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame2));
  host_impl_->DidDrawAllLayers(frame2);

  const std::vector<ui::LatencyInfo>& metadata_latency_after2 =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.latency_info;
  EXPECT_EQ(2u, metadata_latency_after2.size());
  EXPECT_TRUE(metadata_latency_after2[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  EXPECT_TRUE(metadata_latency_after2[1].FindLatency(
      ui::LATENCY_BEGIN_FRAME_UI_COMPOSITOR_COMPONENT, nullptr));

  // UI should not record INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT.
  EXPECT_FALSE(metadata_latency_after2[0].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));
  EXPECT_FALSE(metadata_latency_after2[1].FindLatency(
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT, nullptr));
}

#if defined(OS_ANDROID)
TEST_F(LayerTreeHostImplTest, SelectionBoundsPassedToCompositorFrameMetadata) {
  int root_layer_id = 1;
  std::unique_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), root_layer_id);
  root->SetPosition(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->test_properties()->force_render_surface = true;

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Ensure the default frame selection bounds are empty.
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_top(5, 0);
  gfx::Point selection_bottom(5, 5);
  LayerSelection selection;
  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root_layer_id;
  selection.start.edge_bottom = selection_bottom;
  selection.start.edge_top = selection_top;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();

  gfx::Rect full_frame_damage(
      host_impl_->active_tree()->GetDeviceViewport().size());
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame));
  host_impl_->DidDrawAllLayers(frame);

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_bottom), selection_after.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(selection_top), selection_after.start.edge_top());
  EXPECT_TRUE(selection_after.start.visible());
  EXPECT_TRUE(selection_after.end.visible());
}

TEST_F(LayerTreeHostImplTest, HiddenSelectionBoundsStayHidden) {
  int root_layer_id = 1;
  std::unique_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), root_layer_id);
  root->SetPosition(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->test_properties()->force_render_surface = true;

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Ensure the default frame selection bounds are empty.
  auto* fake_layer_tree_frame_sink =
      static_cast<FakeLayerTreeFrameSink*>(host_impl_->layer_tree_frame_sink());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_top(5, 0);
  gfx::Point selection_bottom(5, 5);
  LayerSelection selection;

  // Mark the start as hidden.
  selection.start.hidden = true;

  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root_layer_id;
  selection.start.edge_bottom = selection_bottom;
  selection.start.edge_top = selection_top;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();

  gfx::Rect full_frame_damage(
      host_impl_->active_tree()->GetDeviceViewport().size());
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame));
  host_impl_->DidDrawAllLayers(frame);

  // Ensure the selection bounds have propagated to the frame metadata.
  const viz::Selection<gfx::SelectionBound>& selection_after =
      fake_layer_tree_frame_sink->last_sent_frame()->metadata.selection;
  EXPECT_EQ(selection.start.type, selection_after.start.type());
  EXPECT_EQ(selection.end.type, selection_after.end.type());
  EXPECT_EQ(gfx::PointF(selection_bottom), selection_after.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(selection_top), selection_after.start.edge_top());
  EXPECT_FALSE(selection_after.start.visible());
  EXPECT_FALSE(selection_after.end.visible());
}
#endif  // defined(OS_ANDROID)

class SimpleSwapPromiseMonitor : public SwapPromiseMonitor {
 public:
  SimpleSwapPromiseMonitor(LayerTreeHost* layer_tree_host,
                           LayerTreeHostImpl* layer_tree_host_impl,
                           int* set_needs_commit_count,
                           int* set_needs_redraw_count,
                           int* forward_to_main_count)
      : SwapPromiseMonitor(
            (layer_tree_host ? layer_tree_host->GetSwapPromiseManager()
                             : nullptr),
            layer_tree_host_impl),
        set_needs_commit_count_(set_needs_commit_count),
        set_needs_redraw_count_(set_needs_redraw_count),
        forward_to_main_count_(forward_to_main_count) {}

  ~SimpleSwapPromiseMonitor() override = default;

  void OnSetNeedsCommitOnMain() override { (*set_needs_commit_count_)++; }

  void OnSetNeedsRedrawOnImpl() override { (*set_needs_redraw_count_)++; }

  void OnForwardScrollUpdateToMainThreadOnImpl() override {
    (*forward_to_main_count_)++;
  }

 private:
  int* set_needs_commit_count_;
  int* set_needs_redraw_count_;
  int* forward_to_main_count_;
};

TEST_F(LayerTreeHostImplTest, SimpleSwapPromiseMonitor) {
  int set_needs_commit_count = 0;
  int set_needs_redraw_count = 0;
  int forward_to_main_count = 0;

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(
            nullptr, host_impl_.get(), &set_needs_commit_count,
            &set_needs_redraw_count, &forward_to_main_count));
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(1, set_needs_redraw_count);
    EXPECT_EQ(0, forward_to_main_count);
  }

  // Now the monitor is destroyed, SetNeedsRedraw() is no longer being
  // monitored.
  host_impl_->SetNeedsRedraw();
  EXPECT_EQ(0, set_needs_commit_count);
  EXPECT_EQ(1, set_needs_redraw_count);
  EXPECT_EQ(0, forward_to_main_count);

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(
            nullptr, host_impl_.get(), &set_needs_commit_count,
            &set_needs_redraw_count, &forward_to_main_count));
    // Redraw with damage.
    host_impl_->SetFullViewportDamage();
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(2, set_needs_redraw_count);
    EXPECT_EQ(0, forward_to_main_count);
  }

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(
            nullptr, host_impl_.get(), &set_needs_commit_count,
            &set_needs_redraw_count, &forward_to_main_count));
    // Redraw without damage.
    host_impl_->SetNeedsRedraw();
    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(3, set_needs_redraw_count);
    EXPECT_EQ(0, forward_to_main_count);
  }

  set_needs_commit_count = 0;
  set_needs_redraw_count = 0;
  forward_to_main_count = 0;

  {
    std::unique_ptr<SimpleSwapPromiseMonitor> swap_promise_monitor(
        new SimpleSwapPromiseMonitor(
            nullptr, host_impl_.get(), &set_needs_commit_count,
            &set_needs_redraw_count, &forward_to_main_count));
    SetupScrollAndContentsLayers(gfx::Size(100, 100));

    // Scrolling normally should not trigger any forwarding.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point()).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_TRUE(
        host_impl_
            ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get())
            .did_scroll);
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(1, set_needs_redraw_count);
    EXPECT_EQ(0, forward_to_main_count);

    // Scrolling with a scroll handler should defer the swap to the main
    // thread.
    host_impl_->active_tree()->set_have_scroll_event_handlers(true);
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point()).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_TRUE(
        host_impl_
            ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, 10)).get())
            .did_scroll);
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_EQ(0, set_needs_commit_count);
    EXPECT_EQ(2, set_needs_redraw_count);
    EXPECT_EQ(1, forward_to_main_count);
  }
}

class LayerTreeHostImplWithBrowserControlsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.commit_to_active_tree = false;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
    host_impl_->active_tree()->SetTopControlsHeight(top_controls_height_);
    host_impl_->active_tree()->SetCurrentBrowserControlsShownRatio(1.f);
  }

 protected:
  static const int top_controls_height_;
};

const int LayerTreeHostImplWithBrowserControlsTest::top_controls_height_ = 50;

TEST_F(LayerTreeHostImplWithBrowserControlsTest, NoIdleAnimations) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 10));
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  EXPECT_FALSE(did_request_redraw_);
  host_impl_->DidFinishImplFrame();
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsHeightIsCommitted) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_FALSE(did_request_redraw_);
  CreatePendingTree();
  host_impl_->sync_tree()->SetTopControlsHeight(100);
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(100, host_impl_->browser_controls_manager()->TopControlsHeight());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsStayFullyVisibleOnHeightChange) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetTopControlsHeight(0);
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ControlsTopOffset());

  CreatePendingTree();
  host_impl_->sync_tree()->SetTopControlsHeight(50);
  host_impl_->ActivateSyncTree();
  EXPECT_EQ(0.f, host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationScheduling) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(scroll_layer->element_id(),
                                                     gfx::ScrollOffset(0, 10));
  host_impl_->DidChangeBrowserControlsPosition();
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       ScrollHandledByBrowserControls) {
  InputHandlerScrollResult result;
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 200));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Scroll just the browser controls and verify that the scroll succeeds.
  const float residue = 10;
  float offset = top_controls_height_ - residue;
  result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get());
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_TRUE(result.did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Scroll across the boundary
  const float content_scroll = 20;
  offset = residue + content_scroll;
  result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, content_scroll).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Now scroll back to the top of the content
  offset = -content_scroll;
  result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // And scroll the browser controls completely into view
  offset = -top_controls_height_;
  result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get());
  EXPECT_TRUE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, 0));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // And attempt to scroll past the end
  result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get());
  EXPECT_FALSE(result.did_scroll);
  EXPECT_EQ(result.unused_scroll_delta, gfx::Vector2d(0, -50));
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       WheelUnhandledByBrowserControls) {
  SetupScrollAndContentsLayers(gfx::Size(100, 200));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 100));
  host_impl_->active_tree()->set_browser_controls_shrink_blink_size(true);
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  LayerImpl* viewport_layer = host_impl_->InnerViewportScrollLayer();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), viewport_layer->CurrentScrollOffset());

  // Wheel scrolls should not affect the browser controls, and should pass
  // directly through to the viewport.
  const float delta = top_controls_height_;
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, delta)).get())
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, delta),
                   viewport_layer->CurrentScrollOffset());

  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, delta)).get())
          .did_scroll);
  EXPECT_FLOAT_EQ(0,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, delta * 2),
                   viewport_layer->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAtOrigin) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 200));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 200));
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Scroll the browser controls partially.
  const float residue = 35;
  float offset = top_controls_height_ - residue;
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get())
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from their limit.
  host_impl_->ScrollEnd(EndState().get());
  ASSERT_TRUE(host_impl_->browser_controls_manager()->has_animation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

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

    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(5);
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    EXPECT_EQ(gfx::Vector2dF().ToString(),
              scroll_layer->CurrentScrollOffset().ToString());

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    // No commit is needed as the controls are animating the content offset,
    // not the scroll offset.
    EXPECT_FALSE(did_request_commit_);

    if (new_offset != old_offset)
      EXPECT_TRUE(did_request_redraw_);

    if (new_offset != 0) {
      EXPECT_TRUE(host_impl_->browser_controls_manager()->has_animation());
      EXPECT_TRUE(did_request_next_frame_);
    }
    host_impl_->DidFinishImplFrame();
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->has_animation());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsAnimationAfterScroll) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 200));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  float initial_scroll_offset = 50;
  scroll_layer->layer_tree_impl()
      ->property_trees()
      ->scroll_tree.UpdateScrollOffsetBaseForTesting(
          scroll_layer->element_id(),
          gfx::ScrollOffset(0, initial_scroll_offset));
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, initial_scroll_offset).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Scroll the browser controls partially.
  const float residue = 15;
  float offset = top_controls_height_ - residue;
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get())
          .did_scroll);
  EXPECT_FLOAT_EQ(-offset,
                  host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF(0, initial_scroll_offset).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  did_request_redraw_ = false;
  did_request_next_frame_ = false;
  did_request_commit_ = false;

  // End the scroll while the controls are still offset from the limit.
  host_impl_->ScrollEnd(EndState().get());
  ASSERT_TRUE(host_impl_->browser_controls_manager()->has_animation());
  EXPECT_TRUE(did_request_next_frame_);
  EXPECT_TRUE(did_request_redraw_);
  EXPECT_FALSE(did_request_commit_);

  // Animate the browser controls to the limit.
  viz::BeginFrameArgs begin_frame_args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks::Now());
  while (did_request_next_frame_) {
    did_request_redraw_ = false;
    did_request_next_frame_ = false;
    did_request_commit_ = false;

    float old_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    begin_frame_args.frame_time += base::TimeDelta::FromMilliseconds(5);
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();

    float new_offset =
        host_impl_->browser_controls_manager()->ControlsTopOffset();

    if (new_offset != old_offset) {
      EXPECT_TRUE(did_request_redraw_);
      EXPECT_TRUE(did_request_commit_);
    }
    host_impl_->DidFinishImplFrame();
  }
  EXPECT_FALSE(host_impl_->browser_controls_manager()->has_animation());
  EXPECT_EQ(-top_controls_height_,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
}

TEST_F(LayerTreeHostImplWithBrowserControlsTest,
       BrowserControlsScrollDeltaInOverScroll) {
  // Verifies that the overscroll delta should not have accumulated in
  // the browser controls if we do a hide and show without releasing finger.
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 200));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));
  host_impl_->browser_controls_manager()->UpdateBrowserControlsState(
      BrowserControlsState::kBoth, BrowserControlsState::kShown, false);
  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  float offset = 50;
  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get())
          .did_scroll);
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());
  EXPECT_EQ(gfx::Vector2dF().ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, offset).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, offset)).get())
          .did_scroll);

  // Should have fully scrolled
  EXPECT_EQ(gfx::Vector2dF(0, scroll_layer->MaxScrollOffset().y()).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  float overscrollamount = 10;

  // Overscroll the content
  EXPECT_FALSE(host_impl_
                   ->ScrollBy(UpdateState(gfx::Point(),
                                          gfx::Vector2d(0, overscrollamount))
                                  .get())
                   .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 2 * offset).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());
  EXPECT_EQ(gfx::Vector2dF(0, overscrollamount).ToString(),
            host_impl_->accumulated_root_overscroll().ToString());

  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(
              UpdateState(gfx::Point(), gfx::Vector2d(0, -2 * offset)).get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 0).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());
  EXPECT_EQ(-offset,
            host_impl_->browser_controls_manager()->ControlsTopOffset());

  EXPECT_TRUE(
      host_impl_
          ->ScrollBy(UpdateState(gfx::Point(), gfx::Vector2d(0, -offset)).get())
          .did_scroll);
  EXPECT_EQ(gfx::Vector2dF(0, 0).ToString(),
            scroll_layer->CurrentScrollOffset().ToString());

  // Browser controls should be fully visible
  EXPECT_EQ(0, host_impl_->browser_controls_manager()->ControlsTopOffset());

  host_impl_->ScrollEnd(EndState().get());
}

// Tests that when we set a child scroller (e.g. a scrolling div) as the outer
// viewport, scrolling it controls the browser controls.
TEST_F(LayerTreeHostImplBrowserControlsTest,
       ReplacedOuterViewportScrollsBrowserControls) {
  const gfx::Size scroll_content_size(400, 400);
  const gfx::Size root_layer_size(200, 200);
  const gfx::Size viewport_size(100, 100);

  SetupBrowserControlsAndScrollLayerWithVirtualViewport(
      viewport_size, viewport_size, root_layer_size);

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  LayerImpl* scroll_layer = nullptr;
  LayerImpl* clip_layer = nullptr;

  // Initialization: Add a child scrolling layer to the outer scroll layer and
  // set its scroll layer as the outer viewport. This simulates setting a
  // scrolling element as the root scroller on the page.
  {
    std::unique_ptr<LayerImpl> clip = LayerImpl::Create(layer_tree_impl, 10);
    clip->SetBounds(root_layer_size);
    clip->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> scroll = LayerImpl::Create(layer_tree_impl, 11);
    scroll->SetBounds(scroll_content_size);
    scroll->SetScrollable(root_layer_size);
    scroll->SetElementId(LayerIdToElementIdForTesting(scroll->id()));
    scroll->SetDrawsContent(true);

    scroll_layer = scroll.get();
    clip_layer = clip.get();

    clip->test_properties()->AddChild(std::move(scroll));
    outer_scroll->test_properties()->AddChild(std::move(clip));
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = layer_tree_impl->PageScaleLayer()->id();
    viewport_ids.inner_viewport_container =
        layer_tree_impl->InnerViewportContainerLayer()->id();
    viewport_ids.outer_viewport_container = clip_layer->id();
    viewport_ids.inner_viewport_scroll = inner_scroll->id();
    viewport_ids.outer_viewport_scroll = scroll_layer->id();
    layer_tree_impl->SetViewportLayersFromIds(viewport_ids);
    layer_tree_impl->BuildPropertyTreesForTesting();
    DrawFrame();
  }

  ASSERT_EQ(1.f, host_impl_->active_tree()->CurrentBrowserControlsShownRatio());

  // Scrolling should scroll the child content and the browser controls. The
  // original outer viewport should get no scroll.
  {
    host_impl_->ScrollBegin(BeginState(gfx::Point(0, 0)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(0, 0), gfx::Vector2dF(100.f, 100.f)).get());
    host_impl_->ScrollEnd(EndState().get());

    EXPECT_VECTOR_EQ(gfx::Vector2dF(), outer_scroll->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(100.f, 50.f),
                     scroll_layer->CurrentScrollOffset());
    EXPECT_EQ(0.f,
              host_impl_->active_tree()->CurrentBrowserControlsShownRatio());
  }
}

class LayerTreeHostImplVirtualViewportTest : public LayerTreeHostImplTest {
 public:
  void SetupVirtualViewportLayers(const gfx::Size& content_size,
                                  const gfx::Size& outer_viewport,
                                  const gfx::Size& inner_viewport) {
    LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
    const int kOuterViewportClipLayerId = 6;
    const int kOuterViewportScrollLayerId = 7;
    const int kInnerViewportScrollLayerId = 2;
    const int kInnerViewportClipLayerId = 4;
    const int kPageScaleLayerId = 5;

    std::unique_ptr<LayerImpl> inner_scroll =
        LayerImpl::Create(layer_tree_impl, kInnerViewportScrollLayerId);
    inner_scroll->test_properties()->is_container_for_fixed_position_layers =
        true;
    inner_scroll->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            inner_scroll->element_id(), gfx::ScrollOffset());

    std::unique_ptr<LayerImpl> inner_clip =
        LayerImpl::Create(layer_tree_impl, kInnerViewportClipLayerId);
    inner_clip->SetBounds(inner_viewport);

    std::unique_ptr<LayerImpl> page_scale =
        LayerImpl::Create(layer_tree_impl, kPageScaleLayerId);

    inner_scroll->SetScrollable(inner_viewport);
    inner_scroll->SetElementId(
        LayerIdToElementIdForTesting(inner_scroll->id()));
    inner_scroll->SetBounds(outer_viewport);
    inner_scroll->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> outer_clip =
        LayerImpl::Create(layer_tree_impl, kOuterViewportClipLayerId);
    outer_clip->SetBounds(outer_viewport);
    outer_clip->test_properties()->is_container_for_fixed_position_layers =
        true;

    std::unique_ptr<LayerImpl> outer_scroll =
        LayerImpl::Create(layer_tree_impl, kOuterViewportScrollLayerId);
    outer_scroll->SetScrollable(outer_viewport);
    outer_scroll->SetElementId(
        LayerIdToElementIdForTesting(outer_scroll->id()));
    outer_scroll->layer_tree_impl()
        ->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            outer_scroll->element_id(), gfx::ScrollOffset());
    outer_scroll->SetBounds(content_size);
    outer_scroll->SetPosition(gfx::PointF());

    std::unique_ptr<LayerImpl> contents = LayerImpl::Create(layer_tree_impl, 8);
    contents->SetDrawsContent(true);
    contents->SetBounds(content_size);
    contents->SetPosition(gfx::PointF());

    outer_scroll->test_properties()->AddChild(std::move(contents));
    outer_clip->test_properties()->AddChild(std::move(outer_scroll));
    inner_scroll->test_properties()->AddChild(std::move(outer_clip));
    page_scale->test_properties()->AddChild(std::move(inner_scroll));
    inner_clip->test_properties()->AddChild(std::move(page_scale));

    inner_clip->test_properties()->force_render_surface = true;
    layer_tree_impl->SetRootLayerForTesting(std::move(inner_clip));
    LayerTreeImpl::ViewportLayerIds viewport_ids;
    viewport_ids.page_scale = kPageScaleLayerId;
    viewport_ids.inner_viewport_container = kInnerViewportClipLayerId;
    viewport_ids.outer_viewport_container = kOuterViewportClipLayerId;
    viewport_ids.inner_viewport_scroll = kInnerViewportScrollLayerId;
    viewport_ids.outer_viewport_scroll = kOuterViewportScrollLayerId;
    layer_tree_impl->SetViewportLayersFromIds(viewport_ids);

    host_impl_->active_tree()->BuildPropertyTreesForTesting();
    host_impl_->active_tree()->DidBecomeActive();
  }
};

TEST_F(LayerTreeHostImplVirtualViewportTest, ScrollBothInnerAndOuterLayer) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupVirtualViewportLayers(content_size, outer_viewport, inner_viewport);

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::ScrollOffset inner_expected;
    gfx::ScrollOffset outer_expected;
    EXPECT_EQ(inner_expected, inner_scroll->CurrentScrollOffset());
    EXPECT_EQ(outer_expected, outer_scroll->CurrentScrollOffset());

    gfx::ScrollOffset current_offset(70.f, 100.f);

    host_impl_->SetSynchronousInputHandlerRootScrollOffset(current_offset);
    EXPECT_EQ(gfx::ScrollOffset(25.f, 40.f), inner_scroll->MaxScrollOffset());
    EXPECT_EQ(gfx::ScrollOffset(50.f, 80.f), outer_scroll->MaxScrollOffset());

    // Outer viewport scrolls first. Then the rest is applied to the inner
    // viewport.
    EXPECT_EQ(gfx::ScrollOffset(20.f, 20.f),
              inner_scroll->CurrentScrollOffset());
    EXPECT_EQ(gfx::ScrollOffset(50.f, 80.f),
              outer_scroll->CurrentScrollOffset());
  }
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       DiagonalScrollBubblesPerfectlyToInner) {
  gfx::Size content_size = gfx::Size(200, 320);
  gfx::Size outer_viewport = gfx::Size(100, 160);
  gfx::Size inner_viewport = gfx::Size(50, 80);

  SetupVirtualViewportLayers(content_size, outer_viewport, inner_viewport);

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();

  DrawFrame();
  {
    gfx::Vector2dF inner_expected;
    gfx::Vector2dF outer_expected;
    EXPECT_VECTOR_EQ(inner_expected, inner_scroll->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(outer_expected, outer_scroll->CurrentScrollOffset());

    // Make sure the scroll goes to the inner viewport first.
    EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
              host_impl_
                  ->ScrollBegin(BeginState(gfx::Point()).get(),
                                InputHandler::TOUCHSCREEN)
                  .thread);
    EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(
        gfx::Point(), InputHandler::TOUCHSCREEN));

    // Scroll near the edge of the outer viewport.
    gfx::Vector2d scroll_delta(inner_viewport.width() / 2.f,
                               inner_viewport.height() / 2.f);
    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    inner_expected += scroll_delta;
    EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(
        gfx::Point(), InputHandler::TOUCHSCREEN));

    EXPECT_VECTOR_EQ(inner_expected, inner_scroll->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(outer_expected, outer_scroll->CurrentScrollOffset());

    // Now diagonal scroll across the outer viewport boundary in a single event.
    // The entirety of the scroll should be consumed, as bubbling between inner
    // and outer viewport layers is perfect.
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::ScaleVector2d(scroll_delta, 2)).get());
    EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(
        gfx::Point(), InputHandler::TOUCHSCREEN));
    outer_expected += scroll_delta;
    inner_expected += scroll_delta;
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(
        gfx::Point(), InputHandler::TOUCHSCREEN));

    EXPECT_VECTOR_EQ(inner_expected, inner_scroll->CurrentScrollOffset());
    EXPECT_VECTOR_EQ(outer_expected, outer_scroll->CurrentScrollOffset());
  }
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       ScrollBeginEventThatTargetsViewportLayerSkipsHitTest) {
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);

  SetupVirtualViewportLayers(content_size, outer_viewport, inner_viewport);

  LayerImpl* outer_scroll = host_impl_->OuterViewportScrollLayer();

  std::unique_ptr<LayerImpl> child = CreateScrollableLayer(10, outer_viewport);
  LayerImpl* child_scroll = child.get();
  outer_scroll->test_properties()->children[0]->test_properties()->AddChild(
      std::move(child));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->RootScrollBegin(BeginState(gfx::Point()).get(),
                                  InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            host_impl_->ViewportMainScrollLayer()->scroll_tree_index());
  host_impl_->ScrollEnd(EndState().get());
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  EXPECT_EQ(host_impl_->CurrentlyScrollingNode()->id,
            child_scroll->scroll_tree_index());
  host_impl_->ScrollEnd(EndState().get());
}

TEST_F(LayerTreeHostImplVirtualViewportTest,
       NoOverscrollWhenInnerViewportCantScroll) {
  InputHandlerScrollResult scroll_result;
  gfx::Size content_size = gfx::Size(100, 160);
  gfx::Size outer_viewport = gfx::Size(50, 80);
  gfx::Size inner_viewport = gfx::Size(25, 40);
  SetupVirtualViewportLayers(content_size, outer_viewport, inner_viewport);
  // Make inner viewport unscrollable.
  LayerImpl* inner_scroll = host_impl_->InnerViewportScrollLayer();
  inner_scroll->test_properties()->user_scrollable_horizontal = false;
  inner_scroll->test_properties()->user_scrollable_vertical = false;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  // Ensure inner viewport doesn't react to scrolls (test it's unscrollable).
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), inner_scroll->CurrentScrollOffset());
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);
  scroll_result = host_impl_->ScrollBy(
      UpdateState(gfx::Point(), gfx::Vector2dF(0, 100)).get());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), inner_scroll->CurrentScrollOffset());

  // When inner viewport is unscrollable, a fling gives zero overscroll.
  EXPECT_FALSE(scroll_result.did_overscroll_root);
  EXPECT_EQ(gfx::Vector2dF(), host_impl_->accumulated_root_overscroll());
}

class LayerTreeHostImplWithImplicitLimitsTest : public LayerTreeHostImplTest {
 public:
  void SetUp() override {
    LayerTreeSettings settings = DefaultSettings();
    settings.max_memory_for_prepaint_percentage = 50;
    CreateHostImpl(settings, CreateLayerTreeFrameSink());
  }
};

TEST_F(LayerTreeHostImplWithImplicitLimitsTest, ImplicitMemoryLimits) {
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

TEST_F(LayerTreeHostImplTest, ExternalTransformReflectedInNextDraw) {
  const gfx::Size layer_size(100, 100);
  gfx::Transform external_transform;
  const gfx::Rect external_viewport(layer_size);
  const bool resourceless_software_draw = false;
  LayerImpl* layer = SetupScrollAndContentsLayers(layer_size);
  layer->SetDrawsContent(true);

  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      external_transform, layer->draw_properties().target_space_transform);

  external_transform.Translate(20, 20);
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      external_transform, layer->draw_properties().target_space_transform);
}

TEST_F(LayerTreeHostImplTest, ExternalTransformSetNeedsRedraw) {
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  const gfx::Size viewport_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

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

TEST_F(LayerTreeHostImplTest, OnMemoryPressure) {
  gfx::Size size(200, 200);
  viz::ResourceFormat format = viz::RGBA_8888;
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  ResourcePool::InUsePoolResource resource =
      host_impl_->resource_pool()->AcquireResource(size, format, color_space);
  host_impl_->resource_pool()->ReleaseResource(std::move(resource));

  size_t current_memory_usage =
      host_impl_->resource_pool()->GetTotalMemoryUsageForTesting();

  base::MemoryPressureListener::SimulatePressureNotification(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  size_t memory_usage_after_memory_pressure =
      host_impl_->resource_pool()->GetTotalMemoryUsageForTesting();

  // Memory usage after the memory pressure should be less than previous one.
  EXPECT_LT(memory_usage_after_memory_pressure, current_memory_usage);
}

TEST_F(LayerTreeHostImplTest, OnDrawConstraintSetNeedsRedraw) {
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  const gfx::Size viewport_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

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
TEST_F(LayerTreeHostImplTest, FullViewportDamageAfterOnDraw) {
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  const gfx::Size viewport_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(gfx::Point(5, 5), viewport_size);
  bool resourceless_software_draw = false;

  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_EQ(draw_viewport, host_impl_->active_tree()->GetDeviceViewport());

  host_impl_->SetFullViewportDamage();
  EXPECT_EQ(gfx::Rect(viewport_size),
            host_impl_->viewport_damage_rect_for_testing());
}

class ResourcelessSoftwareLayerTreeHostImplTest : public LayerTreeHostImplTest {
 protected:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

TEST_F(ResourcelessSoftwareLayerTreeHostImplTest,
       ResourcelessSoftwareSetNeedsRedraw) {
  SetupRootLayerImpl(LayerImpl::Create(host_impl_->active_tree(), 1));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  const gfx::Size viewport_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Clear any damage.
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  last_on_draw_frame_.reset();

  // Always swap even if same draw params.
  resourceless_software_draw = true;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
  last_on_draw_frame_.reset();

  // Next hardware draw has damage.
  resourceless_software_draw = false;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_FALSE(last_on_draw_frame_->has_no_damage);
}

TEST_F(ResourcelessSoftwareLayerTreeHostImplTest,
       ResourcelessSoftwareDrawSkipsUpdateTiles) {
  const gfx::Size viewport_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);

  CreatePendingTree();
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(viewport_size));
  std::unique_ptr<FakePictureLayerImpl> layer(
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                   11, raster_source));
  layer->SetBounds(viewport_size);
  layer->SetDrawsContent(true);
  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(layer));

  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();

  const gfx::Transform draw_transform;
  const gfx::Rect draw_viewport(viewport_size);
  bool resourceless_software_draw = false;

  // Regular draw causes UpdateTiles.
  did_request_prepare_tiles_ = false;
  host_impl_->OnDraw(draw_transform, draw_viewport, resourceless_software_draw,
                     false);
  EXPECT_TRUE(did_request_prepare_tiles_);
  host_impl_->PrepareTiles();

  // Resourceless draw skips UpdateTiles.
  const gfx::Rect new_draw_viewport(50, 50);
  resourceless_software_draw = true;
  did_request_prepare_tiles_ = false;
  host_impl_->OnDraw(draw_transform, new_draw_viewport,
                     resourceless_software_draw, false);
  EXPECT_FALSE(did_request_prepare_tiles_);
}

TEST_F(CommitToPendingTreeLayerTreeHostImplTest,
       ExternalTileConstraintReflectedInPendingTree) {
  EXPECT_FALSE(host_impl_->CommitToActiveTree());
  const gfx::Size layer_size(100, 100);
  host_impl_->active_tree()->SetDeviceViewportSize(layer_size);

  // Set up active and pending tree.
  CreatePendingTree();
  host_impl_->pending_tree()->SetRootLayerForTesting(
      LayerImpl::Create(host_impl_->pending_tree(), 1));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->pending_tree()->UpdateDrawProperties();
  host_impl_->pending_tree()
      ->root_layer_for_testing()
      ->SetNeedsPushProperties();

  host_impl_->ActivateSyncTree();
  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateDrawProperties();

  CreatePendingTree();
  host_impl_->pending_tree()->UpdateDrawProperties();
  host_impl_->active_tree()->UpdateDrawProperties();

  EXPECT_FALSE(host_impl_->pending_tree()->needs_update_draw_properties());
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());

  // Update external constraints should set_needs_update_draw_properties on
  // both trees.
  gfx::Transform external_transform;
  gfx::Rect external_viewport(10, 20);
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);
  EXPECT_TRUE(host_impl_->pending_tree()->needs_update_draw_properties());
  EXPECT_TRUE(host_impl_->active_tree()->needs_update_draw_properties());
}

TEST_F(LayerTreeHostImplTest, ExternalViewportAffectsVisibleRects) {
  const gfx::Size layer_size(100, 100);
  SetupScrollAndContentsLayers(layer_size);
  LayerImpl* content_layer = host_impl_->active_tree()
                                 ->OuterViewportScrollLayer()
                                 ->test_properties()
                                 ->children[0];

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(90, 90));
  host_impl_->active_tree()->UpdateDrawProperties();
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

TEST_F(LayerTreeHostImplTest, ExternalTransformAffectsVisibleRects) {
  const gfx::Size layer_size(100, 100);
  SetupScrollAndContentsLayers(layer_size);
  LayerImpl* content_layer = host_impl_->active_tree()
                                 ->OuterViewportScrollLayer()
                                 ->test_properties()
                                 ->children[0];

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->UpdateDrawProperties();
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

TEST_F(LayerTreeHostImplTest, ExternalTransformAffectsSublayerScaleFactor) {
  const gfx::Size layer_size(100, 100);
  SetupScrollAndContentsLayers(layer_size);
  LayerImpl* content_layer = host_impl_->active_tree()
                                 ->OuterViewportScrollLayer()
                                 ->test_properties()
                                 ->children[0];
  content_layer->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->active_tree(), 100));
  LayerImpl* test_layer = host_impl_->active_tree()->LayerById(100);
  test_layer->test_properties()->force_render_surface = true;
  test_layer->SetDrawsContent(true);
  test_layer->SetBounds(layer_size);
  gfx::Transform perspective_transform;
  perspective_transform.ApplyPerspectiveDepth(2);
  test_layer->test_properties()->transform = perspective_transform;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  host_impl_->active_tree()->UpdateDrawProperties();
  EffectNode* node =
      host_impl_->active_tree()->property_trees()->effect_tree.Node(
          test_layer->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1.f, 1.f));

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
  node = host_impl_->active_tree()->property_trees()->effect_tree.Node(
      test_layer->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2.f, 2.f));

  // Clear the external transform.
  external_transform = gfx::Transform();
  host_impl_->SetExternalTilePriorityConstraints(external_viewport,
                                                 external_transform);

  host_impl_->OnDraw(external_transform, external_viewport,
                     resourceless_software_draw, false);
  node = host_impl_->active_tree()->property_trees()->effect_tree.Node(
      test_layer->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1.f, 1.f));
}

TEST_F(LayerTreeHostImplTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), scrolling_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   scrolling_layer->CurrentScrollOffset());
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame();
}

// Test to ensure that animated scrolls correctly account for the page scale
// factor. That is, if you zoom into the page, a wheel scroll should scroll the
// content *less* than before so that it appears to move the same distance when
// zoomed in.
TEST_F(LayerTreeHostImplTest, ScrollAnimatedWhileZoomed) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);
  LayerImpl* scrolling_layer = host_impl_->InnerViewportScrollLayer();

  DrawFrame();

  // Zoom in to 2X
  {
    float min_page_scale = 1.f, max_page_scale = 4.f;
    float page_scale_factor = 2.f;
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
  }

  // Start an animated scroll then do another animated scroll immediately
  // afterwards. This will ensure we test both the starting animation and
  // animation update code.
  {
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_->ScrollAnimated(gfx::Point(10, 10), gfx::Vector2d(0, 10))
            .thread);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_->ScrollAnimated(gfx::Point(10, 10), gfx::Vector2d(0, 20))
            .thread);

    EXPECT_EQ(scrolling_layer->scroll_tree_index(),
              host_impl_->CurrentlyScrollingNode()->id);
  }

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Tick a frame to get the animation started.
  {
    begin_frame_args.frame_time = start_time;
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_NE(0, scrolling_layer->CurrentScrollOffset().y());
    host_impl_->DidFinishImplFrame();
  }

  // Tick ahead to the end of the animation. We scrolled 30 viewport pixels but
  // since we're zoomed in to 2x we should have scrolled 15 content pixels.
  {
    begin_frame_args.frame_time =
        start_time + base::TimeDelta::FromMilliseconds(1000);
    begin_frame_args.sequence_number++;
    host_impl_->WillBeginImplFrame(begin_frame_args);
    host_impl_->Animate();
    host_impl_->UpdateAnimationState(true);

    EXPECT_EQ(15, scrolling_layer->CurrentScrollOffset().y());
    host_impl_->DidFinishImplFrame();
  }
}

TEST_F(LayerTreeHostImplTest, SecondScrollAnimatedBeginNotIgnored) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimatedBegin(BeginState(gfx::Point()).get()).thread);

  // The second ScrollAnimatedBegin should not get ignored.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimatedBegin(BeginState(gfx::Point()).get()).thread);
}

// Verfify that a smooth scroll animation doesn't jump when UpdateTarget gets
// called before the animation is started.
TEST_F(LayerTreeHostImplTest, AnimatedScrollUpdateTargetBeforeStarting) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(200);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);
  // This will call ScrollOffsetAnimationCurve::UpdateTarget while the animation
  // created above is in state ANIMATION::WAITING_FOR_TARGET_AVAILABILITY and
  // doesn't have a start time.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 100)).thread);

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.sequence_number++;
  // This is when the animation above gets promoted to STARTING.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(300);
  begin_frame_args.sequence_number++;
  // This is when the animation above gets ticked.
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify no jump.
  float y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 1 && y < 49);
}

TEST_F(LayerTreeHostImplTest, ScrollAnimatedWithDelay) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(50, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Create animation with a 100ms delay.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 100),
                                 base::TimeDelta::FromMilliseconds(100))
                .thread);

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // First tick, animation is started.
  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_NE(gfx::ScrollOffset(), scrolling_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  // Second tick after 50ms, animation should be half way done since the
  // duration due to delay is 100ms. Subtract off the frame interval since we
  // progress a full frame on the first tick.
  base::TimeTicks half_way_time = start_time - begin_frame_args.interval +
                                  base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.frame_time = half_way_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(50, scrolling_layer->CurrentScrollOffset().y());
  host_impl_->DidFinishImplFrame();

  // Update target.
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 100),
                                 base::TimeDelta::FromMilliseconds(150))
                .thread);

  // Third tick after 100ms, should be at the target position since update
  // target was called with a large value of jank.
  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(100);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->UpdateAnimationState(true);
  EXPECT_LT(100, scrolling_layer->CurrentScrollOffset().y());
}

// Test that a smooth scroll offset animation is aborted when followed by a
// non-smooth scroll offset animation.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedAborted) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  // Perform animated scroll.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_TRUE(GetImplAnimationHost()->HasAnyAnimationTargetingProperty(
      scrolling_layer->element_id(), TargetProperty::SCROLL_OFFSET));

  EXPECT_NE(gfx::ScrollOffset(), scrolling_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Perform instant scroll.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_
          ->ScrollBegin(BeginState(gfx::Point(0, y)).get(), InputHandler::WHEEL)
          .thread);
  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(0, y),
                                                      InputHandler::WHEEL));
  host_impl_->ScrollBy(
      UpdateState(gfx::Point(0, y), gfx::Vector2d(0, 50)).get());
  EXPECT_TRUE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(0, y + 50),
                                                      InputHandler::WHEEL));
  std::unique_ptr<ScrollState> scroll_state_end = EndState();
  host_impl_->ScrollEnd(scroll_state_end.get());
  EXPECT_FALSE(host_impl_->IsCurrentlyScrollingLayerAt(gfx::Point(),
                                                       InputHandler::WHEEL));

  // The instant scroll should have marked the smooth scroll animation as
  // aborted.
  EXPECT_FALSE(GetImplAnimationHost()->HasTickingKeyframeModelForTesting(
      scrolling_layer->element_id()));

  EXPECT_VECTOR2DF_EQ(gfx::ScrollOffset(0, y + 50),
                      scrolling_layer->CurrentScrollOffset());
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame();
}

// Evolved from LayerTreeHostImplTest.ScrollAnimated.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimated) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), scrolling_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  float y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(0, 50)).thread);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   scrolling_layer->CurrentScrollOffset());
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame();
}

// Test that the scroll delta for an animated scroll is distributed correctly
// between the inner and outer viewport.
TEST_F(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimated) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 2.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll by a small amount, there should be no bubbling to the outer
  // viewport.
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(250);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(10.f, 20.f))
                .thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(inner_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), outer_scroll_layer->CurrentScrollOffset());

  // Scroll by the inner viewport's max scroll extent, the remainder
  // should bubble up to the outer viewport.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(100.f, 100.f))
          .thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(inner_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(350));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(5, 10),
                   outer_scroll_layer->CurrentScrollOffset());

  // Scroll by the outer viewport's max scroll extent, it should all go to the
  // outer viewport.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(190.f, 180.f))
          .thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(outer_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(850));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(100, 100),
                   outer_scroll_layer->CurrentScrollOffset());

  // Scroll upwards by the max scroll extent. The inner viewport should animate
  // and the remainder should bubble to the outer viewport.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(-110.f, -120.f))
          .thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(inner_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(
      begin_frame_args, start_time + base::TimeDelta::FromMilliseconds(1200));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(95, 90),
                   outer_scroll_layer->CurrentScrollOffset());
}

// Test that the correct viewport scroll layer is updated when the target offset
// is updated.
TEST_F(LayerTreeHostImplTimelinesTest, ImplPinchZoomScrollAnimatedUpdate) {
  const gfx::Size content_size(200, 200);
  const gfx::Size viewport_size(100, 100);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  LayerImpl* outer_scroll_layer = host_impl_->OuterViewportScrollLayer();
  LayerImpl* inner_scroll_layer = host_impl_->InnerViewportScrollLayer();

  // Zoom into the page by a 2X factor
  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 2.f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale, max_page_scale);
  host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);

  // Scroll the inner viewport.
  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(50);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(90, 90)).thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(inner_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args, start_time);
  float inner_x = inner_scroll_layer->CurrentScrollOffset().x();
  float inner_y = inner_scroll_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(inner_x > 0 && inner_x < 45);
  EXPECT_TRUE(inner_y > 0 && inner_y < 45);
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());

  // Update target.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(50, 50)).thread);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  EXPECT_EQ(inner_scroll_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  // Verify that all the delta is applied to the inner viewport and nothing is
  // carried forward.
  begin_frame_args.sequence_number++;
  BeginImplFrameAndAnimate(begin_frame_args,
                           start_time + base::TimeDelta::FromMilliseconds(350));
  EXPECT_VECTOR_EQ(gfx::Vector2dF(50, 50),
                   inner_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0),
                   outer_scroll_layer->CurrentScrollOffset());
  EXPECT_VECTOR_EQ(gfx::Vector2dF(), outer_scroll_layer->CurrentScrollOffset());
}

// Test that smooth scroll offset animation doesn't happen for non user
// scrollable layers.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedNotUserScrollable) {
  const gfx::Size content_size(1000, 1000);
  const gfx::Size viewport_size(500, 500);
  CreateBasicVirtualViewportLayers(viewport_size, content_size);

  host_impl_->OuterViewportScrollLayer()
      ->test_properties()
      ->user_scrollable_vertical = true;
  host_impl_->OuterViewportScrollLayer()
      ->test_properties()
      ->user_scrollable_horizontal = false;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(50, 50)).thread);

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_NE(gfx::ScrollOffset(), scrolling_layer->CurrentScrollOffset());
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(50);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  // Should not have scrolled horizontally.
  EXPECT_EQ(0, scrolling_layer->CurrentScrollOffset().x());
  float y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 1 && y < 49);

  // Update target.
  EXPECT_EQ(
      InputHandler::SCROLL_ON_IMPL_THREAD,
      host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(50, 50)).thread);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  y = scrolling_layer->CurrentScrollOffset().y();
  EXPECT_TRUE(y > 50 && y < 100);
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);
  host_impl_->DidFinishImplFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(250);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);

  EXPECT_VECTOR_EQ(gfx::ScrollOffset(0, 100),
                   scrolling_layer->CurrentScrollOffset());
  EXPECT_EQ(nullptr, host_impl_->CurrentlyScrollingNode());
  host_impl_->DidFinishImplFrame();
}

// Test that smooth scrolls clamp correctly when bounds change mid-animation.
TEST_F(LayerTreeHostImplTimelinesTest, ScrollAnimatedChangingBounds) {
  const gfx::Size old_content_size(1000, 1000);
  const gfx::Size new_content_size(750, 750);
  const gfx::Size viewport_size(500, 500);

  LayerImpl* content_layer =
      CreateBasicVirtualViewportLayers(viewport_size, old_content_size);

  DrawFrame();

  base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMilliseconds(100);
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);

  host_impl_->ScrollAnimated(gfx::Point(), gfx::Vector2d(500, 500));

  LayerImpl* scrolling_layer = host_impl_->OuterViewportScrollLayer();
  EXPECT_EQ(scrolling_layer->scroll_tree_index(),
            host_impl_->CurrentlyScrollingNode()->id);

  begin_frame_args.frame_time = start_time;
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  content_layer->SetBounds(new_content_size);
  scrolling_layer->SetBounds(new_content_size);
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  begin_frame_args.frame_time =
      start_time + base::TimeDelta::FromMilliseconds(200);
  begin_frame_args.sequence_number++;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame();

  EXPECT_EQ(gfx::ScrollOffset(250, 250),
            scrolling_layer->CurrentScrollOffset());
}

TEST_F(LayerTreeHostImplTest, InvalidLayerNotAddedToRasterQueue) {
  CreatePendingTree();

  Region empty_invalidation;
  scoped_refptr<RasterSource> raster_source_with_tiles(
      FakeRasterSource::CreateFilled(gfx::Size(10, 10)));

  std::unique_ptr<FakePictureLayerImpl> layer =
      FakePictureLayerImpl::Create(host_impl_->pending_tree(), 11);
  layer->SetBounds(gfx::Size(10, 10));
  layer->set_gpu_raster_max_texture_size(
      host_impl_->active_tree()->GetDeviceViewport().size());
  layer->SetDrawsContent(true);
  layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source_with_tiles);
  layer->UpdateRasterSource(raster_source_with_tiles, &empty_invalidation,
                            nullptr);
  layer->tilings()->tiling_at(0)->set_resolution(
      TileResolution::HIGH_RESOLUTION);
  layer->tilings()->tiling_at(0)->CreateAllTilesForTesting();
  layer->tilings()->UpdateTilePriorities(gfx::Rect(gfx::Size(10, 10)), 1.f, 1.0,
                                         Occlusion(), true);
  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(layer));

  auto* root_layer = static_cast<FakePictureLayerImpl*>(
      host_impl_->pending_tree()->root_layer_for_testing());

  root_layer->set_has_valid_tile_priorities(true);
  std::unique_ptr<RasterTilePriorityQueue> non_empty_raster_priority_queue_all =
      host_impl_->BuildRasterQueue(TreePriority::SAME_PRIORITY_FOR_BOTH_TREES,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_FALSE(non_empty_raster_priority_queue_all->IsEmpty());

  root_layer->set_has_valid_tile_priorities(false);
  std::unique_ptr<RasterTilePriorityQueue> empty_raster_priority_queue_all =
      host_impl_->BuildRasterQueue(TreePriority::SAME_PRIORITY_FOR_BOTH_TREES,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_TRUE(empty_raster_priority_queue_all->IsEmpty());
}

TEST_F(LayerTreeHostImplTest, DidBecomeActive) {
  CreatePendingTree();
  host_impl_->ActivateSyncTree();
  CreatePendingTree();

  LayerTreeImpl* pending_tree = host_impl_->pending_tree();

  std::unique_ptr<FakePictureLayerImpl> pending_layer =
      FakePictureLayerImpl::Create(pending_tree, 10);
  FakePictureLayerImpl* raw_pending_layer = pending_layer.get();
  pending_tree->SetRootLayerForTesting(std::move(pending_layer));
  ASSERT_EQ(raw_pending_layer, pending_tree->root_layer_for_testing());

  EXPECT_EQ(0u, raw_pending_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(1u, raw_pending_layer->did_become_active_call_count());

  std::unique_ptr<FakePictureLayerImpl> mask_layer =
      FakePictureLayerImpl::Create(pending_tree, 11);
  FakePictureLayerImpl* raw_mask_layer = mask_layer.get();
  raw_pending_layer->test_properties()->SetMaskLayer(std::move(mask_layer));
  ASSERT_EQ(raw_mask_layer, raw_pending_layer->test_properties()->mask_layer);
  pending_tree->BuildPropertyTreesForTesting();

  EXPECT_EQ(1u, raw_pending_layer->did_become_active_call_count());
  EXPECT_EQ(0u, raw_mask_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(2u, raw_pending_layer->did_become_active_call_count());
  EXPECT_EQ(1u, raw_mask_layer->did_become_active_call_count());

  pending_tree->BuildPropertyTreesForTesting();

  EXPECT_EQ(2u, raw_pending_layer->did_become_active_call_count());
  EXPECT_EQ(1u, raw_mask_layer->did_become_active_call_count());
  pending_tree->DidBecomeActive();
  EXPECT_EQ(3u, raw_pending_layer->did_become_active_call_count());
  EXPECT_EQ(2u, raw_mask_layer->did_become_active_call_count());
}

TEST_F(LayerTreeHostImplTest, WheelScrollWithPageScaleFactorOnInnerLayer) {
  LayerImpl* scroll_layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(50, 50));
  DrawFrame();

  EXPECT_EQ(scroll_layer, host_impl_->InnerViewportScrollLayer());

  float min_page_scale = 1.f, max_page_scale = 4.f;
  float page_scale_factor = 1.f;

  // The scroll deltas should have the page scale factor applied.
  {
    host_impl_->active_tree()->PushPageScaleFromMainThread(
        page_scale_factor, min_page_scale, max_page_scale);
    host_impl_->active_tree()->SetPageScaleOnActiveTree(page_scale_factor);
    SetScrollOffsetDelta(scroll_layer, gfx::Vector2d());

    float page_scale_delta = 2.f;
    host_impl_->ScrollBegin(BeginState(gfx::Point()).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->PinchGestureBegin();
    host_impl_->PinchGestureUpdate(page_scale_delta, gfx::Point());
    host_impl_->PinchGestureEnd(gfx::Point(), true);
    host_impl_->ScrollEnd(EndState().get());

    gfx::Vector2dF scroll_delta(0, 5);
    EXPECT_EQ(
        InputHandler::SCROLL_ON_IMPL_THREAD,
        host_impl_
            ->ScrollBegin(BeginState(gfx::Point()).get(), InputHandler::WHEEL)
            .thread);
    EXPECT_VECTOR_EQ(gfx::Vector2dF(), scroll_layer->CurrentScrollOffset());

    host_impl_->ScrollBy(UpdateState(gfx::Point(), scroll_delta).get());
    host_impl_->ScrollEnd(EndState().get());
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 2.5),
                     scroll_layer->CurrentScrollOffset());
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

// We do not want to reset context recovery state when we get repeated context
// loss notifications via different paths.
TEST_F(LayerTreeHostImplCountingLostSurfaces, TwiceLostSurface) {
  EXPECT_EQ(0, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
  host_impl_->DidLoseLayerTreeFrameSink();
  EXPECT_EQ(1, num_lost_surfaces_);
}

size_t CountRenderPassesWithId(const viz::RenderPassList& list,
                               viz::RenderPassId id) {
  return std::count_if(
      list.begin(), list.end(),
      [id](const std::unique_ptr<viz::RenderPass>& p) { return p->id == id; });
}

TEST_F(LayerTreeHostImplTest, RemoveUnreferencedRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // Add a quad to each pass so they aren't empty.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::SOLID_COLOR;
  color_quad = pass2->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::SOLID_COLOR;
  color_quad = pass3->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::SOLID_COLOR;

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::RENDER_PASS;
  rpdq->render_pass_id = pass3->id;

  // But pass2 is not referenced by pass1. So pass2 and pass3 should be culled.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
}

TEST_F(LayerTreeHostImplTest, RemoveEmptyRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // pass1 is not empty, but pass2 and pass3 are.
  auto* color_quad = pass1->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
  color_quad->material = viz::DrawQuad::SOLID_COLOR;

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::RENDER_PASS;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::RENDER_PASS;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
  // The viz::RenderPassDrawQuad should be removed from pass1.
  EXPECT_EQ(1u, pass1->quad_list.size());
  EXPECT_EQ(viz::DrawQuad::SOLID_COLOR,
            pass1->quad_list.ElementAt(0)->material);
}

TEST_F(LayerTreeHostImplTest, DoNotRemoveEmptyRootRenderPass) {
  TestFrameData frame;
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass3 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass2 = frame.render_passes.back().get();
  frame.render_passes.push_back(viz::RenderPass::Create());
  viz::RenderPass* pass1 = frame.render_passes.back().get();

  pass1->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass2->SetNew(2, gfx::Rect(), gfx::Rect(), gfx::Transform());
  pass3->SetNew(3, gfx::Rect(), gfx::Rect(), gfx::Transform());

  // pass3 is referenced by pass2.
  auto* rpdq = pass2->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::RENDER_PASS;
  rpdq->render_pass_id = pass3->id;

  // pass2 is referenced by pass1.
  rpdq = pass1->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  rpdq->material = viz::DrawQuad::RENDER_PASS;
  rpdq->render_pass_id = pass2->id;

  // Since pass3 is empty it should be removed. Then pass2 is empty too, and
  // should be removed. Then pass1 is empty too, but it's the root so it should
  // not be removed.
  FakeLayerTreeHostImpl::RemoveRenderPasses(&frame);
  EXPECT_EQ(1u, frame.render_passes.size());
  EXPECT_EQ(1u, CountRenderPassesWithId(frame.render_passes, 1u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 2u));
  EXPECT_EQ(0u, CountRenderPassesWithId(frame.render_passes, 3u));
  EXPECT_EQ(1u, frame.render_passes[0]->id);
  // The viz::RenderPassDrawQuad should be removed from pass1.
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

TEST_F(LayerTreeHostImplTest, AddVideoFrameControllerInsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  EXPECT_FALSE(controller.begin_frame_args().IsValid());
  host_impl_->AddVideoFrameController(&controller);
  EXPECT_TRUE(controller.begin_frame_args().IsValid());
  host_impl_->DidFinishImplFrame();

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

TEST_F(LayerTreeHostImplTest, AddVideoFrameControllerOutsideFrame) {
  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  FakeVideoFrameController controller;

  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->DidFinishImplFrame();

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

// Tests that SetHasGpuRasterizationTrigger behaves as expected.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusTrigger) {
  // Set initial state, before varying GPU rasterization trigger.
  host_impl_->SetHasGpuRasterizationTrigger(false);
  host_impl_->SetContentHasSlowPaths(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::OFF_VIEWPORT,
            host_impl_->gpu_rasterization_status());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();

  // Toggle the trigger on.
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();

  // And off.
  host_impl_->SetHasGpuRasterizationTrigger(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::OFF_VIEWPORT,
            host_impl_->gpu_rasterization_status());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();
}

// Tests that SetContentHasSlowPaths behaves as expected.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusSlowPaths) {
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = 4;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(
                        msaaSettings.gpu_rasterization_msaa_sample_count)));

  // Set initial state, with slow paths on.
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_msaa());
  host_impl_->NotifyReadyToActivate();

  // Toggle slow paths off.
  host_impl_->SetContentHasSlowPaths(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());
  host_impl_->NotifyReadyToActivate();

  // And on.
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->use_msaa());
  host_impl_->NotifyReadyToActivate();
}

// Tests that SetDeviceScaleFactor correctly impacts GPU rasterization.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusDeviceScaleFactor) {
  // Create a host impl with MSAA support (4 samples).
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = -1;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(4)));

  // Set initial state, before varying scale factor.
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  host_impl_->NotifyReadyToActivate();

  // Set device scale factor to 2, which lowers the required MSAA samples from
  // 8 to 4.
  host_impl_->active_tree()->SetDeviceScaleFactor(2.0f);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->use_msaa());
  host_impl_->NotifyReadyToActivate();

  // Set device scale factor back to 1.
  host_impl_->active_tree()->SetDeviceScaleFactor(1.0f);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());
  host_impl_->NotifyReadyToActivate();
}

// Tests that explicit MSAA sample count correctly impacts GPU rasterization.
TEST_F(LayerTreeHostImplTest, GpuRasterizationStatusExplicitMSAACount) {
  // Create a host impl with MSAA support and a forced sample count of 4.
  LayerTreeSettings msaaSettings = DefaultSettings();
  msaaSettings.gpu_rasterization_msaa_sample_count = 4;
  EXPECT_TRUE(CreateHostImpl(
      msaaSettings, FakeLayerTreeFrameSink::Create3dForGpuRasterization(
                        msaaSettings.gpu_rasterization_msaa_sample_count)));

  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_TRUE(host_impl_->use_msaa());
}

class GpuRasterizationDisabledLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink() override {
    return FakeLayerTreeFrameSink::Create3d();
  }
};

// Tests that GPU rasterization overrides work as expected.
TEST_F(GpuRasterizationDisabledLayerTreeHostImplTest,
       GpuRasterizationStatusOverrides) {
  // GPU rasterization explicitly disabled.
  LayerTreeSettings settings = DefaultSettings();
  EXPECT_TRUE(CreateHostImpl(settings, FakeLayerTreeFrameSink::Create3d()));
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::OFF_DEVICE,
            host_impl_->gpu_rasterization_status());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());

  // GPU rasterization explicitly forced.
  settings.gpu_rasterization_forced = true;
  EXPECT_TRUE(CreateHostImpl(settings, FakeLayerTreeFrameSink::Create3d()));

  host_impl_->SetHasGpuRasterizationTrigger(false);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON_FORCED,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
}

class MsaaIsSlowLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void CreateHostImplWithCaps(bool msaa_is_slow, bool avoid_stencil_buffers) {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_msaa_sample_count = 4;
    auto frame_sink =
        FakeLayerTreeFrameSink::Builder()
            .AllContexts(&viz::TestGLES2Interface::SetMaxSamples,
                         settings.gpu_rasterization_msaa_sample_count)
            .AllContexts(&viz::TestGLES2Interface::set_msaa_is_slow,
                         msaa_is_slow)
            .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
            .AllContexts(&viz::TestGLES2Interface::set_avoid_stencil_buffers,
                         avoid_stencil_buffers)
            .Build();
    EXPECT_TRUE(CreateHostImpl(settings, std::move(frame_sink)));
  }
};

TEST_F(MsaaIsSlowLayerTreeHostImplTest, GpuRasterizationStatusMsaaIsSlow) {
  // Ensure that without the msaa_is_slow or avoid_stencil_buffers caps
  // we raster slow paths with msaa.
  CreateHostImplWithCaps(false, false);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Ensure that with either msaa_is_slow or avoid_stencil_buffers caps
  // we don't raster slow paths with msaa (we'll still use GPU raster, though).
  // msaa_is_slow = true, avoid_stencil_buffers = false
  CreateHostImplWithCaps(true, false);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());

  // msaa_is_slow = false, avoid_stencil_buffers = true
  CreateHostImplWithCaps(false, true);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());

  // msaa_is_slow = true, avoid_stencil_buffers = true
  CreateHostImplWithCaps(true, true);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
  EXPECT_FALSE(host_impl_->use_msaa());
}

class MsaaCompatibilityLayerTreeHostImplTest : public LayerTreeHostImplTest {
 public:
  void CreateHostImplWithMultisampleCompatibility(
      bool support_multisample_compatibility) {
    LayerTreeSettings settings = DefaultSettings();
    settings.gpu_rasterization_msaa_sample_count = 4;
    auto frame_sink =
        FakeLayerTreeFrameSink::Builder()
            .AllContexts(&viz::TestGLES2Interface::SetMaxSamples,
                         settings.gpu_rasterization_msaa_sample_count)
            .AllContexts(
                &viz::TestGLES2Interface::set_support_multisample_compatibility,
                support_multisample_compatibility)
            .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
            .Build();
    EXPECT_TRUE(CreateHostImpl(settings, std::move(frame_sink)));
  }
};

TEST_F(MsaaCompatibilityLayerTreeHostImplTest,
       GpuRasterizationStatusNonAAPaint) {
  // Ensure that without non-aa paint and without multisample compatibility, we
  // raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(false);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->SetContentHasNonAAPaint(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Ensure that without non-aa paint and with multisample compatibility, we
  // raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(true);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->SetContentHasNonAAPaint(false);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Ensure that with non-aa paint and without multisample compatibility, we do
  // not raster slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(false);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->SetContentHasNonAAPaint(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::ON, host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Ensure that with non-aa paint and with multisample compatibility, we raster
  // slow paths with msaa.
  CreateHostImplWithMultisampleCompatibility(true);
  host_impl_->SetHasGpuRasterizationTrigger(true);
  host_impl_->SetContentHasSlowPaths(true);
  host_impl_->SetContentHasNonAAPaint(true);
  host_impl_->CommitComplete();
  EXPECT_EQ(GpuRasterizationStatus::MSAA_CONTENT,
            host_impl_->gpu_rasterization_status());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());
}

TEST_F(LayerTreeHostImplTest, UpdatePageScaleFactorOnActiveTree) {
  // Check page scale factor update in property trees when an update is made
  // on the active tree.
  CreatePendingTree();
  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 3.f);
  CreateScrollAndContentsLayers(host_impl_->pending_tree(),
                                gfx::Size(100, 100));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  LOG(ERROR) << "ACTIVATE SYNC TREE";
  host_impl_->ActivateSyncTree();
  LOG(ERROR) << "DONE ACTIVATE SYNC TREE";
  DrawFrame();

  CreatePendingTree();
  host_impl_->active_tree()->SetPageScaleOnActiveTree(2.f);
  LayerImpl* page_scale_layer = host_impl_->active_tree()->PageScaleLayer();

  TransformNode* active_tree_node =
      host_impl_->active_tree()->property_trees()->transform_tree.Node(
          page_scale_layer->transform_tree_index());
  // SetPageScaleOnActiveTree also updates the factors in property trees.
  EXPECT_EQ(active_tree_node->post_local_scale_factor, 2.f);
  EXPECT_EQ(host_impl_->active_tree()->current_page_scale_factor(), 2.f);

  TransformNode* pending_tree_node =
      host_impl_->pending_tree()->property_trees()->transform_tree.Node(
          page_scale_layer->transform_tree_index());
  EXPECT_EQ(pending_tree_node->post_local_scale_factor, 1.f);
  EXPECT_EQ(host_impl_->pending_tree()->current_page_scale_factor(), 2.f);

  host_impl_->pending_tree()->UpdateDrawProperties();
  pending_tree_node =
      host_impl_->pending_tree()->property_trees()->transform_tree.Node(
          page_scale_layer->transform_tree_index());
  EXPECT_EQ(pending_tree_node->post_local_scale_factor, 2.f);

  LOG(ERROR) << "2 ACTIVATE SYNC TREE";
  host_impl_->ActivateSyncTree();
  LOG(ERROR) << "DONE 2 ACTIVATE SYNC TREE";
  host_impl_->active_tree()->UpdateDrawProperties();
  active_tree_node =
      host_impl_->active_tree()->property_trees()->transform_tree.Node(
          page_scale_layer->transform_tree_index());
  EXPECT_EQ(active_tree_node->post_local_scale_factor, 2.f);
}

TEST_F(LayerTreeHostImplTest, SubLayerScaleForNodeInSubtreeOfPageScaleLayer) {
  // Checks that the sublayer scale of a transform node in the subtree of the
  // page scale layer is updated without a property tree rebuild.
  host_impl_->active_tree()->PushPageScaleFromMainThread(1.f, 1.f, 3.f);
  CreateScrollAndContentsLayers(host_impl_->active_tree(), gfx::Size(100, 100));
  LayerImpl* page_scale_layer = host_impl_->active_tree()->PageScaleLayer();
  page_scale_layer->test_properties()->AddChild(
      LayerImpl::Create(host_impl_->active_tree(), 100));

  LayerImpl* in_subtree_of_page_scale_layer =
      host_impl_->active_tree()->LayerById(100);
  in_subtree_of_page_scale_layer->test_properties()->force_render_surface =
      true;
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  DrawFrame();

  EffectNode* node =
      host_impl_->active_tree()->property_trees()->effect_tree.Node(
          in_subtree_of_page_scale_layer->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(1.f, 1.f));

  host_impl_->active_tree()->SetPageScaleOnActiveTree(2.f);

  DrawFrame();

  in_subtree_of_page_scale_layer = host_impl_->active_tree()->LayerById(100);
  node = host_impl_->active_tree()->property_trees()->effect_tree.Node(
      in_subtree_of_page_scale_layer->effect_tree_index());
  EXPECT_EQ(node->surface_contents_scale, gfx::Vector2dF(2.f, 2.f));
}

TEST_F(LayerTreeHostImplTest, JitterTest) {
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(100, 100));

  CreatePendingTree();
  CreateScrollAndContentsLayers(host_impl_->pending_tree(),
                                gfx::Size(100, 100));
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();

  host_impl_->pending_tree()->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
  const int scroll = 5;
  int accumulated_scroll = 0;
  for (int i = 0; i < host_impl_->pending_tree()->kFixedPointHitsThreshold + 1;
       ++i) {
    host_impl_->ActivateSyncTree();
    host_impl_->ScrollBegin(BeginState(gfx::Point(5, 5)).get(),
                            InputHandler::TOUCHSCREEN);
    host_impl_->ScrollBy(
        UpdateState(gfx::Point(), gfx::Vector2dF(0, scroll)).get());
    accumulated_scroll += scroll;
    host_impl_->ScrollEnd(EndState().get());
    host_impl_->active_tree()->UpdateDrawProperties();

    CreatePendingTree();
    host_impl_->pending_tree()->set_source_frame_number(i + 1);
    LayerImpl* content_layer = host_impl_->pending_tree()
                                   ->OuterViewportScrollLayer()
                                   ->test_properties()
                                   ->children[0];
    // The scroll done on the active tree is undone on the pending tree.
    gfx::Transform translate;
    translate.Translate(0, accumulated_scroll);
    content_layer->test_properties()->transform = translate;

    LayerTreeImpl* pending_tree = host_impl_->pending_tree();
    pending_tree->PushPageScaleFromMainThread(1.f, 1.f, 1.f);
    LayerImpl* last_scrolled_layer = pending_tree->LayerById(
        host_impl_->active_tree()->InnerViewportScrollLayer()->id());

    // When building property trees from impl side, the builder uses the scroll
    // offset of layer_impl to initialize the scroll offset in scroll tree:
    //  scroll_tree.synced_scroll_offset.PushMainToPending(
    //                                   layer->CurrentScrollOffset()).
    // However, layer_impl does not store scroll_offset, so it is using scroll
    // tree's scroll offset to initialize itself. Usually this approach works
    // because this is a simple assignment. However if scroll_offset's pending
    // delta is not zero, the delta would be counted twice.
    // This hacking here is to restore the damaged scroll offset.
    gfx::ScrollOffset pending_base =
        pending_tree->property_trees()
            ->scroll_tree.GetScrollOffsetBaseForTesting(
                last_scrolled_layer->element_id());
    pending_tree->BuildPropertyTreesForTesting();
    pending_tree->property_trees()
        ->scroll_tree.UpdateScrollOffsetBaseForTesting(
            last_scrolled_layer->element_id(), pending_base);
    pending_tree->LayerById(content_layer->id())->SetNeedsPushProperties();

    pending_tree->set_needs_update_draw_properties();
    pending_tree->UpdateDrawProperties();
    float jitter = LayerTreeHostCommon::CalculateLayerJitter(content_layer);
    // There should not be any jitter measured till we hit the fixed point hits
    // threshold.
    float expected_jitter =
        (i == pending_tree->kFixedPointHitsThreshold) ? 500 : 0;
    EXPECT_EQ(jitter, expected_jitter);
  }
}

// Checks that if we lose a GPU raster enabled LayerTreeFrameSink and replace
// it with a software LayerTreeFrameSink, LayerTreeHostImpl correctly
// re-computes GPU rasterization status.
TEST_F(LayerTreeHostImplTest, RecomputeGpuRasterOnLayerTreeFrameSinkChange) {
  host_impl_->ReleaseLayerTreeFrameSink();
  host_impl_ = nullptr;

  LayerTreeSettings settings = DefaultSettings();
  settings.gpu_rasterization_forced = true;

  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::IMPL), 0, nullptr);
  host_impl_->SetVisible(true);

  // InitializeFrameSink with a gpu-raster enabled output surface.
  auto gpu_raster_layer_tree_frame_sink = FakeLayerTreeFrameSink::Create3d();
  host_impl_->InitializeFrameSink(gpu_raster_layer_tree_frame_sink.get());
  EXPECT_TRUE(host_impl_->use_gpu_rasterization());

  // Re-initialize with a software output surface.
  layer_tree_frame_sink_ = FakeLayerTreeFrameSink::CreateSoftware();
  host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  EXPECT_FALSE(host_impl_->use_gpu_rasterization());
}

void LayerTreeHostImplTest::SetupMouseMoveAtTestScrollbarStates(
    bool main_thread_scrolling) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::TimeDelta::FromMilliseconds(500);
  settings.scrollbar_fade_duration = base::TimeDelta::FromMilliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size child_layer_size(250, 150);
  gfx::Size scrollbar_size_1(gfx::Size(15, viewport_size.height()));
  gfx::Size scrollbar_size_2(gfx::Size(15, child_layer_size.height()));

  const int scrollbar_1_id = 10;
  const int scrollbar_2_id = 11;
  const int child_scroll_id = 13;

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(1);
  host_impl_->active_tree()->SetDeviceViewportSize(viewport_size);
  CreateScrollAndContentsLayers(host_impl_->active_tree(), content_size);
  host_impl_->active_tree()->InnerViewportContainerLayer()->SetBounds(
      viewport_size);
  LayerImpl* root_scroll =
      host_impl_->active_tree()->OuterViewportScrollLayer();

  if (main_thread_scrolling) {
    root_scroll->set_main_thread_scrolling_reasons(
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  }

  // scrollbar_1 on root scroll.
  std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar_1 =
      SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                           scrollbar_1_id, VERTICAL, 15, 0,
                                           true, true);
  scrollbar_1->SetScrollElementId(root_scroll->element_id());
  scrollbar_1->SetDrawsContent(true);
  scrollbar_1->SetBounds(scrollbar_size_1);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(kTouchActionNone, gfx::Rect(scrollbar_size_1));
  scrollbar_1->SetTouchActionRegion(touch_action_region);
  scrollbar_1->SetCurrentPos(0);
  scrollbar_1->SetPosition(gfx::PointF(0, 0));
  host_impl_->active_tree()
      ->InnerViewportContainerLayer()
      ->test_properties()
      ->AddChild(std::move(scrollbar_1));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateScrollbarGeometries();
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();
  host_impl_->active_tree()->UpdateDrawProperties();

  ScrollbarAnimationController* scrollbar_1_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());
  EXPECT_TRUE(scrollbar_1_animation_controller);

  const float kMouseMoveDistanceToTriggerFadeIn =
      ScrollbarAnimationController::kMouseMoveDistanceToTriggerFadeIn;

  const float kMouseMoveDistanceToTriggerExpand =
      SingleScrollbarAnimationControllerThinning::
          kMouseMoveDistanceToTriggerExpand;

  // Mouse moves close to the scrollbar, goes over the scrollbar, and
  // moves back to where it was.
  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 0));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(15 + kMouseMoveDistanceToTriggerFadeIn, 0));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  // scrollbar_2 on child.
  std::unique_ptr<SolidColorScrollbarLayerImpl> scrollbar_2 =
      SolidColorScrollbarLayerImpl::Create(host_impl_->active_tree(),
                                           scrollbar_2_id, VERTICAL, 15, 0,
                                           true, true);
  std::unique_ptr<LayerImpl> child =
      LayerImpl::Create(host_impl_->active_tree(), child_scroll_id);
  child->SetPosition(gfx::PointF(50, 50));
  child->SetBounds(child_layer_size);
  child->SetDrawsContent(true);
  child->SetScrollable(gfx::Size(100, 100));
  child->SetElementId(LayerIdToElementIdForTesting(child->id()));
  ElementId child_element_id = child->element_id();

  if (main_thread_scrolling) {
    child->set_main_thread_scrolling_reasons(
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
  }

  scrollbar_2->SetScrollElementId(child_element_id);
  scrollbar_2->SetDrawsContent(true);
  scrollbar_2->SetBounds(scrollbar_size_2);
  scrollbar_2->SetCurrentPos(0);
  scrollbar_2->SetPosition(gfx::PointF(0, 0));

  child->test_properties()->AddChild(std::move(scrollbar_2));
  root_scroll->test_properties()->AddChild(std::move(child));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();
  host_impl_->active_tree()->UpdateScrollbarGeometries();
  host_impl_->active_tree()->DidBecomeActive();

  ScrollbarAnimationController* scrollbar_2_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(child_element_id);
  EXPECT_TRUE(scrollbar_2_animation_controller);

  // Mouse goes over scrollbar_2, moves close to scrollbar_2, moves close to
  // scrollbar_1, goes over scrollbar_1.
  host_impl_->MouseMoveAt(gfx::Point(60, 60));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_TRUE(scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  host_impl_->MouseMoveAt(
      gfx::Point(64 + kMouseMoveDistanceToTriggerExpand, 50));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_TRUE(scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(
      gfx::Point(14 + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  host_impl_->MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_TRUE(
      scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbar(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsNearScrollbarThumb(VERTICAL));
  EXPECT_FALSE(
      scrollbar_2_animation_controller->MouseIsOverScrollbarThumb(VERTICAL));

  // Capture scrollbar_1, then move mouse to scrollbar_2's layer, should post an
  // event to fade out scrollbar_1.
  scrollbar_1_animation_controller->DidScrollUpdate();
  animation_task_ = base::Closure();

  host_impl_->MouseDown();
  host_impl_->MouseMoveAt(gfx::Point(60, 50));
  host_impl_->MouseUp();

  EXPECT_FALSE(animation_task_.Equals(base::Closure()));

  // Near scrollbar_1, then mouse down and up, should not post an event to fade
  // out scrollbar_1.
  host_impl_->MouseMoveAt(gfx::Point(40, 150));
  animation_task_ = base::Closure();

  host_impl_->MouseDown();
  host_impl_->MouseUp();
  EXPECT_TRUE(animation_task_.Equals(base::Closure()));

  // Near scrollbar_1, then mouse down and unregister
  // scrollbar_2_animation_controller, then mouse up should not cause crash.
  host_impl_->MouseMoveAt(gfx::Point(40, 150));
  host_impl_->MouseDown();
  host_impl_->UnregisterScrollbarAnimationController(root_scroll->element_id());
  host_impl_->MouseUp();
}

TEST_F(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(true);
}

TEST_F(LayerTreeHostImplTest,
       LayerTreeHostImplTestScrollbarStatesInNotMainThreadScrolling) {
  SetupMouseMoveAtTestScrollbarStates(false);
}

TEST_F(LayerTreeHostImplTest, CheckerImagingTileInvalidation) {
  LayerTreeSettings settings = DefaultSettings();
  settings.commit_to_active_tree = false;
  settings.enable_checker_imaging = true;
  settings.min_image_bytes_to_checker = 512 * 1024;
  settings.default_tile_size = gfx::Size(256, 256);
  settings.max_untiled_layer_size = gfx::Size(256, 256);
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  gfx::Size layer_size = gfx::Size(750, 750);

  std::unique_ptr<FakeRecordingSource> recording_source =
      FakeRecordingSource::CreateFilledRecordingSource(layer_size);
  PaintImage checkerable_image =
      PaintImageBuilder::WithCopy(
          CreateDiscardablePaintImage(gfx::Size(500, 500)))
          .set_decoding_mode(PaintImage::DecodingMode::kAsync)
          .TakePaintImage();
  recording_source->add_draw_image(checkerable_image, gfx::Point(0, 0));

  SkColor non_solid_color = SkColorSetARGB(128, 45, 56, 67);
  PaintFlags non_solid_flags;
  non_solid_flags.setColor(non_solid_color);
  recording_source->add_draw_rect_with_flags(gfx::Rect(510, 0, 200, 600),
                                             non_solid_flags);
  recording_source->add_draw_rect_with_flags(gfx::Rect(0, 510, 200, 400),
                                             non_solid_flags);
  recording_source->Rerecord();
  scoped_refptr<RasterSource> raster_source =
      recording_source->CreateRasterSource();

  viz::BeginFrameArgs begin_frame_args =
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  host_impl_->WillBeginImplFrame(begin_frame_args);

  // Create the pending tree.
  host_impl_->BeginCommit();
  LayerTreeImpl* pending_tree = host_impl_->pending_tree();
  pending_tree->SetDeviceViewportSize(layer_size);
  pending_tree->SetRootLayerForTesting(
      FakePictureLayerImpl::CreateWithRasterSource(pending_tree, 1,
                                                   raster_source));
  auto* root = static_cast<FakePictureLayerImpl*>(*pending_tree->begin());
  root->SetBounds(layer_size);
  root->SetDrawsContent(true);
  pending_tree->BuildPropertyTreesForTesting();

  // Update the decoding state map for the tracker so it knows the correct
  // decoding preferences for the image.
  host_impl_->tile_manager()->checker_image_tracker().UpdateImageDecodingHints(
      raster_source->TakeDecodingModeMap());

  // CompleteCommit which should perform a PrepareTiles, adding tilings for the
  // root layer, each one having a raster task.
  host_impl_->CommitComplete();
  EXPECT_EQ(root->num_tilings(), 1U);
  const PictureLayerTiling* tiling = root->tilings()->tiling_at(0);
  EXPECT_EQ(tiling->AllTilesForTesting().size(), 9U);
  for (auto* tile : tiling->AllTilesForTesting())
    EXPECT_TRUE(tile->HasRasterTask());

  // Activate the pending tree and ensure that all tiles are rasterized.
  while (!did_notify_ready_to_activate_)
    base::RunLoop().RunUntilIdle();
  for (auto* tile : tiling->AllTilesForTesting())
    EXPECT_FALSE(tile->HasRasterTask());

  // PrepareTiles should have scheduled a decode with the ImageDecodeService,
  // ensure that it requests an impl-side invalidation.
  while (!did_request_impl_side_invalidation_)
    base::RunLoop().RunUntilIdle();

  // Invalidate content on impl-side and ensure that the correct tiles are
  // invalidated on the pending tree.
  host_impl_->InvalidateContentOnImplSide();
  pending_tree = host_impl_->pending_tree();
  root = static_cast<FakePictureLayerImpl*>(*pending_tree->begin());
  for (auto* tile : root->tilings()->tiling_at(0)->AllTilesForTesting()) {
    if (tile->tiling_i_index() < 2 && tile->tiling_j_index() < 2)
      EXPECT_TRUE(tile->HasRasterTask());
    else
      EXPECT_FALSE(tile->HasRasterTask());
  }
  const auto expected_invalidation =
      ImageRectsToRegion(raster_source->GetDisplayItemList()
                             ->discardable_image_map()
                             .GetRectsForImage(checkerable_image.stable_id()));
  EXPECT_EQ(expected_invalidation, *(root->GetPendingInvalidation()));
}

TEST_F(LayerTreeHostImplTest, RasterColorSpace) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // The default raster color space should be sRGB.
  EXPECT_EQ(host_impl_->GetRasterColorSpace().color_space,
            gfx::ColorSpace::CreateSRGB());
  // The raster color space should update with tree activation.
  host_impl_->active_tree()->SetRasterColorSpace(
      2, gfx::ColorSpace::CreateDisplayP3D65());
  EXPECT_EQ(host_impl_->GetRasterColorSpace().color_space,
            gfx::ColorSpace::CreateDisplayP3D65());
}

TEST_F(LayerTreeHostImplTest, RasterColorSpaceSoftware) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, FakeLayerTreeFrameSink::CreateSoftware());
  // Software composited resources should always use sRGB as their color space.
  EXPECT_EQ(host_impl_->GetRasterColorSpace().color_space,
            gfx::ColorSpace::CreateSRGB());
  host_impl_->active_tree()->SetRasterColorSpace(
      2, gfx::ColorSpace::CreateDisplayP3D65());
  EXPECT_EQ(host_impl_->GetRasterColorSpace().color_space,
            gfx::ColorSpace::CreateSRGB());
}

TEST_F(LayerTreeHostImplTest, UpdatedTilingsForNonDrawingLayers) {
  gfx::Size layer_bounds(500, 500);

  host_impl_->active_tree()->SetDeviceViewportSize(layer_bounds);
  CreatePendingTree();
  std::unique_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->pending_tree(), 1);
  scoped_root->SetBounds(layer_bounds);
  LayerImpl* root = scoped_root.get();
  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(scoped_root));

  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(layer_bounds));
  std::unique_ptr<FakePictureLayerImpl> scoped_animated_transform_layer =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                   2, raster_source);
  scoped_animated_transform_layer->SetBounds(layer_bounds);
  scoped_animated_transform_layer->SetDrawsContent(true);
  gfx::Transform singular;
  singular.Scale3d(6.f, 6.f, 0.f);
  scoped_animated_transform_layer->test_properties()->transform = singular;
  FakePictureLayerImpl* animated_transform_layer =
      scoped_animated_transform_layer.get();
  root->test_properties()->AddChild(std::move(scoped_animated_transform_layer));

  // A layer with a non-invertible transform is not drawn or rasterized. Since
  // this layer is not rasterized, we shouldn't be creating any tilings for it.
  host_impl_->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  EXPECT_FALSE(animated_transform_layer->HasValidTilePriorities());
  EXPECT_EQ(animated_transform_layer->tilings()->num_tilings(), 0u);
  host_impl_->pending_tree()->UpdateDrawProperties();
  EXPECT_FALSE(animated_transform_layer->raster_even_if_not_drawn());
  EXPECT_FALSE(animated_transform_layer->contributes_to_drawn_render_surface());
  EXPECT_EQ(animated_transform_layer->tilings()->num_tilings(), 0u);

  // Now add a transform animation to this layer. While we don't drawn layers
  // with non-invertible transforms, we still raster them if there is a
  // transform animation.
  host_impl_->pending_tree()->SetElementIdsForTesting();
  TransformOperations start_transform_operations;
  start_transform_operations.AppendMatrix(singular);
  TransformOperations end_transform_operations;
  AddAnimatedTransformToElementWithAnimation(
      animated_transform_layer->element_id(), timeline(), 10.0,
      start_transform_operations, end_transform_operations);

  // The layer is still not drawn, but it will be rasterized. Since the layer is
  // rasterized, we should be creating tilings for it in UpdateDrawProperties.
  // However, none of these tiles should be required for activation.
  host_impl_->pending_tree()->BuildLayerListAndPropertyTreesForTesting();
  host_impl_->pending_tree()->UpdateDrawProperties();
  EXPECT_TRUE(animated_transform_layer->raster_even_if_not_drawn());
  EXPECT_FALSE(animated_transform_layer->contributes_to_drawn_render_surface());
  EXPECT_EQ(animated_transform_layer->tilings()->num_tilings(), 1u);
  EXPECT_FALSE(animated_transform_layer->tilings()
                   ->tiling_at(0)
                   ->can_require_tiles_for_activation());
}

TEST_F(LayerTreeHostImplTest, RasterTilePrioritizationForNonDrawingLayers) {
  gfx::Size layer_bounds(500, 500);

  host_impl_->active_tree()->SetDeviceViewportSize(layer_bounds);
  CreatePendingTree();
  std::unique_ptr<LayerImpl> scoped_root =
      LayerImpl::Create(host_impl_->pending_tree(), 1);
  scoped_root->SetBounds(layer_bounds);
  LayerImpl* root = scoped_root.get();
  host_impl_->pending_tree()->SetRootLayerForTesting(std::move(scoped_root));

  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(layer_bounds));

  std::unique_ptr<FakePictureLayerImpl> scoped_hidden_layer =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                   2, raster_source);
  scoped_hidden_layer->SetBounds(layer_bounds);
  scoped_hidden_layer->SetDrawsContent(true);
  scoped_hidden_layer->set_contributes_to_drawn_render_surface(true);
  FakePictureLayerImpl* hidden_layer = scoped_hidden_layer.get();
  root->test_properties()->AddChild(std::move(scoped_hidden_layer));

  std::unique_ptr<FakePictureLayerImpl> scoped_drawing_layer =
      FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                   3, raster_source);
  scoped_drawing_layer->SetBounds(layer_bounds);
  scoped_drawing_layer->SetDrawsContent(true);
  scoped_drawing_layer->set_contributes_to_drawn_render_surface(true);
  FakePictureLayerImpl* drawing_layer = scoped_drawing_layer.get();
  root->test_properties()->AddChild(std::move(scoped_drawing_layer));

  gfx::Rect layer_rect(0, 0, 500, 500);
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();

  hidden_layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source);
  PictureLayerTiling* hidden_tiling = hidden_layer->tilings()->tiling_at(0);
  hidden_tiling->set_resolution(TileResolution::LOW_RESOLUTION);
  hidden_tiling->CreateAllTilesForTesting();
  hidden_tiling->SetTilePriorityRectsForTesting(
      layer_rect,   // Visible rect.
      layer_rect,   // Skewport rect.
      layer_rect,   // Soon rect.
      layer_rect);  // Eventually rect.

  drawing_layer->tilings()->AddTiling(gfx::AxisTransform2d(), raster_source);
  PictureLayerTiling* drawing_tiling = drawing_layer->tilings()->tiling_at(0);
  drawing_tiling->set_resolution(TileResolution::HIGH_RESOLUTION);
  drawing_tiling->CreateAllTilesForTesting();
  drawing_tiling->SetTilePriorityRectsForTesting(
      layer_rect,   // Visible rect.
      layer_rect,   // Skewport rect.
      layer_rect,   // Soon rect.
      layer_rect);  // Eventually rect.

  // Both layers are drawn. Since the hidden layer has a low resolution tiling,
  // in smoothness priority mode its tile is higher priority.
  std::unique_ptr<RasterTilePriorityQueue> queue =
      host_impl_->BuildRasterQueue(TreePriority::SMOOTHNESS_TAKES_PRIORITY,
                                   RasterTilePriorityQueue::Type::ALL);
  EXPECT_EQ(queue->Top().tile()->layer_id(), 2);

  // Hide the hidden layer and set it to so it still rasters. Now the drawing
  // layer should be prioritized over the hidden layer.
  hidden_layer->set_contributes_to_drawn_render_surface(false);
  hidden_layer->set_raster_even_if_not_drawn(true);
  queue = host_impl_->BuildRasterQueue(TreePriority::SMOOTHNESS_TAKES_PRIORITY,
                                       RasterTilePriorityQueue::Type::ALL);
  EXPECT_EQ(queue->Top().tile()->layer_id(), 3);
}

TEST_F(LayerTreeHostImplTest, DrawAfterDroppingTileResources) {
  LayerTreeSettings settings = DefaultSettings();
  settings.using_synchronous_renderer_compositor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  CreatePendingTree();

  gfx::Size bounds(100, 100);
  scoped_refptr<FakeRasterSource> raster_source(
      FakeRasterSource::CreateFilled(bounds));
  {
    std::unique_ptr<FakePictureLayerImpl> scoped_layer =
        FakePictureLayerImpl::CreateWithRasterSource(host_impl_->pending_tree(),
                                                     1, raster_source);
    scoped_layer->SetBounds(bounds);
    scoped_layer->SetDrawsContent(true);
    host_impl_->pending_tree()->SetRootLayerForTesting(std::move(scoped_layer));
  }
  host_impl_->pending_tree()->BuildPropertyTreesForTesting();
  host_impl_->ActivateSyncTree();

  FakePictureLayerImpl* layer = static_cast<FakePictureLayerImpl*>(
      host_impl_->active_tree()->FindActiveTreeLayerById(1));

  DrawFrame();
  EXPECT_FALSE(host_impl_->active_tree()->needs_update_draw_properties());
  EXPECT_LT(0.f, layer->raster_page_scale());
  EXPECT_GT(layer->tilings()->num_tilings(), 0u);

  const ManagedMemoryPolicy policy = host_impl_->ActualManagedMemoryPolicy();
  const ManagedMemoryPolicy zero_policy(0u);
  host_impl_->SetMemoryPolicy(zero_policy);
  EXPECT_EQ(0.f, layer->raster_page_scale());
  EXPECT_EQ(layer->tilings()->num_tilings(), 0u);

  host_impl_->SetMemoryPolicy(policy);
  DrawFrame();
  EXPECT_LT(0.f, layer->raster_page_scale());
  EXPECT_GT(layer->tilings()->num_tilings(), 0u);
}

TEST_F(LayerTreeHostImplTest, NeedUpdateGpuRasterization) {
  EXPECT_FALSE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());

  host_impl_->SetHasGpuRasterizationTrigger(true);
  EXPECT_TRUE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());

  host_impl_->SetContentHasSlowPaths(true);
  EXPECT_TRUE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());

  host_impl_->SetContentHasNonAAPaint(true);
  EXPECT_TRUE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());
  host_impl_->CommitComplete();
  EXPECT_FALSE(host_impl_->NeedUpdateGpuRasterizationStatusForTesting());
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest1) {
  WhiteListedTouchActionTestHelper(1.0f, 1.0f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest2) {
  WhiteListedTouchActionTestHelper(1.0f, 0.789f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest3) {
  WhiteListedTouchActionTestHelper(2.345f, 1.0f);
}

TEST_F(LayerTreeHostImplTest, WhiteListedTouchActionTest4) {
  WhiteListedTouchActionTestHelper(2.654f, 0.678f);
}

// Test implementation of RenderFrameMetadataObserver which can optionally
// request the frame-token to be sent to the embedder during frame submission.
class TestRenderFrameMetadataObserver : public RenderFrameMetadataObserver {
 public:
  explicit TestRenderFrameMetadataObserver(bool increment_counter)
      : increment_counter_(increment_counter) {}
  ~TestRenderFrameMetadataObserver() override {}

  void BindToCurrentThread() override {}
  void OnRenderFrameSubmission(
      const RenderFrameMetadata& render_frame_metadata,
      viz::CompositorFrameMetadata* compositor_frame_metadata) override {
    if (increment_counter_)
      compositor_frame_metadata->send_frame_token_to_embedder = true;
    last_metadata_ = render_frame_metadata;
  }

  const base::Optional<RenderFrameMetadata>& last_metadata() const {
    return last_metadata_;
  }

 private:
  bool increment_counter_;
  base::Optional<RenderFrameMetadata> last_metadata_;

  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameMetadataObserver);
};

TEST_F(LayerTreeHostImplTest, SelectionBoundsPassedToRenderFrameMetadata) {
  const int root_layer_id = 1;
  std::unique_ptr<SolidColorLayerImpl> root =
      SolidColorLayerImpl::Create(host_impl_->active_tree(), root_layer_id);
  root->SetPosition(gfx::PointF());
  root->SetBounds(gfx::Size(10, 10));
  root->SetDrawsContent(true);
  root->test_properties()->force_render_surface = true;

  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  auto observer = std::make_unique<TestRenderFrameMetadataObserver>(false);
  auto* observer_ptr = observer.get();
  host_impl_->SetRenderFrameObserver(std::move(observer));
  EXPECT_FALSE(observer_ptr->last_metadata());

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();
  TestFrameData frame;
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame));
  host_impl_->DidDrawAllLayers(frame);

  // Ensure the selection bounds propagated to the render frame metadata
  // represent an empty selection.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_1 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.start.type());
  EXPECT_EQ(gfx::SelectionBound::EMPTY, selection_1.end.type());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(), selection_1.start.edge_top());
  EXPECT_FALSE(selection_1.start.visible());
  EXPECT_FALSE(selection_1.end.visible());

  // Plumb the layer-local selection bounds.
  gfx::Point selection_top(5, 0);
  gfx::Point selection_bottom(5, 5);
  LayerSelection selection;
  selection.start.type = gfx::SelectionBound::CENTER;
  selection.start.layer_id = root_layer_id;
  selection.start.edge_bottom = selection_bottom;
  selection.start.edge_top = selection_top;
  selection.end = selection.start;
  host_impl_->active_tree()->RegisterSelection(selection);

  // Trigger a draw-swap sequence.
  host_impl_->SetNeedsRedraw();
  EXPECT_EQ(DRAW_SUCCESS, host_impl_->PrepareToDraw(&frame));
  EXPECT_TRUE(host_impl_->DrawLayers(&frame));
  host_impl_->DidDrawAllLayers(frame);

  // Ensure the selection bounds have propagated to the render frame metadata.
  ASSERT_TRUE(observer_ptr->last_metadata());
  const viz::Selection<gfx::SelectionBound>& selection_2 =
      observer_ptr->last_metadata()->selection;
  EXPECT_EQ(selection.start.type, selection_2.start.type());
  EXPECT_EQ(selection.end.type, selection_2.end.type());
  EXPECT_EQ(gfx::PointF(selection_bottom), selection_2.start.edge_bottom());
  EXPECT_EQ(gfx::PointF(selection_top), selection_2.start.edge_top());
  EXPECT_TRUE(selection_2.start.visible());
  EXPECT_TRUE(selection_2.end.visible());
}

// Tests ScrollBy() to see if the method sets the scroll tree's currently
// scrolling node and the ScrollState properly.
TEST_F(LayerTreeHostImplTest, ScrollByScrollingNode) {
  SetupScrollAndContentsLayers(gfx::Size(100, 100));
  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  // Create a ScrollState object with no scrolling element.
  ScrollStateData scroll_state_data;
  scroll_state_data.set_current_native_scrolling_element(ElementId());
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));

  ScrollTree& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree;

  EXPECT_EQ(InputHandler::SCROLL_ON_IMPL_THREAD,
            host_impl_
                ->ScrollBegin(BeginState(gfx::Point()).get(),
                              InputHandler::TOUCHSCREEN)
                .thread);

  ScrollNode* scroll_node = scroll_tree.CurrentlyScrollingNode();
  EXPECT_TRUE(scroll_node);

  host_impl_->ScrollBy(scroll_state.get());

  // Check to see the scroll tree's currently scrolling node is
  // still the same. |scroll_state|'s scrolling node should match
  // it.
  EXPECT_EQ(scroll_node, scroll_tree.CurrentlyScrollingNode());
  EXPECT_EQ(scroll_state->data()->current_native_scrolling_node(),
            scroll_tree.CurrentlyScrollingNode());
  EXPECT_EQ(scroll_state->data()->current_native_scrolling_element(),
            scroll_tree.CurrentlyScrollingNode()->element_id);

  // Set the scroll tree's currently scrolling node to null. Calling
  // ScrollBy() should set the node to the one inside |scroll_state|.
  host_impl_->active_tree()->SetCurrentlyScrollingNode(nullptr);
  EXPECT_FALSE(scroll_tree.CurrentlyScrollingNode());

  host_impl_->ScrollBy(scroll_state.get());

  EXPECT_EQ(scroll_node, scroll_tree.CurrentlyScrollingNode());
  EXPECT_EQ(scroll_state->data()->current_native_scrolling_node(),
            scroll_tree.CurrentlyScrollingNode());
  EXPECT_EQ(scroll_state->data()->current_native_scrolling_element(),
            scroll_tree.CurrentlyScrollingNode()->element_id);
}

class HitTestRegionListGeneratingLayerTreeHostImplTest
    : public LayerTreeHostImplTest {
 public:
  bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) override {
    // Enable hit test data generation with the CompositorFrame.
    LayerTreeSettings new_settings = settings;
    new_settings.build_hit_test_data = true;
    return CreateHostImplWithTaskRunnerProvider(
        new_settings, std::move(layer_tree_frame_sink), &task_runner_provider_);
  }
};

// When disabled, no HitTestRegionList should be generated.
// Test to ensure that hit test data is created correctly from the active layer
// tree.
TEST_F(LayerTreeHostImplTest, DisabledBuildHitTestData) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();
  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(1024, 768));

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  std::unique_ptr<SurfaceLayerImpl> surface_child =
      SurfaceLayerImpl::Create(host_impl_->active_tree(), 3);

  surface_child->SetPosition(gfx::PointF(50, 50));
  surface_child->SetBounds(gfx::Size(100, 100));
  surface_child->SetDrawsContent(true);
  surface_child->SetSurfaceHitTestable(true);

  root->test_properties()->AddChild(std::move(surface_child));
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  base::Optional<viz::HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();
  EXPECT_FALSE(hit_test_region_list);
}

// Test to ensure that hit test data is created correctly from the active layer
// tree.
TEST_F(HitTestRegionListGeneratingLayerTreeHostImplTest, BuildHitTestData) {
  // Setup surface layers in LayerTreeHostImpl.
  host_impl_->CreatePendingTree();
  host_impl_->ActivateSyncTree();

  // The structure of the layer tree:
  // +-Root (1024x768)
  // +---intermediate_layer (200, 300), 200x200
  // +-----surface_child1 (50, 50), 100x100, Rotate(45)
  // +---surface_child2 (450, 300), 100x100
  // +---overlapping_layer (500, 350), 200x200
  std::unique_ptr<LayerImpl> intermediate_layer =
      LayerImpl::Create(host_impl_->active_tree(), 2);
  std::unique_ptr<SurfaceLayerImpl> surface_child1 =
      SurfaceLayerImpl::Create(host_impl_->active_tree(), 3);
  std::unique_ptr<SurfaceLayerImpl> surface_child2 =
      SurfaceLayerImpl::Create(host_impl_->active_tree(), 4);
  std::unique_ptr<LayerImpl> overlapping_layer =
      LayerImpl::Create(host_impl_->active_tree(), 5);

  host_impl_->active_tree()->SetDeviceViewportSize(gfx::Size(1024, 768));

  intermediate_layer->SetPosition(gfx::PointF(200, 300));
  intermediate_layer->SetBounds(gfx::Size(200, 200));

  surface_child1->SetPosition(gfx::PointF(50, 50));
  surface_child1->SetBounds(gfx::Size(100, 100));
  gfx::Transform rotate;
  rotate.Rotate(45);
  surface_child1->test_properties()->transform = rotate;
  surface_child1->SetDrawsContent(true);
  surface_child1->SetSurfaceHitTestable(true);

  surface_child2->SetPosition(gfx::PointF(450, 300));
  surface_child2->SetBounds(gfx::Size(100, 100));
  surface_child2->SetDrawsContent(true);
  surface_child2->SetSurfaceHitTestable(true);

  overlapping_layer->SetPosition(gfx::PointF(500, 350));
  overlapping_layer->SetBounds(gfx::Size(200, 200));
  overlapping_layer->SetDrawsContent(true);

  viz::LocalSurfaceId child_local_surface_id(2,
                                             base::UnguessableToken::Create());
  viz::FrameSinkId frame_sink_id(2, 0);
  viz::SurfaceId child_surface_id(frame_sink_id, child_local_surface_id);
  surface_child1->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);
  surface_child2->SetRange(viz::SurfaceRange(base::nullopt, child_surface_id),
                           base::nullopt);

  std::unique_ptr<LayerImpl> root =
      LayerImpl::Create(host_impl_->active_tree(), 1);
  host_impl_->active_tree()->SetRootLayerForTesting(std::move(root));
  intermediate_layer->test_properties()->AddChild(std::move(surface_child1));
  host_impl_->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(intermediate_layer));
  host_impl_->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(surface_child2));
  host_impl_->active_tree()
      ->root_layer_for_testing()
      ->test_properties()
      ->AddChild(std::move(overlapping_layer));

  host_impl_->active_tree()->BuildPropertyTreesForTesting();

  constexpr gfx::Rect kFrameRect(0, 0, 1024, 768);

  base::Optional<viz::HitTestRegionList> hit_test_region_list =
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
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[1].rect.ToString());

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
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100).ToString(),
            hit_test_region_list->regions[0].rect.ToString());
}

TEST_F(LayerTreeHostImplTest, ImplThreadPhaseUponImplSideInvalidation) {
  LayerTreeSettings settings = DefaultSettings();
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // In general invalidation should never be ran outside the impl frame.
  host_impl_->WillBeginImplFrame(
      viz::CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1));
  // Expect no crash because the operation is within an impl frame.
  host_impl_->InvalidateContentOnImplSide();

  // Once the impl frame is finished the impl thread phase is set to IDLE.
  host_impl_->DidFinishImplFrame();

  settings.using_synchronous_renderer_compositor = true;
  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  // Expect no crash when using synchronous renderer compositor regardless the
  // impl thread phase.
  host_impl_->InvalidateContentOnImplSide();

  // Test passes when there is no crash.
}

TEST_F(LayerTreeHostImplTest, SkipOnDrawDoesNotUpdateDrawParams) {
  EXPECT_TRUE(CreateHostImpl(DefaultSettings(),
                             FakeLayerTreeFrameSink::CreateSoftware()));
  LayerImpl* layer = SetupScrollAndContentsLayers(gfx::Size(100, 100));
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

}  // namespace
}  // namespace cc
