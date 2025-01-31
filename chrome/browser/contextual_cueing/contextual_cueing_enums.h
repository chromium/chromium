// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_

namespace contextual_cueing {

// LINT.IfChange(NudgeDecision)
enum class NudgeDecision {
  kUnknown = 0,
  // A nudge was available for the page.
  kSuccess = 1,
  // The server had no data for the page.
  kServerDataUnavailable = 2,
  // The server had data for the page, but it was malformed.
  kServerDataMalformed = 3,
  // The server had data for the page, but the client conditions did not
  // evaluate to true.
  kClientConditionsUnmet = 4,

  // New values above this line.
  kMaxValue = kClientConditionsUnmet,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/contextual_cueing/enums.xml:NudgeDecision)

}  // namespace contextual_cueing

// namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_ENUMS_H_
