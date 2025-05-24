// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "components/keyed_service/core/keyed_service.h"
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
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager);
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

  static int ServiceWorkerInfoCacheSizeForTest();

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationTelemetryServiceTest,
                           SendsTelemetryReport);
  FRIEND_TEST_ALL_PREFIXES(NotificationTelemetryServiceTest,
                           EnforcesServiceWorkerInfoCacheSize);

  // Callback used for checking the Safe Browsing allowlist.
  void DatabaseCheckDone(
      ServiceWorkerTelemetryInfo service_worker_info,
      bool allowlisted,
      std::optional<SafeBrowsingDatabaseManager::
                        HighConfidenceAllowlistCheckLoggingDetails>
          logging_details);

  // Used for logging after an upload.
  void UploadComplete(std::unique_ptr<std::string> response_body);

  // Check if a notifications service worker ID matches any of the stored
  // service worker origins.
  void OnNewNotificationServiceWorkerSubscription(int64_t registration_id);

  // Returns the URL to which telemetry reports are to be sent.
  static GURL GetTelemetryReportUrl();

  // Stored service worker info whose size is based on
  // `kNotificationTelemetryServiceWorkerInfoMaxCount`
  std::vector<ServiceWorkerTelemetryInfo> service_worker_infos_;

  // Accessor for an URLLoaderFactory with which reports will be sent.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Used to preform the Safe Browsing allowlist lookup.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  raw_ptr<Profile> profile_;

  raw_ptr<content::ServiceWorkerContext> service_worker_context_ = nullptr;

  base::WeakPtrFactory<NotificationTelemetryService> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_NOTIFICATION_TELEMETRY_NOTIFICATION_TELEMETRY_SERVICE_H_
