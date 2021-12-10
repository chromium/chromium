// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_

#include <cstddef>

#include "ash/ash_export.h"

namespace ash {

// Tells the photo model and controller requirements for how topics are handled
// by the current UI.
struct ASH_EXPORT AmbientPhotoConfig {
  // If true, topics from the IMAX server containing a primary and related image
  // are always split into two topics, where the second topic's primary image
  // is set to the "related" image from the original paired topic. The client
  // will never pairs topics itself, and a topic will only ever have a primary
  // image.
  //
  // If false, topics may contain a pair of primary and related photos.
  bool should_split_topics = false;

  // How many decoded topics to keep buffered at any given time while rendering.
  // Dictated by the number of topics the UI displays at any given time.
  std::size_t num_decoded_topics_to_buffer = 0;

  // TODO(esum): Evaluate whether the following are needed:
  // * Max topics to cache - Since the animation photo config splits topics,
  //   there may be less total images available compared to the slideshow
  //   config. To compensate, the animation photo config may requires a higher
  //   max size for its cache.
  // * Number of topics to prepare (load, save, decode) in parallel - Since the
  //   animation photo config only ever has 1 image in a topic, it will never
  //   prepare images in parallel like the slideshow config can when there's a
  //   single paired topic. It may need to prepare 2 topics at a time.
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
