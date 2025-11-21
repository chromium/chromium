// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_HOST_IMPL_TEST_BASE_H_
#define CC_TREES_LAYER_TREE_HOST_IMPL_TEST_BASE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/input/input_handler.h"
#include "cc/layers/layer_impl.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/frame_data.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_host_impl_client.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class PaintedScrollbarLayerImpl;
class SolidColorScrollbarLayerImpl;

constexpr gfx::Size kDefaultLayerSize(100, 100);

struct TestFrameData : public FrameData {
  TestFrameData();
  ~TestFrameData();
};

class DidDrawCheckLayer : public LayerImpl {
 public:
  static std::unique_ptr<DidDrawCheckLayer> Create(LayerTreeImpl* tree_impl,
                                                   int id);

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* provider) override;

  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void DidDraw(viz::ClientResourceProvider* provider) override;

  bool will_draw_returned_true() const { return will_draw_returned_true_; }
  bool append_quads_called() const { return append_quads_called_; }
  bool did_draw_called() const { return did_draw_called_; }

  void set_will_draw_returns_false() { will_draw_returns_false_ = true; }

  void ClearDidDrawCheck();

 protected:
  DidDrawCheckLayer(LayerTreeImpl* tree_impl, int id);

 private:
  bool will_draw_returns_false_;
  bool will_draw_returned_true_;
  bool append_quads_called_;
  bool did_draw_called_;
};

