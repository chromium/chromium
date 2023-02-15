// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_

#include <cstddef>
#include <ostream>

#include "ash/ash_export.h"
#include "base/containers/flat_set.h"

namespace ash {

// Tells the photo model and controller requirements for how topics are handled
// by the current UI.
struct ASH_EXPORT AmbientPhotoConfig {
  AmbientPhotoConfig();
  AmbientPhotoConfig(const AmbientPhotoConfig& other);
  AmbientPhotoConfig& operator=(const AmbientPhotoConfig& other);
  ~AmbientPhotoConfig();

  std::size_t GetNumDecodedTopicsToBuffer() const {
    return num_topic_sets_to_buffer * topic_set_size;
  }

  bool IsEmpty() const { return GetNumDecodedTopicsToBuffer() == 0; }

  // If true, topics from the IMAX server containing a primary and related image
  // are always split into two topics, where the second topic's primary image
  // is set to the "related" image from the original paired topic. The client
  // will never pairs topics itself, and a topic will only ever have a primary
  // image.
  //
  // If false, topics may contain a pair of primary and related photos.
  bool should_split_topics = false;

  // How many sets of decoded topics to keep buffered at any given time while
  // rendering, where the size of each set is |topic_set_size|.
  std::size_t num_topic_sets_to_buffer = 0;

  // The maximum number of topics that the UI will display at any given time.
  std::size_t topic_set_size = 0;

  // The minimum number of topics required for the UI to launch and be
  // functional. This is only used in the worst-case scenario where
  // |num_topic_sets_to_buffer| is too difficult to reach for whatever reason
  // (network slowness, etc).
  //
  // Must be <= GetNumDecodedTopicsToBuffer().
  std::size_t min_total_topics_required = 0;

  // A marker is any time point of interest in the UI. Note all Ambient UIs have
  // a time component to them. They are cyclic with a period T and progress
  // linearly from time 0 to T. After a cycle completes, the UI loops back to
  // time 0 again, repeating indefinitely.
  enum class Marker {
    // A one-time event when the UI first starts rendering (the start of the
    // very first cycle).
    kUiStartRendering,
    // Marks the end of one UI cycle, and by extension, the start of another
    // cycle.
    kUiCycleEnded
  };
  // Markers at which a new set of topics should be decoded and written to the
  // model, where the size of the new set is |topic_set_size|.
  base::flat_set<Marker> refresh_topic_markers;

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

ASH_EXPORT std::ostream& operator<<(std::ostream& os,
                                    AmbientPhotoConfig::Marker marker);

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_PHOTO_CONFIG_H_
