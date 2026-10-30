// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"

#include <optional>
#include <string>

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "net/base/ip_endpoint.h"

namespace enterprise_connectors {

// static
CdpActorSessionTracker::ClassifiedClient
CdpActorSessionTracker::ClassifyChannel(
    content::DevToolsAgentHostClientChannel* channel) {
  ClassifiedClient result;
  if (!channel) {
    return result;
  }
  content::DevToolsAgentHostClient* client = channel->GetClient();
  if (!client) {
    return result;
  }

  const std::string client_type = client->GetTypeForMetrics();
  if (client_type == "Extension") {
    return result;
  }

  const content::DevToolsAgentHostClient::PeerMetadata peer_metadata =
      client->GetPeerMetadata();
  using TransportType =
      content::DevToolsAgentHostClient::PeerMetadata::TransportType;

  switch (peer_metadata.transport_type) {
    case TransportType::kPipe:
      result.category = SessionCategory::kExternalPipe;
      return result;
    case TransportType::kWebSocket:
      result.category = SessionCategory::kExternalWebSocket;
      result.remote_endpoint = peer_metadata.peer_address;
      return result;
    case TransportType::kUnknown:
      if (client_type == "DevTools") {
        result.category = SessionCategory::kDevToolsUi;
      }
      // Internal "Other" clients with kUnknown transport are ignored.
      return result;
  }
}

// static
bool CdpActorSessionTracker::IsEligibleBrowserContext(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->IsOffTheRecord() || profile->IsSystemProfile()) {
    return false;
  }

  if (!policy::GetDMToken(profile).is_valid()) {
    return false;
  }

  auto* connectors = ConnectorsServiceFactory::GetForBrowserContext(profile);
  if (!connectors) {
    return false;
  }

  const std::optional<ReportingSettings> reporting =
      connectors->GetReportingSettings();
  const std::optional<policy::PolicyScope> url_scope =
      connectors->GetDMTokenForRealTimeUrlCheck().has_value()
          ? connectors->GetRealtimeUrlCheckScope()
          : std::nullopt;

  if (reporting.has_value() &&
      IncludeDeviceInfo(profile, reporting->per_profile)) {
    return true;
  }
  if (url_scope.has_value() &&
      IncludeDeviceInfo(profile, *url_scope == policy::POLICY_SCOPE_USER)) {
    return true;
  }

  // During early startup (e.g. `--remote-debugging-pipe` with a fresh
  // `--user-data-dir` on a CBCM-managed device), async machine cloud policies
  // may not have populated connector prefs yet. Block unaffiliated managed user
  // profiles while allowing unmanaged/affiliated profiles on the CBCM device
  // until connector settings load.
  if (const auto* policy_connector = profile->GetProfilePolicyConnector();
      policy_connector && policy_connector->IsManaged()) {
    return IncludeDeviceInfo(profile, /*per_profile=*/true);
  }
  return !reporting.has_value() && !url_scope.has_value();
}

}  // namespace enterprise_connectors
