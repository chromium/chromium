// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/tab_cdp_actor_tracker.h"

#include <optional>
#include <string>
#include <utility>

#include "base/process/process_handle.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/enterprise/common/proto/synced/browser_events.pb.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/process_extractor.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {
namespace {

using ::chrome::cros::reporting::proto::BrowserFeatureInfo;
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

class TabCdpActorTrackerTest : public ChromeRenderViewHostTestHarness {
 public:
  TabCdpActorTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndEnableFeature(kEventReportingCdpAttribution);
  }
  ~TabCdpActorTrackerTest() override = default;

  TabCdpActorTracker* GetOrCreateTracker() {
    return TabCdpActorTracker::GetOrCreateForWebContents(web_contents());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabCdpActorTrackerTest, EmptyTrackerReturnsNullopt) {
  EXPECT_EQ(std::nullopt, TabCdpActorTracker::GetEventInitiatorInfo(nullptr));
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));

  TabCdpActorTracker* tracker = GetOrCreateTracker();
  ASSERT_NE(nullptr, tracker);
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
}

TEST_F(TabCdpActorTrackerTest, FeatureDisabledReturnsNullopt) {
  base::test::ScopedFeatureList disabled_feature;
  disabled_feature.InitAndDisableFeature(kEventReportingCdpAttribution);

  TabCdpActorTracker* tracker = GetOrCreateTracker();
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE, MakeProcessTree(1001, 500));
  EXPECT_TRUE(tracker->buffer().BuildEventInitiatorInfo().has_value());
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
}

TEST_F(TabCdpActorTrackerTest, FeatureParamsDefaultValues) {
  EXPECT_EQ(5, kCdpAttributionMaxActorsPerTab.Get());
  EXPECT_EQ(base::Seconds(30), kCdpAttributionActorIdleTtl.Get());
}

TEST_F(TabCdpActorTrackerTest, ExternalPipeAndWebSocketProtoSerialization) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();

  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE,
      MakeProcessTree(1001, 500, "/usr/local/bin/node",
                      "/Users/<USER>/puppeteer.js"));
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(51001),
      MakeProcessTree(2002, 0, "/usr/bin/python3", "/Users/<USER>/cdp.py"));

  std::optional<EventInitiatorInfo> initiator =
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
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

TEST_F(TabCdpActorTrackerTest,
       MultiTargetDeduplicationAndActorIdleTtlExpiration) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();
  const net::IPEndPoint endpoint = MakeLoopbackEndpoint(52000);
  const ProcessTree tree = MakeProcessTree(3001, 100);

  // Simulate root page session + two child frame/worker auto-attached sessions
  // from the same WebSocket connection: all deduplicate onto a single actor
  // entry.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, tree);

  ASSERT_EQ(1u, tracker->buffer().GetExternalActorsForTesting().size());

  // Handling a command at t=15s refreshes `last_command_at` so the entry is
  // still within the 30s TTL at t=44s (29s after the last command).
  task_environment()->FastForwardBy(base::Seconds(15));
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);

  task_environment()->FastForwardBy(base::Seconds(29));
  EXPECT_TRUE(
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents()).has_value());

  // At 30 seconds after the last command, the entry is lazily pruned.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
  EXPECT_TRUE(tracker->buffer().GetExternalActorsForTesting().empty());
}

