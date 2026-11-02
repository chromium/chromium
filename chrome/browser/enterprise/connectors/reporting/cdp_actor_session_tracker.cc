// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/policy_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_endpoint.h"

namespace enterprise_connectors {

namespace {

using ::chrome::cros::reporting::proto::EventInitiatorInfo;
using ::chrome::cros::reporting::proto::ExternalProcessInfo;

base::ProcessId GetLeafPidFromTree(const ProcessTree& tree) {
  if (tree.process_nodes.empty()) {
    return base::kNullProcessId;
  }
  return tree.process_nodes.front().process_id;
}

size_t GetMaxPendingExtractions() {
  const int param = kCdpAttributionMaxPendingExtractions.Get();
  return param > 0 ? static_cast<size_t>(param)
                   : static_cast<size_t>(
                         kCdpAttributionMaxPendingExtractions.default_value);
}

}  // namespace

CdpActorSessionTracker::TrackedChannel::TrackedChannel(
    SessionCategory category,
    std::optional<net::IPEndPoint> remote_endpoint,
    base::WeakPtr<content::WebContents> web_contents)
    : category(category),
      remote_endpoint(std::move(remote_endpoint)),
      web_contents(std::move(web_contents)) {}

CdpActorSessionTracker::TrackedChannel::TrackedChannel(
    TrackedChannel&&) noexcept = default;

CdpActorSessionTracker::TrackedChannel&
CdpActorSessionTracker::TrackedChannel::operator=(TrackedChannel&&) noexcept =
    default;

CdpActorSessionTracker::TrackedChannel::~TrackedChannel() = default;

CdpActorSessionTracker::CdpActorSessionTracker()
    : extraction_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      pipe_extractor_(base::BindRepeating(&ExtractProcessTreeForPipe)),
      websocket_extractor_(
          base::BindRepeating(&ExtractProcessTreeForWebSocket)) {}

CdpActorSessionTracker::~CdpActorSessionTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

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

// static
std::optional<EventInitiatorInfo> CdpActorSessionTracker::GetEventInitiatorInfo(
    content::WebContents* web_contents) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return std::nullopt;
  }
  web_contents = web_contents->GetOutermostWebContents();
  std::optional<EventInitiatorInfo> info =
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents);
  if (!info.has_value()) {
    return std::nullopt;
  }
  if (!IsEligibleBrowserContext(web_contents->GetBrowserContext())) {
    return std::nullopt;
  }
  return info;
}

void CdpActorSessionTracker::OnClientAttached(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return;
  }

  content::DevToolsAgentHost* agent_host = channel->GetAgentHost();
  if (!agent_host) {
    return;
  }

  content::WebContents* web_contents = agent_host->GetWebContents();
  if (!web_contents) {
    return;
  }
  web_contents = web_contents->GetOutermostWebContents();

  content::BrowserContext* browser_context = agent_host->GetBrowserContext();
  if (!browser_context) {
    browser_context = web_contents->GetBrowserContext();
  }

  if (!IsEligibleBrowserContext(browser_context)) {
    return;
  }

  const ClassifiedClient classified = ClassifyChannel(channel);
  if (classified.category == SessionCategory::kIgnored) {
    return;
  }

  auto [it, inserted] = active_channels_.emplace(
      channel, TrackedChannel(classified.category, classified.remote_endpoint,
                              web_contents->GetWeakPtr()));
  if (!inserted) {
    return;
  }

  if (classified.category == SessionCategory::kDevToolsUi) {
    it->second.has_handled_command = true;
    TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
        ->buffer()
        .OnDevToolsUiAttached();
  }
}

void CdpActorSessionTracker::OnCommandHandled(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel ||
      !base::FeatureList::IsEnabled(kEventReportingCdpAttribution)) {
    return;
  }

  auto it = active_channels_.find(channel);
  if (it == active_channels_.end() || it->second.has_handled_command) {
    return;
  }

  content::WebContents* web_contents = it->second.web_contents.get();
  if (!web_contents) {
    return;
  }

  // TODO(crbug.com/577141574): Evaluate whether passive/read-only CDP commands
  // should be filtered out before activating external actor attribution.
  switch (it->second.category) {
    case SessionCategory::kIgnored:
    case SessionCategory::kDevToolsUi:
      break;
    case SessionCategory::kExternalPipe:
      it->second.has_handled_command = true;
      ActivatePipeSession(web_contents);
      break;
    case SessionCategory::kExternalWebSocket:
      it->second.has_handled_command = true;
      ActivateWebSocketSession(web_contents, it->second.remote_endpoint);
      break;
  }
}

