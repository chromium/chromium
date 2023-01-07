// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_ANALYSIS_CALLBACK_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_ANALYSIS_CALLBACK_H_

#include <memory>

#include "base/functional/callback_forward.h"

namespace safe_browsing {

class IncidentReceiver;

// A callback used by external components to register a process-wide analysis
// step. The callback will be run after some delay following process launch in
// the blocking pool. The argument is a receiver by which the consumer can add
// incidents to the incident reporting service.
typedef base::OnceCallback<void(std::unique_ptr<IncidentReceiver>)>
    DelayedAnalysisCallback;

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_ANALYSIS_CALLBACK_H_
