// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/client_layer_tree_host_impl.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "cc/paint/paint_worklet_layer_painter.h"
#include "cc/trees/layer_tree_host_impl_delegate.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/trace_utils.h"
#include "cc/trees/tree_synchronizer.h"
#include "components/viz/client/client_resource_provider.h"

namespace cc {

namespace {

bool VerboseLogEnabled() {
  return VLOG_IS_ON(3);
}

const char* ClientNameForVerboseLog() {
  return "ClientLTHI";
}

#define VERBOSE_LOG() \
  VLOG_IF(3, VerboseLogEnabled()) << ClientNameForVerboseLog() << ": "

}  // namespace

std::unique_ptr<ClientLayerTreeHostImpl> ClientLayerTreeHostImpl::Create(
    const LayerTreeSettings& settings,
    LayerTreeHostImplDelegate* delegate,
    TaskRunnerProvider* task_runner_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    TaskGraphRunner* task_graph_runner,
    std::unique_ptr<MutatorHost> mutator_host,
    RasterDarkModeFilter* dark_mode_filter,
    int id,
    scoped_refptr<base::SequencedTaskRunner> image_worker_task_runner,
    LayerTreeHostSchedulingDelegate* scheduling_delegate) {
  CHECK(!settings.trees_in_viz_in_viz_process);
  return base::WrapUnique(new ClientLayerTreeHostImpl(
      settings, delegate, task_runner_provider, rendering_stats_instrumentation,
      task_graph_runner, std::move(mutator_host), dark_mode_filter, id,
      std::move(image_worker_task_runner), scheduling_delegate));
}

ClientLayerTreeHostImpl::~ClientLayerTreeHostImpl() = default;

void ClientLayerTreeHostImpl::SetActiveURL(const GURL& url,
                                           ukm::SourceId source_id) {
  tile_manager_.set_active_url(url);
  has_observed_first_scroll_delay_ = false;
  // The active tree might still be from content for the previous page when the
  // recorder is updated here, since new content will be pushed with the next
  // main frame. But we should only get a few impl frames wrong here in that
  // case. Also, since checkerboard stats are only recorded with user
  // interaction, it must be in progress when the navigation commits for this
  // case to occur.
  // The source id has already been associated to the URL.
  frame_sorter_.Reset(/*reset_fcp=*/true);
}

void ClientLayerTreeHostImpl::BeginMainFrameAborted(
    CommitEarlyOutReason reason,
    std::vector<std::unique_ptr<SwapPromise>> swap_promises,
    const viz::BeginFrameArgs& args,
    bool next_bmf,
    bool scroll_and_viewport_changes_synced) {
  // If the begin frame data was handled, then scroll and scale set was applied
  // by the main thread, so the active tree needs to be updated as if these sent
  // values were applied and committed.
  bool main_frame_applied_deltas = MainFrameAppliedDeltas(reason);
  active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit(
      next_bmf, main_frame_applied_deltas);
  if (main_frame_applied_deltas) {
    if (pending_tree_) {
      pending_tree_->AppendSwapPromises(std::move(swap_promises));
    } else {
      base::TimeTicks timestamp = base::TimeTicks::Now();
      for (const auto& swap_promise : swap_promises) {
        SwapPromise::DidNotSwapAction action =
            swap_promise->DidNotSwap(SwapPromise::COMMIT_NO_UPDATE, timestamp);
        DCHECK_EQ(action, SwapPromise::DidNotSwapAction::BREAK_PROMISE);
      }
    }
  }

  // Notify the browser controls manager that we have processed any
  // controls constraint update.
  if (scroll_and_viewport_changes_synced && browser_controls_manager()) {
    browser_controls_manager()->NotifyConstraintSyncedToMainThread();
  }
}

void ClientLayerTreeHostImpl::BeginCommit(int source_frame_number,
                                          BeginMainFrameTraceId trace_id) {
  TRACE_EVENT0("cc", "LayerTreeHostImpl::BeginCommit");

  if (!CommitsToActiveTree()) {
    CreatePendingTree();
  }
  sync_tree()->set_source_frame_number(source_frame_number);
  sync_tree()->set_trace_id(trace_id);
}

void ClientLayerTreeHostImpl::FinishCommit(
    CommitState& state,
    const ThreadUnsafeCommitState& unsafe_state) {
  TRACE_EVENT0("cc,benchmark", "LayerTreeHostImpl::FinishCommit");
  LayerTreeImpl* tree = sync_tree();
  {
    // Instead of individual `Layer::PushPropertiesTo` triggering separate
    // thread hops to the main-thread, to complete releasing resources. Batch
    // all of them together for after `PullPropertiesFrom` completes.
    viz::ClientResourceProvider::ScopedBatchResourcesRelease
        scoped_resource_release =
            resource_provider_->CreateScopedBatchResourcesRelease();
    tree->PullPropertiesFrom(state, unsafe_state);
  }

  // Check whether the impl scroll animating nodes were removed by the commit.
  mutator_host()->HandleRemovedScrollAnimatingElements(CommitsToActiveTree());

  PullLayerTreeHostPropertiesFrom(state);

  // Transfer image decode requests to the impl thread.
  for (auto& entry : state.queued_image_decodes) {
    QueueImageDecode(std::get<0>(entry), *std::get<1>(entry),
                     std::get<2>(entry));
  }

  for (auto& benchmark : state.benchmarks) {
    ScheduleMicroBenchmark(std::move(benchmark));
  }

  new_local_surface_id_expected_ = false;

  // Dump property trees and layers if VerboseLogEnabled().
  VERBOSE_LOG() << "After finishing commit on impl, the sync tree:"
                << "\nproperty_trees:\n"
                << tree->property_trees()->ToString() << "\n"
                << "cc::LayerImpls:\n"
                << tree->LayerListAsJson();
}

void ClientLayerTreeHostImpl::PullLayerTreeHostPropertiesFrom(
    const CommitState& commit_state) {
  // TODO(bokan): The |external_pinch_gesture_active| should not be going
  // through the LayerTreeHost but directly from InputHandler to InputHandler.
  SetExternalPinchGestureActive(commit_state.is_external_pinch_gesture_active);
  if (commit_state.needs_gpu_rasterization_histogram) {
    RecordGpuRasterizationHistogram();
  }
  SetDebugState(commit_state.debug_state);
  SetVisualDeviceViewportSize(commit_state.visual_device_viewport_size);
  set_viewport_mobile_optimized(commit_state.is_viewport_mobile_optimized);
  SetPrefersReducedMotion(commit_state.prefers_reduced_motion);
  SetMayThrottleIfUndrawnFrames(commit_state.may_throttle_if_undrawn_frames);
}

void ClientLayerTreeHostImpl::RecordGpuRasterizationHistogram() {
  // Record how widely gpu rasterization is enabled.
  // This number takes device/gpu allowlist/denylist into account.
  // Note that we do not consider the forced gpu rasterization mode, which is
  // mostly used for debugging purposes.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuRasterizationEnabled",
                        raster_caps().use_gpu_rasterization);
}

