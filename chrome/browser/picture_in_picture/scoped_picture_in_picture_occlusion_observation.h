// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_PICTURE_IN_PICTURE_OCCLUSION_OBSERVATION_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_PICTURE_IN_PICTURE_OCCLUSION_OBSERVATION_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/views/widget/widget_observer.h"

class PictureInPictureOcclusionObserver;
class PictureInPictureOcclusionTracker;
class PictureInPictureOcclusionTrackerObserver;

// The ScopedPictureInPictureOcclusionObservation observes the
// PictureInPictureOcclusionTracker owned by the PictureInPictureWindowManager.
// It starts observing when `Observe()` is called with a given widget and
// `occlusion_observer_` will be notified of occlusion state updates.
// `occlusion_observer_` is also synchronously notified on the call to
// `Observe()`. The ScopedPictureInPictureOcclusionObservation will stop
// observing for occlusions when EITHER:
// 1) The observed widget is destroyed, OR
// 2) The ScopedPictureInPictureOcclusionObservation itself is destroyed.
//
// See chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h
// for usage examples.
class ScopedPictureInPictureOcclusionObservation
    : public views::WidgetObserver {
 public:
  explicit ScopedPictureInPictureOcclusionObservation(
      PictureInPictureOcclusionObserver* occlusion_observer);
  ScopedPictureInPictureOcclusionObservation(
      const ScopedPictureInPictureOcclusionObservation&) = delete;
  ScopedPictureInPictureOcclusionObservation& operator=(
      const ScopedPictureInPictureOcclusionObservation&) = delete;
  ~ScopedPictureInPictureOcclusionObservation() override;

  // Starts observing the given `widget` for occlusions. If a widget is already
  // being observed, that widget will stop being observed and `widget` will be
  // observed instead. `occlusion_observer_` will immediately and synchronously
  // be notified of the current occlusion state.
  void Observe(views::Widget* widget);

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  std::unique_ptr<PictureInPictureOcclusionTrackerObserver> observer_;
  std::unique_ptr<
      base::ScopedObservation<PictureInPictureOcclusionTracker,
                              PictureInPictureOcclusionTrackerObserver>>
      observation_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  const raw_ptr<PictureInPictureOcclusionObserver> occlusion_observer_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_SCOPED_PICTURE_IN_PICTURE_OCCLUSION_OBSERVATION_H_
