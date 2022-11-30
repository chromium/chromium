// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ENROLLMENT_STATUS_H_
#define CHROME_BROWSER_METRICS_ENROLLMENT_STATUS_H_

// Possible device enrollment status for a Chrome OS device. Used by both
// ash-chrome and lacros-chrome.
// Used by UMA histogram, so entries shouldn't be reordered or removed.
enum class EnrollmentStatus {
  kNonManaged = 0,
  kUnused = 1,  // Formerly MANAGED_EDU, see crbug.com/462770.
  kManaged = 2,
  kErrorGettingStatus = 3,
  kMaxValue = kErrorGettingStatus,
};

#endif  // CHROME_BROWSER_METRICS_ENROLLMENT_STATUS_H_
