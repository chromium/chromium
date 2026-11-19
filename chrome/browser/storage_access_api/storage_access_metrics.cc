// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_metrics.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/private_insights/private_insights_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "components/private_insights/events/storage_access_log_event.pb.h"
#include "components/private_insights/private_insights_features.h"
#include "components/private_insights/private_insights_service.h"
#include "components/version_info/version_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace storage_access_api {

namespace {

using ::net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
using ::net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES;
using ::private_insights::events::StorageAccessLogEvent;

// Returns the eTLD+1 to report for `requesting_site`, or std::nullopt if the
// site is not reportable (see `RecordOutcomeToPrivateInsights()`).
std::optional<std::string> GetPrivateInsightsDomain(
    const net::SchemefulSite& requesting_site) {
  const GURL url = requesting_site.GetURL();
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }
  // Empty or missing registry: IP address, no host, single-label host, unknown
  // TLD, or the host is itself a registry.
  ASSIGN_OR_RETURN(
      std::string_view registry,
      net::registry_controlled_domains::GetRegistry(
          url, EXCLUDE_UNKNOWN_REGISTRIES, EXCLUDE_PRIVATE_REGISTRIES));
  if (registry.empty()) {
    return std::nullopt;
  }
  std::string registrable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, EXCLUDE_PRIVATE_REGISTRIES);
  if (registrable_domain.empty()) {
    return std::nullopt;
  }
  return registrable_domain;
}

StorageAccessLogEvent::Outcome ToPrivateInsightsOutcome(
    RequestOutcome outcome) {
  // Mapped explicitly, never cast: the values don't line up, and the proto
  // reserves 0 for UNSPECIFIED, which the server pipeline drops.
  switch (outcome) {
    case RequestOutcome::kGrantedByAllowance:
      return StorageAccessLogEvent::GRANTED_BY_ALLOWANCE;
    case RequestOutcome::kGrantedByUser:
      return StorageAccessLogEvent::GRANTED_BY_USER;
    case RequestOutcome::kDeniedByUser:
      return StorageAccessLogEvent::DENIED_BY_USER;
    case RequestOutcome::kDeniedByPrerequisites:
      return StorageAccessLogEvent::DENIED_BY_PREREQUISITES;
    case RequestOutcome::kDismissedByUser:
      return StorageAccessLogEvent::DISMISSED_BY_USER;
    case RequestOutcome::kReusedPreviousDecision:
      return StorageAccessLogEvent::REUSED_PREVIOUS_DECISION;
    case RequestOutcome::kDeniedByTopLevelInteractionHeuristic:
      return StorageAccessLogEvent::DENIED_BY_TOP_LEVEL_INTERACTION_HEURISTIC;
    case RequestOutcome::kAllowedByCookieSettings:
      return StorageAccessLogEvent::ALLOWED_BY_COOKIE_SETTINGS;
    case RequestOutcome::kReusedImplicitGrant:
      return StorageAccessLogEvent::REUSED_IMPLICIT_GRANT;
    case RequestOutcome::kDeniedByCookieSettings:
      return StorageAccessLogEvent::DENIED_BY_COOKIE_SETTINGS;
    case RequestOutcome::kAllowedBySameSite:
      return StorageAccessLogEvent::ALLOWED_BY_SAME_SITE;
    case RequestOutcome::kDeniedAborted:
      return StorageAccessLogEvent::DENIED_ABORTED;
    case RequestOutcome::kAllowedByFedCM:
      return StorageAccessLogEvent::ALLOWED_BY_FEDCM;
  }
}

std::optional<StorageAccessLogEvent> BuildStorageAccessLogEvent(
    RequestOutcome outcome,
    const net::SchemefulSite& requesting_site) {
  ASSIGN_OR_RETURN(std::string registrable_domain,
                   GetPrivateInsightsDomain(requesting_site));
  StorageAccessLogEvent event;
  // Only the channel is reported for Storage Access.
  event.mutable_system_profile()->set_chrome_channel(
      version_info::GetChannelString(chrome::GetChannel()));
  event.set_domain(std::move(registrable_domain));
  event.set_outcome(ToPrivateInsightsOutcome(outcome));
  return event;
}

}  // namespace

void RecordOutcomeToPrivateInsights(content::BrowserContext* browser_context,
                                    RequestOutcome outcome,
                                    const net::SchemefulSite& requesting_site) {
  if (!base::FeatureList::IsEnabled(
          private_insights::kPrivateInsightsStorageAccess)) {
    return;
  }
  private_insights::PrivateInsightsService* service =
      private_insights::PrivateInsightsServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context));
  if (!service) {
    return;
  }
  ASSIGN_OR_RETURN(StorageAccessLogEvent event,
                   BuildStorageAccessLogEvent(outcome, requesting_site), [] {});
  service->LogStorageAccessEvent(std::move(event));
}

}  // namespace storage_access_api
