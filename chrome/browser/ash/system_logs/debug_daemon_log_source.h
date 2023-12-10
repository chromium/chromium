// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_DAEMON_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_DAEMON_LOG_SOURCE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Reads the file at |path| into |contents| and returns true on success and
// false on error. For security reasons a |path| containing path traversal
// components ('..') is treated as a read error and |contents| is not changed.
// When the file exceeds |max_size| function returns true with the last
// |max_size| bytes from the file.
bool ReadEndOfFile(const base::FilePath& path,
                   std::string* contents,
                   size_t max_size);

// Exposes the utility methods only for unittests.
#if defined(UNIT_TEST)
std::string ReadUserLogFile(const base::FilePath& log_file_path);
std::string ReadUserLogFilePattern(const base::FilePath& log_file_path_pattern);
#endif  // defined(UNIT_TEST)

// Gathers log data from Debug Daemon.
class DebugDaemonLogSource : public SystemLogsSource {
 public:
  explicit DebugDaemonLogSource(bool scrub);

  DebugDaemonLogSource(const DebugDaemonLogSource&) = delete;
  DebugDaemonLogSource& operator=(const DebugDaemonLogSource&) = delete;

  ~DebugDaemonLogSource() override;

  // SystemLogsSource override:
  // Fetches logs from the daemon over dbus. After the fetch is complete, the
  // results will be forwarded to the request supplied to the constructor and
  // this instance will free itself.
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  typedef std::map<std::string, std::string> KeyValueMap;

  // Callbacks for the dbus calls to debugd.
  void OnGetRoutes(bool is_ipv6,
                   std::optional<std::vector<std::string>> routes);
  void OnGetOneLog(std::string key, std::optional<std::string> status);
  void OnGetLogs(const base::TimeTicks get_start_time,
                 bool succeeded,
                 const KeyValueMap& logs);

  // Reads the logged-in users' log files that have to be read by Chrome as
  // debugd has no access to them. The contents of these logs are appended to
  // |response_|. This is called at the end when all debugd logs are collected
  // so that we can see any debugd related errors surface in feedback reports.
  void GetLoggedInUsersLogFiles();

  void OnGetUserLogFiles(bool succeeded,
                         const KeyValueMap& logs);

  // Merge the responses from ReadUserLogFiles into the main response dict and
  // invoke the callback_.Run method with the assumption that all other logs
  // have already been collected.
  void MergeUserLogFilesResponse(std::unique_ptr<SystemLogsResponse> response);

  // When all the requests are completed, send one last request to collect the
  // user logs and complete the collection by invoking the callback's Run
  // method.
  void RequestCompleted();

  std::unique_ptr<SystemLogsResponse> response_;
  SysLogsSourceCallback callback_;
  int num_pending_requests_;
  bool scrub_;
  base::WeakPtrFactory<DebugDaemonLogSource> weak_ptr_factory_{this};
};


}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_DAEMON_LOG_SOURCE_H_
