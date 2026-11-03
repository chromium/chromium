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
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
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
    tracker_ = CdpActorSessionTracker::CreateForTest();
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
  // command is NOT recorded on the tab and does not trigger OS extraction.
  const PeerMetadata passive_ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(500)};
  FakeDevToolsAgentHostClient passive_ws_client("RemoteDebugger",
                                                passive_ws_meta);
  FakeDevToolsChannel passive_ws_channel(agent_host.get(), &passive_ws_client);
  tracker_->OnClientAttached(&passive_ws_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(websocket_extractor_calls_, 0);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&passive_ws_channel);

  // 3. "DevTools" (native F12 DevTools UI) records FEATURE_TYPE_DEVTOOLS_UI
  // immediately upon OnClientAttached without spawning any OS process tree
  // walk.
  FakeDevToolsAgentHostClient devtools_client("DevTools");
  FakeDevToolsChannel devtools_channel(agent_host.get(), &devtools_client);
  tracker_->OnClientAttached(&devtools_channel);
  EXPECT_EQ(tracker_->GetPendingExtractionsCountForTesting(), 0u);
  EXPECT_EQ(pipe_extractor_calls_, 0);
  EXPECT_EQ(websocket_extractor_calls_, 0);

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
  EXPECT_EQ(websocket_extractor_calls_, 1);

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
  // the same channel are idempotent.
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

  // Detaching keeps the pipe actor within the 30s post-detach TTL and evicts it
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
  // cleanly without leaking `active_sessions`.
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

  // After the 30s detached TTL expires, both tabs must evict cleanly (proving
  // `pipe_channel_shed` matched `resolved_leaf_pid` on `second_tab`).
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
  tracker_ = CdpActorSessionTracker::CreateForTest();

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
  // detached actor on the tab for the 30s post-detach window.
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
  // in flight, `OnExtractionCompleted` still populates the detached actor's
  // `ProcessTree` on the tab for the 30s post-detach grace window and then
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

}  // namespace
}  // namespace enterprise_connectors
