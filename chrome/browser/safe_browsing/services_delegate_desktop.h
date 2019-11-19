// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/resource_request_detector.h"
#include "chrome/browser/safe_browsing/services_delegate.h"

namespace safe_browsing {

class SafeBrowsingDatabaseManager;
struct V4ProtocolConfig;

// Actual ServicesDelegate implementation. Create via
// ServicesDelegate::Create().
class ServicesDelegateDesktop : public ServicesDelegate {
 public:
  ServicesDelegateDesktop(SafeBrowsingService* safe_browsing_service,
                          ServicesDelegate::ServicesCreator* services_creator);
  ~ServicesDelegateDesktop() override;

 private:
  // ServicesDelegate:
  const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager()
      const override;
  void Initialize() override;
  void InitializeCsdService(scoped_refptr<network::SharedURLLoaderFactory>
                                url_loader_factory) override;
  void SetDatabaseManagerForTest(
      SafeBrowsingDatabaseManager* database_manager) override;
  void ShutdownServices() override;
  void RefreshState(bool enable) override;
  void ProcessResourceRequest(const ResourceRequestInfo* request) override;
  std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
  CreatePreferenceValidationDelegate(Profile* profile) override;
  void RegisterDelayedAnalysisCallback(
      const DelayedAnalysisCallback& callback) override;
  void AddDownloadManager(content::DownloadManager* download_manager) override;
  ClientSideDetectionService* GetCsdService() override;
  DownloadProtectionService* GetDownloadService() override;

  void StartOnIOThread(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const V4ProtocolConfig& v4_config) override;
  void StopOnIOThread(bool shutdown) override;

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
  ResourceRequestDetector* CreateResourceRequestDetector();

  void CreateBinaryUploadService(Profile* profile) override;
  void RemoveBinaryUploadService(Profile* profile) override;
  BinaryUploadService* GetBinaryUploadService(Profile* profile) const override;

  std::string GetSafetyNetId() const override;

  std::unique_ptr<ClientSideDetectionService> csd_service_;
  std::unique_ptr<DownloadProtectionService> download_service_;
  std::unique_ptr<IncidentReportingService> incident_service_;
  std::unique_ptr<ResourceRequestDetector> resource_request_detector_;

  // The database manager that handles the database checking and update logic
  // Accessed on both UI and IO thread.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Has the database_manager been set for tests?
  bool database_manager_set_for_tests_ = false;

  // Tracks existing Profiles, and their corresponding BinaryUploadService
  // instances. Accessed on UI thread.
  std::map<Profile*, std::unique_ptr<BinaryUploadService>>
      binary_upload_service_map_;

  DISALLOW_COPY_AND_ASSIGN(ServicesDelegateDesktop);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SERVICES_DELEGATE_DESKTOP_H_
