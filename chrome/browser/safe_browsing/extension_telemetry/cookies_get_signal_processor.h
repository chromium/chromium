// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_js_callstacks.h"
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

  struct CallData {
    CallData();
    ~CallData();
    CallData(const CallData&);

    // Arguments passed to the API call.
    GetArgsInfo args_info;
    // Multiple JS callstacks. Each stack represents an API
    // invocation  with the same `args_info` arguments.
    ExtensionJSCallStacks js_callstacks;
  };
  // Stores a map of API calls with unique argument sets.
  // The key used is a string concatenation of the call arguments.
  using CallDataMap = base::flat_map<std::string, CallData>;

  // Stores get() arguments. If `max_arg_sets_` is exceeded, arguments will not
  // be recorded.
  struct CookiesGetStoreEntry {
    CookiesGetStoreEntry();
    ~CookiesGetStoreEntry();
    CookiesGetStoreEntry(const CookiesGetStoreEntry&);

    CallDataMap call_data_map;
    // Records how many unique arg sets were ignored because the
    // `max_arg_sets_` limit was exceeded.
    size_t max_exceeded_arg_sets_count = 0;
  };
  using CookiesGetStore =
      base::flat_map<extensions::ExtensionId, CookiesGetStoreEntry>;
  CookiesGetStore cookies_get_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_SIGNAL_PROCESSOR_H_
