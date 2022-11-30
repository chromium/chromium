// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_MAC_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_MAC_H_

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace safe_browsing {

class IncidentReceiver;

// Wraps a path to a code object and its specified code requirement.
struct PathAndRequirement {
  PathAndRequirement(const base::FilePath& o_path,
                     const std::string& o_requirement)
      : path(o_path), requirement(o_requirement) {}
  base::FilePath path;
  std::string requirement;
};

// Returns a vector of pairs, each of which contains the paths to the binaries
// to verify, and the codesign requirement to use when verifying.
std::vector<PathAndRequirement> GetCriticalPathsAndRequirements();

// This is a helper stub to allow the signature checking code to be tested with
// custom requirements and files.
void VerifyBinaryIntegrityForTesting(IncidentReceiver* incident_receiver,
                                     const base::FilePath& path,
                                     const std::string& requirement);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_MAC_H_