void ClientLayerTreeHostImpl::CommitComplete() {
  DCHECK(!settings_.trees_in_viz_in_viz_process);

  TRACE_EVENT(
      "cc,benchmark", "LayerTreeHostImpl::CommitComplete",
      [&](perfetto::EventContext ctx) {
        EmitMainFramePipelineStep(
            ctx, sync_tree()->trace_id(),
            perfetto::protos::pbzero::MainFramePipeline::Step::COMMIT_COMPLETE);
      });

  if (input_delegate_) {
    input_delegate_->DidCommit();
  }

  if (CommitsToActiveTree()) {
    active_tree_->HandleScrollbarShowRequests();

    // We have to activate animations here or "IsActive()" is true on the
    // layers but the animations aren't activated yet so they get ignored by
    // UpdateDrawProperties.
    ActivateAnimations();
  }

  // We clear the entries that were never mutated by CC animations from the last
  // commit until now. Moreover, we reset the values of input properties and
  // relies on the fact that CC animation will mutate those values when pending
  // tree is animated below.
  // With that, when CC finishes animating an input property, the value of that
  // property stays at finish state until a commit kicks in, which is consistent
  // with current composited animations.
  base::flat_set<PaintWorkletInput::PropertyKey> used_properties;
  for (auto* layer : sync_tree()->picture_layers_with_paint_worklets()) {
    for (const auto& map_entry : layer->GetPaintWorkletRecords()) {
      const auto& property_keys = map_entry.first->GetPropertyKeys();
      used_properties.insert(property_keys.begin(), property_keys.end());
    }
  }
  paint_worklet_tracker_.ClearUnusedInputProperties(std::move(used_properties));

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results. When doing commit to the active tree, this must happen
  // after ActivateAnimations() in order for this ticking to be propagated
  // to layers on the active tree.
  if (CommitsToActiveTree()) {
    Animate();
  } else {
    AnimatePendingTreeAfterCommit();
  }

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();

  // Normally, we wait until tile tasks are updated (and draw images ref'ed)
  // before incrementing DecodedImageTracker's frame number (which may evict
  // decoded image data for un-ref'ed images). But if tile tasks are not dirty
  // then we won't update them, so do it now.
  if (!tile_priorities_dirty_) {
    tile_manager_.decoded_image_tracker().SetSyncTreeFrameNumber(
        sync_tree()->source_frame_number());
  }

  micro_benchmark_controller_.DidCompleteCommit();

  if (mutator_host_->CurrentFrameHadRAF()) {
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kRAF);
  }
  if (mutator_host_->HasCanvasInvalidation()) {
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kCanvasAnimation);
  }
  if (mutator_host_->CurrentFrameHadRAF() || mutator_host_->HasJSAnimation()) {
    frame_trackers_.StartSequence(FrameSequenceTrackerType::kJSAnimation);
  }

  if (mutator_host_->MainThreadAnimationsCount() > 0 ||
      mutator_host_->HasSmilAnimation()) {
    frame_trackers_.StartSequence(
        FrameSequenceTrackerType::kMainThreadAnimation);
    if (mutator_host_->HasViewTransition()) {
      frame_trackers_.StartSequence(
          FrameSequenceTrackerType::kSETMainThreadAnimation);
    }
  }

  for (const auto& info :
       mutator_host_->TakePendingCompositorMetricsTrackerInfos()) {
    const MutatorHost::TrackedAnimationSequenceId sequence_id = info.id;
    const bool start = info.start;
    if (start) {
      frame_trackers_.StartCustomSequence(sequence_id);
    } else {
      frame_trackers_.StopCustomSequence(sequence_id);
    }
  }
}

