// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host_impl_test_base.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_id_provider.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/trees/compositor_commit_data.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "media/base/media.h"

namespace cc {

using ScrollThread = InputHandler::ScrollThread;

TestFrameData::TestFrameData() {
  // Set ack to something valid, so DCHECKs don't complain.
  begin_frame_ack = viz::BeginFrameAck::CreateManualAckWithDamage();
}

TestFrameData::~TestFrameData() = default;

std::unique_ptr<DidDrawCheckLayer> DidDrawCheckLayer::Create(
    LayerTreeImpl* tree_impl,
    int id) {
  return base::WrapUnique(new DidDrawCheckLayer(tree_impl, id));
}

bool DidDrawCheckLayer::WillDraw(DrawMode draw_mode,
                                 viz::ClientResourceProvider* provider) {
  if (!LayerImpl::WillDraw(draw_mode, provider)) {
    return false;
  }
  if (will_draw_returns_false_) {
    return false;
  }
  will_draw_returned_true_ = true;
  return true;
}

void DidDrawCheckLayer::AppendQuads(const AppendQuadsContext& context,
                                    viz::CompositorRenderPass* render_pass,
                                    AppendQuadsData* append_quads_data) {
  append_quads_called_ = true;
  LayerImpl::AppendQuads(context, render_pass, append_quads_data);
}

void DidDrawCheckLayer::DidDraw(viz::ClientResourceProvider* provider) {
  did_draw_called_ = true;
  LayerImpl::DidDraw(provider);
}

void DidDrawCheckLayer::ClearDidDrawCheck() {
  will_draw_returned_true_ = false;
  append_quads_called_ = false;
  did_draw_called_ = false;
}

DidDrawCheckLayer::DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      will_draw_returns_false_(false),
      will_draw_returned_true_(false),
      append_quads_called_(false),
      did_draw_called_(false) {
  SetBounds(gfx::Size(10, 10));
  SetDrawsContent(true);
  draw_properties().visible_layer_rect = gfx::Rect(0, 0, 10, 10);
}

LayerTreeHostImplTestBase::LayerTreeHostImplTestBase()
    : task_runner_provider_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      always_main_thread_blocked_(&task_runner_provider_),
      on_can_draw_state_changed_called_(false),
      did_notify_ready_to_activate_(false),
      did_request_commit_(false),
      did_request_redraw_(false),
      did_request_next_frame_(false),
      did_request_prepare_tiles_(false),
      did_prepare_tiles_(false),
      did_complete_page_scale_animation_(false),
      reduce_memory_result_(true),
      did_request_impl_side_invalidation_(false) {
  media::InitializeMediaLibrary();
}

LayerTreeHostImplTestBase::~LayerTreeHostImplTestBase() = default;

LayerTreeSettings LayerTreeHostImplTestBase::DefaultSettings() {
  LayerTreeSettings settings = CommitToPendingTreeLayerListSettings();
  settings.minimum_occlusion_tracking_size = gfx::Size();
  settings.enable_smooth_scroll = true;
  settings.single_thread_proxy_scheduler = false;
  return settings;
}

LayerTreeSettings LayerTreeHostImplTestBase::LegacySWSettings() {
  LayerTreeSettings settings = DefaultSettings();
  settings.gpu_rasterization_disabled = true;
  return settings;
}

void LayerTreeHostImplTestBase::SetUp() {
  CreateHostImpl(DefaultSettings(), CreateLayerTreeFrameSink());

  // TODO(bokan): Mac wheel scrolls don't cause smooth scrolling in the real
  // world. In tests, we force it on for consistency. Can be removed when
  // https://crbug.com/574283 is fixed.
  host_impl_->set_force_smooth_wheel_scrolling_for_testing(true);
}

void LayerTreeHostImplTestBase::TearDown() {
  if (host_impl_) {
    host_impl_->ReleaseLayerTreeFrameSink();
  }
}

void LayerTreeHostImplTestBase::EnsureSyncTree() {
  if (!host_impl_->CommitsToActiveTree() && !host_impl_->pending_tree()) {
    CreatePendingTree();
  }
}

void LayerTreeHostImplTestBase::CreatePendingTree() {
  host_impl_->CreatePendingTree();
  LayerTreeImpl* pending_tree = host_impl_->pending_tree();
  pending_tree->SetDeviceViewportRect(
      host_impl_->active_tree()->GetDeviceViewport());
  pending_tree->SetDeviceScaleFactor(
      host_impl_->active_tree()->device_scale_factor());
  // Normally a pending tree will not be fully painted until the commit has
  // happened and any PaintWorklets have been resolved. However many of the
  // unittests never actually commit the pending trees that they create, so to
  // enable them to still treat the tree as painted we forcibly override the
  // state here. Note that this marks a distinct departure from reality in the
  // name of easier testing.
  host_impl_->set_pending_tree_fully_painted_for_testing(true);
}

void LayerTreeHostImplTestBase::DidLoseLayerTreeFrameSinkOnImplThread() {}
void LayerTreeHostImplTestBase::SetBeginFrameSource(
    viz::BeginFrameSource* source) {}
void LayerTreeHostImplTestBase::DidReceiveCompositorFrameAckOnImplThread() {}
void LayerTreeHostImplTestBase::OnCanDrawStateChanged(bool can_draw) {
  on_can_draw_state_changed_called_ = true;
}
void LayerTreeHostImplTestBase::NotifyReadyToActivate() {
  did_notify_ready_to_activate_ = true;
  host_impl_->ActivateSyncTree();
}
bool LayerTreeHostImplTestBase::IsReadyToActivate() {
  // in NotifyReadyToActivate(), call ActivateSyncTree() directly
  // so this is always false
  return false;
}
void LayerTreeHostImplTestBase::NotifyReadyToDraw() {}
void LayerTreeHostImplTestBase::SetNeedsRedrawOnImplThread() {
  did_request_redraw_ = true;
}
void LayerTreeHostImplTestBase::SetNeedsOneBeginImplFrameOnImplThread() {
  did_request_next_frame_ = true;
}
void LayerTreeHostImplTestBase::SetNeedsPrepareTilesOnImplThread() {
  did_request_prepare_tiles_ = true;
}
void LayerTreeHostImplTestBase::SetNeedsCommitOnImplThread(bool urgent) {
  did_request_commit_ = true;
}
void LayerTreeHostImplTestBase::SetVideoNeedsBeginFrames(
    bool needs_begin_frames) {}
void LayerTreeHostImplTestBase::DidChangeBeginFrameSourcePaused(bool paused) {}
void LayerTreeHostImplTestBase::SetDeferBeginMainFrameFromImpl(
    bool defer_begin_main_frame) {}
bool LayerTreeHostImplTestBase::IsInsideDraw() {
  return false;
}
void LayerTreeHostImplTestBase::RenewTreePriority() {}
void LayerTreeHostImplTestBase::PostDelayedAnimationTaskOnImplThread(
    base::OnceClosure task,
    base::TimeDelta delay) {
  animation_task_ = std::move(task);
  requested_animation_delay_ = delay;
}
void LayerTreeHostImplTestBase::DidActivateSyncTree() {
  // Make sure the active tree always has a valid LocalSurfaceId.
  host_impl_->active_tree()->SetLocalSurfaceIdFromParent(
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2u, 3u)));
}
void LayerTreeHostImplTestBase::DidPrepareTiles() {
  did_prepare_tiles_ = true;
}
void LayerTreeHostImplTestBase::DidCompletePageScaleAnimationOnImplThread() {
  did_complete_page_scale_animation_ = true;
}
void LayerTreeHostImplTestBase::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {
  std::unique_ptr<TestFrameData> frame = std::make_unique<TestFrameData>();
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(frame.get()));
  last_on_draw_render_passes_.clear();
  viz::CompositorRenderPass::CopyAllForTest(frame->render_passes,
                                            &last_on_draw_render_passes_);
  host_impl_->DrawLayers(frame.get());
  host_impl_->DidDrawAllLayers(*frame);
  last_on_draw_frame_ = std::move(frame);
}
void LayerTreeHostImplTestBase::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {
  did_request_impl_side_invalidation_ = true;
}
void LayerTreeHostImplTestBase::NotifyImageDecodeRequestFinished(
    int request_id,
    bool speculative,
    bool decode_succeeded) {}
