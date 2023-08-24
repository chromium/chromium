// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_API_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_API_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

using TabsApiCallDetails =
    ExtensionTelemetryReportRequest::SignalInfo::TabsApiInfo::CallDetails;

// A class that processes chrome.tabs API signal data to generate telemetry
// reports.
class TabsApiSignalProcessor : public ExtensionSignalProcessor {
 public:
  TabsApiSignalProcessor();
  ~TabsApiSignalProcessor() override;

  TabsApiSignalProcessor(TabsApiSignalProcessor&) = delete;
  TabsApiSignalProcessor& operator=(const TabsApiSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxUniqueCallDetailsForTest(size_t max_unique_call_details);

 protected:
  // Max number of unique API call details stored per extension.
  // The signal processor only stores up to this number of unique API call
  // details.
  size_t max_unique_call_details_;

  // Stores a map of unique API call details. The key used is a string
  // concatenation of the call arguments stored.
  using TabsApiCallDetailsMap = base::flat_map<std::string, TabsApiCallDetails>;

  // Stores chrome.tabs API signal info for a single extension. If
  // `max_unique_call_details_` is exceeded, no more call details will be
  // recorded.
  struct TabsApiInfoStoreEntry {
    TabsApiInfoStoreEntry();
    ~TabsApiInfoStoreEntry();
    TabsApiInfoStoreEntry(const TabsApiInfoStoreEntry&);

    TabsApiCallDetailsMap tabs_api_call_details_map;
  };

  // Stores chrome.tabs API signal for multiple extensions, keyed by extension
  // id.
  using TabsApiInfoStore =
      base::flat_map<extensions::ExtensionId, TabsApiInfoStoreEntry>;
  TabsApiInfoStore tabs_api_info_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_API_SIGNAL_PROCESSOR_H_
