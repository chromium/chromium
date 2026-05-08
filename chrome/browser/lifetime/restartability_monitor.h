// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_
#define CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_

#include <stdint.h>

namespace smart_restart {

// Simple data container representing the browser state relevant for restart.
struct RestartabilityState {
  // Represents the bitmask of combination states for impacting a potential
  // restart, or kNone if it is viable to restart. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  //
  // LINT.IfChange(SmartRestartStateFactor)
  enum SmartRestartStateFactor {
    kNone = 0,
    kIncognito = 1 << 0,
    kBeforeUnloadHandler = 1 << 1,
    kDownload = 1 << 2,
    kMedia = 1 << 3,
    kAppWindow = 1 << 4,
    kTotalBrowserCountZero = 1 << 5,
    kMaxValue = (1 << 6) - 1
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SmartRestartability)

  int download_count = 0;
  bool is_audio_playing = false;
  bool has_app_windows = false;
  bool has_dirty_tabs = false;
  bool has_incognito = false;
  bool total_browser_count_is_zero = false;

  // Returns a bitmask of `SmartRestartStateFactor` indicating whether the
  // browser can be restarted based on the snapshot state.
  uint32_t GetRestartabilityStateFactor() const;

  // Returns true if there are any active factors blocking a restart (e.g.
  // downloads, media, incognito).
  bool HasAnyActiveBlockers() const;
};

class RestartabilityMonitor {
 public:
  RestartabilityMonitor() = delete;
  RestartabilityMonitor(const RestartabilityMonitor&) = delete;
  RestartabilityMonitor& operator=(const RestartabilityMonitor&) = delete;

  // Gathers the current state of the browser from various global services.
  static RestartabilityState ComputeCurrentState();
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_