void LayerTreeHostImplTestBase::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    PresentationTimeCallbackBuffer::PendingCallbacks activated,
    const viz::FrameTimingDetails& details) {
  // We don't call main thread callbacks in this test.
  activated.main_callbacks.clear();
  activated.main_successful_callbacks.clear();

  host_impl_->NotifyDidPresentCompositorFrameOnImplThread(
      frame_token, std::move(activated.compositor_successful_callbacks),
      details);
}
void LayerTreeHostImplTestBase::NotifyAnimationWorkletStateChange(
    AnimationWorkletMutationState state,
    ElementListType tree_type) {}
void LayerTreeHostImplTestBase::NotifyPaintWorkletStateChange(
    Scheduler::PaintWorkletState state) {}
void LayerTreeHostImplTestBase::NotifyCompositorMetricsTrackerResults(
    CustomTrackerResults results) {}

void LayerTreeHostImplTestBase::DidObserveFirstScrollDelay(
    int source_frame_number,
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {
  first_scroll_observed++;
}
bool LayerTreeHostImplTestBase::IsInSynchronousComposite() const {
  return false;
}
void LayerTreeHostImplTestBase::FrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {}
void LayerTreeHostImplTestBase::ClearHistory() {}
void LayerTreeHostImplTestBase::SetHasActiveThreadedScroll(bool is_scrolling) {}
void LayerTreeHostImplTestBase::SetWaitingForScrollEvent(
    bool waiting_for_scroll_event) {}
size_t LayerTreeHostImplTestBase::CommitDurationSampleCountForTesting() const {
  return 0;
}
void LayerTreeHostImplTestBase::NotifyTransitionRequestFinished(
    uint32_t sequence_id,
    const viz::ViewTransitionElementResourceRects&) {}

AnimationHost* LayerTreeHostImplTestBase::GetImplAnimationHost() const {
  return static_cast<AnimationHost*>(host_impl_->mutator_host());
}

bool LayerTreeHostImplTestBase::CreateHostImpl(
    const LayerTreeSettings& settings,
    std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink) {
  if (host_impl_) {
    host_impl_->ReleaseLayerTreeFrameSink();
  }
  host_impl_.reset();
  InitializeImageWorker(settings);
  host_impl_ = LayerTreeHostImpl::Create(
      settings, this, &task_runner_provider_, &stats_instrumentation_,
      &task_graph_runner_,
      AnimationHost::CreateForTesting(ThreadInstance::kImpl), nullptr, 0,
      image_worker_ ? image_worker_->task_runner() : nullptr, nullptr);
  InputHandler::Create(static_cast<CompositorDelegateForInput&>(*host_impl_));
  layer_tree_frame_sink_ = std::move(layer_tree_frame_sink);
  host_impl_->SetVisible(true);
  bool init = host_impl_->InitializeFrameSink(layer_tree_frame_sink_.get());
  host_impl_->active_tree()->SetDeviceViewportRect(gfx::Rect(10, 10));
  host_impl_->active_tree()->PushPageScaleFromMainThread(1, 1, 1);
  host_impl_->active_tree()->SetLocalSurfaceIdFromParent(
      viz::LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2u, 3u)));
  // Set the viz::BeginFrameArgs so that methods which use it are able to.
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 1, base::TimeTicks() + base::Milliseconds(1));
  host_impl_->WillBeginImplFrame(args);
  host_impl_->DidFinishImplFrame(args);

  timeline_ = AnimationTimeline::Create(AnimationIdProvider::NextTimelineId());
  GetImplAnimationHost()->AddAnimationTimeline(timeline_);

  return init;
}

