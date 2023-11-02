// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_FRAME_METADATA_H_
#define CC_PAINT_FRAME_METADATA_H_

#include "base/time/time.h"
#include "cc/paint/paint_export.h"

namespace cc {

// TODO(khushalsagar): Find a better name?
struct CC_PAINT_EXPORT FrameMetadata {
  FrameMetadata() = default;
  FrameMetadata(bool complete, base::TimeDelta duration)
      : complete(complete), duration(duration) {}

  bool operator==(const FrameMetadata& other) const {
    return complete == other.complete && duration == other.duration;
  }

  // True if the decoder has all encoded content for this frame.
  bool complete = true;

  // The duration for which this frame should be displayed.
  base::TimeDelta duration;
};

}  // namespace cc

#endif  // CC_PAINT_FRAME_METADATA_H_
