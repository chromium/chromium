// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/pointer_tracker.h"

#include <algorithm>
#include <iterator>

#include "ui/gfx/geometry/vector2d_f.h"

namespace ttc {

namespace {

// A maximal sequence of samples that stayed near the same spot.
struct Run {
  // When the pointer arrived, i.e. the time of the first sample.
  base::TimeTicks start;
  // The running mean of the run's samples.
  gfx::PointF centroid;
  size_t sample_count = 0;
  bool is_press = false;
  // The point of the run's last press, if `is_press`.
  gfx::PointF press_point;
};

}  // namespace

PointerTracker::PointerTracker() : PointerTracker(Config()) {}

PointerTracker::PointerTracker(const Config& config) : config_(config) {}

PointerTracker::~PointerTracker() = default;

void PointerTracker::RecordSample(base::TimeTicks time,
                                  const gfx::PointF& point,
                                  bool is_press) {
  if (!samples_.empty()) {
    time = std::max(time, samples_.back().time);
    if (!is_press &&
        (point == samples_.back().point ||
         time - samples_.back().time < config_.min_sample_interval)) {
      return;
    }
  }
  samples_.emplace_back(point, time, is_press);
  Prune(time);
}

std::vector<PointerTracker::Location> PointerTracker::ResolveLocations(
    base::TimeTicks turn_start,
    base::TimeTicks now) {
  // Samples older than `history` as of now are only still here because nothing
  // has been recorded since.
  Prune(now);

  std::vector<Location> results;
  const base::TimeTicks window_start = turn_start - config_.pre_turn_lookback;

  // The first sample in the window, or the one before it if that is where the
  // pointer was when the window opened and it had arrived there within the
  // idle limit, so it was pointing rather than parked.
  auto first =
      std::ranges::lower_bound(samples_, window_start, {}, &Sample::time);
  if (first != samples_.begin() &&
      std::prev(first)->time >= window_start - config_.idle_limit) {
    --first;
  }
  if (first == samples_.end()) {
    return results;
  }

  // Group the samples into runs. A sample joins the current run if it is
  // within the dwell radius of the run's centroid; a press also marks the run
  // as a press. Anything else starts a new run. A press before the window
  // (e.g. from an earlier turn) does not count.
  std::vector<Run> runs;
  for (auto it = first; it != samples_.end(); ++it) {
    const bool is_press = it->is_press && it->time >= window_start;
    if (runs.empty() || (it->point - runs.back().centroid).Length() >
                            config_.dwell_radius_dip) {
      runs.push_back({.start = it->time, .centroid = it->point});
    }
    Run& run = runs.back();
    ++run.sample_count;
    run.centroid += gfx::ScaleVector2d(
        it->point - run.centroid, 1.0f / static_cast<float>(run.sample_count));
    if (is_press) {
      run.is_press = true;
      run.press_point = it->point;
    }
  }

  // A run lasts until the next run starts, or until now for the last one, so a
  // pointer that stopped moving is dwelling even though no samples arrive.
  // Only the part of a run inside the window counts towards its duration.
  // Collect the dwells most recent first. A press is the user's own choice of
  // point, so it stands in for the centroid.
  for (size_t i = runs.size(); i > 0; --i) {
    const Run& run = runs[i - 1];
    const base::TimeTicks end = i < runs.size() ? runs[i].start : now;
    const base::TimeDelta duration = end - std::max(run.start, window_start);
    if (!run.is_press && duration < config_.min_dwell_duration) {
      continue;
    }
    results.push_back({.point = run.is_press ? run.press_point : run.centroid,
                       .time = run.start,
                       .is_press = run.is_press,
                       .is_dwell = true});
    if (results.size() >= config_.max_results) {
      break;
    }
  }

  // No dwell means not even the last run lasted the minimum duration, so the
  // pointer is still moving and the user is most likely pointing at wherever
  // it is now. (A press would have made its run a dwell, so this is not one.)
  if (results.empty()) {
    const Sample& latest = samples_.back();
    results.push_back({.point = latest.point, .time = latest.time});
  }
  return results;
}

void PointerTracker::Clear() {
  samples_.clear();
}

void PointerTracker::Prune(base::TimeTicks now) {
  const base::TimeTicks oldest_allowed = now - config_.history;
  while (!samples_.empty() && samples_.front().time < oldest_allowed) {
    samples_.pop_front();
  }
  while (samples_.size() > config_.max_samples) {
    samples_.pop_front();
  }
}

}  // namespace ttc
