// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_OUTCOME_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_OUTCOME_H_

// This represents the outcome of displaying a prompt to the user when the
// account tailored security bit changes.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TailoredSecurityOutcome {
  kAccepted = 0,
  kDismissed = 1,
  kSettings = 2,
  kShown = 3,
  kRejected = 4,
  kMaxValue = kRejected,
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_TAILORED_SECURITY_OUTCOME_H_
