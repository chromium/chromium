// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

// A class that processes DOM access signal data to generate telemetry reports.
class DOMAccessSignalProcessor : public ExtensionSignalProcessor {
 public:
  DOMAccessSignalProcessor();
  ~DOMAccessSignalProcessor() override;

  DOMAccessSignalProcessor(DOMAccessSignalProcessor&) = delete;
  DOMAccessSignalProcessor& operator=(const DOMAccessSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxAggregatedSignalsForTest(size_t max_aggregated_signals);

 protected:
  // Max number of unique signals stored per extension.
  size_t max_aggregated_signals_;

  struct DOMAccessData {
    DOMAccessData();
    ~DOMAccessData();
    DOMAccessData(const DOMAccessData&);

    std::string api_name;
    std::string url;
    ExtensionTelemetryReportRequest::SignalInfo::DOMAccessInfo::DOMAccess::
        AccessType access_type;
    base::Time last_timestamp;
    uint32_t count = 0;
  };

  // Stores a map of unique DOM access signals, keyed by their aggregation key.
  using DOMAccessDataMap = base::flat_map<std::string, DOMAccessData>;

  struct DOMAccessStoreEntry {
    DOMAccessStoreEntry();
    ~DOMAccessStoreEntry();
    DOMAccessStoreEntry(const DOMAccessStoreEntry&);

    DOMAccessDataMap dom_access_data_map;
  };

  using DOMAccessStore =
      base::flat_map<extensions::ExtensionId, DOMAccessStoreEntry>;
  DOMAccessStore dom_access_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_PROCESSOR_H_
