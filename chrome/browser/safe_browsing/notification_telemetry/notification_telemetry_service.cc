// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service.h"

#include <optional>
#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_service_factory.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/safe_browsing/notification_telemetry/notification_telemetry_service_factory.h"
#include "components/safe_browsing/content/browser/notification_content_detection/notifications_global_cache_list.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_registration_information.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
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

// The probability of sending a ServiceWorkerBehavior CSBRR off device.
const double kNotificationTelemetrySwbReportingProbability = 0.01;



}  // namespace

// static
NotificationTelemetryService* NotificationTelemetryService::Get(
    Profile* profile) {
  return NotificationTelemetryServiceFactory::GetInstance()->GetForProfile(
      profile);
}

NotificationTelemetryService::NotificationTelemetryService(
    Profile* profile,
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    scoped_refptr<SafeBrowsingUIManager> ui_manager)
    : database_manager_(database_manager),
      ui_manager_(ui_manager),
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

    // TODO(crbug.com/433543634): Clean up the use of `database_manager_` post
    // GlobalCacheListForGatingNotificationProtections launch.
    if (database_manager_ == nullptr) {
      MaybeStoreServiceWorkerInfo(
          service_worker_info,
          ShouldSkipNotificationProtectionsDueToGlobalCacheList(scope));
    } else {
      database_manager_->CheckUrlForHighConfidenceAllowlist(
          scope,
          base::BindOnce(&NotificationTelemetryService::DatabaseCheckDone,
                         weak_factory_.GetWeakPtr(), service_worker_info));
    }
  }
}

std::vector<GURL> NotificationTelemetryService::NormalizeURLs(
    std::vector<GURL> urls) {
  std::vector<GURL> normalized_urls;
  normalized_urls.reserve(urls.size());

  for (const auto& url : urls) {
    if (!url.is_valid()) {
      // Skip invalid URLs.
      continue;
    }
    if (!url.has_query()) {
      // No query, add as is.
      normalized_urls.push_back(url);
      continue;
    }
    std::string new_query;
    net::QueryIterator query_iterator(url);

    while (!query_iterator.IsAtEnd()) {
      if (!new_query.empty()) {
        new_query += "&";
      }
      // Append only the key, strip the value.
      new_query += query_iterator.GetKey();
      query_iterator.Advance();
    }

    GURL::Replacements replacements;
    replacements.SetQueryStr(new_query);
    normalized_urls.push_back(url.ReplaceComponents(replacements));
  }
  return normalized_urls;
}
void NotificationTelemetryService::OnPushEventFinished(
    const GURL& script_url,
    const std::optional<std::vector<GURL>>& requested_urls) {
  if (!requested_urls.has_value()) {
    return;
  }
  // Only collect information for ESB users
  if (!IsEnhancedProtectionEnabled(*profile_->GetPrefs())) {
    return;
  }

  std::vector<GURL> normalized_requested_urls =
      NormalizeURLs(requested_urls.value());
  // Remove duplicate URLs.
  base::flat_set<GURL> requested_urls_set(normalized_requested_urls.begin(),
                                          normalized_requested_urls.end());
  if (should_send_report_for_test_ ||
      base::RandDouble() < kNotificationTelemetrySwbReportingProbability) {
    auto report = std::make_unique<CSBRR>();
    report->set_type(CSBRR::SERVICE_WORKER_BEHAVIOR);
    report->set_page_url(script_url.spec());
    CSBRR::ServiceWorkerBehavior* service_worker_behavior =
        report->add_service_worker_behaviors();
    service_worker_behavior->set_script_url(script_url.spec());
    for (const auto& url : requested_urls_set) {
      service_worker_behavior->add_requested_urls(url.spec());
    }
    if (ui_manager_ && profile_) {
      ui_manager_->SendThreatDetails(profile_, std::move(report));
    }
  }
}

// static
int NotificationTelemetryService::ServiceWorkerInfoCacheSizeForTest() {
  return kNotificationTelemetryServiceWorkerInfoMaxCount;
}

void NotificationTelemetryService::SetShouldSendReportForTest(
    bool should_send) {
  should_send_report_for_test_ = should_send;
}

void NotificationTelemetryService::DatabaseCheckDone(
    ServiceWorkerTelemetryInfo service_worker_info,
    bool allow_listed,
    std::optional<
        SafeBrowsingDatabaseManager::HighConfidenceAllowlistCheckLoggingDetails>
        logging_details) {
  MaybeStoreServiceWorkerInfo(service_worker_info, allow_listed);
}

void NotificationTelemetryService::MaybeStoreServiceWorkerInfo(
    ServiceWorkerTelemetryInfo service_worker_info,
    bool allow_listed) {
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

  // Send `import_script_url` as ServiceWorkerBehavior in
  // ClientSafeBrowsingReportRequest. Given the low volume of these requests
  // no downsampling is applied.
  // Send `import_script_url` as ServiceWorkerBehavior in
  // ClientSafeBrowsingReportRequest. Given the low volume of these requests
  // no downsampling is applied.
  auto report = std::make_unique<CSBRR>();
  report->set_type(CSBRR::SERVICE_WORKER_BEHAVIOR);
  report->set_page_url(report_data.scope.spec());
  CSBRR::ServiceWorkerBehavior* service_worker_behavior =
      report->add_service_worker_behaviors();
  service_worker_behavior->set_scope_url(report_data.scope.spec());
  for (const auto& resource : report_data.resources) {
    service_worker_behavior->add_import_script_urls(resource.spec());
  }
  if (ui_manager_ && profile_) {
    ui_manager_->SendThreatDetails(profile_, std::move(report));
  }
}
}  // namespace safe_browsing
