// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service_configuration.h"

#include <stdint.h>

#include <string_view>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/system/statistics_provider.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||           \
    ((BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
     !BUILDFLAG(IS_ANDROID))
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

std::string DeviceManagementServiceConfiguration::GetDMServerUrl() const {
  return dm_server_url_;
}

std::string DeviceManagementServiceConfiguration::GetAgentParameter() const {
  return base::StrCat({version_info::GetProductName(), " ",
                       version_info::GetVersionNumber(), "(",
                       version_info::GetLastChange(), ")"});
}

std::string DeviceManagementServiceConfiguration::GetPlatformParameter() const {
  std::string os_name = base::SysInfo::OperatingSystemName();
  std::string os_hardware = base::SysInfo::OperatingSystemArchitecture();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();

  const std::optional<std::string_view> hwclass =
      provider->GetMachineStatistic(ash::system::kHardwareClassKey);
  if (!hwclass) {
    LOG(ERROR) << "Failed to get machine information";
  }
  os_name += ",CrOS," + base::SysInfo::GetLsbReleaseBoard();
  os_hardware += "," + std::string(hwclass.value_or(""));
#endif

  std::string os_version("-");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
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
DeviceManagementServiceConfiguration::GetRealtimeReportingServerUrl() const {
  return realtime_reporting_server_url_;
}

std::string
DeviceManagementServiceConfiguration::GetEncryptedReportingServerUrl() const {
  return encrypted_reporting_server_url_;
}

}  // namespace policy
