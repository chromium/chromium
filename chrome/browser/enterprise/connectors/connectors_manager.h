// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_service_settings.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS)
#include "content/public/browser/browser_context.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace storage {
class FileSystemURL;
}

namespace enterprise_connectors {

// This class overrides `ConnectorsManagerBase` for desktop and Android usage.
// It manages access to Reporting and Analysis Connector policies for a given
// profile.
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
class ConnectorsManager : public ConnectorsManagerBase,
                          public BrowserListObserver,
                          public TabStripModelObserver {
#else
class ConnectorsManager : public ConnectorsManagerBase {
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

 public:
  using ConnectorsManagerBase::AnalysisConnectorsSettings;
  using ConnectorsManagerBase::GetAnalysisSettings;

  ConnectorsManager(PrefService* pref_service,
                    const ServiceProviderConfig* config,
                    bool observe_prefs = true);
  ~ConnectorsManager() override;

#if BUILDFLAG(IS_CHROMEOS)
  std::optional<AnalysisSettings> GetAnalysisSettings(
      content::BrowserContext* context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      AnalysisConnector connector);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Check if the corresponding connector is enabled for any local agent.
  bool IsConnectorEnabledForLocalAgent(AnalysisConnector connector) const;
#endif

 private:
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  void CacheAnalysisConnectorPolicy(AnalysisConnector connector) const override;

  // Get data location region from policy.
  DataRegion GetDataRegion(AnalysisConnector connector) const override;

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // Close connection with local agent if all the relevant connectors are turned
  // off for it.
  void MaybeCloseLocalContentAnalysisAgentConnection();
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

  // Re-cache analysis connector policy and update local agent connection if
  // needed.
  void OnPrefChanged(AnalysisConnector connector);

  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  void StartObservingPref(AnalysisConnector connector);

  // ConnectorsManagerBase overrides:
  void StartObservingPrefs(PrefService* pref_service) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_MANAGER_H_
