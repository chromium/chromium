// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
#define CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_

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
  bool ShouldShowRestartDialog(std::wstring* title,
                               std::wstring* message,
                               bool* is_rtl_locale) override;
  bool AboutToRestart() override;
  bool GetIsPerUserInstall() override;
  bool GetShouldDumpLargerDumps() override;
  int GetResultCodeRespawnFailed() override;

  bool GetCrashDumpLocation(std::wstring* crash_dir) override;
  bool GetCrashMetricsLocation(std::wstring* metrics_dir) override;

  bool IsRunningUnattended() override;

  bool GetCollectStatsConsent() override;

  bool GetCollectStatsInSample() override;

  bool ReportingIsEnforcedByPolicy(bool* breakpad_enabled) override;

  bool ShouldMonitorCrashHandlerExpensively() override;

  bool EnableBreakpadForProcess(const std::string& process_type) override;

  std::wstring GetWerRuntimeExceptionModule() override;
};

#endif  // CHROME_APP_CHROME_CRASH_REPORTER_CLIENT_WIN_H_