void CdpActorSessionTracker::OnClientDetached(
    content::DevToolsAgentHostClientChannel* channel) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel) {
    return;
  }

  auto it = active_channels_.find(channel);
  if (it == active_channels_.end()) {
    return;
  }

  TrackedChannel tracked = std::move(it->second);
  active_channels_.erase(it);

  content::WebContents* web_contents = tracked.web_contents.get();
  if (!web_contents) {
    return;
  }
  auto* tab_tracker = TabCdpActorTracker::FromWebContents(web_contents);
  if (!tab_tracker) {
    return;
  }

  switch (tracked.category) {
    case SessionCategory::kIgnored:
      break;
    case SessionCategory::kDevToolsUi:
      tab_tracker->buffer().OnDevToolsUiDetached();
      break;
    case SessionCategory::kExternalPipe:
      if (tracked.has_handled_command) {
        tab_tracker->buffer().OnExternalActorDetached(
            ExternalProcessInfo::TRANSPORT_TYPE_PIPE, std::nullopt,
            tracked.resolved_leaf_pid);
      }
      break;
    case SessionCategory::kExternalWebSocket:
      if (tracked.has_handled_command) {
        tab_tracker->buffer().OnExternalActorDetached(
            ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
            tracked.remote_endpoint, tracked.resolved_leaf_pid);
      }
      break;
  }
}

void CdpActorSessionTracker::SetExtractorsForTesting(
    PipeExtractorCallback pipe_extractor,
    WebSocketExtractorCallback websocket_extractor,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pipe_extractor) {
    pipe_extractor_ = std::move(pipe_extractor);
  }
  if (websocket_extractor) {
    websocket_extractor_ = std::move(websocket_extractor);
  }
  if (task_runner) {
    extraction_task_runner_ = std::move(task_runner);
  }
}

size_t CdpActorSessionTracker::GetPendingExtractionsCountForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pending_extractions_count_;
}

void CdpActorSessionTracker::ActivatePipeSession(
    content::WebContents* web_contents) {
  // Register placeholder immediately while background extraction runs.
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorAttached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  StartExtraction(/*remote_endpoint=*/std::nullopt, web_contents->GetWeakPtr());
}

void CdpActorSessionTracker::ActivateWebSocketSession(
    content::WebContents* web_contents,
    std::optional<net::IPEndPoint> remote_endpoint) {
  // Register placeholder immediately while background extraction runs.
  TabCdpActorTracker::GetOrCreateForWebContents(web_contents)
      ->buffer()
      .OnExternalActorAttached(ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
                               remote_endpoint);
  if (remote_endpoint.has_value()) {
    StartExtraction(*remote_endpoint, web_contents->GetWeakPtr());
  }
}

void CdpActorSessionTracker::StartExtraction(
    std::optional<net::IPEndPoint> remote_endpoint,
    base::WeakPtr<content::WebContents> web_contents) {
  if (pending_extractions_count_ >= GetMaxPendingExtractions()) {
    // Bounded queue shedding: drop background walk when queue is saturated.
    return;
  }

  ++pending_extractions_count_;
  base::OnceCallback<ProcessTree()> extractor_task =
      remote_endpoint.has_value()
          ? base::BindOnce(websocket_extractor_, *remote_endpoint)
          : base::OnceCallback<ProcessTree()>(pipe_extractor_);

  extraction_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(extractor_task),
      base::BindOnce(&CdpActorSessionTracker::OnExtractionCompleted,
                     weak_factory_.GetWeakPtr(), std::move(remote_endpoint),
                     std::move(web_contents)));
}

void CdpActorSessionTracker::OnExtractionCompleted(
    std::optional<net::IPEndPoint> remote_endpoint,
    base::WeakPtr<content::WebContents> web_contents,
    ProcessTree tree) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_extractions_count_ > 0) {
    --pending_extractions_count_;
  }

  const SessionCategory expected_category =
      remote_endpoint.has_value() ? SessionCategory::kExternalWebSocket
                                  : SessionCategory::kExternalPipe;
  const ExternalProcessInfo::TransportType transport_type =
      remote_endpoint.has_value()
          ? ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET
          : ExternalProcessInfo::TRANSPORT_TYPE_PIPE;

  const base::ProcessId leaf_pid = GetLeafPidFromTree(tree);
  bool updated_initiating_tab = false;
  for (auto& [channel, tracked] : active_channels_) {
    if (tracked.category == expected_category && tracked.has_handled_command &&
        tracked.remote_endpoint == remote_endpoint) {
      tracked.resolved_leaf_pid = leaf_pid;
      if (content::WebContents* tracked_wc = tracked.web_contents.get()) {
        if (tracked_wc == web_contents.get()) {
          updated_initiating_tab = true;
        }
        if (auto* tab_tracker =
                TabCdpActorTracker::FromWebContents(tracked_wc)) {
          tab_tracker->buffer().UpdateExternalActorProcessTree(
              transport_type, remote_endpoint, tree);
        }
      }
    }
  }

  if (!updated_initiating_tab) {
    if (content::WebContents* wc = web_contents.get()) {
      if (auto* tab_tracker = TabCdpActorTracker::FromWebContents(wc)) {
        tab_tracker->buffer().UpdateExternalActorProcessTree(
            transport_type, remote_endpoint, std::move(tree));
      }
    }
  }
}

}  // namespace enterprise_connectors
