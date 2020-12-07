// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service_configuration.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/system/statistics_provider.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && !defined(OS_ANDROID))
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#endif

namespace policy {

DeviceManagementServiceConfiguration::DeviceManagementServiceConfiguration(
    const std::string& dm_server_url,
    const std::string& realtime_reporting_server_url,
    const std::string& encrypted_reporting_server_url)
    : dm_server_url_(dm_server_url),
      realtime_reporting_server_url_(realtime_reporting_server_url),
      encrypted_reporting_server_url_(encrypted_reporting_server_url) {}

DeviceManagementServiceConfiguration::~DeviceManagementServiceConfiguration() {
}

std::string DeviceManagementServiceConfiguration::GetDMServerUrl() {
  return dm_server_url_;
}

std::string DeviceManagementServiceConfiguration::GetAgentParameter() {
  return base::StringPrintf("%s %s(%s)",
                            version_info::GetProductName().c_str(),
                            version_info::GetVersionNumber().c_str(),
                            version_info::GetLastChange().c_str());
}

std::string DeviceManagementServiceConfiguration::GetPlatformParameter() {
  std::string os_name = base::SysInfo::OperatingSystemName();
  std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();

  std::string hwclass;
  if (!provider->GetMachineStatistic(chromeos::system::kHardwareClassKey,
                                     &hwclass)) {
    LOG(ERROR) << "Failed to get machine information";
  }
  os_name += ",CrOS," + base::SysInfo::GetLsbReleaseBoard();
  os_hardware += "," + hwclass;
#endif

  std::string os_version("-");
#if defined(OS_WIN) || defined(OS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(&os_major_version,
                                               &os_minor_version,
                                               &os_bugfix_version);
  os_version = base::StringPrintf("%d.%d.%d",
                                  os_major_version,
                                  os_minor_version,
                                  os_bugfix_version);
#endif

  return base::StringPrintf(
      "%s|%s|%s", os_name.c_str(), os_hardware.c_str(), os_version.c_str());
}

std::string
DeviceManagementServiceConfiguration::GetRealtimeReportingServerUrl() {
  return realtime_reporting_server_url_;
}

std::string
DeviceManagementServiceConfiguration::GetEncryptedReportingServerUrl() {
  return encrypted_reporting_server_url_;
}

std::string
DeviceManagementServiceConfiguration::GetReportingConnectorServerUrl(
    content::BrowserContext* context) {
#if defined(OS_WIN) || defined(OS_MAC) || \
    ((defined(OS_LINUX) || defined(OS_CHROMEOS)) && !defined(OS_ANDROID))
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          context);
  if (!service)
    return std::string();

  auto settings = service->GetReportingSettings(
      enterprise_connectors::ReportingConnector::SECURITY_EVENT);
  return settings ? settings->reporting_url.spec() : std::string();
#else
  return std::string();
#endif
}

}  // namespace policy