void ClientLayerTreeHostImpl::ReadyToCommit(
    bool scroll_and_viewport_changes_synced,
    const BeginMainFrameMetrics* begin_main_frame_metrics,
    bool commit_timeout) {
  if (((begin_main_frame_metrics &&
        begin_main_frame_metrics->should_measure_smoothness) ||
       commit_timeout) &&
      !frame_sorter_.first_contentful_paint_received()) {
    frame_sorter_.OnFirstContentfulPaintReceived();
  }

  // Notify the browser controls manager that we have processed any
  // controls constraint update.
  if (scroll_and_viewport_changes_synced && browser_controls_manager()) {
    browser_controls_manager()->NotifyConstraintSyncedToMainThread();
  }

  // If the scroll offsets were not synchronized, undo the sending of offsets
  // similar to what's done when the commit is aborted.
  if (!scroll_and_viewport_changes_synced) {
    active_tree_->ApplySentScrollAndScaleDeltasFromAbortedCommit(
        /*next_bmf=*/false, /*main_frame_applied_deltas=*/false);
  }
}

void ClientLayerTreeHostImpl::InvalidateContentOnImplSide() {
  DCHECK(!pending_tree_ && !settings_.trees_in_viz_in_viz_process);
  // Invalidation should never be ran outside the impl frame for non
  // synchronous compositor mode. For devices that use synchronous compositor,
  // e.g. Android Webview, the assertion is not guaranteed because it may ask
  // for a frame at any time.
  DCHECK(impl_thread_phase_ == ImplThreadPhase::INSIDE_IMPL_FRAME ||
         settings_.using_synchronous_renderer_compositor);

  if (!CommitsToActiveTree()) {
    CreatePendingTree();
    if (frame_trackers_.GetScrollingThread() ==
        FrameInfo::SmoothEffectDrivingThread::kRaster) {
      // If scrolling via raster, take EventMetrics and associate
      // them with newly-created pending tree.
      pending_tree()->AppendEventMetricsFromRasterThread(
          events_metrics_manager_.TakeSavedEventsMetrics());
    }
    AnimatePendingTreeAfterCommit();
  }

  if (input_delegate_) {
    input_delegate_->DidImplSideInvalidate();
  }

  UpdateSyncTreeAfterCommitOrImplSideInvalidation();
}