LayerImpl* LayerTreeHostImplTestBase::SetupDefaultRootLayer(
    const gfx::Size& viewport_size) {
  return SetupRootLayer<LayerImpl>(host_impl_->active_tree(), viewport_size);
}

LayerImpl* LayerTreeHostImplTestBase::root_layer() {
  return host_impl_->active_tree()->root_layer();
}

gfx::Size LayerTreeHostImplTestBase::DipSizeToPixelSize(const gfx::Size& size) {
  return gfx::ScaleToRoundedSize(
      size, host_impl_->active_tree()->device_scale_factor());
}

void LayerTreeHostImplTestBase::PushScrollOffsetsToPendingTree(
    const base::flat_map<ElementId, gfx::PointF>& offsets) {
  PropertyTrees property_trees(*host_impl_);
  auto& scroll_tree =
      host_impl_->active_tree()->property_trees()->scroll_tree_mutable();
  if (auto* layer = InnerViewportScrollLayer()) {
    property_trees.scroll_tree_mutable().SetBaseScrollOffset(
        layer->element_id(),
        scroll_tree.current_scroll_offset(layer->element_id()));
  }
  if (auto* layer = OuterViewportScrollLayer()) {
    property_trees.scroll_tree_mutable().SetBaseScrollOffset(
        layer->element_id(),
        scroll_tree.current_scroll_offset(layer->element_id()));
  }
  for (auto& entry : offsets) {
    property_trees.scroll_tree_mutable().SetBaseScrollOffset(entry.first,
                                                             entry.second);
  }
  host_impl_->sync_tree()
      ->property_trees()
      ->scroll_tree_mutable()
      .PushScrollUpdatesFromMainThread(
          property_trees, host_impl_->sync_tree(),
          host_impl_->settings().commit_fractional_scroll_deltas);
}

void LayerTreeHostImplTestBase::ClearNonScrollSyncTreeDeltasForTesting() {
  auto* tree = host_impl_->sync_tree();
  tree->page_scale_factor()->AbortCommit(
      /* next_bmf */ false, /* main_frame_applied_deltas */ false);
  tree->top_controls_shown_ratio()->AbortCommit(
      /* next_bmf */ false, /* main_frame_applied_deltas */ false);
  tree->bottom_controls_shown_ratio()->AbortCommit(
      /* next_bmf */ false, /* main_frame_applied_deltas */ false);
}

