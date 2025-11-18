// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_manager.h"

#include <memory>

#include "base/check.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"  // nogncheck
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
static constexpr enterprise_connectors::AnalysisConnector
    kLocalAnalysisConnectors[] = {
        AnalysisConnector::BULK_DATA_ENTRY,
        AnalysisConnector::FILE_ATTACHED,
        AnalysisConnector::PRINT,
};
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

}  // namespace

ConnectorsManager::ConnectorsManager(PrefService* pref_service,
                                     const ServiceProviderConfig* config,
                                     bool observe_prefs)
    : ConnectorsManagerBase(pref_service, config, observe_prefs) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Start observing tab strip models for all browsers.
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser_window_interface) {
        OnBrowserAdded(browser_window_interface->GetBrowserForMigrationOnly());
        return true;
      });
  BrowserList::GetInstance()->AddObserver(this);
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  if (observe_prefs) {
    StartObservingPrefs(pref_service);
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
    MaybeCloseLocalContentAnalysisAgentConnection();
#endif
  }
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
ConnectorsManager::~ConnectorsManager() {
  BrowserList::GetInstance()->RemoveObserver(this);
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser_window_interface) {
        OnBrowserRemoved(
            browser_window_interface->GetBrowserForMigrationOnly());
        return true;
      });
}
#else
ConnectorsManager::~ConnectorsManager() = default;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
bool ConnectorsManager::IsConnectorEnabledForLocalAgent(
    AnalysisConnector connector) const {
  if (!IsAnalysisConnectorEnabled(connector)) {
    return false;
  }
  return analysis_connector_settings_.at(connector)[0]->is_local_analysis();
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS)
std::optional<AnalysisSettings> ConnectorsManager::GetAnalysisSettings(
    content::BrowserContext* context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    AnalysisConnector connector) {
  if (!IsAnalysisConnectorEnabled(connector)) {
    return std::nullopt;
  }

  if (analysis_connector_settings_.count(connector) == 0)
    CacheAnalysisConnectorPolicy(connector);

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0)
    return std::nullopt;

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  auto* analysis_connector_settings = static_cast<AnalysisServiceSettings*>(
      analysis_connector_settings_[connector][0].get());

  return analysis_connector_settings->GetAnalysisSettings(
      context, source_url, destination_url, GetDataRegion(connector));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
void ConnectorsManager::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void ConnectorsManager::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void ConnectorsManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Checking only when new tab is open.
  if (change.type() != TabStripModelChange::kInserted) {
    return;
  }

  for (auto connector : kLocalAnalysisConnectors) {
    if (!IsConnectorEnabledForLocalAgent(connector)) {
      continue;
    }

    // Send a connection event to the local agent. If all the enabled connectors
    // are configured to use the same agent, the same connection is reused here.
    auto configs = GetAnalysisServiceConfigs(connector);
    enterprise_connectors::ContentAnalysisSdkManager::Get()->GetClient(
        {configs[0]->local_path, configs[0]->user_specific});
  }
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

void ConnectorsManager::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);

  const base::Value::List& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    analysis_connector_settings_[connector].push_back(
        std::make_unique<AnalysisServiceSettings>(service_settings,
                                                  *service_provider_config_));
  }
}

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
void ConnectorsManager::MaybeCloseLocalContentAnalysisAgentConnection() {
  for (auto connector : kLocalAnalysisConnectors) {
    if (IsConnectorEnabledForLocalAgent(connector)) {
      // Return early because at lease one access point is enabled for local
      // agent.
      return;
    }
  }
  // Delete connection with local agents when no access point is enabled.
  ContentAnalysisSdkManager::Get()->ResetAllClients();
}
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

void ConnectorsManager::OnPrefChanged(AnalysisConnector connector) {
  CacheAnalysisConnectorPolicy(connector);
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  MaybeCloseLocalContentAnalysisAgentConnection();
#endif
}

DataRegion ConnectorsManager::GetDataRegion(AnalysisConnector connector) const {
#if BUILDFLAG(IS_ANDROID)
  return DataRegion::NO_PREFERENCE;
#else
  // Connector's policy scope determines the DRZ policy scope to use.
  policy::PolicyScope scope = static_cast<policy::PolicyScope>(
      prefs()->GetInteger(AnalysisConnectorScopePref(connector)));

  const PrefService* pref_service =
      (scope == policy::PolicyScope::POLICY_SCOPE_MACHINE)
          ? g_browser_process->local_state()
          : prefs();

  if (!pref_service ||
      !pref_service->HasPrefPath(prefs::kChromeDataRegionSetting)) {
    return DataRegion::NO_PREFERENCE;
  }

  return ChromeDataRegionSettingToEnum(
      pref_service->GetInteger(prefs::kChromeDataRegionSetting));
#endif
}

void ConnectorsManager::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingPref(AnalysisConnector::FILE_ATTACHED);
  StartObservingPref(AnalysisConnector::FILE_DOWNLOADED);
  StartObservingPref(AnalysisConnector::BULK_DATA_ENTRY);
  StartObservingPref(AnalysisConnector::PRINT);
#if BUILDFLAG(IS_CHROMEOS)
  StartObservingPref(AnalysisConnector::FILE_TRANSFER);
#endif
  ConnectorsManagerBase::StartObservingPref();
}

void ConnectorsManager::StartObservingPref(AnalysisConnector connector) {
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(
                  static_cast<void (ConnectorsManager::*)(AnalysisConnector)>(
                      &ConnectorsManager::OnPrefChanged),
                  base::Unretained(this), connector));
  }
}

}  // namespace enterprise_connectors