TEST_F(TabCdpActorTrackerTest,
       CircularBufferCapacityEvictsOldestLastCommandAt) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();

  // Record 5 distinct WebSocket actors (reaching the default capacity of 5).
  for (int i = 1; i <= 5; ++i) {
    tracker->buffer().OnExternalActorCommandHandled(
        ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
        MakeLoopbackEndpoint(53000 + i), MakeProcessTree(4000 + i, 1));
    task_environment()->FastForwardBy(base::Seconds(1));
  }
  ASSERT_EQ(5u, tracker->buffer().GetExternalActorsForTesting().size());

  // Refresh `last_command_at` on actors 1 and 2 so actor 3 now has the oldest
  // `last_command_at`, followed by actor 4.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(53001));
  task_environment()->FastForwardBy(base::Seconds(1));
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(53002));
  task_environment()->FastForwardBy(base::Seconds(1));

  // Record actor 6 while all 5 slots are within TTL. Actor 3 (oldest
  // `last_command_at`) must be evicted first, keeping actors {1, 2, 4, 5, 6}.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(53006), MakeProcessTree(4006, 1));
  {
    const auto& actors = tracker->buffer().GetExternalActorsForTesting();
    ASSERT_EQ(5u, actors.size());
    EXPECT_EQ(4001, actors[0].leaf_pid);
    EXPECT_EQ(4002, actors[1].leaf_pid);
    EXPECT_EQ(4004, actors[2].leaf_pid);
    EXPECT_EQ(4005, actors[3].leaf_pid);
    EXPECT_EQ(4006, actors[4].leaf_pid);
  }

  // Record actor 7. Actor 4 (next oldest `last_command_at`) must be evicted,
  // leaving {1, 2, 5, 6, 7}.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(53007), MakeProcessTree(4007, 1));
  {
    const auto& actors = tracker->buffer().GetExternalActorsForTesting();
    ASSERT_EQ(5u, actors.size());
    EXPECT_EQ(4001, actors[0].leaf_pid);
    EXPECT_EQ(4002, actors[1].leaf_pid);
    EXPECT_EQ(4005, actors[2].leaf_pid);
    EXPECT_EQ(4006, actors[3].leaf_pid);
    EXPECT_EQ(4007, actors[4].leaf_pid);
  }

  // Record actor 8. Actor 5 (next oldest `last_command_at`) must be evicted,
  // leaving {1, 2, 6, 7, 8}.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET,
      MakeLoopbackEndpoint(53008), MakeProcessTree(4008, 1));
  {
    const auto& actors = tracker->buffer().GetExternalActorsForTesting();
    ASSERT_EQ(5u, actors.size());
    EXPECT_EQ(4001, actors[0].leaf_pid);
    EXPECT_EQ(4002, actors[1].leaf_pid);
    EXPECT_EQ(4006, actors[2].leaf_pid);
    EXPECT_EQ(4007, actors[3].leaf_pid);
    EXPECT_EQ(4008, actors[4].leaf_pid);
  }
}

TEST_F(TabCdpActorTrackerTest, DevToolsUiTrackingAndExternalActorPrecedence) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();

  // 1. Built-in DevTools UI attaches -> reports `FEATURE_TYPE_DEVTOOLS_UI`.
  tracker->buffer().OnDevToolsUiAttached();
  {
    std::optional<EventInitiatorInfo> initiator =
        TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
    ASSERT_TRUE(initiator.has_value());
    EXPECT_FALSE(initiator->has_external_process_info());
    ASSERT_TRUE(initiator->has_browser_feature_info());
    EXPECT_EQ(BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI,
              initiator->browser_feature_info().feature_type());
  }

  // 2. External Pipe actor handles a command while DevTools UI is open ->
  // external actor takes precedence over `FEATURE_TYPE_DEVTOOLS_UI`.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE, MakeProcessTree(5001, 5000));
  {
    std::optional<EventInitiatorInfo> initiator =
        TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
    ASSERT_TRUE(initiator.has_value());
    EXPECT_TRUE(initiator->has_external_process_info());
    EXPECT_FALSE(initiator->has_browser_feature_info());
  }

  // 3. External Pipe actor's 30s command TTL expires while DevTools UI remains
  // open -> falls back to `FEATURE_TYPE_DEVTOOLS_UI`.
  task_environment()->FastForwardBy(base::Seconds(30));
  {
    std::optional<EventInitiatorInfo> initiator =
        TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
    ASSERT_TRUE(initiator.has_value());
    EXPECT_FALSE(initiator->has_external_process_info());
    ASSERT_TRUE(initiator->has_browser_feature_info());
    EXPECT_EQ(BrowserFeatureInfo::FEATURE_TYPE_DEVTOOLS_UI,
              initiator->browser_feature_info().feature_type());
  }

  // 4. DevTools UI sessions are reference-counted and clear immediately (no
  // post-detach grace window) once the final session detaches so normal human
  // browsing after closing DevTools is not misattributed.
  tracker->buffer().OnDevToolsUiAttached();
  tracker->buffer().OnDevToolsUiDetached();
  EXPECT_TRUE(
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents()).has_value());

  tracker->buffer().OnDevToolsUiDetached();
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
}

TEST_F(TabCdpActorTrackerTest, UpdateExternalActorProcessTreeInPlace) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();
  const net::IPEndPoint endpoint = MakeLoopbackEndpoint(54000);

  // Register actor initially with an empty `ProcessTree` (while async
  // extraction is in flight).
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint);
  ASSERT_EQ(1u, tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_TRUE(
      tracker->buffer().GetExternalActorsForTesting()[0].process_tree.empty());

  // Complete async extraction -> updates `ProcessTree` in place.
  tracker->buffer().UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint,
      MakeProcessTree(6001, 6000));
  ASSERT_EQ(1u, tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_EQ(6001, tracker->buffer().GetExternalActorsForTesting()[0].leaf_pid);

  std::optional<EventInitiatorInfo> initiator =
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
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
  tracker->buffer().UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, endpoint, ProcessTree{});
  EXPECT_EQ(6001, tracker->buffer().GetExternalActorsForTesting()[0].leaf_pid);
}