void ClientLayerTreeHostImpl::InvalidateLayerTreeFrameSink(bool needs_redraw) {
  DCHECK(layer_tree_frame_sink());

  layer_tree_frame_sink()->Invalidate(needs_redraw);
}

void ClientLayerTreeHostImpl::SetTreePriority(TreePriority priority) {
  global_tile_state_.tree_priority = priority;
  DidModifyTilePriorities(/*pending_update_tiles=*/false);
}

void ClientLayerTreeHostImpl::CreatePendingTree() {
  CHECK(!CommitsToActiveTree());
  CHECK(!pending_tree_);
  if (recycle_tree_) {
    recycle_tree_.swap(pending_tree_);
  } else {
    pending_tree_ = std::make_unique<LayerTreeImpl>(
        *this, CurrentBeginFrameArgs(), active_tree()->page_scale_factor(),
        active_tree()->top_controls_shown_ratio(),
        active_tree()->bottom_controls_shown_ratio());
  }
  pending_tree_fully_painted_ = false;

  delegate_->OnCanDrawStateChanged(CanDraw());
  TRACE_EVENT_BEGIN("cc", "PendingTree:waiting",
                    GetTracingTrack(pending_tree_.get()), "active_lsid",
                    active_tree()->local_surface_id_from_parent().ToString());
}

