// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_PROCESSOR_H_

#include <memory>

#include "extensions/common/extension_id.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

// An abstract signal processing class. Subclasses provide type-specific
// functionality to process the signal for telemetry reports.
class ExtensionSignalProcessor {
 public:
  virtual ~ExtensionSignalProcessor() = default;

  // Processes (ie. aggregates/derives/adds additional data) the signal and
  // updates internal state & data.
  virtual void ProcessSignal(const ExtensionSignal& signal) = 0;

  // Returns all signal data collected for the specified extension.
  // Also clears out the existing data.
  virtual std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) = 0;

  // Determines if there is any processed signal data present.
  virtual bool HasDataToReportForTest() const = 0;

 protected:
  ExtensionSignalProcessor() = default;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_SIGNAL_PROCESSOR_H_
