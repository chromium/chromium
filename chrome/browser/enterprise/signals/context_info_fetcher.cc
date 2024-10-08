// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/enterprise/signals/context_info_fetcher.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/signals/signals_utils.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/site_isolation_policy.h"
#include "device_management_backend.pb.h"

#if BUILDFLAG(IS_POSIX)
#include "net/dns/public/resolv_reader.h"
#include "net/dns/public/scoped_res_state.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/mac_util.h"
#include "base/process/launch.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <netfw.h>
#include <wrl/client.h>

#include "net/dns/public/win_dns_system_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/constants/dbus_switches.h"
#endif

namespace enterprise_signals {

namespace {

std::optional<std::string> GetEnterpriseProfileId(Profile* profile) {
  auto* profile_id_service =
      enterprise::ProfileIdServiceFactory::GetForProfile(profile);
  if (profile_id_service)
    return profile_id_service->GetProfileId();
  return std::nullopt;
}

#if BUILDFLAG(IS_LINUX)
const char** GetUfwConfigPath() {
  static const char* path = "/etc/ufw/ufw.conf";
  return &path;
}

SettingValue GetUfwStatus() {
  base::FilePath path(*GetUfwConfigPath());
  std::string file_content;
  base::StringPairs values;

  if (!base::PathExists(path) || !base::PathIsReadable(path) ||
      !base::ReadFileToString(path, &file_content)) {
    return SettingValue::UNKNOWN;
  }
  base::SplitStringIntoKeyValuePairs(file_content, '=', '\n', &values);
  auto is_ufw_enabled = base::ranges::find(
      values, "ENABLED", &std::pair<std::string, std::string>::first);
  if (is_ufw_enabled == values.end())
    return SettingValue::UNKNOWN;

  if (is_ufw_enabled->second == "yes")
    return SettingValue::ENABLED;
  else if (is_ufw_enabled->second == "no")
    return SettingValue::DISABLED;
  else
    return SettingValue::UNKNOWN;
}
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
SettingValue GetWinOSFirewall() {
  Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy;
  HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&firewall_policy));
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return SettingValue::UNKNOWN;
  }

  long profile_types = 0;
  hr = firewall_policy->get_CurrentProfileTypes(&profile_types);
  if (FAILED(hr))
    return SettingValue::UNKNOWN;

  // The most restrictive active profile takes precedence.
  constexpr NET_FW_PROFILE_TYPE2 kProfileTypes[] = {
      NET_FW_PROFILE2_PUBLIC, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_DOMAIN};
  for (size_t i = 0; i < std::size(kProfileTypes); ++i) {
    if ((profile_types & kProfileTypes[i]) != 0) {
      VARIANT_BOOL enabled = VARIANT_TRUE;
      hr = firewall_policy->get_FirewallEnabled(kProfileTypes[i], &enabled);
      if (FAILED(hr))
        return SettingValue::UNKNOWN;
      if (enabled == VARIANT_TRUE)
        return SettingValue::ENABLED;
      else if (enabled == VARIANT_FALSE)
        return SettingValue::DISABLED;
      else
        return SettingValue::UNKNOWN;
    }
  }
  return SettingValue::UNKNOWN;
}
#endif

