// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/cdp_actor_session_tracker.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/devtools_agent_host_client_channel.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

using PeerMetadata = ::content::DevToolsAgentHostClient::PeerMetadata;
using SessionCategory = CdpActorSessionTracker::SessionCategory;

net::IPEndPoint MakeLoopbackEndpoint(uint16_t port) {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), port);
}

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
  ~FakeDevToolsChannel() override = default;

  void DispatchProtocolMessageToClient(
      std::vector<uint8_t> /*message*/) override {}
  content::DevToolsAgentHost* GetAgentHost() override { return nullptr; }
  content::DevToolsAgentHostClient* GetClient() override { return client_; }

 private:
  raw_ptr<content::DevToolsAgentHostClient> client_;
};

class CdpActorSessionTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-dm-token"));
  }

  void TearDown() override {
    policy::SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    ChromeRenderViewHostTestHarness::TearDown();
  }
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

}  // namespace
}  // namespace enterprise_connectors
