// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/scoped_picture_in_picture_occlusion_observation.h"

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_observer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker_observer.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"

ScopedPictureInPictureOcclusionObservation::
    ScopedPictureInPictureOcclusionObservation(
        PictureInPictureOcclusionObserver* occlusion_observer)
    : occlusion_observer_(occlusion_observer) {
  CHECK(occlusion_observer_);
}

ScopedPictureInPictureOcclusionObservation::
    ~ScopedPictureInPictureOcclusionObservation() {
  // Ensure `observation_` is destroyed before `observer_`.
  observation_.reset();
  observer_.reset();
}

void ScopedPictureInPictureOcclusionObservation::Observe(
    views::Widget* widget) {
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (!tracker) {
    return;
  }

  // If there's an existing observation, we need to ensure the old
  // `observation_` is destroyed before the old `observer_`. We also need to
  // reset the `widget_observation_` since a base::ScopedObservation requires a
  // reset before observing again.
  observation_.reset();
  observer_.reset();
  widget_observation_.Reset();

  observer_ = std::make_unique<PictureInPictureOcclusionTrackerObserver>(
      widget, occlusion_observer_);
  observation_ = std::make_unique<
      base::ScopedObservation<PictureInPictureOcclusionTracker,
                              PictureInPictureOcclusionTrackerObserver>>(
      observer_.get());
  widget_observation_.Observe(widget);
  observation_->Observe(tracker);
}

void ScopedPictureInPictureOcclusionObservation::OnWidgetDestroying(
    views::Widget* widget) {
  widget_observation_.Reset();
  observation_.reset();
  observer_.reset();
}
