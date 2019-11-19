// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/feedback/system_logs/system_logs_source.h"

#if defined(OS_CHROMEOS)
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace system_logs {

// Fetches internal Chrome logs.
class ChromeInternalLogSource : public SystemLogsSource {
 public:
  ChromeInternalLogSource();
  ~ChromeInternalLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void PopulateSyncLogs(SystemLogsResponse* response);
  void PopulateExtensionInfoLogs(SystemLogsResponse* response);
  void PopulatePowerApiLogs(SystemLogsResponse* response);
  void PopulateDataReductionProxyLogs(SystemLogsResponse* response);

#if defined(OS_CHROMEOS)
  void PopulateLocalStateSettings(SystemLogsResponse* response);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
  void PopulateUsbKeyboardDetected(SystemLogsResponse* response);
  void PopulateEnrolledToDomain(SystemLogsResponse* response);
  void PopulateInstallerBrandCode(SystemLogsResponse* response);
  void PopulateLastUpdateState(SystemLogsResponse* response);
#endif

#if defined(OS_CHROMEOS)
  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeInternalLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
