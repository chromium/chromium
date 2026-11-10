// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/reporting/event_initiator_utils.h"
#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "content/public/browser/web_contents.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_connectors {
namespace {

using ::chrome::cros::reporting::proto::BrowserFeatureInfo;
using ::chrome::cros::reporting::proto::EventInitiatorInfo;
using ::chrome::cros::reporting::proto::ExternalProcessInfo;
using PeerMetadata = ::content::DevToolsAgentHostClient::PeerMetadata;
using SessionCategory = CdpActorSessionTracker::SessionCategory;

net::IPEndPoint MakeLoopbackEndpoint(uint16_t port) {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), port);
}

ProcessTree MakeTestProcessTree(base::ProcessId leaf_pid,
                                std::string leaf_exec,
                                std::string leaf_script = "") {
  return ProcessTree{
      .process_nodes = {{leaf_pid, /*parent_process_id=*/100,
                         std::move(leaf_exec), std::move(leaf_script)}}};
}

class FakeDevToolsAgentHost : public content::DevToolsAgentHost {
 public:
  FakeDevToolsAgentHost(content::WebContents* web_contents,
                        content::BrowserContext* browser_context)
      : web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
        browser_context_(browser_context) {}

  // content::DevToolsAgentHost:
  std::string CreateIOStreamFromData(
      scoped_refptr<base::RefCountedMemory> /*data*/) override {
    return "";
  }
  bool AttachClient(content::DevToolsAgentHostClient* /*client*/) override {
    return true;
  }
  bool DetachClient(content::DevToolsAgentHostClient* /*client*/) override {
    return true;
  }
  bool IsAttached() override { return true; }
  void DispatchProtocolMessage(content::DevToolsAgentHostClient* /*client*/,
                               base::span<const uint8_t> /*message*/) override {
  }
  void InspectElement(content::RenderFrameHost* /*frame_host*/,
                      int /*x*/,
                      int /*y*/) override {}
  std::string GetId() override { return "fake-agent-host"; }
  std::string GetParentId() override { return ""; }
  std::string GetOpenerId() override { return ""; }
  bool CanAccessOpener() override { return false; }
  std::string GetOpenerFrameId() override { return ""; }
  std::string GetParentFrameId() override { return ""; }
  content::WebContents* GetWebContents() override {
    return web_contents_.get();
  }
  content::BrowserContext* GetBrowserContext() override {
    return browser_context_;
  }
  void DisconnectWebContents() override { web_contents_.reset(); }
  void ConnectWebContents(content::WebContents* wc) override {
    web_contents_ = wc ? wc->GetWeakPtr() : nullptr;
  }
  std::string GetType() override {
    return web_contents_ ? content::DevToolsAgentHost::kTypePage
                         : content::DevToolsAgentHost::kTypeBrowser;
  }
  std::string GetTitle() override { return ""; }
  std::string GetDescription() override { return ""; }
  GURL GetURL() override { return GURL(); }
  GURL GetFaviconURL() override { return GURL(); }
  std::string GetFrontendURL() override { return ""; }
  bool Activate() override { return true; }
  void Reload() override {}
  bool Close() override { return true; }
  base::TimeTicks GetLastActivityTime() override { return base::TimeTicks(); }
  void ForceDetachAllSessions() override {}
  content::RenderProcessHost* GetProcessHost() override { return nullptr; }

 private:
  ~FakeDevToolsAgentHost() override = default;

  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
};

class FakeDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  explicit FakeDevToolsAgentHostClient(
      std::string type_for_metrics,
      PeerMetadata peer_metadata = PeerMetadata{})
      : type_for_metrics_(std::move(type_for_metrics)),
        peer_metadata_(std::move(peer_metadata)) {}
  ~FakeDevToolsAgentHostClient() override = default;

  void DispatchProtocolMessage(content::DevToolsAgentHost* /*agent_host*/,
                               base::span<const uint8_t> /*message*/) override {
  }
  void AgentHostClosed(content::DevToolsAgentHost* /*agent_host*/) override {}
  std::string GetTypeForMetrics() override { return type_for_metrics_; }
  PeerMetadata GetPeerMetadata() override { return peer_metadata_; }

 private:
  std::string type_for_metrics_;
  PeerMetadata peer_metadata_;
};

class FakeDevToolsChannel : public content::DevToolsAgentHostClientChannel {
 public:
  explicit FakeDevToolsChannel(content::DevToolsAgentHostClient* client)
      : client_(client) {}
  FakeDevToolsChannel(content::DevToolsAgentHost* agent_host,
                      content::DevToolsAgentHostClient* client)
      : agent_host_(agent_host), client_(client) {}
  ~FakeDevToolsChannel() override = default;

  void DispatchProtocolMessageToClient(
      std::vector<uint8_t> /*message*/) override {}
  content::DevToolsAgentHost* GetAgentHost() override { return agent_host_; }
  content::DevToolsAgentHostClient* GetClient() override { return client_; }

 private:
  raw_ptr<content::DevToolsAgentHost> agent_host_ = nullptr;
  raw_ptr<content::DevToolsAgentHostClient> client_ = nullptr;
};

class CdpActorSessionTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  CdpActorSessionTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitAndEnableFeature(kEventReportingCdpAttribution);
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-dm-token"));
    tracker_ = CdpActorSessionTracker::CreateIfEnabled();
    tracker_->SetExtractorsForTesting(
        base::BindLambdaForTesting([this]() {
          ++pipe_extractor_calls_;
          return MakeTestProcessTree(7001, "/usr/local/bin/node",
                                     "/Users/test/pipe_agent.js");
        }),
        base::BindLambdaForTesting([this](const net::IPEndPoint& endpoint) {
          ++websocket_extractor_calls_;
          return MakeTestProcessTree(
              static_cast<base::ProcessId>(8000 + endpoint.port()),
              "/usr/bin/python3", "/Users/test/ws_agent.py");
        }));
  }

  void TearDown() override {
    tracker_.reset();
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<CdpActorSessionTracker> tracker_;
  int pipe_extractor_calls_ = 0;
  int websocket_extractor_calls_ = 0;
};

TEST_F(CdpActorSessionTrackerTest, CreateIfEnabledRespectsFeatureFlag) {
  EXPECT_NE(CdpActorSessionTracker::CreateIfEnabled(), nullptr);

  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(kEventReportingCdpAttribution);
  EXPECT_EQ(CdpActorSessionTracker::CreateIfEnabled(), nullptr);
}

TEST_F(CdpActorSessionTrackerTest,
       ClassifyChannelHandlesPipeWebSocketDevToolsUiExtensionAndOther) {
  // Null channel / null client are ignored.
  EXPECT_EQ(CdpActorSessionTracker::ClassifyChannel(nullptr).category,
            SessionCategory::kIgnored);
  FakeDevToolsChannel null_client_channel(nullptr);
  EXPECT_EQ(
      CdpActorSessionTracker::ClassifyChannel(&null_client_channel).category,
      SessionCategory::kIgnored);

  // 1. "Extension" client is ignored even if PeerMetadata is set.
  PeerMetadata pipe_meta;
  pipe_meta.transport_type = PeerMetadata::TransportType::kPipe;
  FakeDevToolsAgentHostClient extension_client("Extension", pipe_meta);
  FakeDevToolsChannel extension_channel(&extension_client);
  EXPECT_EQ(
      CdpActorSessionTracker::ClassifyChannel(&extension_channel).category,
      SessionCategory::kIgnored);

  // 2. "Other" internal client with kUnknown transport is ignored.
  FakeDevToolsAgentHostClient other_client("Other");
  FakeDevToolsChannel other_channel(&other_client);
  EXPECT_EQ(CdpActorSessionTracker::ClassifyChannel(&other_channel).category,
            SessionCategory::kIgnored);

  // 3. Built-in "DevTools" UI with kUnknown transport is classified as
  // kDevToolsUi.
  FakeDevToolsAgentHostClient devtools_client("DevTools");
  FakeDevToolsChannel devtools_channel(&devtools_client);
  EXPECT_EQ(CdpActorSessionTracker::ClassifyChannel(&devtools_channel).category,
            SessionCategory::kDevToolsUi);

  // 4. "Pipe" client or forwarded TransportType::kPipe child session is
  // classified as kExternalPipe.
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel(&pipe_client);
  EXPECT_EQ(CdpActorSessionTracker::ClassifyChannel(&pipe_channel).category,
            SessionCategory::kExternalPipe);

  FakeDevToolsAgentHostClient pipe_child_client("DevTools", pipe_meta);
  FakeDevToolsChannel pipe_child_channel(&pipe_child_client);
  EXPECT_EQ(
      CdpActorSessionTracker::ClassifyChannel(&pipe_child_channel).category,
      SessionCategory::kExternalPipe);

  // 5. "RemoteDebugger" WebSocket client and forwarded
  // TransportType::kWebSocket child session are classified as
  // kExternalWebSocket with remote_endpoint.
  const net::IPEndPoint ws_ep = MakeLoopbackEndpoint(501);
  PeerMetadata ws_meta;
  ws_meta.transport_type = PeerMetadata::TransportType::kWebSocket;
  ws_meta.peer_address = ws_ep;
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(&ws_client);
  const auto classified_ws =
      CdpActorSessionTracker::ClassifyChannel(&ws_channel);
  EXPECT_EQ(classified_ws.category, SessionCategory::kExternalWebSocket);
  EXPECT_EQ(classified_ws.remote_endpoint, ws_ep);
}

TEST_F(CdpActorSessionTrackerTest,
       IsEligibleBrowserContextRequiresManagedDeviceAndNonOffTheRecordContext) {
  // Null context is ineligible.
  EXPECT_FALSE(CdpActorSessionTracker::IsEligibleBrowserContext(nullptr));

  // Regular profile on a CBCM-managed device is eligible.
  EXPECT_TRUE(CdpActorSessionTracker::IsEligibleBrowserContext(profile()));

  // OffTheRecord (Incognito) profile is ineligible even on a managed device.
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(CdpActorSessionTracker::IsEligibleBrowserContext(otr_profile));

  // Unmanaged / BYOD device without a valid browser DM token is ineligible.
  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  EXPECT_FALSE(CdpActorSessionTracker::IsEligibleBrowserContext(profile()));

  // Restore browser DM token and verify that an unaffiliated managed user
  // profile on a CBCM device is ineligible when no machine-scoped connector
  // allows device info.
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);
  EXPECT_FALSE(CdpActorSessionTracker::IsEligibleBrowserContext(profile()));

  // Enabling machine-scoped security event reporting on the CBCM device makes
  // the profile eligible (`IncludeDeviceInfo(profile, false)` is true).
  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true,
                                    /*enabled_event_names=*/{},
                                    /*enabled_opt_in_events=*/{},
                                    /*machine_scope=*/true);
  EXPECT_TRUE(CdpActorSessionTracker::IsEligibleBrowserContext(profile()));
}

