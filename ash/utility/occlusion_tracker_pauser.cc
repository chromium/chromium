// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/occlusion_tracker_pauser.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace ash {

OcclusionTrackerPauser::OcclusionTrackerPauser() = default;

OcclusionTrackerPauser::~OcclusionTrackerPauser() = default;

void OcclusionTrackerPauser::PauseUntilAnimationsEnd(
    const base::TimeDelta& extra_pause_duration) {
  auto* a11y_controller = Shell::Get()->accessibility_controller();
  /*
   * Ddo not pause the occlusion tracker when accessibility feature is enabled
   * because it may add a permnent CompositorAnimationObserver.
   * TODO(crbug.com/1222698): Remove this if when the accessibility layer
   * migrates to use LayerAnimationElement.
   */
  for (int type = AccessibilityControllerImpl::kAutoclick;
       type < AccessibilityControllerImpl::kFeatureCount; type++) {
    auto casted_type =
        static_cast<AccessibilityControllerImpl::FeatureType>(type);
    if (a11y_controller->GetFeature(casted_type).enabled())
      return;
  }
  for (auto* root : Shell::GetAllRootWindows())
    Pause(root->GetHost()->compositor(), extra_pause_duration);
}

void OcclusionTrackerPauser::OnLastAnimationEnded(ui::Compositor* compositor) {
  OnFinish(compositor);
}

void OcclusionTrackerPauser::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  OnFinish(compositor);
}

void OcclusionTrackerPauser::Pause(
    ui::Compositor* compositor,
    const base::TimeDelta& extra_pause_duration) {
  timer_.Stop();

  if (extra_pause_duration_ < extra_pause_duration)
    extra_pause_duration_ = extra_pause_duration;

  if (!observations_.IsObservingSource(compositor))
    observations_.AddObservation(compositor);

  if (!scoped_pause_) {
    scoped_pause_ =
        std::make_unique<aura::WindowOcclusionTracker::ScopedPause>();
  }
}

void OcclusionTrackerPauser::OnFinish(ui::Compositor* compositor) {
  if (!observations_.IsObservingSource(compositor))
    return;
  observations_.RemoveObservation(compositor);

  if (observations_.IsObservingAnySource())
    return;

  if (extra_pause_duration_.is_zero()) {
    Unpause();
  } else {
    timer_.Start(FROM_HERE, extra_pause_duration_,
                 base::BindOnce(&OcclusionTrackerPauser::Unpause,
                                base::Unretained(this)));
    extra_pause_duration_ = base::TimeDelta();
  }
}

void OcclusionTrackerPauser::Unpause() {
  DCHECK(scoped_pause_);
  scoped_pause_.reset();
}

}  // namespace ash
