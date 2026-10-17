// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_IMPRESSION_CUI_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_IMPRESSION_CUI_TRACKER_H_

#include <optional>

#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

// Tracker for the Glic invocation -> FRE impression CUI interaction
// (b/555808587). Measures latency and outcome from opening Glic when the user
// has not yet consented until the FRE opt-in prompt is rendered
// (`kOptInImpression`), cancelled because the web client deferred FRE (e.g.,
// Message-First FRE focusing the input box via `kInputFocused`), abandoned on
// close/hide, or failed.
class GlicFreImpressionCuiTracker : public GlicCuiTracker {
 public:
  GlicFreImpressionCuiTracker();
  ~GlicFreImpressionCuiTracker() override;

  GlicFreImpressionCuiTracker(const GlicFreImpressionCuiTracker&) = delete;
  GlicFreImpressionCuiTracker& operator=(const GlicFreImpressionCuiTracker&) =
      delete;

 protected:
  std::optional<GlicCuiOutcome> GetEventOutcome(
      GlicInstanceEvent event) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_FRE_IMPRESSION_CUI_TRACKER_H_