TEST_F(TabCdpActorTrackerTest, PipeDeduplicationUpdateAndTtlExpiration) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();

  // Record root and child pipe sessions initially with empty `ProcessTree`s
  // (`kNullProcessId`) while async extraction is in flight; both deduplicate
  // onto the single browser-wide pipe channel.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  ASSERT_EQ(1u, tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_EQ(base::kNullProcessId,
            tracker->buffer().GetExternalActorsForTesting()[0].leaf_pid);
  EXPECT_TRUE(
      tracker->buffer().GetExternalActorsForTesting()[0].process_tree.empty());

  // Complete async extraction -> updates the pipe actor from placeholder
  // `kNullProcessId` to a resolved `ProcessTree` in place.
  tracker->buffer().UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE, std::nullopt,
      MakeProcessTree(4001, 4000));
  ASSERT_EQ(1u, tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_EQ(4001, tracker->buffer().GetExternalActorsForTesting()[0].leaf_pid);
  EXPECT_FALSE(
      tracker->buffer().GetExternalActorsForTesting()[0].process_tree.empty());

  // Refresh command timestamp via pipe fallback -> expires 30s after the
  // refreshed command.
  task_environment()->FastForwardBy(base::Seconds(10));
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_PIPE);
  task_environment()->FastForwardBy(base::Seconds(29));
  EXPECT_TRUE(
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents()).has_value());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
  EXPECT_TRUE(tracker->buffer().GetExternalActorsForTesting().empty());
}

TEST_F(TabCdpActorTrackerTest, EndpointlessWebSocketsDoNotConflateOnCommand) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();

  // Two unextracted WebSocket sessions without endpoints remain distinct
  // actors instead of conflating, and both expire after the 30s TTL.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET);
  ASSERT_EQ(2u, tracker->buffer().GetExternalActorsForTesting().size());

  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_TRUE(tracker->buffer().GetExternalActorsForTesting().empty());
}

TEST_F(TabCdpActorTrackerTest,
       SameProcessSecondWebSocketUpdatesExactEndpointAndDeduplicatesProto) {
  TabCdpActorTracker* tracker = GetOrCreateTracker();
  const net::IPEndPoint ep1 = MakeLoopbackEndpoint(54321);
  const net::IPEndPoint ep2 = MakeLoopbackEndpoint(54322);

  // First WebSocket connection from PID 2000 resolves immediately.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, ep1,
      MakeProcessTree(2000, 1000));
  // Second WebSocket connection from the same process handles a command as a
  // placeholder before its background extraction finishes.
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, ep2, ProcessTree{});
  ASSERT_EQ(2u, tracker->buffer().GetExternalActorsForTesting().size());
  EXPECT_TRUE(
      tracker->buffer().GetExternalActorsForTesting()[1].process_tree.empty());

  // When extraction for `ep2` finishes with the same `leaf_pid` (`2000`), the
  // exact `peer_endpoint` match updates `ep2`'s placeholder rather than
  // stopping at `ep1`.
  tracker->buffer().UpdateExternalActorProcessTree(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, ep2,
      MakeProcessTree(2000, 1000));
  EXPECT_EQ(2000, tracker->buffer().GetExternalActorsForTesting()[1].leaf_pid);
  EXPECT_FALSE(
      tracker->buffer().GetExternalActorsForTesting()[1].process_tree.empty());

  // Verify that two distinct WebSocket endpoints from the same process are
  // deduplicated by `(transport_type, leaf_pid)` in `BuildEventInitiatorInfo()`
  // while tracking independent `last_command_at` timestamps per endpoint.
  {
    std::optional<EventInitiatorInfo> initiator =
        TabCdpActorTracker::GetEventInitiatorInfo(web_contents());
    ASSERT_TRUE(initiator.has_value());
    ASSERT_EQ(1, initiator->external_process_info().external_processes_size());
    EXPECT_EQ(2000, initiator->external_process_info()
                        .external_processes(0)
                        .process_nodes(0)
                        .process_id());
  }
  task_environment()->FastForwardBy(base::Seconds(10));
  tracker->buffer().OnExternalActorCommandHandled(
      ExternalProcessInfo::TRANSPORT_TYPE_WEBSOCKET, ep2);
  // 35s after `ep1`'s command (25s after `ep2`'s last command), `ep2` is still
  // within its 30s TTL even after `ep1` expires at 30s.
  task_environment()->FastForwardBy(base::Seconds(25));
  EXPECT_TRUE(
      TabCdpActorTracker::GetEventInitiatorInfo(web_contents()).has_value());
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_EQ(std::nullopt,
            TabCdpActorTracker::GetEventInitiatorInfo(web_contents()));
}

}  // namespace
}  // namespace enterprise_connectors