void ClientLayerTreeHostImpl::
    UpdateSyncTreeAfterCommitOrImplSideInvalidation() {
  DCHECK(!settings_.trees_in_viz_in_viz_process);

  sync_tree()->set_needs_update_draw_properties();

  // We need an update immediately post-commit to have the opportunity to create
  // tilings.
  // We can avoid updating the ImageAnimationController during this
  // DrawProperties update since it will be done when we animate the controller
  // below.
  bool update_tiles = true;
  bool update_image_animation_controller = false;
  sync_tree()->UpdateDrawProperties(update_tiles,
                                    update_image_animation_controller);

  sync_tree()->InvalidateRasterInducingScrolls(
      pending_invalidation_raster_inducing_scrolls_);
  pending_invalidation_raster_inducing_scrolls_.clear();

  base::flat_map<PaintWorkletInput::PropertyKey,
                 std::pair<PaintWorkletInput::PropertyValue,
                           PaintWorkletInput::PropertyValue>>
      animated_properties =
          paint_worklet_tracker_.TakeAndResetAnimatedProperties();
  bool worklets_invalidated = false;

  for (auto* layer : sync_tree()->picture_layers_with_paint_worklets()) {
    for (const auto& map_entry : layer->GetPaintWorkletRecords()) {
      for (const auto& property_key : map_entry.first->GetPropertyKeys()) {
        const auto& it = animated_properties.find(property_key);
        if (it != animated_properties.end()) {
          worklets_invalidated = true;
          layer->InvalidatePaintWorklets(property_key, it->second.first,
                                         it->second.second);
        }
      }
    }
  }

  if (worklets_invalidated) {
    delegate_->SetNeedsImplSideInvalidation(
        true /* needs_first_draw_on_activation */);
    if (sync_tree()->property_change_forces_commit_criteria() ==
        PropertyChangeForcesCommitCriteria::kAny) {
      SetNeedsCommit();
    }
  }

  PaintImageIdFlatSet dirty_paint_worklet_ids;
  PaintWorkletJobMap dirty_paint_worklets =
      GatherDirtyPaintWorklets(&dirty_paint_worklet_ids);

  PaintImageIdFlatSet images_to_invalidate =
      tile_manager_.TakeImagesToInvalidateOnSyncTree();

  CHECK(!settings_.trees_in_viz_in_viz_process);
  CHECK(image_animation_controller_);
  const auto& animated_images = image_animation_controller_->AnimateForSyncTree(
      CurrentBeginFrameArgs(), GatherAnimatedImageDriverState());
  images_to_invalidate.insert(animated_images.begin(), animated_images.end());
  if (image_animation_controller_->HasAdvancedAnimationClients()) {
    SetNeedsCommit();
  }

  images_to_invalidate.insert(dirty_paint_worklet_ids.begin(),
                              dirty_paint_worklet_ids.end());

  sync_tree()->InvalidateRegionForImages(images_to_invalidate);

  // Note that it is important to push the state for checkerboarded and animated
  // images prior to PrepareTiles here when committing to the active tree. This
  // is because new tiles on the active tree depend on tree specific state
  // cached in these components, which must be pushed to active before preparing
  // tiles for the updated active tree.
  if (CommitsToActiveTree()) {
    ActivateStateForImages();
  }

  sync_tree()->SetCreatedBeginFrameArgs(CurrentBeginFrameArgs());

  if (!paint_worklet_painter_) {
    // Blink should not send us any PaintWorklet inputs until we have a painter
    // registered.
    DCHECK(sync_tree()->picture_layers_with_paint_worklets().empty());
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  if (!dirty_paint_worklets.size()) {
    pending_tree_fully_painted_ = true;
    NotifyPendingTreeFullyPainted();
    return;
  }

  delegate_->NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState::PROCESSING);
  auto done_callback =
      base::BindOnce(&ClientLayerTreeHostImpl::OnPaintWorkletResultsReady,
                     base::Unretained(this));
  paint_worklet_painter_->DispatchWorklets(std::move(dirty_paint_worklets),
                                           std::move(done_callback));
}

void ClientLayerTreeHostImpl::OnPaintWorkletResultsReady(
    PaintWorkletJobMap results) {
#if DCHECK_IS_ON()
  // Nothing else should have painted the PaintWorklets while we were waiting,
  // and the results should have painted every PaintWorklet, so these should be
  // the same.
  PaintImageIdFlatSet dirty_paint_worklet_ids;
  DCHECK_EQ(results.size(),
            GatherDirtyPaintWorklets(&dirty_paint_worklet_ids).size());
#endif

  for (const auto& entry : results) {
    for (const PaintWorkletJob& job : entry.second->data) {
      LayerImpl* layer_impl =
          pending_tree_->FindPendingTreeLayerById(job.layer_id());
      // Painting the pending tree occurs asynchronously but stalls the pending
      // tree pipeline, so nothing should have changed while we were doing that.
      DCHECK(layer_impl);
      static_cast<PictureLayerImpl*>(layer_impl)
          ->SetPaintWorkletRecord(job.input(), job.output());
    }
  }

  // While the pending tree is being painted by PaintWorklets, we restrict the
  // tiles the TileManager is able to see. This may cause the TileManager to
  // believe that it has finished rastering all the necessary tiles. When we
  // finish painting the tree and release all the tiles, we need to mark the
  // tile priorities as dirty so that the TileManager logic properly re-runs.
  tile_priorities_dirty_ = true;

  // Set the painted state before calling the scheduler, to ensure any callback
  // running as a result sees the correct painted state.
  pending_tree_fully_painted_ = true;
  delegate_->NotifyPaintWorkletStateChange(Scheduler::PaintWorkletState::IDLE);

  // The pending tree may have been force activated from the signal to the
  // scheduler above, in which case there is no longer a tree to paint.
  if (pending_tree_) {
    NotifyPendingTreeFullyPainted();
  }
}

