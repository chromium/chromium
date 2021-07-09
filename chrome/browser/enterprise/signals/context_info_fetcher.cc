// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/context_info_fetcher.h"

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/site_isolation_policy.h"
#include "device_management_backend.pb.h"

namespace enterprise_signals {

namespace {

bool IsURLBlocked(const GURL& url, content::BrowserContext* browser_context_) {
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context_);

  if (!service)
    return false;

  policy::URLBlocklist::URLBlocklistState state =
      service->GetURLBlocklistState(url);

  return state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
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
  // TODO(domfc): Add platform overrides of the class once they are needed for
  // an attribute.
  return std::make_unique<ContextInfoFetcher>(browser_context,
                                              connectors_service);
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
  info.realtime_url_check_mode = GetRealtimeUrlCheckMode();
  info.on_security_event_providers = GetOnSecurityEventProviders();
  info.browser_version = version_info::GetVersionNumber();
  info.safe_browsing_protection_level = GetSafeBrowsingProtectionLevel();
  info.site_isolation_enabled =
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
  info.built_in_dns_client_enabled = GetBuiltInDnsClientEnabled();
  info.password_protection_warning_trigger =
      GetPasswordProtectionWarningTrigger();
  info.chrome_cleanup_enabled = GetChromeCleanupEnabled();
  info.chrome_remote_desktop_app_blocked = GetChromeRemoteDesktopAppBlocked();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(info)));
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

safe_browsing::EnterpriseRealTimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  return connectors_service_->GetAppliedRealTimeUrlCheck();
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  return connectors_service_->GetReportingServiceProviderNames(
      enterprise_connectors::ReportingConnector::SECURITY_EVENT);
}

safe_browsing::SafeBrowsingState
ContextInfoFetcher::GetSafeBrowsingProtectionLevel() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  bool safe_browsing_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled)
      return safe_browsing::ENHANCED_PROTECTION;
    else
      return safe_browsing::STANDARD_PROTECTION;
  } else {
    return safe_browsing::NO_SAFE_BROWSING;
  }
}

bool ContextInfoFetcher::GetBuiltInDnsClientEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kBuiltInDnsClientEnabled);
}

absl::optional<safe_browsing::PasswordProtectionTrigger>
ContextInfoFetcher::GetPasswordProtectionWarningTrigger() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile->GetPrefs()->HasPrefPath(
          prefs::kPasswordProtectionWarningTrigger))
    return absl::nullopt;
  return static_cast<safe_browsing::PasswordProtectionTrigger>(
      profile->GetPrefs()->GetInteger(
          prefs::kPasswordProtectionWarningTrigger));
}

absl::optional<bool> ContextInfoFetcher::GetChromeCleanupEnabled() {
#if defined(OS_WIN)
  return g_browser_process->local_state()->GetBoolean(
      prefs::kSwReporterEnabled);
#else
  return absl::nullopt;
#endif
}

bool ContextInfoFetcher::GetChromeRemoteDesktopAppBlocked() {
  return IsURLBlocked(GURL("https://remotedesktop.google.com"),
                      browser_context_) ||
         IsURLBlocked(GURL("https://remotedesktop.corp.google.com"),
                      browser_context_);
}

}  // namespace enterprise_signals
