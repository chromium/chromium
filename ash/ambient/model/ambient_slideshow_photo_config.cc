// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_slideshow_photo_config.h"

namespace ash {

AmbientPhotoConfig CreateAmbientSlideshowPhotoConfig() {
  AmbientPhotoConfig config;
  // The UI can render both the primary and related photo at the same time in
  // certain cases (ex: 2 portrait photos displayed on the left and right
  // halves of the screen).
  config.should_split_topics = false;

  // Always having 2 topics available prevents any chance of screen burn. In
  // the worst case scenario that no more assets become available, the slideshow
  // can alternate between the 2 topics indefinitely.
  config.num_topic_sets_to_buffer = 2;
  config.topic_set_size = 1;
  config.min_total_topics_required = config.num_topic_sets_to_buffer;

  // The view for this UI listens for when a new topic has been committed to
  // the model and uses this as a signal to immediately update the UI. Thus,
  // it only makes sense to refresh at the end of each cycle. Don't refresh
  // at the UI_START_RENDERING mark, otherwise the UI will start rendering the
  // initial topic, then immediately transition to another topic once the
  // new one is prepared.
  config.refresh_topic_markers = {AmbientPhotoConfig::Marker::kUiCycleEnded};
  return config;
}

AmbientPhotoConfig CreateAmbientManagedSlideshowPhotoConfig() {
  AmbientPhotoConfig config = CreateAmbientSlideshowPhotoConfig();
  // Note: This isn't used by the managed code path right now, but for the
  // sake of consistency this is being set to the correct value as we
  // don't want image pairing in the managed screensaver code path.
  config.should_split_topics = true;

  return config;
}

}  // namespace ash
