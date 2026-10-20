// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_RESPONSE_COMPLETION_CUI_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_RESPONSE_COMPLETION_CUI_TRACKER_H_

#include <optional>

#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

// Tracker for the full turn response completion CUI interaction (CUJ 3 T3.5).
// Measures latency and outcome from user query submission until the streaming
// response finishes (`kResponseStopped`, where `ResponseStopCause::kUnknown`
// represents an omitted stop cause indicating success), is cancelled/abandoned,
// or fails.
class GlicResponseCompletionCuiTracker : public GlicCuiTracker {
 public:
  GlicResponseCompletionCuiTracker();
  ~GlicResponseCompletionCuiTracker() override;

  GlicResponseCompletionCuiTracker(const GlicResponseCompletionCuiTracker&) =
      delete;
  GlicResponseCompletionCuiTracker& operator=(
      const GlicResponseCompletionCuiTracker&) = delete;

 protected:
  std::optional<GlicCuiOutcome> GetEventOutcome(
      GlicInstanceEvent event) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_RESPONSE_COMPLETION_CUI_TRACKER_H_