void LayerTreeHostImplTestBase::ExpectClearedScrollDeltasRecursive(
    LayerImpl* root) {
  for (auto* layer : *root->layer_tree_impl()) {
    ASSERT_EQ(ScrollDelta(layer), gfx::Vector2d());
  }
}

::testing::AssertionResult LayerTreeHostImplTestBase::ScrollInfoContains(
    const CompositorCommitData& commit_data,
    ElementId id,
    const gfx::Vector2dF& scroll_delta) {
  int times_encountered = 0;

  for (size_t i = 0; i < commit_data.scrolls.size(); ++i) {
    if (commit_data.scrolls[i].element_id != id) {
      continue;
    }

    if (scroll_delta != commit_data.scrolls[i].scroll_delta) {
      return ::testing::AssertionFailure()
             << "Expected " << scroll_delta.ToString() << ", not "
             << commit_data.scrolls[i].scroll_delta.ToString();
    }
    times_encountered++;
  }

  if (id == commit_data.inner_viewport_scroll.element_id) {
    if (scroll_delta != commit_data.inner_viewport_scroll.scroll_delta) {
      return ::testing::AssertionFailure()
             << "Expected " << scroll_delta.ToString() << ", not "
             << commit_data.inner_viewport_scroll.scroll_delta.ToString();
    }
    times_encountered++;
  }

  if (times_encountered != 1) {
    return ::testing::AssertionFailure() << "No scroll found with id " << id;
  }
  return ::testing::AssertionSuccess();
}

void LayerTreeHostImplTestBase::ExpectNone(
    const CompositorCommitData& commit_data,
    ElementId id) {
  int times_encountered = 0;

  for (size_t i = 0; i < commit_data.scrolls.size(); ++i) {
    if (commit_data.scrolls[i].element_id != id) {
      continue;
    }
    times_encountered++;
  }

  ASSERT_EQ(0, times_encountered);
}

LayerImpl* LayerTreeHostImplTestBase::AddLayerInActiveTree() {
  return AddLayer<LayerImpl>(host_impl_->active_tree());
}

void LayerTreeHostImplTestBase::SetupViewportLayers(
    LayerTreeImpl* layer_tree_impl,
    const gfx::Size& inner_viewport_size,
    const gfx::Size& outer_viewport_size,
    const gfx::Size& content_size) {
  DCHECK(!layer_tree_impl->root_layer());
  auto* root = SetupRootLayer<LayerImpl>(layer_tree_impl, inner_viewport_size);
  SetupViewport(root, outer_viewport_size, content_size);
  host_impl_->SetVisualDeviceViewportSize(inner_viewport_size);

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();
}

void LayerTreeHostImplTestBase::SetupViewportLayersInnerScrolls(
    const gfx::Size& inner_viewport_size,
    const gfx::Size& content_size) {
  const auto& outer_viewport_size = content_size;
  SetupViewportLayers(host_impl_->active_tree(), inner_viewport_size,
                      outer_viewport_size, content_size);
}

void LayerTreeHostImplTestBase::SetupViewportLayersOuterScrolls(
    const gfx::Size& viewport_size,
    const gfx::Size& content_size) {
  SetupViewportLayers(host_impl_->active_tree(), viewport_size, viewport_size,
                      content_size);
}

LayerImpl* LayerTreeHostImplTestBase::AddContentLayer() {
  LayerImpl* scroll_layer = OuterViewportScrollLayer();
  DCHECK(scroll_layer);
  LayerImpl* layer = AddLayerInActiveTree();
  layer->SetBounds(scroll_layer->bounds());
  layer->SetDrawsContent(true);
  CopyProperties(scroll_layer, layer);
  return layer;
}

void LayerTreeHostImplTestBase::SetupViewportLayersNoScrolls(
    const gfx::Size& bounds) {
  SetupViewportLayers(host_impl_->active_tree(), bounds, bounds, bounds);
}

