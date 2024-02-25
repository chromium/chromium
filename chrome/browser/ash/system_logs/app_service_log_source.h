// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_APP_SERVICE_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_APP_SERVICE_LOG_SOURCE_H_

#include "base/values.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Gathers information from app service about installed and running apps.
class AppServiceLogSource : public SystemLogsSource {
 public:
  AppServiceLogSource();
  AppServiceLogSource(const AppServiceLogSource&) = delete;
  AppServiceLogSource& operator=(const AppServiceLogSource&) = delete;
  ~AppServiceLogSource() override;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_APP_SERVICE_LOG_SOURCE_H_