TEST_F(CdpActorSessionTrackerTest,
       TracksDevToolsUiOnAttachAndExternalActorsOnFirstCommandHandled) {
  auto agent_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  // 1. "Extension" and "Other" clients are ignored on both attach and command.
  FakeDevToolsAgentHostClient extension_client("Extension");
  FakeDevToolsChannel extension_channel(agent_host.get(), &extension_client);
  tracker_->OnClientAttached(&extension_channel);
  tracker_->OnCommandHandled(&extension_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_EQ(websocket_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&extension_channel);

  FakeDevToolsAgentHostClient other_client("Other");
  FakeDevToolsChannel other_channel(agent_host.get(), &other_client);
  tracker_->OnClientAttached(&other_channel);
  tracker_->OnCommandHandled(&other_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_EQ(websocket_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&other_channel);

  // 2. Passive external attachment that detaches without ever handling a CDP
  // command pre-warms the endpoint cache on attach, but is NEVER recorded on
  // the tab and evicts the cache on detach.
  const PeerMetadata passive_ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(500)};
  FakeDevToolsAgentHostClient passive_ws_client("RemoteDebugger",
                                                passive_ws_meta);
  FakeDevToolsChannel passive_ws_channel(agent_host.get(), &passive_ws_client);
  tracker_->OnClientAttached(&passive_ws_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 1u);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(websocket_extractor_calls_, 1);
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(
      MakeLoopbackEndpoint(500)));
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&passive_ws_channel);
  EXPECT_FALSE(tracker_->HasCachedWebSocketProcessTreeForTesting(
      MakeLoopbackEndpoint(500)));

  // 3. "DevTools" (native F12 DevTools UI) records FEATURE_TYPE_DEVTOOLS_UI
  // immediately upon OnClientAttached without spawning any OS process tree
  // walk.
  FakeDevToolsAgentHostClient devtools_client("DevTools");
  FakeDevToolsChannel devtools_channel(agent_host.get(), &devtools_client);
  tracker_->OnClientAttached(&devtools_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_EQ(websocket_extractor_calls_, 1);

  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_browser_feature_info());
  EXPECT_EQ(info->browser_feature_info().feature_type(),
            BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI);

  // 4. External "RemoteDebugger" WebSocket client records on its first
  // OnCommandHandled, takes precedence over FEATURE_TYPE_DEVTOOLS_UI, and
  // resolves its process tree asynchronously.
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(501)};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(agent_host.get(), &ws_client);
  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(websocket_extractor_calls_, 2);

  info = CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  EXPECT_FALSE(info->has_browser_feature_info());
  ASSERT_TRUE(info->has_external_process_info());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  ASSERT_EQ(
      info->external_process_info().external_processes(0).process_nodes_size(),
      1);
  EXPECT_EQ(info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            8501);

  // Detach WebSocket client and advance past 30s TTL; active DevTools UI
  // session becomes the reported initiator again.
  tracker_->OnClientDetached(&ws_channel);
  task_environment()->FastForwardBy(base::Seconds(31));

  info = CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_browser_feature_info());
  EXPECT_EQ(info->browser_feature_info().feature_type(),
            BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI);

  tracker_->OnClientDetached(&devtools_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
}

TEST_F(CdpActorSessionTrackerTest,
       TracksExternalPipeActorOnFirstCommandHandledAndDetachesWithTtl) {
  auto agent_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel(agent_host.get(), &pipe_client);

  // Attaching without handling a command does not record the pipe actor.
  tracker_->OnClientAttached(&pipe_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // First handled command records `TRANSPORT_TYPE_PIPE`; subsequent commands on
  // the same channel refresh `last_command_at` without re-extracting.
  tracker_->OnCommandHandled(&pipe_channel);
  tracker_->OnCommandHandled(&pipe_channel);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(pipe_extractor_calls_, 1);

  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  EXPECT_FALSE(info->has_browser_feature_info());
  ASSERT_TRUE(info->has_external_process_info());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  ASSERT_EQ(
      info->external_process_info().external_processes(0).process_nodes_size(),
      1);
  EXPECT_EQ(info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            7001);

  // Detaching keeps the pipe actor within the 30s command TTL and evicts it
  // once the TTL expires.
  tracker_->OnClientDetached(&pipe_channel);
  task_environment()->FastForwardBy(base::Seconds(20));
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());

  task_environment()->FastForwardBy(base::Seconds(11));
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
}

