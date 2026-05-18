// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_
#define CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_

#include <stdint.h>

#include "base/containers/enum_set.h"

namespace smart_restart {

// Simple data container representing the baseline browser state relevant for
// restartability. This is used for lightweight checks (e.g. macOS Zero-Window).
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

// Extended telemetry captured during complex events (e.g. OS Lock, Deep Idle)
// to evaluate restart opportunities with high fidelity.
//
// This struct is designed to be used as a short-lived snapshot of the browser
// state computed on demand. It does not support removing blockers after they
// are added.
struct ExtendedRestartabilityState {
  ExtendedRestartabilityState();
  ExtendedRestartabilityState(const ExtendedRestartabilityState&);
  ExtendedRestartabilityState(ExtendedRestartabilityState&&);
  ExtendedRestartabilityState& operator=(const ExtendedRestartabilityState&);
  ExtendedRestartabilityState& operator=(ExtendedRestartabilityState&&);
  ~ExtendedRestartabilityState();

  // Specific reasons why a restart might be disruptive.
  //
  // LINT.IfChange(SmartRestartBlocker)
  enum class SmartRestartBlocker {
    // --- Baseline Checks (Session-level or Global) ---
    kIncognito = 0,
    kBeforeUnloadHandler = 1,  // Unsaved data (Interaction + BeforeUnload)
    kDownload = 2,
    kMedia = 3,  // Global audio playing
    kAppWindow = 4,

    // --- Tab Discarding Signals (Aligned with relative PM order) ---
    // See chrome/browser/performance_manager/policies/cannot_discard_reason.h
    kNoMainFrame = 5,
    kVisible = 6,
    kAudible = 7,
    kPictureInPicture = 8,
    kPdf = 9,
    kNotWebOrInternal = 10,
    kInvalidURL = 11,
    kNotificationsEnabled = 12,
    kExtensionProtected = 13,
    kCapturingVideo = 14,
    kCapturingAudio = 15,
    kBeingMirrored = 16,
    kCapturingWindow = 17,
    kCapturingDisplay = 18,
    kConnectedToBluetooth = 19,
    kConnectedToUSB = 20,
    kActiveTab = 21,
    kPinnedTab = 22,
    kDevToolsOpen = 23,
    kBackgroundActivity = 24,
    kFormInteractions = 25,
    kUserEdits = 26,
    kGlicShared = 27,
    kWebApp = 28,

    // --- Additional Signals ---
    kVisiblePausedMedia = 29,
    kLensShared = 30,

    // The following states from tab discarding are not currently checked:
    // - kAlreadyDiscarded: Zero-friction state; skipped for scanning
    //   efficiency.
    // - kNotATab: Iteration is already scoped to tabs.
    // - kDiscardAttempted: Tab discarding internal state.
    // - kRecentlyVisible: Redundant when restart delay serves as the grace
    //   period.
    // - kRecentlyAudible: Tab discarding graph-only state; active audio/capture
    //   signals already cover critical disruption cases (e.g. meetings).
    // - kOptedOut: Enterprise discard policy; updates use separate policies.

    kMaxValue = kLensShared
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SmartRestartBlocker)

  // Perceived impact level of a restart, mapped directly to UMA outcomes.
  enum class SmartRestartDisruptionLevel {
    kNoDisruption = 0,
    kLowDisruption = 1,
    kMediumDisruption = 2,
    kHighDisruption = 3,
    kMaxValue = kHighDisruption
  };

  RestartabilityState baseline;
  int total_tab_count = 0;
  SmartRestartDisruptionLevel max_disruption_level =
      SmartRestartDisruptionLevel::kNoDisruption;

  using BlockerSet = base::EnumSet<SmartRestartBlocker,
                                   SmartRestartBlocker::kIncognito,
                                   SmartRestartBlocker::kMaxValue>;
  BlockerSet blockers;

  // Adds a blocker and updates the maximum disruption level.
  void AddBlocker(SmartRestartBlocker blocker);
};

class RestartabilityMonitor {
 public:
  RestartabilityMonitor() = delete;
  RestartabilityMonitor(const RestartabilityMonitor&) = delete;
  RestartabilityMonitor& operator=(const RestartabilityMonitor&) = delete;

  // Gathers the current state of the browser from various global services.
  static RestartabilityState ComputeCurrentState();

  // Gathers extended state to evaluate restart opportunities with high
  // fidelity. Intended for complex events like OS Lock or Deep Idle.
  static ExtendedRestartabilityState ComputeExtendedRestartabilityState();
};

}  // namespace smart_restart

#endif  // CHROME_BROWSER_LIFETIME_RESTARTABILITY_MONITOR_H_
