// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_CONTEXT_INFO_FETCHER_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_CONTEXT_INFO_FETCHER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/signals/signals_common.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_connectors {
enum AnalysisConnector : int;
class ConnectorsService;
}  // namespace enterprise_connectors

namespace enterprise_signals {

struct ContextInfo {
  ContextInfo();
  ContextInfo(ContextInfo&&);
  ContextInfo(const ContextInfo&) = delete;
  ContextInfo& operator=(const ContextInfo&) = delete;
  ~ContextInfo();

  std::vector<std::string> browser_affiliation_ids;
  std::vector<std::string> profile_affiliation_ids;
  std::vector<std::string> on_file_attached_providers;
  std::vector<std::string> on_file_downloaded_providers;
  std::vector<std::string> on_bulk_data_entry_providers;
  std::vector<std::string> on_print_providers;
  std::vector<std::string> on_security_event_providers;
  enterprise_connectors::EnterpriseRealTimeUrlCheckMode realtime_url_check_mode;
  std::string browser_version;
  safe_browsing::SafeBrowsingState safe_browsing_protection_level;
  bool site_isolation_enabled;
  bool built_in_dns_client_enabled;
  std::optional<safe_browsing::PasswordProtectionTrigger>
      password_protection_warning_trigger;
  bool chrome_remote_desktop_app_blocked;
  std::optional<bool> third_party_blocking_enabled;
  SettingValue os_firewall;
  std::vector<std::string> system_dns_servers;
  std::optional<std::string> enterprise_profile_id;
};

// Interface used by the chrome.enterprise.reportingPrivate.getContextInfo()
// function that fetches context information on Chrome. Each supported platform
// has its own subclass implementation.
class ContextInfoFetcher {
 public:
  using ContextInfoCallback = base::OnceCallback<void(ContextInfo)>;
  ContextInfoFetcher(
      content::BrowserContext* browser_context,
      enterprise_connectors::ConnectorsService* connectors_service);
  virtual ~ContextInfoFetcher();

  ContextInfoFetcher(const ContextInfoFetcher&) = delete;
  ContextInfoFetcher operator=(const ContextInfoFetcher&) = delete;

  // Returns a platform specific instance of ContextInfoFetcher.
  static std::unique_ptr<ContextInfoFetcher> CreateInstance(
      content::BrowserContext* browser_context,
      enterprise_connectors::ConnectorsService* connectors_service);

  // Fetches the context information for the current platform. Eventually calls
  // |callback_|. This function takes a callback to return a ContextInfo instead
  // of returning synchronously because some attributes need to be fetched
  // asynchronously.
  void Fetch(ContextInfoCallback callback);

 private:
  // The following private methods each populate an attribute of ContextInfo. If
  // an attribute can't share implementation across platforms, its corresponding
  // function should be virtual and overridden in the platform subclasses.

  std::vector<std::string> GetBrowserAffiliationIDs();

  std::vector<std::string> GetProfileAffiliationIDs();

  std::vector<std::string> GetAnalysisConnectorProviders(
      enterprise_connectors::AnalysisConnector connector);

  enterprise_connectors::EnterpriseRealTimeUrlCheckMode
  GetRealtimeUrlCheckMode();

  std::vector<std::string> GetOnSecurityEventProviders();

  SettingValue GetOSFirewall();

  ContextInfo FetchAsyncSignals(ContextInfo info);

  std::vector<std::string> GetDnsServers();

  raw_ptr<content::BrowserContext> browser_context_;

  // |connectors_service| is used to obtain the value of each Connector policy.
  raw_ptr<enterprise_connectors::ConnectorsService, DanglingUntriaged>
      connectors_service_;
};

#if BUILDFLAG(IS_LINUX)
class ScopedUfwConfigPathForTesting {
 public:
  explicit ScopedUfwConfigPathForTesting(const char* path);
  ~ScopedUfwConfigPathForTesting();

  ScopedUfwConfigPathForTesting& operator=(
      const ScopedUfwConfigPathForTesting&) = delete;
  ScopedUfwConfigPathForTesting(const ScopedUfwConfigPathForTesting&) = delete;

 private:
  const char* initial_path_;
};
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace enterprise_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_CONTEXT_INFO_FETCHER_H_