TEST_F(CdpActorSessionTrackerTest,
       SkipsUnmanagedDevicesOffTheRecordContextsAndNullWebContents) {
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);

  // 1. Unmanaged / BYOD device without a browser DM token is skipped.
  policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
  auto normal_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());
  FakeDevToolsChannel unmanaged_channel(normal_host.get(), &pipe_client);
  tracker_->OnClientAttached(&unmanaged_channel);
  tracker_->OnCommandHandled(&unmanaged_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // 2. Managed device, but OffTheRecord (Incognito) BrowserContext is skipped.
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken("fake-dm-token"));
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  auto otr_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), otr_profile);
  FakeDevToolsChannel otr_channel(otr_host.get(), &pipe_client);
  tracker_->OnClientAttached(&otr_channel);
  tracker_->OnCommandHandled(&otr_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // 3. Root browser session without an associated WebContents is ignored.
  auto browser_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(nullptr, profile());
  FakeDevToolsChannel browser_channel(browser_host.get(), &pipe_client);
  tracker_->OnClientAttached(&browser_channel);
  tracker_->OnCommandHandled(&browser_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  EXPECT_FALSE(
      CdpActorSessionTracker::GetEventInitiatorInfo(nullptr).has_value());
  tracker_->OnClientDetached(&browser_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       SafelyHandlesWebContentsDestructionDuringAsyncExtraction) {
  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(777)};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);

  // Start async extraction on `web_contents()` when first command is handled.
  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 1u);

  // Destroy `web_contents()` while the background extraction task is still in
  // flight.
  DeleteContents();

  // Completing the background task must not UAF/crash and still populates the
  // endpoint cache.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(
      MakeLoopbackEndpoint(777)));
  tracker_->OnClientDetached(&ws_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       RetainsPlaceholderWhileInFlightAndPopulatesCacheOnCompletion) {
  auto manual_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  tracker_->SetExtractorsForTesting(
      CdpActorSessionTracker::PipeExtractorCallback(),
      CdpActorSessionTracker::WebSocketExtractorCallback(), manual_runner);

  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());
  const net::IPEndPoint ws_ep = MakeLoopbackEndpoint(888);
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = ws_ep};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);

  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel(tab_host.get(), &pipe_client);

  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);
  tracker_->OnClientAttached(&pipe_channel);
  tracker_->OnCommandHandled(&pipe_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 2u);

  // While both tasks on `manual_runner` are still in flight, the tab retains
  // placeholder entries (empty `process_nodes`) and neither cache is populated
  // yet.
  EXPECT_FALSE(tracker_->HasCachedPipeProcessTreeForTesting());
  EXPECT_FALSE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));
  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_external_process_info());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 2);
  EXPECT_EQ(
      info->external_process_info().external_processes(0).process_nodes_size(),
      0);
  EXPECT_EQ(
      info->external_process_info().external_processes(1).process_nodes_size(),
      0);

  // Allow the background tasks to finish and reply; both caches and tab entries
  // are populated and `pending_extractions_count_` drops to 0.
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_TRUE(tracker_->HasCachedPipeProcessTreeForTesting());
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));

  tracker_->OnClientDetached(&ws_channel);
  tracker_->OnClientDetached(&pipe_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       ShedsExtractionsWhenPendingQueueExceedsMaxCapacity) {
  auto manual_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  tracker_->SetExtractorsForTesting(
      CdpActorSessionTracker::PipeExtractorCallback(),
      CdpActorSessionTracker::WebSocketExtractorCallback(), manual_runner);

  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  constexpr size_t kTotalConnections = 18;
  std::vector<std::unique_ptr<FakeDevToolsAgentHostClient>> clients;
  std::vector<std::unique_ptr<FakeDevToolsChannel>> channels;
  for (size_t i = 0; i < kTotalConnections; ++i) {
    const PeerMetadata ws_meta = {
        .transport_type = PeerMetadata::TransportType::kWebSocket,
        .peer_address = MakeLoopbackEndpoint(static_cast<uint16_t>(1000 + i))};
    clients.push_back(std::make_unique<FakeDevToolsAgentHostClient>(
        "RemoteDebugger", ws_meta));
    channels.push_back(std::make_unique<FakeDevToolsChannel>(
        tab_host.get(), clients.back().get()));
    tracker_->OnClientAttached(channels.back().get());
    tracker_->OnCommandHandled(channels.back().get());
  }

  // Pending extractions are capped at 16
  // (`kCdpAttributionMaxPendingExtractions`).
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 16u);

  // Attach a queue-shed Pipe session on a second tab while queue is full, then
  // let the first 16 finish and trigger a Pipe extraction on `web_contents()`
  // to verify both Pipe tabs receive the resolved ProcessTree and detach
  // cleanly.
  std::unique_ptr<content::WebContents> second_tab = CreateTestWebContents();
  auto second_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(second_tab.get(), profile());
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client_shed("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel_shed(second_host.get(), &pipe_client_shed);
  tracker_->OnClientAttached(&pipe_channel_shed);
  tracker_->OnCommandHandled(&pipe_channel_shed);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 16u);

  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(websocket_extractor_calls_, 16);

  FakeDevToolsAgentHostClient pipe_client_active("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel_active(tab_host.get(), &pipe_client_active);
  tracker_->OnClientAttached(&pipe_channel_active);
  tracker_->OnCommandHandled(&pipe_channel_active);
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);

  tracker_->OnClientDetached(&pipe_channel_shed);
  tracker_->OnClientDetached(&pipe_channel_active);
  for (const auto& channel : channels) {
    tracker_->OnClientDetached(channel.get());
  }

  // After the 30s command TTL expires, both tabs must evict cleanly.
  task_environment()->FastForwardBy(base::Seconds(31));
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(second_tab.get())
                   .has_value());
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
}

