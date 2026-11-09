// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_VIEW_TRANSITION_SNAPSHOT_STATE_H_
#define CC_SCHEDULER_VIEW_TRANSITION_SNAPSHOT_STATE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "cc/cc_export.h"

namespace cc {

class ViewTransitionRequest;

// Tracks cross-document ViewTransition snapshot capture
// (`ViewTransitionRequest::IsSaveDirectiveForSnapshot()`) when early snapshot
// release (`POST_COMMIT`) is enabled.
//
// Because `ClientLayerTreeHostImpl` notifies Blink of the captured element
// rects at `Action::POST_COMMIT`, the Browser commits the navigation in
// parallel while CC is still rasterizing, activating, and drawing the `kSave`
// CompositorFrame. During this window (`IsCapturing()`):
// - `ProxyImpl` defers `SetVisible(false)` and `SetPauseRendering(false, ...)`
//   until the `kSave` frame is drawn and submitted, then replays them. If cc
//   aborts drawing the `kSave` frame, cannot draw the activated tree, or loses
//   the LayerTreeFrameSink, it drops the `kSave` directive and replays them.
// - `SchedulerStateMachine` queries `IsCapturing()` to suppress new main
//   frames, impl-side invalidations, and non-snapshot redraws while preserving
//   and unthrottling the `kSave` frame.
// - `Scheduler` issues a manual BeginFrame if `pending_tree_` activates while
//   `BeginImplFrameState` is `IDLE` (see
//   `SchedulerStateMachine::ShouldIssueBeginFrameForSnapshotSave()`), because
//   the external `BeginFrameSource` may already be paused.
class CC_EXPORT ViewTransitionSnapshotState {
 public:
  ViewTransitionSnapshotState() = default;
  ~ViewTransitionSnapshotState() = default;

  static bool HasSaveDirectiveForSnapshot(
      const std::vector<std::unique_ptr<ViewTransitionRequest>>& requests);

  static std::vector<uint32_t> GetDirectivesToNotifyInPostCommit(
      const std::vector<std::unique_ptr<ViewTransitionRequest>>& requests);

  static bool ShouldResetBrowserControlsAnimations(
      bool is_active_tree,
      const ViewTransitionRequest& request);

  // Returns the cached capture state updated via `SetCapturing()`. Unlike the
  // static `HasSaveDirectiveForSnapshot()`, which inspects a tree's requests
  // live, `IsCapturing()` remains true until refreshed via `SetCapturing()`.
  bool IsCapturing() const { return capturing_; }

  // Updates `capturing_` based on whether a snapshot `kSave` directive is
  // present on the pending or active tree. Transitions back to false once the
  // `kSave` frame is drawn and submitted, or if cc drops the `kSave` directive
  // (e.g. on an aborted draw or frame sink loss).
  void SetCapturing(bool capturing);

  // Returns true if `SetVisible(visible)` should be deferred until the snapshot
  // `kSave` frame is drawn.
  bool ShouldDeferSetVisible(bool visible);

  // Returns true if `SetPauseRendering(pause_rendering, ...)` should be
  // deferred until the snapshot `kSave` frame is drawn.
  bool ShouldDeferSetPauseRendering(bool pause_rendering,
                                    bool delay_until_visibility_change);

  // Returns true once the snapshot `kSave` frame is no longer capturing
  // (`!IsCapturing()`) and a deferred hide or unpause is ready to be replayed.
  bool HasReadyDeferredActions() const;
  bool TakeDeferredHide();
  bool TakeDeferredUnpause(bool& delay_until_visibility_change);

 private:
  bool capturing_ = false;
  bool deferred_hide_ = false;
  bool deferred_unpause_ = false;
  bool deferred_unpause_delay_until_visibility_change_ = false;
};

}  // namespace cc

#endif  // CC_SCHEDULER_VIEW_TRANSITION_SNAPSHOT_STATE_H_
