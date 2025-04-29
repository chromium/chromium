// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service_factory.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_registration_information.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace safe_browsing {

ServiceWorkerTelemetryInfo::ServiceWorkerTelemetryInfo() noexcept = default;
ServiceWorkerTelemetryInfo::ServiceWorkerTelemetryInfo(
    const ServiceWorkerTelemetryInfo& other) noexcept = default;
ServiceWorkerTelemetryInfo::~ServiceWorkerTelemetryInfo() = default;

namespace {

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

}  // namespace

// static
NotificationTelemetryService* NotificationTelemetryService::Get(
    Profile* profile) {
  return NotificationTelemetryServiceFactory::GetInstance()->GetForProfile(
      profile);
}

NotificationTelemetryService::NotificationTelemetryService(
    Profile* profile,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager)
    : url_loader_factory_(url_loader_factory),
      database_manager_(database_manager),
      profile_(profile) {
  service_worker_context_ =
      profile_->GetDefaultStoragePartition()->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);
}

NotificationTelemetryService::~NotificationTelemetryService() {
  service_worker_context_->RemoveObserver(this);
}

void NotificationTelemetryService::OnRegistrationStoredForTest(
    int64_t registration_id,
    const GURL& scope,
    const content::ServiceWorkerRegistrationInformation&
        service_worker_registration_info) {
  OnRegistrationStored(registration_id, scope,
                       service_worker_registration_info);
}

// Overload from observer
void NotificationTelemetryService::OnRegistrationStored(
    int64_t registration_id,
    const GURL& scope,
    const content::ServiceWorkerRegistrationInformation&
        service_worker_registration_info) {
  // Empty For Now
}

void NotificationTelemetryService::DatabaseCheckDone(
    ServiceWorkerTelemetryInfo service_worker_info,
    bool allow_listed,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  // Empty For Now
}

void NotificationTelemetryService::UploadComplete(
    std::unique_ptr<std::string> response_body) {
  // Empty For Now
}

void NotificationTelemetryService::OnNewNotificationServiceWorkerSubscription(
    int notification_id) {
  // Empty For Now
}

// static
GURL NotificationTelemetryService::GetTelemetryReportUrl() {
  GURL url(kSbIncidentReportUrl);
  std::string api_key(google_apis::GetAPIKey());
  if (api_key.empty()) {
    return url;
  }
  return url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
}
}  // namespace safe_browsing
