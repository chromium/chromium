// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_OCCLUSION_TRACKER_PAUSER_H_
#define ASH_UTILITY_OCCLUSION_TRACKER_PAUSER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"

namespace ash {

// A utility class to pause aura's WindowOcclusionTracker until animations are
// finished on all compositors.
class ASH_EXPORT OcclusionTrackerPauser : public ui::CompositorObserver {
 public:
  OcclusionTrackerPauser();
  OcclusionTrackerPauser(const OcclusionTrackerPauser&) = delete;
  OcclusionTrackerPauser& operator=(const OcclusionTrackerPauser&) = delete;
  ~OcclusionTrackerPauser() override;

  // Pause the occlusion tracker until all new animations added after this are
  // finished. If the timeout is elapsed before all new animations are finished,
  // the pause will be unpaused. This function requires there to be at least one
  // animation running, or at least one animation about to run as a
  // precondition.
  void PauseUntilAnimationsEnd(base::TimeDelta timeout);

  // ui::CompositorObserver:
  void OnFirstAnimationStarted(ui::Compositor* compositor) override;
  void OnFirstNonAnimatedFrameStarted(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

 private:
  void Pause(ui::Compositor* compositor);
  void OnFinish(ui::Compositor* compositor);
  void Shutdown(bool timed_out);

  base::OneShotTimer timer_;
  base::ScopedMultiSourceObservation<ui::Compositor, ui::CompositorObserver>
      observations_{this};

  // Keeps track of compositors that are animating. We can unpause when this is
  // empty.
  base::flat_set<raw_ptr<ui::Compositor, CtnExperimental>>
      animating_compositors_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause> scoped_pause_;
};

}  // namespace ash

#endif  // ASH_UTILITY_OCCLUSION_TRACKER_PAUSER_H_
