// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_SINGLE_DEBUG_DAEMON_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_SINGLE_DEBUG_DAEMON_LOG_SOURCE_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Gathers log data from a single debugd log source, via DebugDaemonClient.
class SingleDebugDaemonLogSource : public SystemLogsSource {
 public:
  enum class SupportedSource {
    // For "modetest" command.
    kModetest,

    // For "lsusb" command.
    kLsusb,

    // For "lspci" command.
    kLspci,

    // For "ifconfig" command.
    kIfconfig,

    // For "/proc/uptime" entry.
    kUptime,
  };

  explicit SingleDebugDaemonLogSource(SupportedSource source);

  SingleDebugDaemonLogSource(const SingleDebugDaemonLogSource&) = delete;
  SingleDebugDaemonLogSource& operator=(const SingleDebugDaemonLogSource&) =
      delete;

  ~SingleDebugDaemonLogSource() override;

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;

 private:
  // Callback for handling response from DebugDaemonClient.
  void OnFetchComplete(const std::string& log_name,
                       SysLogsSourceCallback callback,
                       std::optional<std::string> result) const;

  base::WeakPtrFactory<SingleDebugDaemonLogSource> weak_ptr_factory_{this};
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_SINGLE_DEBUG_DAEMON_LOG_SOURCE_H_
