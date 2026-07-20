// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_SUBMIT_QUERY_CUI_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_SUBMIT_QUERY_CUI_TRACKER_H_

#include <optional>

#include "chrome/browser/glic/public/glic_cui_tracker.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"

namespace glic {

// Tracker for the "User submits a typed query" CUI interaction.
// Measures latency from user submission until the response starts or fails.
class GlicSubmitQueryCuiTracker : public GlicCuiTracker {
 public:
  GlicSubmitQueryCuiTracker();
  ~GlicSubmitQueryCuiTracker() override;

  GlicSubmitQueryCuiTracker(const GlicSubmitQueryCuiTracker&) = delete;
  GlicSubmitQueryCuiTracker& operator=(const GlicSubmitQueryCuiTracker&) =
      delete;

 protected:
  std::optional<GlicCuiOutcome> GetEventOutcome(
      GlicInstanceEvent event) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_SUBMIT_QUERY_CUI_TRACKER_H_
