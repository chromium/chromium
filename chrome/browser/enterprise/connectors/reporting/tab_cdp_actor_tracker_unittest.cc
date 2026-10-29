// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"

#include <optional>
#include <string>
#include <utility>

#include "base/process/process_handle.h"
#include "base/test/scoped_feature_list.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

using ::chrome::cros::reporting::proto::EventInitiatorInfo;
using ::chrome::cros::reporting::proto::ExternalProcessInfo;

ProcessTree MakeProcessTree(
    base::ProcessId leaf_pid,
    base::ProcessId parent_pid,
    std::string executable_path = "/usr/local/bin/node",
    std::string script_path = "/Users/<USER>/agent.js") {
  ProcessTree tree;
  tree.process_nodes.emplace_back(
      leaf_pid, parent_pid, std::move(executable_path), std::move(script_path));

  if (parent_pid > 0) {
    tree.process_nodes.emplace_back(parent_pid, 1, "/bin/zsh", "");
  }
  return tree;
}

net::IPEndPoint MakeLoopbackEndpoint(uint16_t port) {
  return net::IPEndPoint(net::IPAddress::IPv4Localhost(), port);
}

class CdpActorBufferTest : public testing::Test {
 public:
  CdpActorBufferTest() {
    scoped_feature_list_.InitAndEnableFeature(kEventReportingCdpAttribution);
  }
  ~CdpActorBufferTest() override = default;

  CdpActorBuffer* buffer() { return &buffer_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  CdpActorBuffer buffer_;
};

TEST_F(CdpActorBufferTest, EmptyBufferReturnsNullopt) {
  EXPECT_EQ(std::nullopt, buffer()->BuildEventInitiatorInfo());
}

TEST_F(CdpActorBufferTest, FeatureDisabledReturnsNullopt) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(kEventReportingCdpAttribution);

  buffer()->OnExternalActorAttached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE,
                                    MakeProcessTree(1001, 500));
  EXPECT_EQ(std::nullopt, buffer()->BuildEventInitiatorInfo());
}

TEST_F(CdpActorBufferTest, ExternalPipeAndWebSocketProtoSerialization) {
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE,
      MakeProcessTree(1001, 500, "/usr/local/bin/node",
                      "/Users/<USER>/puppeteer.js"));
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(51001),
      MakeProcessTree(2002, 0, "/usr/bin/python3", "/Users/<USER>/cdp.py"));

  std::optional<EventInitiatorInfo> initiator =
      buffer()->BuildEventInitiatorInfo();
  ASSERT_TRUE(initiator.has_value());
  ASSERT_TRUE(initiator->has_external_process_info());
  EXPECT_FALSE(initiator->has_browser_feature_info());

  const auto& processes =
      initiator->external_process_info().external_processes();
  ASSERT_EQ(2, processes.size());

  EXPECT_EQ(ExternalProcessInfo::TRANSPORT_TYPE_PIPE,
            processes[0].transport_type());
  ASSERT_EQ(2, processes[0].process_nodes_size());
  EXPECT_EQ(1001, processes[0].process_nodes(0).process_id());
  EXPECT_EQ(500, processes[0].process_nodes(0).parent_process_id());
  EXPECT_EQ("/usr/local/bin/node",
            processes[0].process_nodes(0).executable_path());
  EXPECT_EQ("/Users/<USER>/puppeteer.js",
            processes[0].process_nodes(0).script_path());
  EXPECT_EQ(500, processes[0].process_nodes(1).process_id());

  EXPECT_EQ(ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
            processes[1].transport_type());
  ASSERT_EQ(1, processes[1].process_nodes_size());
  EXPECT_EQ(2002, processes[1].process_nodes(0).process_id());
  EXPECT_EQ("/usr/bin/python3",
            processes[1].process_nodes(0).executable_path());
  EXPECT_EQ("/Users/<USER>/cdp.py",
            processes[1].process_nodes(0).script_path());
}

