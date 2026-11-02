// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"

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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
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
    tracker_ = std::make_unique<CdpActorSessionTracker>();
  }

  void TearDown() override {
    tracker_.reset();
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<CdpActorSessionTracker> tracker_;
};

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
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&extension_channel);

  FakeDevToolsAgentHostClient other_client("Other");
  FakeDevToolsChannel other_channel(agent_host.get(), &other_client);
  tracker_->OnClientAttached(&other_channel);
  tracker_->OnCommandHandled(&other_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&other_channel);

  // 2. Passive external attachment that detaches without ever handling a CDP
  // command is NOT recorded on the tab.
  const PeerMetadata passive_ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(500)};
  FakeDevToolsAgentHostClient passive_ws_client("RemoteDebugger",
                                                passive_ws_meta);
  FakeDevToolsChannel passive_ws_channel(agent_host.get(), &passive_ws_client);
  tracker_->OnClientAttached(&passive_ws_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  tracker_->OnClientDetached(&passive_ws_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // 3. "DevTools" (native F12 DevTools UI) records FEATURE_TYPE_DEVTOOLS_UI
  // immediately upon OnClientAttached.
  FakeDevToolsAgentHostClient devtools_client("DevTools");
  FakeDevToolsChannel devtools_channel(agent_host.get(), &devtools_client);
  tracker_->OnClientAttached(&devtools_channel);

  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  ASSERT_TRUE(info->has_browser_feature_info());
  EXPECT_EQ(info->browser_feature_info().feature_type(),
            BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI);

  // 4. External "RemoteDebugger" WebSocket client records on its first
  // OnCommandHandled and takes precedence over FEATURE_TYPE_DEVTOOLS_UI.
  const PeerMetadata ws_meta = {
      .transport_type = PeerMetadata::TransportType::kWebSocket,
      .peer_address = MakeLoopbackEndpoint(501)};
  FakeDevToolsAgentHostClient ws_client("RemoteDebugger", ws_meta);
  FakeDevToolsChannel ws_channel(agent_host.get(), &ws_client);
  tracker_->OnClientAttached(&ws_channel);
  // Before OnCommandHandled, DevTools UI is still the only recorded actor.
  info = CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  EXPECT_TRUE(info->has_browser_feature_info());

  // First command activates the external WebSocket actor; subsequent commands
  // on the same channel are idempotent.
  tracker_->OnCommandHandled(&ws_channel);
  tracker_->OnCommandHandled(&ws_channel);

  info = CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  EXPECT_FALSE(info->has_browser_feature_info());
  ASSERT_TRUE(info->has_external_process_info());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);

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

  std::optional<EventInitiatorInfo> info =
      CdpActorSessionTracker::GetEventInitiatorInfo(web_contents());
  ASSERT_TRUE(info.has_value());
  EXPECT_FALSE(info->has_browser_feature_info());
  ASSERT_TRUE(info->has_external_process_info());
  ASSERT_EQ(info->external_process_info().external_processes_size(), 1);
  EXPECT_EQ(
      info->external_process_info().external_processes(0).transport_type(),
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);

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
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());

  // 3. Root browser session without an associated WebContents is ignored.
  auto browser_host =
      base::MakeRefCounted<FakeDevToolsAgentHost>(nullptr, profile());
  FakeDevToolsChannel browser_channel(browser_host.get(), &pipe_client);
  tracker_->OnClientAttached(&browser_channel);
  tracker_->OnCommandHandled(&browser_channel);
  EXPECT_FALSE(CdpActorSessionTracker::GetEventInitiatorInfo(web_contents())
                   .has_value());
  EXPECT_FALSE(
      CdpActorSessionTracker::GetEventInitiatorInfo(nullptr).has_value());
  tracker_->OnClientDetached(&browser_channel);
}

}  // namespace
}  // namespace enterprise_connectors
