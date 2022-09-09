// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_MAC_STARTUP_PROFILER_H_
#define CHROME_BROWSER_MAC_MAC_STARTUP_PROFILER_H_

#include <map>

#include "base/memory/singleton.h"
#include "base/time/time.h"

// A lightweight profiler of startup performance. Records UMA metrics for the
// time delta between Chrome's launch and major initialization phases.
class MacStartupProfiler {
 public:
  // Returns pointer to the singleton instance for the current process.
  static MacStartupProfiler* GetInstance();

  MacStartupProfiler();

  MacStartupProfiler(const MacStartupProfiler&) = delete;
  MacStartupProfiler& operator=(const MacStartupProfiler&) = delete;

  ~MacStartupProfiler();

  // These locations correspond to major phases of Chrome startup.
  // Profiling of these locations should occur at the beginning of the method
  // indicated by the enum name.
  // The ordering of the enum matches the order in which the initialization
  // phases are reached.
  enum Location {
    PRE_MAIN_MESSAGE_LOOP_START,
    AWAKE_FROM_NIB,
    POST_MAIN_MESSAGE_LOOP_START,
    PRE_PROFILE_INIT,
    POST_PROFILE_INIT,
    WILL_FINISH_LAUNCHING,
    DID_FINISH_LAUNCHING,
  };

  // Record timestamp for the given location event.
  void Profile(Location location);

  // Call once to record all UMA metrics for all profiled locations.
  void RecordMetrics();

 private:
  friend struct base::DefaultSingletonTraits<MacStartupProfiler>;

  // Returns the name of the histogram for the given location.
  const std::string HistogramName(Location location);

  // Records UMA metrics for a specific location.
  void RecordHistogram(Location location, const base::TimeDelta& delta);

  // Keeps track of the time at which each initialization phase was reached.
  std::map<Location, base::TimeTicks> profiled_ticks_;

  // Whether UMA metrics have been recorded. Only record UMA metrics once.
  bool recorded_metrics_;
};

#endif  // CHROME_BROWSER_MAC_MAC_STARTUP_PROFILER_H_