void LayerTreeHostImplTestBase::CreateAndTestNonScrollableLayers(
    bool transparent_layer) {
  LayerTreeImpl* layer_tree_impl = host_impl_->active_tree();
  gfx::Size content_size = gfx::Size(360, 600);
  gfx::Size scroll_content_size = gfx::Size(345, 3800);
  gfx::Size scrollbar_size = gfx::Size(15, 600);

  SetupViewportLayersNoScrolls(content_size);
  LayerImpl* outer_scroll = OuterViewportScrollLayer();
  LayerImpl* scroll =
      AddScrollableLayer(outer_scroll, content_size, scroll_content_size);

  auto* squash2 = AddLayer<LayerImpl>(layer_tree_impl);
  squash2->SetBounds(gfx::Size(140, 300));
  squash2->SetDrawsContent(true);
  squash2->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(scroll, squash2);
  squash2->SetOffsetToTransformParent(gfx::Vector2dF(220, 300));

  auto* scrollbar = AddLayer<PaintedScrollbarLayerImpl>(
      layer_tree_impl, ScrollbarOrientation::kVertical, false, true);
  SetupScrollbarLayer(scroll, scrollbar);
  scrollbar->SetBounds(scrollbar_size);
  scrollbar->SetOffsetToTransformParent(gfx::Vector2dF(345, 0));

  auto* squash1 = AddLayer<LayerImpl>(layer_tree_impl);
  squash1->SetBounds(gfx::Size(140, 300));
  CopyProperties(outer_scroll, squash1);
  squash1->SetOffsetToTransformParent(gfx::Vector2dF(220, 0));
  if (transparent_layer) {
    CreateEffectNode(squash1).opacity = 0.0f;
    // The transparent layer should still participate in hit testing even
    // through it does not draw content.
    squash1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  } else {
    squash1->SetDrawsContent(true);
    squash1->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  }

  UpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->DidBecomeActive();

  // The point hits squash1 layer and also scroll layer, because scroll layer
  // is not an ancestor of squash1 layer, we cannot scroll on impl thread
  // (without at least a hit test on the main thread).
  InputHandler::ScrollStatus status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(230, 150), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);

  // The point hits squash1 layer and also scrollbar layer.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(350, 150), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
  ASSERT_EQ(MainThreadScrollingReason::kFailedHitTest,
            status.main_thread_hit_test_reasons);

  // The point hits squash2 layer and also scroll layer, because scroll layer
  // is an ancestor of squash2 layer, we should scroll on impl.
  status = GetInputHandler().ScrollBegin(
      BeginState(gfx::Point(230, 450), gfx::Vector2dF(0, 10),
                 ui::ScrollInputType::kWheel)
          .get(),
      ui::ScrollInputType::kWheel);
  ASSERT_EQ(ScrollThread::kScrollOnImplThread, status.thread);
}

LayerImpl* LayerTreeHostImplTestBase::AddScrollableLayer(
    LayerImpl* container,
    const gfx::Size& scroll_container_bounds,
    const gfx::Size& content_size) {
  LayerImpl* layer = AddLayer<LayerImpl>(container->layer_tree_impl());
  layer->SetElementId(LayerIdToElementIdForTesting(layer->id()));
  layer->SetDrawsContent(true);
  layer->SetBounds(content_size);
  layer->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CopyProperties(container, layer);
  CreateTransformNode(layer);
  CreateScrollNode(layer, scroll_container_bounds);
  return layer;
}

void LayerTreeHostImplTestBase::SetupScrollbarLayerCommon(
    LayerImpl* scroll_layer,
    ScrollbarLayerImplBase* scrollbar) {
  auto* tree_impl = scroll_layer->layer_tree_impl();
  scrollbar->SetScrollElementId(scroll_layer->element_id());
  scrollbar->SetDrawsContent(true);
  CopyProperties(scroll_layer, scrollbar);
  if (scroll_layer == tree_impl->OuterViewportScrollLayerForTesting()) {
    scrollbar->SetTransformTreeIndex(tree_impl->PageScaleTransformNode()->id);
    scrollbar->SetScrollTreeIndex(tree_impl->root_layer()->scroll_tree_index());
  } else {
    scrollbar->SetTransformTreeIndex(GetTransformNode(scroll_layer)->parent_id);
    scrollbar->SetScrollTreeIndex(GetScrollNode(scroll_layer)->parent_id);
  }
}

void LayerTreeHostImplTestBase::SetupScrollbarLayer(
    LayerImpl* scroll_layer,
    SolidColorScrollbarLayerImpl* scrollbar) {
  scrollbar->SetElementId(LayerIdToElementIdForTesting(scrollbar->id()));
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  auto& effect = CreateEffectNode(scrollbar);
  effect.opacity = 0;
  effect.has_potential_opacity_animation = true;
}

void LayerTreeHostImplTestBase::SetupScrollbarLayer(
    LayerImpl* scroll_layer,
    PaintedScrollbarLayerImpl* scrollbar) {
  SetupScrollbarLayerCommon(scroll_layer, scrollbar);
  scrollbar->SetHitTestOpaqueness(HitTestOpaqueness::kMixed);
  CreateEffectNode(scrollbar).opacity = 1;
}

LayerImpl* LayerTreeHostImplTestBase::InnerViewportScrollLayer() {
  return host_impl_->active_tree()->InnerViewportScrollLayerForTesting();
}
LayerImpl* LayerTreeHostImplTestBase::OuterViewportScrollLayer() {
  return host_impl_->active_tree()->OuterViewportScrollLayerForTesting();
}