class LayerTreeHostImplTestBase : public testing::Test,
                                  public LayerTreeHostImplClient {
 public:
  LayerTreeHostImplTestBase();
  ~LayerTreeHostImplTestBase() override;

  virtual LayerTreeSettings DefaultSettings();
  LayerTreeSettings LegacySWSettings();

  void SetUp() override;
  void TearDown() override;

  void EnsureSyncTree();
  void CreatePendingTree();

  // LayerTreeHostImplClient implementation.
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  bool IsReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread(bool urgent) override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void DidChangeBeginFrameSourcePaused(bool paused) override;
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override;
  void SetNeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) override;
  void NotifyImageDecodeRequestFinished(int request_id,
                                        bool speculative,
                                        bool decode_succeeded) override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::PendingCallbacks activated,
      const viz::FrameTimingDetails& details) override;
  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override;
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override;
  void NotifyCompositorMetricsTrackerResults(
      CustomTrackerResults results) override;
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) override;
  void ClearHistory() override;
  void SetHasActiveThreadedScroll(bool is_scrolling) override;
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override;
  size_t CommitDurationSampleCountForTesting() const override;
  void NotifyTransitionRequestFinished(
      uint32_t sequence_id,
      const viz::ViewTransitionElementResourceRects&) override;

  void set_reduce_memory_result(bool reduce_memory_result) {
    reduce_memory_result_ = reduce_memory_result;
  }

  AnimationHost* GetImplAnimationHost() const;

  virtual bool CreateHostImpl(
      const LayerTreeSettings& settings,
      std::unique_ptr<LayerTreeFrameSink> layer_tree_frame_sink);

  template <typename T, typename... Args>
  T* SetupRootLayer(LayerTreeImpl* layer_tree_impl,
                    const gfx::Size& viewport_size,
                    Args&&... args) {
    const int kRootLayerId = 1;
    DCHECK(!layer_tree_impl->root_layer());
    DCHECK(!layer_tree_impl->LayerById(kRootLayerId));
    layer_tree_impl->SetRootLayerForTesting(
        T::Create(layer_tree_impl, kRootLayerId, std::forward<Args>(args)...));
    auto* root = layer_tree_impl->root_layer();
    root->SetBounds(viewport_size);
    layer_tree_impl->SetDeviceViewportRect(
        gfx::Rect(DipSizeToPixelSize(viewport_size)));
    SetupRootProperties(root);
    return static_cast<T*>(root);
  }

  LayerImpl* SetupDefaultRootLayer(
      const gfx::Size& viewport_size = kDefaultLayerSize);
  LayerImpl* root_layer();
  gfx::Size DipSizeToPixelSize(const gfx::Size& size);

  void PushScrollOffsetsToPendingTree(
      const base::flat_map<ElementId, gfx::PointF>& offsets);

  void ClearNonScrollSyncTreeDeltasForTesting();

  static void ExpectClearedScrollDeltasRecursive(LayerImpl* root);

  static ::testing::AssertionResult ScrollInfoContains(
      const CompositorCommitData& commit_data,
      ElementId id,
      const gfx::Vector2dF& scroll_delta);

  static void ExpectNone(const CompositorCommitData& commit_data, ElementId id);

  template <typename T, typename... Args>
  T* AddLayer(LayerTreeImpl* layer_tree_impl, Args&&... args) {
    std::unique_ptr<T> layer = T::Create(layer_tree_impl, next_layer_id_++,
                                         std::forward<Args>(args)...);
    T* result = layer.get();
    layer_tree_impl->AddLayer(std::move(layer));
    return result;
  }

  LayerImpl* AddLayerInActiveTree();

  void SetupViewportLayers(LayerTreeImpl* layer_tree_impl,
                           const gfx::Size& inner_viewport_size,
                           const gfx::Size& outer_viewport_size,
                           const gfx::Size& content_size);

  void SetupViewportLayersInnerScrolls(const gfx::Size& inner_viewport_size,
                                       const gfx::Size& content_size);

  void SetupViewportLayersOuterScrolls(const gfx::Size& viewport_size,
                                       const gfx::Size& content_size);

  LayerImpl* AddContentLayer();

  void SetupViewportLayersNoScrolls(const gfx::Size& bounds);

  void CreateAndTestNonScrollableLayers(bool transparent_layer);

  LayerImpl* AddScrollableLayer(LayerImpl* container,
                                const gfx::Size& scroll_container_bounds,
                                const gfx::Size& content_size);

  void SetupScrollbarLayerCommon(LayerImpl* scroll_layer,
                                 ScrollbarLayerImplBase* scrollbar);

  void SetupScrollbarLayer(LayerImpl* scroll_layer,
                           SolidColorScrollbarLayerImpl* scrollbar);

  void SetupScrollbarLayer(LayerImpl* scroll_layer,
                           PaintedScrollbarLayerImpl* scrollbar);

  LayerImpl* InnerViewportScrollLayer();
  LayerImpl* OuterViewportScrollLayer();

  std::unique_ptr<ScrollState> BeginState(const gfx::Point& point,
                                          const gfx::Vector2dF& delta_hint,
                                          ui::ScrollInputType type);

  ScrollState UpdateState(const gfx::Point& point,
                          const gfx::Vector2dF& delta,
                          ui::ScrollInputType type);

  ScrollState AnimatedUpdateState(const gfx::Point& point,
                                  const gfx::Vector2dF& delta);

  void DrawFrame();
  void DrawFrameWithArgs(const viz::BeginFrameArgs& args);

  RenderFrameMetadata StartDrawAndProduceRenderFrameMetadata();

  void AllowedTouchActionTestHelper(float device_scale_factor,
                                    float page_scale_factor);

  LayerImpl* CreateLayerForSnapping();

  std::optional<SnapContainerData> GetSnapContainerData(LayerImpl* layer);

  void ClearLayersAndPropertyTrees(LayerTreeImpl* layer_tree_impl);

  void pinch_zoom_pan_viewport_forces_commit_redraw(float device_scale_factor);
  void pinch_zoom_pan_viewport_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_test(float device_scale_factor);
  void pinch_zoom_pan_viewport_and_scroll_boundary_test(
      float device_scale_factor);

  void SetupMouseMoveAtWithDeviceScale(float device_scale_factor);

  void SetupMouseMoveAtTestScrollbarStates(bool main_thread_scrolling);

  scoped_refptr<AnimationTimeline> timeline() { return timeline_; }

 protected:
  virtual std::unique_ptr<LayerTreeFrameSink> CreateLayerTreeFrameSink();

  void DrawOneFrame();

  static void SetScrollOffsetDelta(LayerImpl* layer_impl,
                                   const gfx::Vector2dF& delta);

  void BeginImplFrameAndAnimate(viz::BeginFrameArgs begin_frame_args,
                                base::TimeTicks frame_time);

  void InitializeImageWorker(const LayerTreeSettings& settings);

  InputHandler& GetInputHandler();

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
  bool did_prepare_tiles_;
  bool did_complete_page_scale_animation_;
  bool reduce_memory_result_;
  bool did_request_impl_side_invalidation_;
  base::OnceClosure animation_task_;
  base::TimeDelta requested_animation_delay_;
  std::unique_ptr<TestFrameData> last_on_draw_frame_;
  viz::CompositorRenderPassList last_on_draw_render_passes_;
  scoped_refptr<AnimationTimeline> timeline_;
  std::unique_ptr<base::Thread> image_worker_;
  int next_layer_id_ = 2;
  int first_scroll_observed = 0;
};

class LayerTreeHostImplTest
    : public LayerTreeHostImplTestBase,
      public testing::WithParamInterface<LayerTreeImplTestMode> {
 public:
  LayerTreeHostImplTest();
  ~LayerTreeHostImplTest() override;
  static bool CommitsToActiveTree();
  LayerTreeSettings DefaultSettings() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_HOST_IMPL_TEST_BASE_H_
