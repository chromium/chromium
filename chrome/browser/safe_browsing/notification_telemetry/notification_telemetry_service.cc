// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service_factory.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/utils.h"
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

// Size of the stored service worker info cache.
const int kNotificationTelemetryServiceWorkerInfoMaxCount = 20;

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

constexpr net::NetworkTrafficAnnotationTag
    kSafeBrowsingIncidentTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("notification_telemetry", R"(
          semantics {
            sender: "Notification Telemetry Service"
            description:
              "Chrome will upload registration data for service workers that "
              "subscribe to push messages. The data uploaded consists of the "
              "service worker scope URL and the URLs of the scripts imported "
              "by the service worker during installation. This data is only "
              "collected if the service worker's scope URL is not on the "
              "Safe Browsing allowlist. It will be used to detect websites "
              "that install service workers to display abusive notifications."
            trigger:
              "User navigates to a website that installs a service worker "
              "to display push notifications."
            data:
              "The service worker scope URL and the URLs of the imported "
              "scripts that don't match the scope origin. See "
              "ServiceWorkerRegistrationIncident in 'https://cs.chromium.org/ "
              "chromium/src/components/safe_browsing/csd.proto' for details."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                owners: "chrome-counter-abuse-alerts@google.com"
              }
            }
            user_data {
              type: SENSITIVE_URL
            }
            last_reviewed: "2025-04-28"
          }
          policy {
            cookies_allowed: YES
            cookies_store: "Safe Browsing cookie store"
            setting:
              "Users can enable this feature by selecting the "
              "'Enhanced protection' option in "
              "'Settings->Privacy and security->Security->Safe Browsing`. "
              "The feature is disabled by default because the default "
              "Safe Browsing setting is 'Standard protection'."
            chrome_policy {
               SafeBrowsingEnabled {
                 policy_options {mode: MANDATORY}
                 SafeBrowsingEnabled: false
              }
            }
          })");

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
  PushMessagingServiceImpl* push_messaging_service =
      PushMessagingServiceFactory::GetForProfile(profile_);
  // Notification Telemetry Service is a keyed service and will outlive
  // any invocations of the callback being registered with the push messaging
  // service (also a keyed service).
  push_messaging_service->SetSubscribeFromWorkerCallback(base::BindRepeating(
      &NotificationTelemetryService::OnNewNotificationServiceWorkerSubscription,
      base::Unretained(this)));
}

NotificationTelemetryService::~NotificationTelemetryService() {
  service_worker_context_->RemoveObserver(this);
}

void NotificationTelemetryService::OnRegistrationStored(
    int64_t registration_id,
    const GURL& scope,
    const content::ServiceWorkerRegistrationInformation&
        service_worker_registration_info) {
  // Only collect information for ESB users
  if (!IsEnhancedProtectionEnabled(*profile_->GetPrefs())) {
    return;
  }
  // Check feature flag after ESB check so that the Finch experiment
  // groups only include clients that can send telemetry reports.
  if (!base::FeatureList::IsEnabled(safe_browsing::kNotificationTelemetry)) {
    return;
  }
  // Check that at least one of the resources belongs to an external domain
  bool external_resource = false;
  url::Origin scope_origin = url::Origin::Create(scope);
  for (auto& resource : service_worker_registration_info.resources) {
    url::Origin resource_url = url::Origin::Create(resource);
    if (resource_url != scope_origin) {
      external_resource = true;
      break;
    }
  }
  // Check with safe browsing to see if the origin is allowlisted.
  if (external_resource) {
    ServiceWorkerTelemetryInfo service_worker_info;
    service_worker_info.scope = scope;
    service_worker_info.registration_id = registration_id;
    service_worker_info.resources = service_worker_registration_info.resources;
    database_manager_->CheckUrlForHighConfidenceAllowlist(
        scope, base::BindOnce(&NotificationTelemetryService::DatabaseCheckDone,
                              weak_factory_.GetWeakPtr(), service_worker_info));
  }
}

