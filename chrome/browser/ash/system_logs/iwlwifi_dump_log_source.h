// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

constexpr char kIwlwifiDumpKey[] = "iwlwifi_dump";

// The classes here are used to attach debug dump information from
// Intel Wi-Fi NICs that will be produced when those NICs have issues
// such as firmware crashes. This information will be used to help
// diagnose Wi-Fi issues.

// This logs source is used to check for the existence of the Wi-Fi
// debug dump. It will place an explainer string in the system logs
// map if it finds the dump.
class IwlwifiDumpChecker : public SystemLogsSource {
 public:
  IwlwifiDumpChecker();

  IwlwifiDumpChecker(const IwlwifiDumpChecker&) = delete;
  IwlwifiDumpChecker& operator=(const IwlwifiDumpChecker&) = delete;

  ~IwlwifiDumpChecker() override;

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;
};

// Fetches information from the /var/log/last_iwlwifi_dump file, if
// the explainer string is present in the passed-in logs map.
class IwlwifiDumpLogSource : public SystemLogsSource {
 public:
  IwlwifiDumpLogSource();

  IwlwifiDumpLogSource(const IwlwifiDumpLogSource&) = delete;
  IwlwifiDumpLogSource& operator=(const IwlwifiDumpLogSource&) = delete;

  ~IwlwifiDumpLogSource() override;

  // system_logs::SystemLogsSource:
  void Fetch(SysLogsSourceCallback callback) override;
};

// Checks to see if |sys_logs| contains the iwlwifi logs key.
bool ContainsIwlwifiLogs(const FeedbackCommon::SystemLogsMap* sys_logs);

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_IWLWIFI_DUMP_LOG_SOURCE_H_
