// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_OBSERVER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_OBSERVER_H_

// A PictureInPictureOcclusionObserver is the part of the
// PictureInPictureOcclusionTrackerObserver that gets notified of occlusion
// state changes.
class PictureInPictureOcclusionObserver {
 public:
  PictureInPictureOcclusionObserver(const PictureInPictureOcclusionObserver&) =
      delete;
  PictureInPictureOcclusionObserver& operator=(
      const PictureInPictureOcclusionObserver&) = delete;

  // Called when the observed widget starts or stops being occluded by a
  // picture-in-picture window. Also immediately and synchronously called with
  // the current state when observation starts.
  virtual void OnOcclusionStateChanged(bool occluded) {}

 protected:
  PictureInPictureOcclusionObserver();
  virtual ~PictureInPictureOcclusionObserver();
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_OCCLUSION_OBSERVER_H_
