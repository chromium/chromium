// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_SCHEDULE_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_SCHEDULE_H_

#include <list>
#include <ostream>
#include <vector>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"

namespace ash {

// Zero duration is a special value in //viz meaning no custom frame rate
// control. The frame rate will default to the display's native refresh interval
// (usually 60 fps).
inline constexpr base::TimeDelta kDefaultFrameInterval = base::TimeDelta();

// The animation cycle is divided into sections, each of which has its own
// desired frame interval. The sections are contiguous, and each one spans from:
// [start_timestamp, end_timestamp).
//
// The |end_timestamp| of section N will always match the |start_timestamp|
// of section N + 1. The AmbientAnimationFrameRateSchedule is cyclic. The first
// section always has a |start_timestamp| of 0, and the last section always has
// an |end_timestamp| of +inf. This means that every possible timestamp falls
// within one and only one section of the schedule.
//
// Example with requested throttled sections. Timestamps are normalized in the
// range [0.f, 1.f]:
// * [.2, .4) - 20 fps
// * [.8, .9) - 30 fps
//
// Corresponding FrameRateSchedule:
// * start_timestamp:  0, end_timestamp: .2,  frame_interval: default
// * start_timestamp: .2, end_timestamp: .4,  frame_interval: 20 fps
// * start_timestamp: .4, end_timestamp: .8,  frame_interval: default
// * start_timestamp: .8, end_timestamp: .9,  frame_interval: 30 fps
// * start_timestamp: .9, end_timestamp: inf, frame_interval: default
//
// Why model it this way: The caller can cache the current section that the
// animation is on and start searching from there each time a new frame
// timestamp is available. Most of the time, the search will stop immediately
// since animations progress linearly in time with small increments. This makes
// the implementation both simple and efficient.
//
// Sections always contain a non-empty range of time.
struct ASH_EXPORT AmbientAnimationFrameRateSection {
  AmbientAnimationFrameRateSection(float start_timestamp,
                                   float end_timestamp,
                                   base::TimeDelta frame_interval);

  // Sorts by |start_timestamp|.
  bool operator<(const AmbientAnimationFrameRateSection& other) const;

  // Whether timestamp |t| falls within this section's boundaries.
  bool Contains(float t) const;

  // Whether this section intersects with another section.
  bool IntersectsWith(const AmbientAnimationFrameRateSection& other) const;

  // These are normalized timestamps in the range [0.f, 1.f] (with the exception
  // of the last section's |end_timestamp| as stated above).
  //
  // Inclusive.
  float start_timestamp = 0.f;
  // Exclusive
  float end_timestamp = 0.f;
  // 1 / (frame rate). This may be |kDefaultFrameInterval| to denote the
  // default frame rate from the display (i.e. this section does not need a
  // custom throttled frame rate).
  base::TimeDelta frame_interval;
};

using AmbientAnimationFrameRateSchedule =
    std::list<AmbientAnimationFrameRateSection>;
using AmbientAnimationFrameRateScheduleIterator =
    AmbientAnimationFrameRateSchedule::const_iterator;

// Returns a schedule that plays the entire animation at the default frame
// rate.
ASH_EXPORT AmbientAnimationFrameRateSchedule BuildDefaultFrameRateSchedule();

// Returns the schedule for the given set of |animation_markers|. Markers
// unrelated to frame rate throttling are permitted in |animation_markers|; they
// will be ignored. Returns an empty schedule if the animation is invalid.
ASH_EXPORT AmbientAnimationFrameRateSchedule
BuildAmbientAnimationFrameRateSchedule(
    const std::vector<cc::SkottieMarker>& animation_markers);

ASH_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const AmbientAnimationFrameRateSection& section);

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_FRAME_RATE_SCHEDULE_H_
