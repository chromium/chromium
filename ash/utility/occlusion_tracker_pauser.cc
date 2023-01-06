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
  for (auto* root : Shell::GetAllRootWindows())
    Pause(root->GetHost()->compositor());

  if (!scoped_pause_) {
    scoped_pause_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
  }

  timer_.Stop();
  if (!timeout.is_zero()) {
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&OcclusionTrackerPauser::Timeout,
                                base::Unretained(this)));
  }
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
}

void OcclusionTrackerPauser::OnFinish(ui::Compositor* compositor) {
  if (!observations_.IsObservingSource(compositor))
    return;

  observations_.RemoveObservation(compositor);

  if (observations_.IsObservingAnySource())
    return;

  DCHECK(scoped_pause_);
  timer_.Stop();
  scoped_pause_.reset();
}

void OcclusionTrackerPauser::Timeout() {
  DCHECK(scoped_pause_);
  LOG(WARNING) << "Unpausing because animations didn't start and end in time";

  scoped_pause_.reset();
  observations_.RemoveAllObservations();
}

}  // namespace ash