void ClientLayerTreeHostImpl::NotifyPendingTreeFullyPainted() {
  // The pending tree must be fully painted at this point.
  DCHECK(pending_tree_fully_painted_ && !settings_.trees_in_viz_in_viz_process);

  // Nobody should claim the pending tree is fully painted if there is an
  // ongoing dispatch.
  DCHECK(!paint_worklet_painter_ ||
         !paint_worklet_painter_->HasOngoingDispatch());

  // Start working on newly created tiles immediately if needed.
  // TODO(vmpstr): Investigate always having PrepareTiles issue
  // NotifyReadyToActivate, instead of handling it here.
  bool did_prepare_tiles = PrepareTiles();
  if (!did_prepare_tiles) {
    NotifyReadyToActivate();

    // Ensure we get ReadyToDraw signal even when PrepareTiles not run. This
    // is important for SingleThreadProxy and impl-side painting case. For
    // STP, we commit to active tree and RequiresHighResToDraw, and set
    // Scheduler to wait for ReadyToDraw signal to avoid Checkerboard.
    if (CommitsToActiveTree() ||
        settings_.wait_for_all_pipeline_stages_before_draw) {
      NotifyReadyToDraw();
    }
  }
}

void ClientLayerTreeHostImpl::AnimatePendingTreeAfterCommit() {
  DCHECK(pending_tree_);

  // Start animations before UpdateDrawProperties and PrepareTiles, as they can
  // change the results.
  base::TimeTicks monotonic_time = CurrentBeginFrameArgs().frame_time;
  AnimateLayers(monotonic_time, /* is_active_tree */ false);

  if (input_delegate_) {
    input_delegate_->TickAnimations(monotonic_time);
  }
}

PaintWorkletJobMap ClientLayerTreeHostImpl::GatherDirtyPaintWorklets(
    PaintImageIdFlatSet* dirty_paint_worklet_ids) const {
  PaintWorkletJobMap dirty_paint_worklets;
  for (PictureLayerImpl* layer :
       sync_tree()->picture_layers_with_paint_worklets()) {
    for (const auto& entry : layer->GetPaintWorkletRecordMap()) {
      const scoped_refptr<const PaintWorkletInput>& input = entry.first;
      const PaintImage::Id& paint_image_id = entry.second.first;
      const std::optional<PaintRecord>& record = entry.second.second;
      // If we already have a record we can reuse it and so the
      // PaintWorkletInput isn't dirty.
      if (record) {
        continue;
      }

      // Mark this PaintWorklet as needing invalidation.
      dirty_paint_worklet_ids->insert(paint_image_id);

      // Create an entry in the appropriate PaintWorkletJobVector for this dirty
      // PaintWorklet.
      int worklet_id = input->WorkletId();
      auto& job_vector = dirty_paint_worklets[worklet_id];
      if (!job_vector) {
        job_vector = base::MakeRefCounted<PaintWorkletJobVector>();
      }

      PaintWorkletJob::AnimatedPropertyValues animated_property_values;
      for (const auto& element : input->GetPropertyKeys()) {
        DCHECK(!animated_property_values.contains(element));
        const PaintWorkletInput::PropertyValue& animated_property_value =
            paint_worklet_tracker_.GetPropertyAnimationValue(element);
        // No value indicates that the input property was not mutated by CC
        // animation.
        if (animated_property_value.has_value()) {
          animated_property_values.emplace(element, animated_property_value);
        }
      }

      job_vector->data.emplace_back(layer->id(), input,
                                    std::move(animated_property_values));
    }
  }
  return dirty_paint_worklets;
}

}  // namespace cc