std::unique_ptr<ScrollState> LayerTreeHostImplTestBase::BeginState(
    const gfx::Point& point,
    const gfx::Vector2dF& delta_hint,
    ui::ScrollInputType type) {
  ScrollStateData scroll_state_data;
  scroll_state_data.is_beginning = true;
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  scroll_state_data.delta_x_hint = delta_hint.x();
  scroll_state_data.delta_y_hint = delta_hint.y();
  scroll_state_data.is_direct_manipulation =
      type == ui::ScrollInputType::kTouchscreen;
  scroll_state_data.is_scrollbar_interaction =
      type == ui::ScrollInputType::kScrollbar;
  std::unique_ptr<ScrollState> scroll_state(new ScrollState(scroll_state_data));
  return scroll_state;
}

ScrollState LayerTreeHostImplTestBase::UpdateState(const gfx::Point& point,
                                                   const gfx::Vector2dF& delta,
                                                   ui::ScrollInputType type) {
  ScrollStateData scroll_state_data;
  scroll_state_data.delta_x = delta.x();
  scroll_state_data.delta_y = delta.y();
  scroll_state_data.position_x = point.x();
  scroll_state_data.position_y = point.y();
  scroll_state_data.is_direct_manipulation =
      type == ui::ScrollInputType::kTouchscreen;
  scroll_state_data.is_scrollbar_interaction =
      type == ui::ScrollInputType::kScrollbar;
  return ScrollState(scroll_state_data);
}

ScrollState LayerTreeHostImplTestBase::AnimatedUpdateState(
    const gfx::Point& point,
    const gfx::Vector2dF& delta) {
  auto state = UpdateState(point, delta, ui::ScrollInputType::kWheel);
  state.data()->delta_granularity = ui::ScrollGranularity::kScrollByPixel;
  return state;
}

void LayerTreeHostImplTestBase::DrawFrame() {
  auto args = viz::CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId, 1,
      base::TimeTicks() + base::Milliseconds(1));
  DrawFrameWithArgs(args);
}

void LayerTreeHostImplTestBase::DrawFrameWithArgs(
    const viz::BeginFrameArgs& args) {
  PrepareForUpdateDrawProperties(host_impl_->active_tree());
  TestFrameData frame;
  host_impl_->WillBeginImplFrame(args);
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
}

RenderFrameMetadata
LayerTreeHostImplTestBase::StartDrawAndProduceRenderFrameMetadata() {
  TestFrameData frame;
  EXPECT_EQ(DrawResult::kSuccess, host_impl_->PrepareToDraw(&frame));
  return host_impl_->MakeRenderFrameMetadata(&frame);
}

void LayerTreeHostImplTestBase::AllowedTouchActionTestHelper(
    float device_scale_factor,
    float page_scale_factor) {
  SetupViewportLayersInnerScrolls(gfx::Size(100, 100), gfx::Size(200, 200));
  DrawFrame();

  // Just hard code some random number, we care about the actual page scale
  // factor on the active tree.
  float min_page_scale_factor = 0.1f;
  float max_page_scale_factor = 5.0f;
  host_impl_->active_tree()->PushPageScaleFromMainThread(
      page_scale_factor, min_page_scale_factor, max_page_scale_factor);
  host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);

  LayerImpl* child = AddLayerInActiveTree();
  child->SetDrawsContent(true);
  child->SetBounds(gfx::Size(25, 25));
  CopyProperties(InnerViewportScrollLayer(), child);

  TouchActionRegion root_touch_action_region;
  root_touch_action_region.Union(TouchAction::kPanX, gfx::Rect(0, 0, 50, 50));
  root_layer()->SetTouchActionRegion(root_touch_action_region);
  TouchActionRegion child_touch_action_region;
  child_touch_action_region.Union(TouchAction::kPanLeft,
                                  gfx::Rect(0, 0, 25, 25));
  child->SetTouchActionRegion(child_touch_action_region);

  TouchAction touch_action = TouchAction::kAuto;
  GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
      gfx::Rect(gfx::Point(10, 10), gfx::Size()), &touch_action);
  EXPECT_EQ(TouchAction::kPanLeft, touch_action);
  touch_action = TouchAction::kAuto;
  GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
      gfx::Rect(gfx::Point(30, 30), gfx::Size()), &touch_action);
  EXPECT_EQ(TouchAction::kPanX, touch_action);

  TouchActionRegion new_child_region;
  new_child_region.Union(TouchAction::kPanY, gfx::Rect(0, 0, 25, 25));
  child->SetTouchActionRegion(new_child_region);
  touch_action = TouchAction::kAuto;
  GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
      gfx::Rect(gfx::Point(10, 10), gfx::Size()), &touch_action);
  EXPECT_EQ(TouchAction::kPanY, touch_action);
  touch_action = TouchAction::kAuto;
  GetInputHandler().EventListenerTypeForTouchStartOrMoveAt(
      gfx::Rect(gfx::Point(30, 30), gfx::Size()), &touch_action);
  EXPECT_EQ(TouchAction::kPanX, touch_action);
}