#if BUILDFLAG(IS_MAC)
SettingValue GetMacOSFirewall() {
  if (base::mac::MacOSMajorVersion() < 15) {
    // There is no official Apple documentation on how to obtain the enabled
    // status of the firewall (System Preferences> Security & Privacy> Firewall)
    // prior to MacOS versions 15. Reading globalstate from com.apple.alf is the
    // closest way to get such an API in Chrome without delegating to
    // potentially unstable commands. Values of "globalstate":
    //   0 = de-activated
    //   1 = on for specific services
    //   2 = on for essential services
    // You can get 2 by, e.g., enabling the "Block all incoming connections"
    // firewall functionality.
    Boolean key_exists_with_valid_format = false;
    CFIndex globalstate = CFPreferencesGetAppIntegerValue(
        CFSTR("globalstate"), CFSTR("com.apple.alf"),
        &key_exists_with_valid_format);

    if (!key_exists_with_valid_format) {
      return SettingValue::UNKNOWN;
    }

    switch (globalstate) {
      case 0:
        return SettingValue::DISABLED;
      case 1:
      case 2:
        return SettingValue::ENABLED;
      default:
        return SettingValue::UNKNOWN;
    }
  }

  // Based on this recommendation from Apple:
  // https://developer.apple.com/documentation/macos-release-notes/macos-15-release-notes/#Application-Firewall
  base::FilePath fw_util("/usr/libexec/ApplicationFirewall/socketfilterfw");
  if (!base::PathExists(fw_util)) {
    return SettingValue::UNKNOWN;
  }

  base::CommandLine command(fw_util);
  command.AppendSwitch("getglobalstate");
  std::string output;
  if (!base::GetAppOutput(command, &output)) {
    return SettingValue::UNKNOWN;
  }

  // State 1 is when the Firewall is simply enabled.
  // State 2 is when the Firewall is enabled and all incoming connections are
  // blocked.
  if (output.find("(State = 1)") != std::string::npos ||
      output.find("(State = 2)") != std::string::npos) {
    return SettingValue::ENABLED;
  }
  if (output.find("(State = 0)") != std::string::npos) {
    return SettingValue::DISABLED;
  }

  return SettingValue::UNKNOWN;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
  info.browser_version = version_info::GetVersionNumber();
  info.site_isolation_enabled =
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
  info.built_in_dns_client_enabled =
      utils::GetBuiltInDnsClientEnabled(g_browser_process->local_state());
  info.chrome_remote_desktop_app_blocked =
      utils::GetChromeRemoteDesktopAppBlocked(
          PolicyBlocklistFactory::GetForBrowserContext(browser_context_));
  info.third_party_blocking_enabled =
      utils::GetThirdPartyBlockingEnabled(g_browser_process->local_state());

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  info.safe_browsing_protection_level =
      utils::GetSafeBrowsingProtectionLevel(profile->GetPrefs());
  info.password_protection_warning_trigger =
      utils::GetPasswordProtectionWarningTrigger(profile->GetPrefs());
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

std::vector<std::string> ContextInfoFetcher::GetAnalysisConnectorProviders(
    enterprise_connectors::AnalysisConnector connector) {
  return connectors_service_->GetAnalysisServiceProviderNames(connector);
}

enterprise_connectors::EnterpriseRealTimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  return connectors_service_->GetAppliedRealTimeUrlCheck();
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  return connectors_service_->GetReportingServiceProviderNames(
      enterprise_connectors::ReportingConnector::SECURITY_EVENT);
}

SettingValue ContextInfoFetcher::GetOSFirewall() {
#if BUILDFLAG(IS_LINUX)
  return GetUfwStatus();
#elif BUILDFLAG(IS_WIN)
  return GetWinOSFirewall();
#elif BUILDFLAG(IS_MAC)
  return GetMacOSFirewall();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return GetChromeosFirewall();
#else
  return SettingValue::UNKNOWN;
#endif
}

#if BUILDFLAG(IS_LINUX)
ScopedUfwConfigPathForTesting::ScopedUfwConfigPathForTesting(const char* path)
    : initial_path_(*GetUfwConfigPath()) {
  *GetUfwConfigPath() = path;
}

ScopedUfwConfigPathForTesting::~ScopedUfwConfigPathForTesting() {
  *GetUfwConfigPath() = initial_path_;
}
#endif  // BUILDFLAG(IS_LINUX)

std::vector<std::string> ContextInfoFetcher::GetDnsServers() {
  std::vector<std::string> dns_addresses;
#if BUILDFLAG(IS_POSIX)
  std::unique_ptr<net::ScopedResState> res = net::ResolvReader().GetResState();
  if (res) {
    std::optional<std::vector<net::IPEndPoint>> nameservers =
        net::GetNameservers(res->state());
    if (nameservers) {
      // If any name server is 0.0.0.0, assume the configuration is invalid.
      for (const net::IPEndPoint& nameserver : nameservers.value()) {
        if (nameserver.address().IsZero())
          return std::vector<std::string>();
        else
          dns_addresses.push_back(nameserver.ToString());
      }
    }
  }
#elif BUILDFLAG(IS_WIN)
  std::optional<std::vector<net::IPEndPoint>> nameservers;
  base::expected<net::WinDnsSystemSettings, net::ReadWinSystemDnsSettingsError>
      settings = net::ReadWinSystemDnsSettings();
  if (settings.has_value()) {
    nameservers = settings->GetAllNameservers();
  }

  if (nameservers.has_value()) {
    for (const net::IPEndPoint& nameserver : nameservers.value()) {
      dns_addresses.push_back(nameserver.ToString());
    }
  }
#endif
  return dns_addresses;
}

}  // namespace enterprise_signals
