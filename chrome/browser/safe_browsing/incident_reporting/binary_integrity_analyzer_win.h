// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_WIN_H_

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace safe_browsing {

// Returns a vector containing the paths to all the binaries to verify.
std::vector<base::FilePath> GetCriticalBinariesPath();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_WIN_H_