LayerImpl* LayerTreeHostImplTestBase::CreateLayerForSnapping() {
  SetupViewportLayersInnerScrolls(gfx::Size(50, 50), gfx::Size(100, 100));

  gfx::Size overflow_size(400, 400);
  LayerImpl* overflow = AddScrollableLayer(OuterViewportScrollLayer(),
                                           gfx::Size(100, 100), overflow_size);
  SnapContainerData container_data(
      ScrollSnapType(false, SnapAxis::kBoth, SnapStrictness::kMandatory),
      gfx::RectF(0, 0, 200, 200), gfx::PointF(300, 300));
  SnapAreaData area_data(ScrollSnapAlign(SnapAlignment::kStart),
                         gfx::RectF(50, 50, 100, 100), false, false,
                         ElementId(10));
  container_data.AddSnapAreaData(area_data);
  GetScrollNode(overflow)->snap_container_data.emplace(container_data);
  DrawFrame();

  return overflow;
}

std::optional<SnapContainerData>
LayerTreeHostImplTestBase::GetSnapContainerData(LayerImpl* layer) {
  return GetScrollNode(layer) ? GetScrollNode(layer)->snap_container_data
                              : std::nullopt;
}

void LayerTreeHostImplTestBase::ClearLayersAndPropertyTrees(
    LayerTreeImpl* layer_tree_impl) {
  layer_tree_impl->SetRootLayerForTesting(nullptr);
  layer_tree_impl->DetachLayers();
  layer_tree_impl->property_trees()->clear();
  layer_tree_impl->SetViewportPropertyIds(ViewportPropertyIds());
}

std::unique_ptr<LayerTreeFrameSink>
LayerTreeHostImplTestBase::CreateLayerTreeFrameSink() {
  return FakeLayerTreeFrameSink::Create3dForGpuRasterization();
}

void LayerTreeHostImplTestBase::DrawOneFrame() {
  PrepareForUpdateDrawProperties(host_impl_->active_tree());
  TestFrameData frame_data;
  host_impl_->PrepareToDraw(&frame_data);
  host_impl_->DidDrawAllLayers(frame_data);
}

void LayerTreeHostImplTestBase::SetScrollOffsetDelta(
    LayerImpl* layer_impl,
    const gfx::Vector2dF& delta) {
  if (layer_impl->layer_tree_impl()
          ->property_trees()
          ->scroll_tree_mutable()
          .SetScrollOffsetDeltaForTesting(layer_impl->element_id(), delta)) {
    layer_impl->layer_tree_impl()->DidUpdateScrollOffset(
        layer_impl->element_id(), /*pushed_from_main_or_pending_tree=*/false);
  }
}

void LayerTreeHostImplTestBase::BeginImplFrameAndAnimate(
    viz::BeginFrameArgs begin_frame_args,
    base::TimeTicks frame_time) {
  begin_frame_args.frame_time = frame_time;
  host_impl_->WillBeginImplFrame(begin_frame_args);
  host_impl_->Animate();
  host_impl_->UpdateAnimationState(true);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

void LayerTreeHostImplTestBase::InitializeImageWorker(
    const LayerTreeSettings& settings) {
  if (settings.enable_checker_imaging) {
    image_worker_ = std::make_unique<base::Thread>("ImageWorker");
    ASSERT_TRUE(image_worker_->Start());
  } else {
    image_worker_.reset();
  }
}

InputHandler& LayerTreeHostImplTestBase::GetInputHandler() {
  return host_impl_->GetInputHandler();
}

void LayerTreeHostImplTestBase::SetupMouseMoveAtWithDeviceScale(
    float device_scale_factor) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  const int thumb_thickness = 15;
  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size scrollbar_size(gfx::Size(thumb_thickness, viewport_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(device_scale_factor);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();
  // The scrollbar is on the left side.
  auto* scrollbar = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical,
      thumb_thickness, 0, true);
  SetupScrollbarLayer(root_scroll, scrollbar);
  scrollbar->SetBounds(scrollbar_size);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size));
  scrollbar->SetTouchActionRegion(touch_action_region);
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();

  ScrollbarAnimationController* scrollbar_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());

  const float kMouseMoveDistanceToTriggerFadeIn =
      scrollbar_animation_controller
          ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
          .MouseMoveDistanceToTriggerFadeIn();

  const float kMouseMoveDistanceToTriggerExpand =
      scrollbar_animation_controller
          ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
          .MouseMoveDistanceToTriggerExpand();

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerFadeIn + 1, 1));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerExpand, 10));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerFadeIn + 1, 100));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));

  did_request_redraw_ = false;
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));
  GetInputHandler().MouseMoveAt(gfx::Point(10, 10));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));
  GetInputHandler().MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));
  GetInputHandler().MouseMoveAt(gfx::Point(150, 120));
  EXPECT_FALSE(scrollbar_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));
}

