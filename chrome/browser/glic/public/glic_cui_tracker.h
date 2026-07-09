// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_CUI_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_CUI_TRACKER_H_

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"

namespace glic {

// Base class for tracking the latency and outcome of Glic Critical User
// Interaction (CUI) interactions. CUI represents the critical user interactions
// for Glic. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(GlicCuiOutcome)
enum class GlicCuiOutcome {
  kSuccess = 0,
  kUnknownCancel = 1,
  // The user explicitly closed the UI before the client was ready. This does
  // not include tab switches or other implicit backgrounding events.
  kAbandoned = 2,
  kFailed = 3,
  kFailedLatency = 4,
  kMaxValue = kFailedLatency,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicCuiOutcome)

enum class GlicInstanceEvent;

// GlicCuiTracker is intended to be a base class to measure various important
// "Critical User Interactions" (CUIs) across the glic codebase. These CUIs
// power the top-level Critical User Journeys (CUJs). Examples of CUIs include
// clicking the entry point, typing the first character, submitting a query,
// or accepting an opt-in.
class GlicCuiTracker {
 public:
  GlicCuiTracker();
  virtual ~GlicCuiTracker();

  GlicCuiTracker(const GlicCuiTracker&) = delete;
  GlicCuiTracker& operator=(const GlicCuiTracker&) = delete;

  void Resolve(GlicCuiOutcome reason);
  bool OnEvent(GlicInstanceEvent event);

  bool IsResolved() const { return is_resolved_; }

 protected:
  virtual std::optional<GlicCuiOutcome> GetEventOutcome(
      GlicInstanceEvent event) const;

  virtual const char* GetMetricName() const = 0;
  virtual base::TimeDelta GetHistogramMax() const;
  virtual base::TimeDelta GetMaxLatency() const;

 private:
  base::TimeTicks start_time_;
  bool is_resolved_ = false;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_CUI_TRACKER_H_
