// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_PROCESSOR_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

// A class that processes declarativeNetRequest API signal data to generate
// telemetry reports.
class DeclarativeNetRequestSignalProcessor : public ExtensionSignalProcessor {
 public:
  DeclarativeNetRequestSignalProcessor();
  ~DeclarativeNetRequestSignalProcessor() override;

  DeclarativeNetRequestSignalProcessor(DeclarativeNetRequestSignalProcessor&) =
      delete;
  DeclarativeNetRequestSignalProcessor& operator=(
      const DeclarativeNetRequestSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxRulesForTest(size_t max_rules);

 protected:
  // Max number of rules stored per extension.
  size_t max_rules_;

  // Stores declarativeNetRequest rules, and the number of rules not recorded
  // after `max_rules_` limit is reached.
  struct DeclarativeNetRequestStoreEntry {
    DeclarativeNetRequestStoreEntry();
    ~DeclarativeNetRequestStoreEntry();
    DeclarativeNetRequestStoreEntry(const DeclarativeNetRequestStoreEntry&);

    base::flat_set<std::string> rules;
    size_t max_exceeded_rules_count = 0;
  };

  using DeclarativeNetRequestStore =
      base::flat_map<extensions::ExtensionId, DeclarativeNetRequestStoreEntry>;
  DeclarativeNetRequestStore dnr_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_PROCESSOR_H_
