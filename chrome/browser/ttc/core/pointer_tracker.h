// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_TRACKER_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_TRACKER_H_

#include <stddef.h>

#include <vector>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace ttc {

// Keeps a short history of where the pointer has been, so that when the user
// says "this" or "here" the spot they meant can be recovered after the fact:
// when asked, it resolves the places where the pointer rested ("dwells")
// during the user's current turn. Samples are positions in the root widget's
// DIP space; the tracker does not care which device produced them. It holds at
// most `Config::max_samples` samples spanning at most `Config::history`, so it
// is cheap to keep running for a whole session.
class PointerTracker {
 public:
  struct Config {
    // How far back samples are kept.
    base::TimeDelta history = base::Seconds(20);
    // Hard cap on the number of samples kept.
    size_t max_samples = 256;
    // A move closer than this in time to the last kept sample is dropped, so
    // that a 120 Hz pointer does not fill the history in two seconds. Presses
    // are always kept.
    base::TimeDelta min_sample_interval = base::Milliseconds(16);
    // Samples within this distance of a run's centroid belong to the run.
    float dwell_radius_dip = 12.0f;
    // A run must last at least this long to count as a dwell.
    base::TimeDelta min_dwell_duration = base::Milliseconds(300);
    // People often point just before they start speaking, so the turn's
    // window starts this much before the turn.
    base::TimeDelta pre_turn_lookback = base::Seconds(1);
    // A pointer that had not moved in this long when the turn's window opened
    // is parked, not pointing.
    base::TimeDelta idle_limit = base::Seconds(15);
    // The most locations ResolveLocations() returns.
    size_t max_results = 1;
  };

  // A recorded pointer position.
  struct Sample {
    // Position in root-widget DIPs.
    gfx::PointF point;
    // When the pointer was there.
    base::TimeTicks time;
    // Whether the sample comes from a press (e.g. a mouse-down).
    bool is_press = false;
  };

  // A place the user pointed at, as resolved by ResolveLocations().
  struct Location {
    // Root-widget DIPs: for a dwell, the point of its last press if it has one,
    // otherwise the centroid of its samples.
    gfx::PointF point;
    // When the pointer arrived there.
    base::TimeTicks time;
    // Whether it includes a press made during the turn's window.
    bool is_press = false;
    // False only for the fallback, the most recent sample.
    bool is_dwell = false;
  };

  // Uses the default `Config`.
  PointerTracker();
  explicit PointerTracker(const Config& config);
  PointerTracker(const PointerTracker&) = delete;
  PointerTracker& operator=(const PointerTracker&) = delete;
  ~PointerTracker();

  // Records that the pointer was at `point` at `time`. `is_press` marks a
  // sample from a press (e.g. a mouse-down) rather than a move; a press is the
  // strongest pointing gesture and always counts as a dwell (see
  // ResolveLocations()). A move to the same point as the last sample is
  // ignored, since it is not movement: Aura synthesizes such moves when content
  // changes under a still pointer. So is a move within `min_sample_interval`
  // of the last kept sample. Samples are expected in roughly increasing time
  // order; a sample earlier than the last is treated as if it had the last
  // sample's time.
  void RecordSample(base::TimeTicks time,
                    const gfx::PointF& point,
                    bool is_press = false);

  // Resolves where the user pointed during the turn that started at
  // `turn_start`, as of `now`. Returns up to `Config::max_results` locations,
  // most recent first:
  //
  // - Each run of samples that stayed within `dwell_radius_dip` of its centroid
  //   for at least `min_dwell_duration` is a dwell. A run lasts from its first
  //   sample until the first sample of the next run (or `now`), so a pointer
  //   that stops moving is dwelling even though no samples arrive.
  // - A press marks its run as a dwell regardless of duration. A press within
  //   `dwell_radius_dip` of the current run joins it; one farther away starts
  //   a run of its own.
  // - Only the turn's window, which opens at `turn_start - pre_turn_lookback`,
  //   counts. If the pointer had arrived at its position within `idle_limit`
  //   when the window opened, that position starts the first run, but its
  //   duration is measured from the window's start, and a press before the
  //   window does not count.
  // - If there is no dwell, the pointer is still moving, and the most recent
  //   sample is returned as a single non-dwell location.
  // - If the pointer was parked for longer than `idle_limit` when the window
  //   opened and has not moved since, nothing is returned.
  //
  // Also drops samples older than `history` as of `now`, as RecordSample()
  // would.
  std::vector<Location> ResolveLocations(base::TimeTicks turn_start,
                                         base::TimeTicks now);

  // Forgets all samples.
  void Clear();

  size_t sample_count_for_testing() const { return samples_.size(); }

 private:
  // Drops samples older than `history` as of `now`, and over `max_samples`.
  void Prune(base::TimeTicks now);

  const Config config_;
  base::circular_deque<Sample> samples_;
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_POINTER_TRACKER_H_
