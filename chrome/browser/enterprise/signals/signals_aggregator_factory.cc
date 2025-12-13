// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/signals/profile_signals_collector.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/file_system_signals_collector.h"
#include "components/device_signals/core/browser/settings_signals_collector.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/signals_aggregator_impl.h"
#include "components/device_signals/core/browser/signals_collector.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/device_signals/core/browser/android/android_os_signals_collector.h"
#else
#include "chrome/browser/enterprise/signals/system_signals_service_host_factory.h"
#include "components/device_signals/core/browser/desktop/desktop_os_signals_collector.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "components/device_signals/core/browser/mac/plist_settings_client.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "components/device_signals/core/browser/win/registry_settings_client.h"
#include "components/device_signals/core/browser/win/win_signals_collector.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "components/device_signals/core/browser/settings_client.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/agent_signals_collector.h"
#include "components/device_signals/core/browser/crowdstrike_client.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace enterprise_signals {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
std::unique_ptr<device_signals::SettingsClient> CreateSettingsClient() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<device_signals::RegistrySettingsClient>();
#else
  return std::make_unique<device_signals::PlistSettingsClient>();
#endif  // BUILDFLAG(IS_WIN)
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

// static
SignalsAggregatorFactory* SignalsAggregatorFactory::GetInstance() {
  static base::NoDestructor<SignalsAggregatorFactory> instance;
  return instance.get();
}

// static
device_signals::SignalsAggregator* SignalsAggregatorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<device_signals::SignalsAggregator*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

SignalsAggregatorFactory::SignalsAggregatorFactory()
    : ProfileKeyedServiceFactory(
          "SignalsAggregator",
          ProfileSelections::BuildForRegularAndIncognito()) {
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(SystemSignalsServiceHostFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
  DependsOn(UserPermissionServiceFactory::GetInstance());
  DependsOn(enterprise::ProfileIdServiceFactory::GetInstance());
}

SignalsAggregatorFactory::~SignalsAggregatorFactory() = default;

std::unique_ptr<KeyedService>
SignalsAggregatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  auto* user_permission_service =
      UserPermissionServiceFactory::GetForProfile(profile);
  if (!user_permission_service) {
    // Unsupported configuration (e.g. CrOS login Profile supported, but not
    // incognito).
    return nullptr;
  }

  std::vector<std::unique_ptr<device_signals::SignalsCollector>> collectors;
#if !BUILDFLAG(IS_ANDROID)
  auto* service_host = SystemSignalsServiceHostFactory::GetForProfile(profile);
  collectors.push_back(
      std::make_unique<device_signals::FileSystemSignalsCollector>(
          service_host));
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  collectors.push_back(std::make_unique<device_signals::AgentSignalsCollector>(
      device_signals::CrowdStrikeClient::Create()));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  collectors.push_back(
      std::make_unique<device_signals::SettingsSignalsCollector>(
          CreateSettingsClient()));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
  collectors.push_back(
      std::make_unique<device_signals::WinSignalsCollector>(service_host));
#endif  // BUILDFLAG(IS_WIN)

  auto* management_service =
      policy::ManagementServiceFactory::GetForProfile(profile);

  policy::CloudPolicyManager* browser_policy_manager = nullptr;

  if (management_service->HasManagementAuthority(
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN)) {
    auto* browser_policy_connector =
        g_browser_process->browser_policy_connector();
    if (browser_policy_connector) {
      browser_policy_manager =
          browser_policy_connector->machine_level_user_cloud_policy_manager();
    }
  }

#if BUILDFLAG(IS_ANDROID)
  collectors.push_back(
      std::make_unique<device_signals::AndroidOsSignalsCollector>(
          browser_policy_manager));
#else
  collectors.push_back(
      std::make_unique<device_signals::DesktopOsSignalsCollector>(
          browser_policy_manager));
#endif  // BUILDFLAG(IS_ANDROID)
  collectors.push_back(
      std::make_unique<device_signals::ProfileSignalsCollector>(profile));

  return std::make_unique<device_signals::SignalsAggregatorImpl>(
      user_permission_service, std::move(collectors));
}

}  // namespace enterprise_signals
