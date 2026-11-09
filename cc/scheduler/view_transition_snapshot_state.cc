// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/view_transition_snapshot_state.h"

#include <utility>

#include "cc/view_transition/view_transition_request.h"

namespace cc {

// static
bool ViewTransitionSnapshotState::HasSaveDirectiveForSnapshot(
    const std::vector<std::unique_ptr<ViewTransitionRequest>>& requests) {
  for (const auto& request : requests) {
    if (request->IsSaveDirectiveForSnapshot()) {
      return true;
    }
  }
  return false;
}

// static
std::vector<uint32_t>
ViewTransitionSnapshotState::GetDirectivesToNotifyInPostCommit(
    const std::vector<std::unique_ptr<ViewTransitionRequest>>& requests) {
  std::vector<uint32_t> sequence_ids;
  for (const auto& request : requests) {
    if (request->IsSaveDirectiveForSnapshot()) {
      sequence_ids.push_back(request->sequence_id());
    }
  }
  return sequence_ids;
}

// static
bool ViewTransitionSnapshotState::ShouldResetBrowserControlsAnimations(
    bool is_active_tree,
    const ViewTransitionRequest& request) {
  if (request.type() != ViewTransitionRequest::Type::kSave) {
    return false;
  }
  // Prevent browser controls animations from ticking when the next frame will
  // capture view transition snapshots (https://crbug.com/40855375). Browser
  // controls start animating to shown at navigation start (or during an active
  // scroll/fling when JS initiates a transition or navigation) and may still be
  // animating on the compositor thread when the snapshot `kSave` commit
  // arrives. For cross-document snapshot `kSave` directives, also reset on the
  // pending tree during commit because `POST_COMMIT` immediately runs
  // `sync_tree()->UpdateDrawProperties()`.
  return is_active_tree || request.IsSaveDirectiveForSnapshot();
}

void ViewTransitionSnapshotState::SetCapturing(bool capturing) {
  capturing_ = capturing;
}

bool ViewTransitionSnapshotState::ShouldDeferSetVisible(bool visible) {
  if (visible) {
    deferred_hide_ = false;
    return false;
  }
  if (capturing_) {
    deferred_hide_ = true;
    return true;
  }
  return false;
}

bool ViewTransitionSnapshotState::ShouldDeferSetPauseRendering(
    bool pause_rendering,
    bool delay_until_visibility_change) {
  if (pause_rendering) {
    deferred_unpause_ = false;
    deferred_unpause_delay_until_visibility_change_ = false;
    return false;
  }
  if (capturing_) {
    deferred_unpause_ = true;
    deferred_unpause_delay_until_visibility_change_ =
        delay_until_visibility_change;
    return true;
  }
  return false;
}

bool ViewTransitionSnapshotState::HasReadyDeferredActions() const {
  return !capturing_ && (deferred_hide_ || deferred_unpause_);
}

bool ViewTransitionSnapshotState::TakeDeferredHide() {
  return std::exchange(deferred_hide_, false);
}

bool ViewTransitionSnapshotState::TakeDeferredUnpause(
    bool& delay_until_visibility_change) {
  if (!deferred_unpause_) {
    return false;
  }
  deferred_unpause_ = false;
  delay_until_visibility_change =
      std::exchange(deferred_unpause_delay_until_visibility_change_, false);
  return true;
}

}  // namespace cc
