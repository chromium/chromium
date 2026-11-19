// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_CORE_GET_POINTER_METRICS_H_
#define CHROME_BROWSER_TTC_CORE_GET_POINTER_METRICS_H_

namespace ttc {

// How a get_pointer call ended, for the Ttc.GetPointer.Outcome histogram.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(TtcGetPointerOutcome)
enum class GetPointerOutcome {
  // Answered with an image of what the user pointed at or, if they were not
  // pointing, of the text they had selected.
  kOk = 0,
  // The user was not pointing at anything.
  kNoActivePointer = 1,
  // The page is one that may not be captured, going by its URL or otherwise.
  kPageNotAllowed = 2,
  // The user left the page before the answer was ready.
  kPageChanged = 3,
  // Capturing the page took too long.
  kTimedOut = 4,
  // The page could not be captured, e.g. because the user left it before the
  // capture began, or the image could not be made.
  kCaptureFailed = 5,
  // Any other error, e.g. no tab to capture.
  kOtherError = 6,
  kMaxValue = kOtherError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ttc/enums.xml:TtcGetPointerOutcome)

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_CORE_GET_POINTER_METRICS_H_
