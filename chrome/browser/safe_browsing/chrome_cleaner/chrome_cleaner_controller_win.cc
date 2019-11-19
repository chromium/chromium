// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

#include <ostream>

#include "base/metrics/histogram_macros.h"

namespace safe_browsing {

using UserResponse = ChromeCleanerController::UserResponse;

// Keep the printable names of these enums short since they're used in tests
// with very long parameter lists.
//
std::ostream& operator<<(std::ostream& out,
                         ChromeCleanerController::State state) {
  return out << "State" << static_cast<int>(state);
}

std::ostream& operator<<(std::ostream& out, UserResponse response) {
  return out << "Resp" << static_cast<int>(response);
}

void RecordCleanupStartedHistogram(CleanupStartedHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.CleanupStarted", value,
                            CLEANUP_STARTED_MAX);
}

ChromeCleanerController::ChromeCleanerController() = default;

ChromeCleanerController::~ChromeCleanerController() = default;

}  // namespace safe_browsing
