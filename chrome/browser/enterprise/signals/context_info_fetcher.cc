// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/context_info_fetcher.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/device_signals/core/browser/browser_utils.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/version_info/version_info.h"
#include "device_management_backend.pb.h"


#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/constants/dbus_switches.h"
#endif

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#endif

namespace enterprise_signals {

using SettingValue = device_signals::SettingValue;

namespace {

std::optional<std::string> GetEnterpriseProfileId(Profile* profile) {
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactory::GetForProfile(profile);
  if (profile_id_service)
    return profile_id_service->GetProfileId();
  return std::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS)
SettingValue GetChromeosFirewall() {
  // The firewall is always enabled and can only be disabled in dev mode on
  // ChromeOS. If the device isn't in dev mode, the firewall is guaranteed to be
  // enabled whereas if it's in dev mode, the firewall could be enabled or not.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(chromeos::switches::kSystemDevMode)
             ? SettingValue::UNKNOWN
             : SettingValue::ENABLED;
}
#endif

bool GetBuiltInDnsClientEnabled(PrefService* local_state) {
  DCHECK(local_state);
  return local_state->GetBoolean(prefs::kBuiltInDnsClientEnabled);
}

}  // namespace

ContextInfo::ContextInfo() = default;
ContextInfo::ContextInfo(ContextInfo&&) = default;
ContextInfo::~ContextInfo() = default;

ContextInfoFetcher::ContextInfoFetcher(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service)
    : browser_context_(browser_context),
      connectors_service_(connectors_service) {
  DCHECK(connectors_service_);
}

ContextInfoFetcher::~ContextInfoFetcher() = default;

// static
std::unique_ptr<ContextInfoFetcher> ContextInfoFetcher::CreateInstance(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service) {
  return std::make_unique<ContextInfoFetcher>(browser_context,
                                              connectors_service);
}

ContextInfo ContextInfoFetcher::FetchAsyncSignals(ContextInfo info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Add other async signals here
  info.system_dns_servers = GetDnsServers();
  info.os_firewall = GetOSFirewall();
  return info;
}

void ContextInfoFetcher::Fetch(ContextInfoCallback callback) {
  ContextInfo info;

  info.browser_affiliation_ids = GetBrowserAffiliationIDs();
  info.profile_affiliation_ids = GetProfileAffiliationIDs();
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  info.on_file_attached_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_ATTACHED);
  info.on_file_downloaded_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_DOWNLOADED);
  info.on_bulk_data_entry_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::BULK_DATA_ENTRY);
  info.on_print_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::PRINT);
  info.realtime_url_check_mode = GetRealtimeUrlCheckMode();
  info.on_security_event_providers = GetOnSecurityEventProviders();
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  info.browser_version = version_info::GetVersionNumber();
  info.site_isolation_enabled = device_signals::GetSiteIsolationEnabled();
  info.built_in_dns_client_enabled =
      GetBuiltInDnsClientEnabled(g_browser_process->local_state());
  info.chrome_remote_desktop_app_blocked =
      device_signals::GetChromeRemoteDesktopAppBlocked(
          PolicyBlocklistFactory::GetForBrowserContext(browser_context_));

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  info.safe_browsing_protection_level =
      device_signals::GetSafeBrowsingProtectionLevel(profile->GetPrefs());
  info.password_protection_warning_trigger =
      device_signals::GetPasswordProtectionWarningTrigger(profile->GetPrefs());
  info.enterprise_profile_id = GetEnterpriseProfileId(profile);

#if BUILDFLAG(IS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})
      .get()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ContextInfoFetcher::FetchAsyncSignals,
                         base::Unretained(this), std::move(info)),
          std::move(callback));
#else
  base::ThreadPool::CreateTaskRunner({base::MayBlock()})
      .get()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ContextInfoFetcher::FetchAsyncSignals,
                         base::Unretained(this), std::move(info)),
          std::move(callback));
#endif
}

std::vector<std::string> ContextInfoFetcher::GetBrowserAffiliationIDs() {
  auto ids =
      g_browser_process->browser_policy_connector()->device_affiliation_ids();
  return {ids.begin(), ids.end()};
}

std::vector<std::string> ContextInfoFetcher::GetProfileAffiliationIDs() {
  auto ids = Profile::FromBrowserContext(browser_context_)
                 ->GetProfilePolicyConnector()
                 ->user_affiliation_ids();
  return {ids.begin(), ids.end()};
}

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
std::vector<std::string> ContextInfoFetcher::GetAnalysisConnectorProviders(
    enterprise_connectors::AnalysisConnector connector) {
  return connectors_service_->GetAnalysisServiceProviderNames(connector);
}

enterprise_connectors::EnterpriseRealTimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  return connectors_service_->GetAppliedRealTimeUrlCheck();
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  return connectors_service_->GetReportingServiceProviderNames();
}
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

SettingValue ContextInfoFetcher::GetOSFirewall() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return device_signals::GetOSFirewall();
#elif BUILDFLAG(IS_CHROMEOS)
  return GetChromeosFirewall();
#else
  return SettingValue::UNKNOWN;
#endif
}

#if BUILDFLAG(IS_LINUX)
ScopedUfwConfigPathForTesting::ScopedUfwConfigPathForTesting(const char* path)
    : initial_path_(*device_signals::GetUfwConfigPath()) {
  *device_signals::GetUfwConfigPath() = path;
}

ScopedUfwConfigPathForTesting::~ScopedUfwConfigPathForTesting() {
  *device_signals::GetUfwConfigPath() = initial_path_;
}
#endif  // BUILDFLAG(IS_LINUX)

std::vector<std::string> ContextInfoFetcher::GetDnsServers() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return device_signals::GetSystemDnsServers();
#else
  return std::vector<std::string>();
#endif
}

}  // namespace enterprise_signals
