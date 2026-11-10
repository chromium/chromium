// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_POINTER_TRACKER_H_
#define CHROME_BROWSER_TTC_CORE_POINTER_TRACKER_H_

#include <stddef.h>

#include <optional>

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace ttc {

// Keeps a short history of where the pointer has been, so that when the user
// says "this" or "here" the spot they meant can be recovered after the fact.
// Samples are positions in the root widget's DIP space; the tracker does not
// care which device produced them. It holds at most `Config::max_samples`
// samples spanning at most `Config::history`, so it is cheap to keep running
// for a whole session.
class PointerTracker {
 public:
  struct Config {
    // How far back samples are kept.
    base::TimeDelta history = base::Seconds(20);
    // Hard cap on the number of samples kept.
    size_t max_samples = 256;
    // A pointer that has not moved in this long is parked, not pointing.
    base::TimeDelta idle_limit = base::Seconds(15);
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

  // Uses the default `Config`.
  PointerTracker();
  explicit PointerTracker(const Config& config);
  PointerTracker(const PointerTracker&) = delete;
  PointerTracker& operator=(const PointerTracker&) = delete;
  ~PointerTracker();

  // Records that the pointer was at `point` at `time`. `is_press` marks a
  // sample from a press (e.g. a mouse-down) rather than a move. A move to the
  // same point as the last sample is ignored, since it is not movement: Aura
  // synthesizes such moves when content changes under a still pointer. Samples
  // are expected in roughly increasing time order; a sample earlier than the
  // last is treated as if it had the last sample's time.
  void RecordSample(base::TimeTicks time,
                    const gfx::PointF& point,
                    bool is_press = false);

  // Resolves where the user is pointing as of `now`: the most recent sample if
  // the pointer moved within `Config::idle_limit`. Otherwise the pointer is
  // parked, not pointing, and nothing is returned.
  std::optional<Sample> ResolveSample(base::TimeTicks now) const;

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
