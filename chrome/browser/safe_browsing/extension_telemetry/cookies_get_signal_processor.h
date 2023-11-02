// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

using GetArgsInfo =
    ExtensionTelemetryReportRequest::SignalInfo::CookiesGetInfo::GetArgsInfo;

// A class that processes cookies.get signal data to generate telemetry
// reports.
class CookiesGetSignalProcessor : public ExtensionSignalProcessor {
 public:
  CookiesGetSignalProcessor();
  ~CookiesGetSignalProcessor() override;

  CookiesGetSignalProcessor(CookiesGetSignalProcessor&) = delete;
  CookiesGetSignalProcessor& operator=(const CookiesGetSignalProcessor&) =
      delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxArgSetsForTest(size_t max_arg_sets);

 protected:
  // Max number of API arguments stored per extension.
  size_t max_arg_sets_;

  // Maps concated string of arguments to GetArgsInfos.
  using GetArgsInfos = base::flat_map<std::string, GetArgsInfo>;

  // Stores get() arguments. If |max_arg_sets_| exceeded, arguments will not
  // be recorded.
  struct CookiesGetStoreEntry {
    CookiesGetStoreEntry();
    ~CookiesGetStoreEntry();
    CookiesGetStoreEntry(const CookiesGetStoreEntry&);

    GetArgsInfos get_args_infos;
    // Records count of new unique arg sets after |max_arg_sets_| limit is
    // reached.
    size_t max_exceeded_arg_sets_count = 0;
  };
  // Records how many unique arg sets were not recorded because |max_arg_sets_|
  // limit was exceeded.
  using CookiesGetStore =
      base::flat_map<extensions::ExtensionId, CookiesGetStoreEntry>;
  CookiesGetStore cookies_get_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_
