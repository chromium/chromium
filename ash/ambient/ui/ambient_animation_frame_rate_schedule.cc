// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_animation_frame_rate_schedule.h"

#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "ash/utility/lottie_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "cc/paint/skottie_marker.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {
namespace {

constexpr float kMinNormalizedTimestamp = 0.f;
constexpr float kMaxNormalizedTimestamp = 1.f;
// The last section of the schedule will always have an |end_timestamp|
// (exclusive) matching this. Since the largest possible normalized timestamp is
// 1.f, this could theoretically be 1.f + <some small amount>, but just using
// the largest possible float value for this ceiling is simplest.
constexpr float kInfiniteTimestamp = std::numeric_limits<float>::max();

// Frame rate markers have names with format
// "_CrOS_Marker_Throttled_<FrameRate>fps".
//
// Fills |parsed_fps| with the "FrameRate" embedded in the name on success.
bool IsFrameRateMarker(const std::string& marker_name, int& parsed_fps) {
  static const base::NoDestructor<std::string> kMarkerPatternStr(base::StrCat(
      {kLottieCustomizableIdPrefix, R"(_Marker_Throttled_([[:digit:]]+)fps)"}));
  static const base::NoDestructor<RE2> kMarkerPattern(*kMarkerPatternStr);
  return RE2::FullMatch(marker_name, *kMarkerPattern, &parsed_fps);
}

// Returns a new AmbientAnimationFrameRateSection, or nullopt if the |marker| or
// |fps| are invalid.
std::optional<AmbientAnimationFrameRateSection>
BuildAmbientAnimationFrameRateSection(const cc::SkottieMarker& marker,
                                      int fps) {
  // Requesting a frame rate over 60 fps will not break the graphics pipeline.
  // But it is currently unnecessary and simply not supported on some displays
  // at the hardware-level, so flag an error. If theoretically, the graphics
  // pipeline is told to render at a frame rate higher than what's supported, it
  // will handle it gracefully and just render at the default frame rate.
  static constexpr int kMaxFrameRateFps = 60;
  if (fps <= 0 || fps > kMaxFrameRateFps) {
    LOG(ERROR) << "Invalid frame rate (fps) specified: " << fps;
    return std::nullopt;
  }

  if (marker.begin_time < kMinNormalizedTimestamp ||
      marker.end_time > kMaxNormalizedTimestamp ||
      marker.end_time <= marker.begin_time) {
    LOG(ERROR) << "Frame rate marker has invalid timestamps ["
               << marker.begin_time << ", " << marker.end_time << ")";
    return std::nullopt;
  }
  return AmbientAnimationFrameRateSection(marker.begin_time, marker.end_time,
                                          base::Hertz(fps));
}

}  // namespace

AmbientAnimationFrameRateSection::AmbientAnimationFrameRateSection(
    float start_timestamp,
    float end_timestamp,
    base::TimeDelta frame_interval)
    : start_timestamp(start_timestamp),
      end_timestamp(end_timestamp),
      frame_interval(frame_interval) {}

bool AmbientAnimationFrameRateSection::operator<(
    const AmbientAnimationFrameRateSection& other) const {
  return start_timestamp < other.start_timestamp;
}

bool AmbientAnimationFrameRateSection::Contains(float t) const {
  return t >= start_timestamp && t < end_timestamp;
}

bool AmbientAnimationFrameRateSection::IntersectsWith(
    const AmbientAnimationFrameRateSection& other) const {
  return start_timestamp < other.end_timestamp &&
         end_timestamp > other.start_timestamp;
}

AmbientAnimationFrameRateSchedule BuildDefaultFrameRateSchedule() {
  return {AmbientAnimationFrameRateSection(
      kMinNormalizedTimestamp, kInfiniteTimestamp, kDefaultFrameInterval)};
}

AmbientAnimationFrameRateSchedule BuildAmbientAnimationFrameRateSchedule(
    const std::vector<cc::SkottieMarker>& animation_markers) {
  AmbientAnimationFrameRateSchedule frame_rate_schedule;
  for (const cc::SkottieMarker& marker : animation_markers) {
    int fps = 0;
    if (!IsFrameRateMarker(marker.name, fps))
      continue;

    std::optional<AmbientAnimationFrameRateSection> section =
        BuildAmbientAnimationFrameRateSection(marker, fps);
    if (section) {
      frame_rate_schedule.push_back(*section);
    } else {
      LOG(ERROR) << "Found invalid marker: " << marker.name;
      return AmbientAnimationFrameRateSchedule();
    }
  }

  if (frame_rate_schedule.empty())
    return BuildDefaultFrameRateSchedule();

  frame_rate_schedule.sort();

  // A section of the animation cannot have 2 frame rates simultaneously. If
  // the animation specifies 2 overlapping frame rate markers, this should be
  // fixed in the animation.
  auto current_section = frame_rate_schedule.begin();
  auto next_section = std::next(current_section);
  for (; next_section != frame_rate_schedule.end();
       ++current_section, ++next_section) {
    if (current_section->IntersectsWith(*next_section)) {
      LOG(ERROR) << "AmbientAnimationFrameRateSection " << *current_section
                 << " intersects with " << *next_section;
      return AmbientAnimationFrameRateSchedule();
    }
  }

  // Fill in the gaps in the schedule with the default frame rate.
  current_section = frame_rate_schedule.begin();
  next_section = std::next(current_section);
  for (; next_section != frame_rate_schedule.end();
       next_section = std::next(current_section)) {
    if (current_section->end_timestamp == next_section->start_timestamp) {
      ++current_section;
    } else {
      current_section = frame_rate_schedule.emplace(
          next_section, current_section->end_timestamp,
          next_section->start_timestamp, kDefaultFrameInterval);
    }
  }
  // Corner cases: Fill in gaps at the beginning and end of the animation.
  if (frame_rate_schedule.front().start_timestamp > kMinNormalizedTimestamp) {
    frame_rate_schedule.emplace_front(
        kMinNormalizedTimestamp, frame_rate_schedule.front().start_timestamp,
        kDefaultFrameInterval);
  }
  if (frame_rate_schedule.back().end_timestamp < 1.f) {
    frame_rate_schedule.emplace_back(frame_rate_schedule.back().end_timestamp,
                                     kInfiniteTimestamp, kDefaultFrameInterval);
  }
  // The last section's |end_timestamp| should always be inf to ensure all
  // timestamps fall within it (including |kMaxNormalizedTimestamp|).
  frame_rate_schedule.back().end_timestamp = kInfiniteTimestamp;
  return frame_rate_schedule;
}

std::ostream& operator<<(std::ostream& os,
                         const AmbientAnimationFrameRateSection& section) {
  return os << "start_timestamp=" << section.start_timestamp
            << " end_timestamp=" << section.end_timestamp
            << " frame_interval=" << section.frame_interval;
}

}  // namespace ash