TEST_F(CdpActorBufferTest, MultiTargetRefCounting) {
  const net::IPEndPoint endpoint = MakeLoopbackEndpoint(52000);
  const ProcessTree tree = MakeProcessTree(3001, 100);

  // Simulate root page session + two child frame/worker auto-attached sessions
  // from the same WebSocket connection.
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);

  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(3u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting()[0].IsAttached());

  // Detaching two child sessions leaves the actor actively attached (refcount
  // 1).
  buffer()->OnExternalActorDetached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);
  buffer()->OnExternalActorDetached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);
  EXPECT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(1u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_TRUE(buffer()->BuildEventInitiatorInfo().has_value());

  // Detaching the final session removes the active actor.
  buffer()->OnExternalActorDetached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);
  EXPECT_EQ(std::nullopt, buffer()->BuildEventInitiatorInfo());
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting().empty());
}

TEST_F(CdpActorBufferTest, UpdateExternalActorProcessTreeInPlace) {
  const net::IPEndPoint endpoint = MakeLoopbackEndpoint(54000);

  // Register actor initially with an empty `ProcessTree` (while async
  // extraction is in flight).
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting()[0].process_tree.empty());

  // Complete async extraction -> updates `ProcessTree` in place.
  buffer()->UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint,
      MakeProcessTree(6001, 6000));
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(6001, buffer()->GetExternalActorsForTesting()[0].leaf_pid);

  std::optional<EventInitiatorInfo> initiator =
      buffer()->BuildEventInitiatorInfo();
  ASSERT_TRUE(initiator.has_value());
  ASSERT_EQ(1, initiator->external_process_info().external_processes_size());
  ASSERT_EQ(2, initiator->external_process_info()
                   .external_processes(0)
                   .process_nodes_size());
  EXPECT_EQ(6001, initiator->external_process_info()
                      .external_processes(0)
                      .process_nodes(0)
                      .process_id());

  // Updating with an empty `ProcessTree` is a no-op and preserves the resolved
  // tree.
  buffer()->UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, ProcessTree{});
  EXPECT_EQ(6001, buffer()->GetExternalActorsForTesting()[0].leaf_pid);
}

TEST_F(CdpActorBufferTest, PipeRefCountingUpdateAndDetach) {
  // Attach root and child pipe sessions initially with empty `ProcessTree`s
  // (`kNullProcessId`) while async extraction is in flight; both deduplicate
  // onto the single browser-wide pipe channel.
  buffer()->OnExternalActorAttached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  buffer()->OnExternalActorAttached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(2u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_EQ(base::kNullProcessId,
            buffer()->GetExternalActorsForTesting()[0].leaf_pid);
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting()[0].process_tree.empty());

  // Complete async extraction -> updates the pipe actor from placeholder
  // `kNullProcessId` to a resolved `ProcessTree` in place.
  buffer()->UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE, std::nullopt,
      MakeProcessTree(4001, 4000));
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(2u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_EQ(4001, buffer()->GetExternalActorsForTesting()[0].leaf_pid);
  EXPECT_FALSE(buffer()->GetExternalActorsForTesting()[0].process_tree.empty());

  // Detach once omitting `pid` -> matches via pipe fallback and decrements
  // `active_sessions` to 1.
  buffer()->OnExternalActorDetached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(1u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_TRUE(buffer()->BuildEventInitiatorInfo().has_value());

  // Detach final session with the resolved `pid` -> removes the active actor.
  buffer()->OnExternalActorDetached(ExternalProcessInfo::TRANSPORT_TYPE_PIPE,
                                    4001);
  EXPECT_EQ(std::nullopt, buffer()->BuildEventInitiatorInfo());
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting().empty());
}

TEST_F(CdpActorBufferTest, EndpointlessWebSocketsDoNotConflateOnAttach) {
  // Two unextracted WebSocket sessions without endpoints remain distinct
  // actors instead of conflating, while each detaches cleanly.
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  buffer()->OnExternalActorAttached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  ASSERT_EQ(2u, buffer()->GetExternalActorsForTesting().size());
  EXPECT_EQ(1u, buffer()->GetExternalActorsForTesting()[0].active_sessions);
  EXPECT_EQ(1u, buffer()->GetExternalActorsForTesting()[1].active_sessions);

  buffer()->OnExternalActorDetached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  ASSERT_EQ(1u, buffer()->GetExternalActorsForTesting().size());

  buffer()->OnExternalActorDetached(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  EXPECT_TRUE(buffer()->GetExternalActorsForTesting().empty());
}

}  // namespace
}  // namespace enterprise_connectors