void LayerTreeHostImplTestBase::SetupMouseMoveAtTestScrollbarStates(
    bool main_thread_scrolling) {
  LayerTreeSettings settings = DefaultSettings();
  settings.scrollbar_fade_delay = base::Milliseconds(500);
  settings.scrollbar_fade_duration = base::Milliseconds(300);
  settings.scrollbar_animator = LayerTreeSettings::AURA_OVERLAY;

  const int thumb_thickness = 15;
  gfx::Size viewport_size(300, 200);
  gfx::Size content_size(1000, 1000);
  gfx::Size child_layer_size(250, 150);
  gfx::Size scrollbar_size_1(
      gfx::Size(thumb_thickness, viewport_size.height()));
  gfx::Size scrollbar_size_2(
      gfx::Size(thumb_thickness, child_layer_size.height()));

  CreateHostImpl(settings, CreateLayerTreeFrameSink());
  host_impl_->active_tree()->SetDeviceScaleFactor(1);
  SetupViewportLayersInnerScrolls(viewport_size, content_size);
  LayerImpl* root_scroll = OuterViewportScrollLayer();

  if (main_thread_scrolling) {
    GetScrollNode(root_scroll)->main_thread_repaint_reasons =
        MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects;
  }

  // scrollbar_1 on root scroll.
  auto* scrollbar_1 = AddLayer<SolidColorScrollbarLayerImpl>(
      host_impl_->active_tree(), ScrollbarOrientation::kVertical,
      thumb_thickness, 0, true);
  SetupScrollbarLayer(root_scroll, scrollbar_1);
  scrollbar_1->SetBounds(scrollbar_size_1);
  TouchActionRegion touch_action_region;
  touch_action_region.Union(TouchAction::kNone, gfx::Rect(scrollbar_size_1));
  scrollbar_1->SetTouchActionRegion(touch_action_region);

  host_impl_->active_tree()->UpdateAllScrollbarGeometriesForTesting();
  host_impl_->active_tree()->DidBecomeActive();

  DrawFrame();

  ScrollbarAnimationController* scrollbar_1_animation_controller =
      host_impl_->ScrollbarAnimationControllerForElementId(
          root_scroll->element_id());
  EXPECT_TRUE(scrollbar_1_animation_controller);

  const float kMouseMoveDistanceToTriggerFadeIn =
      scrollbar_1_animation_controller
          ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
          .MouseMoveDistanceToTriggerFadeIn();

  const float kMouseMoveDistanceToTriggerExpand =
      scrollbar_1_animation_controller
          ->GetScrollbarAnimationController(ScrollbarOrientation::kVertical)
          .MouseMoveDistanceToTriggerExpand();

  // Mouse moves close to the scrollbar, goes over the scrollbar, and
  // moves back to where it was.
  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerFadeIn + 1, 0));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerExpand + 1, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(gfx::Point(10, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));

  GetInputHandler().MouseMoveAt(
      gfx::Point(thumb_thickness + kMouseMoveDistanceToTriggerExpand, 0));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbar(
      ScrollbarOrientation::kVertical));
  EXPECT_TRUE(scrollbar_1_animation_controller->MouseIsNearScrollbarThumb(
      ScrollbarOrientation::kVertical));
  EXPECT_FALSE(scrollbar_1_animation_controller->MouseIsOverScrollbarThumb(
      ScrollbarOrientation::kVertical));
}

LayerTreeHostImplTest::LayerTreeHostImplTest() {
  const auto test_mode = GetParam();
  switch (test_mode) {
    case CommitToActiveTreeTreesInVizClient:
    case CommitToPendingTreeTreesInVizClient:
    case CommitToActiveTreeTreesInVizService:
      scoped_feature_list_.InitAndEnableFeature(features::kTreesInViz);
      break;
    case CommitToActiveTree:
    case CommitToPendingTree:
      scoped_feature_list_.InitAndDisableFeature(features::kTreesInViz);
      break;
  }
}

LayerTreeHostImplTest::~LayerTreeHostImplTest() = default;

bool LayerTreeHostImplTest::CommitsToActiveTree() {
  const auto test_mode = GetParam();
  switch (test_mode) {
    case CommitToActiveTree:
    case CommitToActiveTreeTreesInVizClient:
    case CommitToActiveTreeTreesInVizService:
      return true;
    case CommitToPendingTree:
    case CommitToPendingTreeTreesInVizClient:
      return false;
  }
}

LayerTreeSettings LayerTreeHostImplTest::DefaultSettings() {
  const auto test_mode = GetParam();

  LayerTreeSettings settings = LayerTreeHostImplTestBase::DefaultSettings();
  settings.commit_to_active_tree = CommitsToActiveTree();
  settings.trees_in_viz_in_viz_process =
      (test_mode == CommitToActiveTreeTreesInVizService);

  return settings;
}

}  // namespace cc
