// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_

#include <stddef.h>

#include <memory>
#include <string>

namespace safe_browsing {

class IncidentReceiver;

// Registers a process-wide analysis with the incident reporting service that
// will verify the signature of the most critical binaries used by Chrome. It
// will send an incident report every time a signature verification fails.
void RegisterBinaryIntegrityAnalysis();

// Callback to pass to the incident reporting service. The incident reporting
// service will decide when to start the analysis.
void VerifyBinaryIntegrity(std::unique_ptr<IncidentReceiver> incident_receiver);

// Clear past incident reports for a file or bundle. This is used if the code
// object is now integral, as it will allow future incidents to be reported.
void ClearBinaryIntegrityForFile(IncidentReceiver* incident_receiver,
                                 const std::string& basename);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_BINARY_INTEGRITY_ANALYZER_H_