TEST_F(CdpActorSessionTrackerTest,
       DefaultExtractorsRunSafelyOnBackgroundMayBlockSequence) {
  // Replace `tracker_` with a fresh instance that uses the default production
  // extractors (`ExtractProcessTreeForPipe()` and
  // `ExtractProcessTreeForWebSocket()`) on the background `MayBlock()`
  // sequence.
  tracker_.reset();
  tracker_ = CdpActorSessionTracker::CreateIfEnabled();

  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel(tab_host.get(), &pipe_client);

  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(999)};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);

  tracker_->OnClientAttached(&pipe_channel);
  tracker_->OnCommandHandled(&pipe_channel);
  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));

  // Because `TaskEnvironment` uses `MOCK_TIME`, waiting in `RunUntil()` for the
  // real background OS extractor thread can auto-advance mock time past the 30s
  // command TTL. Issue a command at the current mock timestamp before querying.
  tracker_->OnCommandHandled(&pipe_channel);
  tracker_->OnCommandHandled(&ws_channel);

  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_TRUE(tracker_->HasCachedPipeProcessTreeForTesting());
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(
      MakeLoopbackEndpoint(999)));

  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_external_process_info());
  EXPECT_EQ(info->external_process_info().external_processes_size(), 2);

  tracker_->OnClientDetached(&ws_channel);
  tracker_->OnClientDetached(&pipe_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       CachesPipeAcrossBrowserLifetimeAndWebSocketWhileEndpointActive) {
  auto manual_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  tracker_->SetExtractorsForTesting(
      CdpActorSessionTracker::PipeExtractorCallback(),
      CdpActorSessionTracker::WebSocketExtractorCallback(), manual_runner);

  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  // 1. Pipe extraction runs once and is cached for the entire browser lifetime.
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client_1("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel_1(tab_host.get(), &pipe_client_1);
  tracker_->OnClientAttached(&pipe_channel_1);
  tracker_->OnCommandHandled(&pipe_channel_1);
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 1);
  EXPECT_TRUE(tracker_->HasCachedPipeProcessTreeForTesting());

  // Child session (Target.attachToTarget) with forwarded `PeerMetadata::kPipe`
  // reuses the cached Pipe `ProcessTree` without spawning a second OS walk,
  // even after detaching and advancing past 30s.
  tracker_->OnClientDetached(&pipe_channel_1);
  task_environment()->FastForwardBy(base::Seconds(120));
  EXPECT_TRUE(tracker_->HasCachedPipeProcessTreeForTesting());

  FakeDevToolsAgentHostClient pipe_child_client("DevTools", pipe_meta);
  FakeDevToolsChannel pipe_child_channel(tab_host.get(), &pipe_child_client);
  tracker_->OnClientAttached(&pipe_child_channel);
  tracker_->OnCommandHandled(&pipe_child_channel);
  EXPECT_EQ(pipe_extractor_calls_, 1);
  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            7001);
  tracker_->OnClientDetached(&pipe_child_channel);
  task_environment()->FastForwardBy(base::Seconds(31));

  // 2. WebSocket extraction is cached per `net::IPEndPoint` across active
  // channels sharing that endpoint and evicted once the last channel for that
  // endpoint detaches.
  const net::IPEndPoint ws_ep = MakeLoopbackEndpoint(600);
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = ws_ep};

  FakeDevToolsAgentHostClient ws_root_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_root_channel(tab_host.get(), &ws_root_client);
  FakeDevToolsAgentHostClient ws_child_client("DevTools", ws_meta);
  FakeDevToolsChannel ws_child_channel(tab_host.get(), &ws_child_client);

  tracker_->OnClientAttached(&ws_root_channel);
  tracker_->OnCommandHandled(&ws_root_channel);
  tracker_->OnClientAttached(&ws_child_channel);
  tracker_->OnCommandHandled(&ws_child_channel);
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(websocket_extractor_calls_, 1);
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));

  // Detaching one channel while another channel on the same endpoint remains
  // active preserves the endpoint cache.
  tracker_->OnClientDetached(&ws_child_channel);
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));

  // Attaching another channel while `ws_root_channel` is still active hits the
  // cache without re-extracting.
  FakeDevToolsChannel ws_reattach_channel(tab_host.get(), &ws_child_client);
  tracker_->OnClientAttached(&ws_reattach_channel);
  tracker_->OnCommandHandled(&ws_reattach_channel);
  EXPECT_EQ(websocket_extractor_calls_, 1);

  // Detaching all channels for `ws_ep` immediately evicts the endpoint cache so
  // a new connection reusing the same ip:port cannot pick up the previous
  // connection's process tree, while `TabCdpActorTracker` still retains the
  // actor on the tab for the 30s command TTL window.
  tracker_->OnClientDetached(&ws_reattach_channel);
  tracker_->OnClientDetached(&ws_root_channel);
  EXPECT_FALSE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));

  FakeDevToolsChannel ws_fresh_channel(tab_host.get(), &ws_root_client);
  tracker_->OnClientAttached(&ws_fresh_channel);
  tracker_->OnCommandHandled(&ws_fresh_channel);
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(websocket_extractor_calls_, 2);
  tracker_->OnClientDetached(&ws_fresh_channel);
  task_environment()->FastForwardBy(base::Seconds(31));

  // If the last WebSocket channel detaches while background extraction is still
  // in flight, `OnExtractionCompleted` still populates the actor's
  // `ProcessTree` on the tab for the 30s command TTL window and then
  // immediately evicts the endpoint from `websocket_cache_`.
  FakeDevToolsChannel ws_inflight_detach_channel(tab_host.get(),
                                                 &ws_root_client);
  tracker_->OnClientAttached(&ws_inflight_detach_channel);
  tracker_->OnCommandHandled(&ws_inflight_detach_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 1u);
  tracker_->OnClientDetached(&ws_inflight_detach_channel);

  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(websocket_extractor_calls_, 3);
  EXPECT_FALSE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));

  info = CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  ASSERT_EQ(
      info->external_process_info().external_processes(0).process_nodes_size(),
      1);
  EXPECT_EQ(info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            8600);
}

