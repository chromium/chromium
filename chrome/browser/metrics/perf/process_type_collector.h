// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_PROCESS_TYPE_COLLECTOR_H_
#define CHROME_BROWSER_METRICS_PERF_PROCESS_TYPE_COLLECTOR_H_

#include <map>
#include <string_view>
#include <vector>

#include "third_party/metrics_proto/execution_context.pb.h"

namespace metrics {
// Enables collection of process and thread types for Chrome PIDs and TIDs.
class ProcessTypeCollector {
 public:
  // NOTE: ChromeProcessTypes and ChromeThreadTypes methods make blocking call
  // to base::GetAppOutput. Callers, who are calling these methods from a scope
  // that disallows blocking, should post a task with MayBlock() task trait to
  // execute these methods or make sure to call these methods asynchronously.
  // Collects process types by running ps command and returns a map of Chrome
  // PIDs to their process types. It also populates a list of Lacros PID's and
  // the Lacros binary path, if any.
  static std::map<uint32_t, Process> ChromeProcessTypes(
      std::vector<uint32_t>& lacros_pids,
      std::string& lacros_path);

  // Collects thread types by running ps command and returns a map of Chrome
  // TIDs to their thread types.
  static std::map<uint32_t, Thread> ChromeThreadTypes();

 protected:
  ProcessTypeCollector() = delete;
  ~ProcessTypeCollector() = delete;
  // Parses the output of `ps -ewwo pid,cmd` command and returns a map of Chrome
  // PIDs to their process types. It also populates a list of Lacros PID's and
  // the Lacros binary path, if any.
  static std::map<uint32_t, Process> ParseProcessTypes(
      std::string_view contents,
      std::vector<uint32_t>& lacros_pids,
      std::string& lacros_path);

  // Parses the output of `ps -ewLo pid,lwp,comm` command and returns a map of
  // Chrome TIDs to their thread types.
  static std::map<uint32_t, Thread> ParseThreadTypes(std::string_view contents);

  // Enumeration representing success and various failure modes for collecting
  // types data. These values are persisted to logs. Entries should not be
  // renumbered and numeric values should never be reused.
  enum class CollectionAttemptStatus {
    kProcessTypeCmdError,
    kThreadTypeCmdError,
    kEmptyProcessType,
    kEmptyThreadType,
    kProcessTypeTruncated,
    kThreadTypeTruncated,
    kProcessTypeSuccess,
    kThreadTypeSuccess,
    // Magic constant used by the histogram macros.
    kMaxValue = kThreadTypeSuccess,
  };
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_PROCESS_TYPE_COLLECTOR_H_
