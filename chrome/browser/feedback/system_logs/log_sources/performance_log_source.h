// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_PERFORMANCE_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_PERFORMANCE_LOG_SOURCE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/performance_manager/public/user_tuning/battery_saver_mode_manager.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches memory usage details.
class PerformanceLogSource : public SystemLogsSource {
 public:
  PerformanceLogSource();

  PerformanceLogSource(const PerformanceLogSource&) = delete;
  PerformanceLogSource& operator=(const PerformanceLogSource&) = delete;

  ~PerformanceLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void PopulatePerformanceSettingLogs(SystemLogsResponse* response);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Battery and battery saver logs are not used on ChromeOS.
  void PopulateBatteryDetailLogs(SystemLogsResponse* response);
#endif

  raw_ptr<performance_manager::user_tuning::BatterySaverModeManager>
      battery_saver_mode_manager_ = nullptr;
  raw_ptr<performance_manager::user_tuning::UserPerformanceTuningManager>
      tuning_manager_ = nullptr;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_PERFORMANCE_LOG_SOURCE_H_
