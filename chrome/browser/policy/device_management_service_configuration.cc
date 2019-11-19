// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_service_configuration.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/version_info/version_info.h"

#if defined(OS_CHROMEOS)
#include "chromeos/system/statistics_provider.h"
#endif

namespace policy {

DeviceManagementServiceConfiguration::DeviceManagementServiceConfiguration(
    const std::string& server_url,
    const std::string& reporting_server_url)
    : server_url_(server_url), reporting_server_url_(reporting_server_url) {}

DeviceManagementServiceConfiguration::~DeviceManagementServiceConfiguration() {
}

std::string DeviceManagementServiceConfiguration::GetDMServerUrl() {
  return server_url_;
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

#if defined(OS_CHROMEOS)
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
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
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

std::string DeviceManagementServiceConfiguration::GetReportingServerUrl() {
  return reporting_server_url_;
}

}  // namespace policy
