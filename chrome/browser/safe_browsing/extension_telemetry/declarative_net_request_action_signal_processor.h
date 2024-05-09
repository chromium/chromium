// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

using ActionDetails = ExtensionTelemetryReportRequest::SignalInfo::
    DeclarativeNetRequestActionInfo::ActionDetails;

// A class that processes declarativeNetRequest action signal data to generate
// telemetry reports.
class DeclarativeNetRequestActionSignalProcessor
    : public ExtensionSignalProcessor {
 public:
  DeclarativeNetRequestActionSignalProcessor();
  ~DeclarativeNetRequestActionSignalProcessor() override;

  DeclarativeNetRequestActionSignalProcessor(
      DeclarativeNetRequestActionSignalProcessor&) = delete;
  DeclarativeNetRequestActionSignalProcessor& operator=(
      const DeclarativeNetRequestActionSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxUniqueActionDetailsForTest(size_t max_unique_action_details);

 protected:
  // Max number of unique action details stored per extension.
  // The signal processor only stores up to this number of unique action
  // details.
  size_t max_unique_action_details_;

  // Stores a map of unique action details. The key used is a string
  // concatenation of the action details data stored.
  using ActionDetailsMap = base::flat_map<std::string, ActionDetails>;

  // Stores chrome.tabs API signal info for a single extension. If
  // `max_unique_call_details_` is exceeded, no more call details will be
  // recorded.
  struct DeclarativeNetRequestActionInfoStoreEntry {
    DeclarativeNetRequestActionInfoStoreEntry();
    ~DeclarativeNetRequestActionInfoStoreEntry();
    DeclarativeNetRequestActionInfoStoreEntry(
        const DeclarativeNetRequestActionInfoStoreEntry&);

    ActionDetailsMap action_details_map;
  };

  // Stores declarativeNetRequest action signal for multiple extensions, keyed
  // by extension id.
  using DeclarativeNetRequestActionInfoStore =
      base::flat_map<extensions::ExtensionId,
                     DeclarativeNetRequestActionInfoStoreEntry>;
  DeclarativeNetRequestActionInfoStore
      declarative_net_request_action_info_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_PROCESSOR_H_
