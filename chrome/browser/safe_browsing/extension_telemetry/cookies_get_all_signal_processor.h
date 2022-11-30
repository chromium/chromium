// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

using GetAllArgsInfo = ExtensionTelemetryReportRequest::SignalInfo::
    CookiesGetAllInfo::GetAllArgsInfo;

// A class that processes cookies.getAll signal data to generate telemetry
// reports.
class CookiesGetAllSignalProcessor : public ExtensionSignalProcessor {
 public:
  CookiesGetAllSignalProcessor();
  ~CookiesGetAllSignalProcessor() override;

  CookiesGetAllSignalProcessor(CookiesGetAllSignalProcessor&) = delete;
  CookiesGetAllSignalProcessor& operator=(const CookiesGetAllSignalProcessor&) =
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

  // Maps concated string of arguments to GetAllArgsInfos.
  using GetAllArgsInfos = base::flat_map<std::string, GetAllArgsInfo>;

  // Stores getAll() arguments. If |max_arg_sets_| exceeded, arguments will not
  // be recorded.
  struct CookiesGetAllStoreEntry {
    CookiesGetAllStoreEntry();
    ~CookiesGetAllStoreEntry();
    CookiesGetAllStoreEntry(const CookiesGetAllStoreEntry&);

    GetAllArgsInfos get_all_args_infos;
    // Records count of new unique arg sets after |max_arg_sets_| limit is
    // reached.
    size_t max_exceeded_arg_sets_count = 0;
  };
  // Records how many unique arg sets were not recorded because |max_arg_sets_|
  // limit was exceeded.
  using CookiesGetAllStore =
      base::flat_map<extensions::ExtensionId, CookiesGetAllStoreEntry>;
  CookiesGetAllStore cookies_get_all_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_PROCESSOR_H_