TEST_F(CdpActorSessionTrackerTest,
       PreWarmsCacheOnClientAttachedBeforeFirstCommandHandled) {
  test::SetOnSecurityEventReporting(profile()->GetPrefs(), /*enabled=*/true);
  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  // 1. Pipe (`--remote-debugging-pipe`): `OnClientAttached` starts warming the
  // Pipe cache immediately while keeping the tab unattributed until the first
  // command is handled.
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("RemoteDebugger", pipe_meta);
  FakeDevToolsChannel pipe_channel(tab_host.get(), &pipe_client);

  tracker_->OnClientAttached(&pipe_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 1u);
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents()).has_value());
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // Complete the background walk during the handshake before any command is
  // handled; the tab remains unattributed when 0 commands have been handled.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(pipe_extractor_calls_, 1);
  EXPECT_TRUE(tracker_->HasCachedPipeProcessTreeForTesting());
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents()).has_value());
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // When the first `OnCommandHandled` fires, the tab's `CdpActorBuffer` is
  // populated immediately with the full cached `process_nodes` without
  // triggering another extraction.
  tracker_->OnCommandHandled(&pipe_channel);
  EXPECT_EQ(pipe_extractor_calls_, 1);
  std::optional<EventInitiatorInfo> pipe_info =
      GetActiveEventInitiatorInfo(web_contents());
  ASSERT_TRUE(pipe_info.has_value());
  ASSERT_EQ(pipe_info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      pipe_info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  ASSERT_EQ(pipe_info->external_process_info()
                .external_processes(0)
                .process_nodes_size(),
            1);
  EXPECT_EQ(pipe_info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            7001);

  tracker_->OnClientDetached(&pipe_channel);
  task_environment()->FastForwardBy(base::Seconds(31));

  // 2. WebSocket: `OnClientAttached` starts warming the endpoint cache before
  // `OnCommandHandled`, keeps the tab unattributed while 0 commands have been
  // handled, and populates `process_nodes` immediately on the first command.
  const net::IPEndPoint ws_ep = MakeLoopbackEndpoint(700);
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = ws_ep};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);

  tracker_->OnClientAttached(&ws_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 1u);
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents()).has_value());
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return tracker_->GetPendingExtractionsCountForTesting() == 0u;
  }));
  EXPECT_EQ(websocket_extractor_calls_, 1);
  EXPECT_TRUE(tracker_->HasCachedWebSocketProcessTreeForTesting(ws_ep));
  EXPECT_FALSE(GetActiveEventInitiatorInfo(web_contents()).has_value());
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  tracker_->OnCommandHandled(&ws_channel);
  EXPECT_EQ(websocket_extractor_calls_, 1);
  std::optional<EventInitiatorInfo> ws_info =
      GetActiveEventInitiatorInfo(web_contents());
  ASSERT_TRUE(ws_info.has_value());
  ASSERT_EQ(ws_info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      ws_info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  ASSERT_EQ(ws_info->external_process_info()
                .external_processes(0)
                .process_nodes_size(),
            1);
  EXPECT_EQ(ws_info->external_process_info()
                .external_processes(0)
                .process_nodes(0)
                .process_id(),
            8700);

  tracker_->OnClientDetached(&ws_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       RecordsUmaHistogramsForWalksCollisionsAndCommandTtlLookups) {
  base::HistogramTester histograms;
  auto manual_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  bool return_empty_ws_tree = false;
  tracker_->SetExtractorsForTesting(
      base::BindLambdaForTesting([&]() {
        ++pipe_extractor_calls_;
        task_environment()->AdvanceClock(base::Milliseconds(15));
        ProcessTree tree;
        tree.process_nodes.emplace_back(
            /*process_id=*/7001, /*parent_process_id=*/100, "/usr/bin/node",
            "--pipe-runner");
        return tree;
      }),
      base::BindLambdaForTesting([&](const net::IPEndPoint& endpoint) {
        ++websocket_extractor_calls_;
        task_environment()->AdvanceClock(base::Milliseconds(30));
        ProcessTree tree;
        if (!return_empty_ws_tree) {
          tree.process_nodes.emplace_back(
              /*process_id=*/8000 + endpoint.port(),
              /*parent_process_id=*/100, "/usr/bin/python3", "--ws-runner");
        }
        return tree;
      }),
      manual_runner);

  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());

  // 1. Attach a Pipe actor, query `EventInitiatorInfo` while the walk is still
  // in flight (`HasEmptyProcessTree` = true), advance mock queue time by 10ms,
  // and run the 15ms walk task (`ProcessTreeWalk.Latency` = 15ms excluding
  // queue time).
  const PeerMetadata pipe_meta = {.transport_type =
                                      PeerMetadata::TransportType::kPipe};
  FakeDevToolsAgentHostClient pipe_client("Pipe", pipe_meta);
  FakeDevToolsChannel pipe_channel(tab_host.get(), &pipe_client);
  tracker_->OnClientAttached(&pipe_channel);
  tracker_->OnCommandHandled(&pipe_channel);

  std::optional<EventInitiatorInfo> inflight_info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(inflight_info.has_value());
  histograms.ExpectUniqueSample(
      "Enterprise.CdpAttribution.UploadSecurityEvent.HasEmptyProcessTree", true,
      1);

  task_environment()->FastForwardBy(base::Milliseconds(10));
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);

  histograms.ExpectUniqueTimeSample(
      "Enterprise.CdpAttribution.ProcessTreeWalk.Latency",
      base::Milliseconds(15), 1);
  histograms.ExpectUniqueSample(
      "Enterprise.CdpAttribution.ProcessTreeWalk.IsEmpty", false, 1);

  // 2. Attach a second Pipe session that hits the cache immediately without
  // spawning a new process tree walk.
  FakeDevToolsAgentHostClient pipe_child_client("DevTools", pipe_meta);
  FakeDevToolsChannel pipe_child_channel(tab_host.get(), &pipe_child_client);
  tracker_->OnClientAttached(&pipe_child_channel);
  tracker_->OnCommandHandled(&pipe_child_channel);

  histograms.ExpectTotalCount(
      "Enterprise.CdpAttribution.ProcessTreeWalk.Latency", 1);

  // 3. Advance mock time by 5ms and attach a concurrent WebSocket actor on the
  // same tab (2 external actors) after 10ms queue wait + 30ms walk execution so
  // the WebSocket actor has the most recent `last_command_at`.
  task_environment()->FastForwardBy(base::Milliseconds(5));
  const net::IPEndPoint ws_ep = MakeLoopbackEndpoint(3100);
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = ws_ep};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);
  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);

  task_environment()->FastForwardBy(base::Milliseconds(10));
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);

  histograms.ExpectTotalCount(
      "Enterprise.CdpAttribution.ProcessTreeWalk.Latency", 2);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.ProcessTreeWalk.Latency", 30, 1);
  histograms.ExpectUniqueSample(
      "Enterprise.CdpAttribution.ProcessTreeWalk.IsEmpty", false, 2);

  // Querying `EventInitiatorInfo` with 2 resolved active external actors
  // records `BuildLatency2`, `ActorCollisionCount2` (sample = 2), `Category`
  // (`kExternalWebSocket`, the actor with the most recent `last_command_at`),
  // `HasEmptyProcessTree` (false), and `LastCommandEventDelay` for both actors.
  std::optional<EventInitiatorInfo> active_info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(active_info.has_value());
  histograms.ExpectTotalCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.BuildLatency2", 2);
  histograms.ExpectBucketCount("Enterprise.CdpAttribution.ActorCollisionCount2",
                               2, 1);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.Category",
      SessionCategory::kExternalPipe, 1);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.Category",
      SessionCategory::kExternalWebSocket, 1);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.HasEmptyProcessTree",
      false, 1);
  histograms.ExpectTotalCount("Enterprise.CdpAttribution.LastCommandEventDelay",
                              3);

  // 4. Attach built-in DevTools UI alongside the 2 external actors (3 active
  // actors total across external CDP and DevTools UI), advance mock time by 5
  // seconds (within the 30s command TTL window), and query `EventInitiatorInfo`
  // again.
  FakeDevToolsAgentHostClient devtools_ui_client("DevTools");
  FakeDevToolsChannel devtools_ui_channel(tab_host.get(), &devtools_ui_client);
  tracker_->OnClientAttached(&devtools_ui_channel);
  tracker_->OnClientDetached(&ws_channel);
  task_environment()->FastForwardBy(base::Seconds(5));

  std::optional<EventInitiatorInfo> ttl_info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(ttl_info.has_value());
  histograms.ExpectTotalCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.BuildLatency2", 3);
  histograms.ExpectBucketCount("Enterprise.CdpAttribution.ActorCollisionCount2",
                               3, 1);
  histograms.ExpectTotalCount("Enterprise.CdpAttribution.LastCommandEventDelay",
                              5);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.LastCommandEventDelay", 5000, 2);

  // 5. Detach the Pipe and DevTools UI sessions and expire the command TTL
  // window, then attach a WebSocket actor whose walk returns an empty
  // ProcessTree and verify `ProcessTreeWalk.IsEmpty` (true),
  // `HasEmptyProcessTree` (true), and `UploadSecurityEvent.Category`
  // (`kExternalWebSocket`).
  tracker_->OnClientDetached(&devtools_ui_channel);
  tracker_->OnClientDetached(&pipe_child_channel);
  tracker_->OnClientDetached(&pipe_channel);
  task_environment()->FastForwardBy(base::Seconds(31));

  return_empty_ws_tree = true;
  const net::IPEndPoint empty_ws_ep = MakeLoopbackEndpoint(3200);
  const PeerMetadata empty_ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = empty_ws_ep};
  FakeDevToolsAgentHostClient empty_ws_client("RemoteDebugger", empty_ws_meta);
  FakeDevToolsChannel empty_ws_channel(tab_host.get(), &empty_ws_client);
  tracker_->OnClientAttached(&empty_ws_channel);
  tracker_->OnCommandHandled(&empty_ws_channel);
  manual_runner->RunPendingTasks();
  task_environment()->FastForwardBy(base::TimeDelta());

  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.ProcessTreeWalk.IsEmpty", true, 1);
  ASSERT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.HasEmptyProcessTree", true,
      2);
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.Category",
      SessionCategory::kExternalWebSocket, 3);

  tracker_->OnClientDetached(&empty_ws_channel);

  // 6. Expire the command TTL window, attach a built-in DevTools UI session,
  // and verify `UploadSecurityEvent.Category` records `kDevToolsUi` and
  // `ActorCollisionCount2` records 1.
  task_environment()->FastForwardBy(base::Seconds(31));
  FakeDevToolsAgentHostClient ui_client("DevTools");
  FakeDevToolsChannel ui_channel(tab_host.get(), &ui_client);
  tracker_->OnClientAttached(&ui_channel);
  tracker_->OnCommandHandled(&ui_channel);

  ASSERT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());
  histograms.ExpectBucketCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.Category",
      SessionCategory::kDevToolsUi, 1);
  histograms.ExpectTotalCount(
      "Enterprise.CdpAttribution.UploadSecurityEvent.Category", 5);
  histograms.ExpectBucketCount("Enterprise.CdpAttribution.ActorCollisionCount2",
                               1, 3);
  histograms.ExpectTotalCount("Enterprise.CdpAttribution.ActorCollisionCount2",
                              5);
  tracker_->OnClientDetached(&ui_channel);
}

