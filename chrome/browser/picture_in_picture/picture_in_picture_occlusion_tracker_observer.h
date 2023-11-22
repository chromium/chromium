// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_OBSERVER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace views {
class Widget;
}  // namespace views

class PictureInPictureOcclusionObserver;

// The PictureInPictureOcclusionTrackerObserver is a base::CheckedObserver that
// associates a PictureInPictureOcclusionObserver with its observed
// views::Widget. Observers should not subclass
// PictureInPictureOcclusionTrackerObserver directly, but instead should
// subclass PictureInPictureOcclusionObserver and use a
// ScopedPictureInPictureOcclusionObservation to observe the
// PictureInPictureOcclusionTracker.
//
// See chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h
// for usage examples.
class PictureInPictureOcclusionTrackerObserver final
    : public base::CheckedObserver {
 public:
  PictureInPictureOcclusionTrackerObserver(
      views::Widget* occludable_widget,
      PictureInPictureOcclusionObserver* observer);
  PictureInPictureOcclusionTrackerObserver(
      const PictureInPictureOcclusionTrackerObserver&) = delete;
  PictureInPictureOcclusionTrackerObserver& operator=(
      const PictureInPictureOcclusionTrackerObserver&) = delete;
  ~PictureInPictureOcclusionTrackerObserver() override;

  views::Widget* occludable_widget() const { return occludable_widget_; }

  PictureInPictureOcclusionObserver* occlusion_observer() const {
    return occlusion_observer_;
  }

 private:
  const raw_ptr<views::Widget> occludable_widget_;
  const raw_ptr<PictureInPictureOcclusionObserver> occlusion_observer_;
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_TRACKER_OBSERVER_H_
