// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_source.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace system_logs {

// Fetches internal Chrome logs.
class ChromeInternalLogSource : public SystemLogsSource {
 public:
  ChromeInternalLogSource();

  ChromeInternalLogSource(const ChromeInternalLogSource&) = delete;
  ChromeInternalLogSource& operator=(const ChromeInternalLogSource&) = delete;

  ~ChromeInternalLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  void PopulateSyncLogs(SystemLogsResponse* response);
  void PopulateExtensionInfoLogs(SystemLogsResponse* response);
  void PopulatePowerApiLogs(SystemLogsResponse* response);
  void PopulateDataReductionProxyLogs(SystemLogsResponse* response);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void PopulateLocalStateSettings(SystemLogsResponse* response);
  void PopulateArcPolicyStatus(SystemLogsResponse* response);
  void PopulateOnboardingTime(SystemLogsResponse* response);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
  void PopulateUsbKeyboardDetected(SystemLogsResponse* response);
  void PopulateEnrolledToDomain(SystemLogsResponse* response);
  void PopulateInstallerBrandCode(SystemLogsResponse* response);
  void PopulateLastUpdateState(SystemLogsResponse* response);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController>
      cros_display_config_;
#endif
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_CHROME_INTERNAL_LOG_SOURCE_H_