TEST_F(CdpActorSessionTrackerTest,
       IdleAttachedExternalSessionExpiresAfterTtlAndReactivatesOnCommand) {
  auto tab_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(web_contents(), profile());
  const net::IPEndPoint endpoint = MakeLoopbackEndpoint(55000);
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = endpoint};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(tab_host.get(), &ws_client);
  FakeDevToolsChannel ws_channel_2(tab_host.get(), &ws_client);

  // 1. Attach -> handle command -> attributes.
  tracker_->OnClientAttached(&ws_channel);
  tracker_->OnClientAttached(&ws_channel_2);
  tracker_->OnCommandHandled(&ws_channel);
  ASSERT_TRUE(base::test::RunUntil([this, &endpoint]() {
    return tracker_->HasCachedWebSocketProcessTreeForTesting(endpoint);
  }));
  EXPECT_EQ(websocket_extractor_calls_, 1);
  TabCdpActorTracker* tab_tracker =
      TabCdpActorTracker::FromWebContents(web_contents());
  ASSERT_NE(nullptr, tab_tracker);
  EXPECT_EQ(1u, tab_tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());

  // 2. While the channel remains attached and idle, attribution remains active
  // at t=29s and is pruned at t=30s.
  task_environment()->FastForwardBy(base::Seconds(29));
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  EXPECT_TRUE(tab_tracker->buffer().GetExternalActorsForTesting().empty());

  // 3. Handling another command on the same channel after inactivity
  // re-inserts the actor from the endpoint cache and attributes again without
  // a second OS walk.
  task_environment()->FastForwardBy(base::Seconds(90));
  tracker_->OnCommandHandled(&ws_channel);
  EXPECT_EQ(websocket_extractor_calls_, 1);
  EXPECT_EQ(1u, tab_tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());

  // 4. Detach `ws_channel` -> wait 30s so the entry is pruned -> handle
  // command on `ws_channel_2` -> re-inserted and attributed again.
  task_environment()->FastForwardBy(base::Seconds(20));
  tracker_->OnCommandHandled(&ws_channel);
  tracker_->OnClientDetached(&ws_channel);

  task_environment()->FastForwardBy(base::Seconds(29));
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  EXPECT_TRUE(tab_tracker->buffer().GetExternalActorsForTesting().empty());

  tracker_->OnCommandHandled(&ws_channel_2);
  EXPECT_EQ(websocket_extractor_calls_, 1);
  EXPECT_EQ(1u, tab_tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_TRUE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                  .has_value());
  tracker_->OnClientDetached(&ws_channel_2);
}

}  // namespace
}  // namespace enterprise_connectors
