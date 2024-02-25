// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker_observer.h"

PictureInPictureOcclusionTrackerObserver::
    PictureInPictureOcclusionTrackerObserver(
        views::Widget* occludable_widget,
        PictureInPictureOcclusionObserver* occlusion_observer)
    : occludable_widget_(occludable_widget),
      occlusion_observer_(occlusion_observer) {}

PictureInPictureOcclusionTrackerObserver::
    ~PictureInPictureOcclusionTrackerObserver() = default;
