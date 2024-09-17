// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_

#include <memory>

#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/core/browser/db/hash_prefix_map.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

class DownloadProtectionService;
class IncidentReportingService;
class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;

// Actual ServicesDelegate implementation. Create via
// ServicesDelegate::Create().
class ServicesDelegateDesktop : public ServicesDelegate {
 public:
  ServicesDelegateDesktop(SafeBrowsingServiceImpl* safe_browsing_service,
                          ServicesDelegate::ServicesCreator* services_creator);

  ServicesDelegateDesktop(const ServicesDelegateDesktop&) = delete;
  ServicesDelegateDesktop& operator=(const ServicesDelegateDesktop&) = delete;

  ~ServicesDelegateDesktop() override;

 private:
  // ServicesDelegate:
  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const override;
  void Initialize() override;
  void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      DelayedAnalysisCallback callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;
  DownloadProtectionService* GetDownloadService() override;

  void StartOnUIThread(
      scoped_refptr<network::SharedURLLoaderFactory> browser_url_loader_factory,
      const V4ProtocolConfig& v4_config) override;
  void StopOnUIThread(bool shutdown) override;

  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Reports the current extended reporting level. Note that this is an
  // estimation and may not always be correct. It is possible that the
  // estimation finds both Scout and legacy extended reporting to be enabled.
  // This can happen, for instance, if one profile has Scout enabled and another
  // has legacy extended reporting enabled. In such a case, this method reports
  // LEGACY as the current level.
  ExtendedReportingLevel GetEstimatedExtendedReportingLevel() const;

  scoped_refptr<SafeBrowsingDatabaseManager> CreateDatabaseManager();
  DownloadProtectionService* CreateDownloadProtectionService();
  IncidentReportingService* CreateIncidentReportingService();

  static void UpdateSyntheticFieldTrial(HashPrefixMap::MigrateResult result);

  std::unique_ptr<DownloadProtectionService> download_service_;
  std::unique_ptr<IncidentReportingService> incident_service_;

  // The database manager that handles the database checking and update logic
  // Accessed on both UI and IO thread.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Has the database_manager been set for tests?
  bool database_manager_set_for_tests_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_
