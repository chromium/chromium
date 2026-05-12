// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_
#define CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_

#include <memory>
#include <vector>

#include "cc/cc_export.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace cc {

class CC_EXPORT ClientLayerTreeHostImpl : public LayerTreeHostImpl {
 public:
  static std::unique_ptr<ClientLayerTreeHostImpl> Create(
      const LayerTreeSettings& settings,
      LayerTreeHostImplDelegate* delegate,
      TaskRunnerProvider* task_runner_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      TaskGraphRunner* task_graph_runner,
      std::unique_ptr<MutatorHost> mutator_host,
      RasterDarkModeFilter* dark_mode_filter,
      int id,
      scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
      LayerTreeHostSchedulingDelegate* scheduling_delegate);

  using LayerTreeHostImpl::LayerTreeHostImpl;
  ~ClientLayerTreeHostImpl() override;

  void SetActiveURL(const GURL& url, ukm::SourceId source_id);

  // LayerTreeHostImpl methods only used in the client.
  virtual void BeginMainFrameAborted(
      CommitEarlyOutReason reason,
      std::vector<std::unique_ptr<SwapPromise>> swap_promises,
      const viz::BeginFrameArgs& args,
      bool next_bmf,
      bool scroll_and_viewport_changes_synced);
  virtual void BeginCommit(int source_frame_number,
                           BeginMainFrameTraceId trace_id);
  virtual void FinishCommit(CommitState& commit_state,
                            const ThreadUnsafeCommitState& unsafe_state);
  virtual void CommitComplete();
  virtual void ReadyToCommit(
      bool scroll_and_viewport_changes_synced,
      const BeginMainFrameMetrics* begin_main_frame_metrics,
      bool commit_timeout);
  virtual void InvalidateContentOnImplSide();
  virtual void InvalidateLayerTreeFrameSink(bool needs_redraw);
  virtual void SetTreePriority(TreePriority priority);
  virtual void CreatePendingTree();

  void AnimatePendingTreeAfterCommit();
  void RecordGpuRasterizationHistogram();

 private:
  void PullLayerTreeHostPropertiesFrom(const CommitState&);
  void UpdateSyncTreeAfterCommitOrImplSideInvalidation();
  PaintWorkletJobMap GatherDirtyPaintWorklets(
      PaintImageIdFlatSet* dirty_paint_worklet_ids) const;
  void OnPaintWorkletResultsReady(PaintWorkletJobMap results);
  void NotifyPendingTreeFullyPainted();
};

}  // namespace cc

#endif  // CC_TREES_CLIENT_LAYER_TREE_HOST_IMPL_H_
