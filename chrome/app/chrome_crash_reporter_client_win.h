// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/app/chrome_crash_user_streams.h"
#include "components/crash/core/app/crash_reporter_client.h"

class ChromeCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  // Instantiates a process wide instance of the ChromeCrashReporterClient
  // class and initializes crash reporting for the process. The instance is
  // leaked.
  static void InitializeCrashReportingForProcess();

  ChromeCrashReporterClient();

  ChromeCrashReporterClient(const ChromeCrashReporterClient&) = delete;
  ChromeCrashReporterClient& operator=(const ChromeCrashReporterClient&) =
      delete;

  ~ChromeCrashReporterClient() override;

  // crash_reporter::CrashReporterClient implementation.
  bool GetAlternativeCrashDumpLocation(std::wstring* crash_dir) override;
  void GetProductNameAndVersion(const std::wstring& exe_path,
                                std::wstring* product_name,
                                std::wstring* version,
                                std::wstring* special_build,
                                std::wstring* channel_name) override;
  void GetProductInfo(ProductInfo* product_info) override;
  bool GetShouldDumpLargerDumps() override;

  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
  bool GetCrashMetricsLocation(std::wstring* metrics_dir) override;

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

  bool GetCollectStatsInSample() override;

  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;

  bool ShouldMonitorCrashHandlerExpensively() override;

  bool EnableBreakpadForProcess(const std::string& process_type) override;

  std::vector<base::ReadOnlySharedMemoryRegion>
  GetUserStreamSharedMemoryRegions() override;

  bool WriteUserStreamData(uint32_t user_stream_type,
                           base::span<const uint8_t> data) override;

  std::wstring GetWerRuntimeExceptionModule() override;

 private:
  // Created lazily, since only the browser process uses it.
  std::optional<ChromeCrashUserStreams> user_streams_;
};

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
