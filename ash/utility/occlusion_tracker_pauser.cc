// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/occlusion_tracker_pauser.h"

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

OcclusionTrackerPauser::OcclusionTrackerPauser() = default;

OcclusionTrackerPauser::~OcclusionTrackerPauser() {
  DCHECK(!observations_.IsObservingAnySource());
}

void OcclusionTrackerPauser::PauseUntilAnimationsEnd(base::TimeDelta timeout) {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    // Compositors may not be animating yet - start observing all of them. In
    // fact, it is the common case that none will be animating at this point. We
    // assume at least one compositor will start animating very soon, and
    // unpause when all compositors are not animating after that.
    Pause(root->GetHost()->compositor());
  }

  if (!scoped_pause_) {
    scoped_pause_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
  }

  timer_.Stop();
  if (!timeout.is_zero()) {
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&OcclusionTrackerPauser::Shutdown,
                                base::Unretained(this), /*timed_out=*/true));
  }
}

void OcclusionTrackerPauser::OnFirstAnimationStarted(
    ui::Compositor* compositor) {
  animating_compositors_.insert(compositor);
}

void OcclusionTrackerPauser::OnFirstNonAnimatedFrameStarted(
    ui::Compositor* compositor) {
  OnFinish(compositor);
}

void OcclusionTrackerPauser::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  OnFinish(compositor);
}

void OcclusionTrackerPauser::Pause(ui::Compositor* compositor) {
  if (!observations_.IsObservingSource(compositor))
    observations_.AddObservation(compositor);
  if (compositor->IsAnimating()) {
    animating_compositors_.insert(compositor);
  }
}

void OcclusionTrackerPauser::OnFinish(ui::Compositor* compositor) {
  animating_compositors_.erase(compositor);
  if (observations_.IsObservingSource(compositor)) {
    observations_.RemoveObservation(compositor);
  }

  if (!animating_compositors_.empty()) {
    return;
  }

  Shutdown(/*timed_out=*/false);
}

void OcclusionTrackerPauser::Shutdown(bool timed_out) {
  DCHECK(scoped_pause_);
  LOG_IF(WARNING, timed_out)
      << "Unpausing because animations didn't start and end in time";

  // We are finished pausing, cancel any outstanding timer which would call
  // `Shutdown` again.
  timer_.Stop();
  scoped_pause_.reset();
  observations_.RemoveAllObservations();
  animating_compositors_.clear();
}

}  // namespace ash