// static
int NotificationTelemetryService::ServiceWorkerInfoCacheSizeForTest() {
  return kNotificationTelemetryServiceWorkerInfoMaxCount;
}

void NotificationTelemetryService::DatabaseCheckDone(
    ServiceWorkerTelemetryInfo service_worker_info,
    bool allow_listed,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  base::UmaHistogramBoolean(
      "SafeBrowsing.NotificationTelemetry.ServiceWorkerScopeURL.IsAllowlisted",
      allow_listed);

  // No handling required for service workers with allowlisted scope URLs.
  if (allow_listed) {
    return;
  }
  // Only store up to `kNotificationTelemetryServiceWorkerInfoMaxCount` entries.
  // Remove the oldest entry in the store if necessary to accommodate a new one.
  if (service_worker_infos_.size() >=
      kNotificationTelemetryServiceWorkerInfoMaxCount) {
    service_worker_infos_.erase(service_worker_infos_.begin());
  }
  service_worker_infos_.push_back(service_worker_info);
}

void NotificationTelemetryService::OnNewNotificationServiceWorkerSubscription(
    int64_t registration_id) {
  // Only collect information for ESB users
  if (!IsEnhancedProtectionEnabled(*profile_->GetPrefs())) {
    return;
  }
  // Check feature flag after ESB check so that the Finch experiment
  // groups only include clients that can send telemetry reports.
  if (!base::FeatureList::IsEnabled(safe_browsing::kNotificationTelemetry)) {
    return;
  }
  // Check the stored service worker list to see if there is an
  // entry that has the same registration id for which we received
  // the notification.
  auto it =
      std::find_if(service_worker_infos_.begin(), service_worker_infos_.end(),
                   [registration_id](const ServiceWorkerTelemetryInfo& info) {
                     return registration_id == info.registration_id;
                   });
  // No match found, so return without doing anything.
  if (it == service_worker_infos_.end()) {
    return;
  }
  // Match found, save the matched registration data
  // and delete the matched entry in the stored list.
  ServiceWorkerTelemetryInfo report_data = std::move(*it);
  service_worker_infos_.erase(it);

  // Generate a telemetry report with the matched data.
  std::unique_ptr<ClientIncidentReport_IncidentData> incident_data =
      std::make_unique<ClientIncidentReport_IncidentData>();
  ClientIncidentReport_IncidentData_ServiceWorkerRegistrationIncident*
      notification_resource_report =
          incident_data->mutable_notification_import_script();

  notification_resource_report->set_scope_url(report_data.scope.spec());
  for (const auto& resource : report_data.resources) {
    std::string* import_script_url =
        notification_resource_report->add_import_script_url();
    *import_script_url = resource.spec();
  }

  std::unique_ptr<ClientIncidentReport> report =
      std::make_unique<ClientIncidentReport>();
  report->mutable_incident()->AddAllocated(incident_data.release());
  std::string post_data;
  if (!report->SerializeToString(&post_data)) {
    return;
  }

  // Send report for upload
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetTelemetryReportUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kSafeBrowsingIncidentTrafficAnnotation);
  url_loader_->AttachStringForUpload(post_data, "application/octet-stream");
  // Using base::Unretained is safe here as Network Telemetry Service owns
  // `url_loader_`.
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&NotificationTelemetryService::UploadComplete,
                     base::Unretained(this)));
}

void NotificationTelemetryService::UploadComplete(
    std::unique_ptr<std::string> response_body) {
  // Take ownership of the loader in this scope.
  std::unique_ptr<network::SimpleURLLoader> url_loader(std::move(url_loader_));
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers) {
    response_code = url_loader->ResponseInfo()->headers->response_code();
  }
  RecordHttpResponseOrErrorCode(
      "SafeBrowsing.NotificationTelemetry.NetworkResult",
      url_loader->NetError(), response_code);
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
