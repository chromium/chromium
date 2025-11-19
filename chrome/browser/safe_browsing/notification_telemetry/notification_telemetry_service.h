// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_store.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "content/public/browser/service_worker_context_observer.h"

class Profile;

namespace content {

class ServiceWorkerContext;
struct ServiceWorkerRegistrationInformation;

}  // namespace content

namespace network {

class SharedURLLoaderFactory;
class SimpleURLLoader;

}  // namespace network

namespace safe_browsing {

struct ServiceWorkerTelemetryInfo {
  ServiceWorkerTelemetryInfo() noexcept;
  ServiceWorkerTelemetryInfo(const ServiceWorkerTelemetryInfo& other) noexcept;
  ~ServiceWorkerTelemetryInfo();

  // The Registration ID assigned to the service worker
  int64_t registration_id = 0;
  // The scope url for the service worker
  GURL scope;
  // A vector of URLs that have been used to import service worker resources
  std::vector<GURL> resources;
};

// This service tracks and reports telemetry regarding user notifications. It
// sends data directly to the Chrome Incident Reporting backend without using
// the normal incident reporting workflow.
class NotificationTelemetryService
    : public content::ServiceWorkerContextObserver,
      public KeyedService {
 public:
  // Convenience method to get the service for a profile.
  static NotificationTelemetryService* Get(Profile* profile);

  explicit NotificationTelemetryService(
      Profile* profile,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      std::unique_ptr<NotificationTelemetryStoreInterface> telemetry_store,
      scoped_refptr<SafeBrowsingUIManager> ui_manager);
  explicit NotificationTelemetryService(const NotificationTelemetryService&) =
      delete;
  NotificationTelemetryService& operator=(const NotificationTelemetryService&) =
      delete;

  ~NotificationTelemetryService() override;

  // ServiceWorkerContextObserver:
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope,
                            const content::ServiceWorkerRegistrationInformation&
                                service_worker_registration_info) override;
  void OnPushEventFinished(
      const GURL& script_url,
      const std::optional<std::vector<GURL>>& requested_urls) override;

  void OnGetServiceWorkerBehaviors(
      bool success,
      std::unique_ptr<std::vector<CSBRR::ServiceWorkerBehavior>> entries);

  static int ServiceWorkerInfoCacheSizeForTest();

  NotificationTelemetryStoreInterface* GetTelemetryStoreForTest();

  int GetEmptyDbFoundCountForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationTelemetryServiceTest,
                           OnGetServiceWorkerBehaviors);
  FRIEND_TEST_ALL_PREFIXES(NotificationTelemetryServiceTest,
                           SendsTelemetryReport);
  FRIEND_TEST_ALL_PREFIXES(NotificationTelemetryServiceTest,
                           EnforcesServiceWorkerInfoCacheSize);
  FRIEND_TEST_ALL_PREFIXES(
      NotificationTelemetryServiceTest,
      ServiceWorkerSubscriptionRecordsServiceWorkerBehavior);
  // TODO(crbug.com/433543634): Clean up post
  // GlobalCacheListForGatingNotificationProtections launch.
  FRIEND_TEST_ALL_PREFIXES(
      NotificationTelemetryServiceFactoryTest,
      CreatedWithoutDatabaseManagerWhenGlobalCacheListEnabled);
  FRIEND_TEST_ALL_PREFIXES(
      NotificationTelemetryServiceFactoryTest,
      CreatedWithDatabaseManagerWhenGlobalCacheListDisabled);

  // Callback used for checking the Safe Browsing allowlist.
  void DatabaseCheckDone(
      ServiceWorkerTelemetryInfo service_worker_info,
      bool allowlisted,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Store the service work info if the scope URL is not allowlisted.
  void MaybeStoreServiceWorkerInfo(
      ServiceWorkerTelemetryInfo service_worker_info,
      bool allowlisted);

  // Used for logging after an upload.
  void UploadComplete(std::optional<std::string> response_body);

  // Check if a notifications service worker ID matches any of the stored
  // service worker origins.
  void OnNewNotificationServiceWorkerSubscription(int64_t registration_id);

  // Returns the URL to which telemetry reports are to be sent.
  static GURL GetTelemetryReportUrl();

  // Attempts to upload a CSBRR report.
  void MaybeUploadReport();

  // Called after the NotificationTelemetryStore has deleted all data because
  // the user is no longer an ESB user.
  void OnTelemetryStoreDeleted(bool success);

  // Called after a new ServiceWorkerBehavior has been added to storage.
  void OnAddServiceWorkerBehavior(bool success);

  // Removes any duplicate requested urls from a ServiceWorkerBehavior.
  void DedupeRequestedURLs(
      CSBRR::ServiceWorkerBehavior* service_worker_behavior);

  // Normalizes URLs by stripping any query param values. Since query param
  // values aren't important aspects of the URL, removing them reduces noise
  // and storage usage.
  std::vector<GURL> NormalizeURLs(std::vector<GURL> urls);

  // Stored service worker info whose size is based on
  // `kNotificationTelemetryServiceWorkerInfoMaxCount`
  std::vector<ServiceWorkerTelemetryInfo> service_worker_infos_;

  // Accessor for an URLLoaderFactory with which reports will be sent.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // TODO(crbug.com/433543634): Remove `database_manager_` post
  // GlobalCacheListForGatingNotificationProtections launch.
  // Used to preform the Safe Browsing allowlist lookup.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Responsible for storing the ServiceWorkerBehaviors.
  std::unique_ptr<NotificationTelemetryStoreInterface> telemetry_store_;

  // Responsible for sending the CSBRRs.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  raw_ptr<Profile> profile_;

  raw_ptr<content::ServiceWorkerContext> service_worker_context_ = nullptr;

  // Tracks how many consecutive times an empty database was encountered.
  int empty_db_found_count_ = 0;

  // Used to periodically attempt to send a report to Safe Browsing.
  base::RepeatingTimer timer_;

  base::WeakPtrFactory<NotificationTelemetryService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_
