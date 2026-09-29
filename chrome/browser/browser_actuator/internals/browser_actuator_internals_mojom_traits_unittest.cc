// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_mojom_traits.h"

#include <cstdint>
#include <limits>

#include "base/time/time.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "components/browser_actuator/internal/session_stream_recorder.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

namespace mojom = browser_actuator_internals::mojom;

SessionEventSnapshot MakeEvent(bool is_downstream) {
  SessionEventSnapshot event;
  event.timestamp = base::Time::FromMillisecondsSinceUnixEpoch(1'000'000);
  event.is_downstream = is_downstream;
  event.payload_types = {"Control", "Unspecified"};
  event.message = R"({"session_id":"s1"})";
  event.message_truncated = true;
  return event;
}

TEST(BrowserActuatorInternalsMojomTraitsTest, RecordedEventRoundTrip) {
  for (bool is_downstream : {true, false}) {
    SessionEventSnapshot input = MakeEvent(is_downstream);
    SessionEventSnapshot output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RecordedEvent>(
        input, output));
    EXPECT_EQ(output.timestamp, input.timestamp);
    EXPECT_EQ(output.is_downstream, is_downstream);
    EXPECT_THAT(output.payload_types,
                testing::ElementsAre("Control", "Unspecified"));
    EXPECT_EQ(output.message, input.message);
    EXPECT_TRUE(output.message_truncated);
  }
}

TEST(BrowserActuatorInternalsMojomTraitsTest, SessionSummaryRoundTrip) {
  SessionSnapshot input;
  input.session_id = "session_1";
  input.start_wall_time = base::Time::FromMillisecondsSinceUnixEpoch(500'000);
  input.end_wall_time = input.start_wall_time + base::Seconds(10);
  input.total_downstream_messages = 3;
  input.total_upstream_messages = 2;
  input.total_events = 5;
  input.events.push_back(MakeEvent(/*is_downstream=*/true));
  input.events.push_back(MakeEvent(/*is_downstream=*/false));

  SessionSnapshot output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SessionSummary>(
      input, output));
  EXPECT_EQ(output.session_id, "session_1");
  EXPECT_EQ(output.start_wall_time, input.start_wall_time);
  EXPECT_EQ(output.end_wall_time, input.end_wall_time);
  EXPECT_EQ(output.total_downstream_messages, 3u);
  EXPECT_EQ(output.total_upstream_messages, 2u);
  EXPECT_EQ(output.total_events, 5u);
  ASSERT_EQ(output.events.size(), 2u);
  EXPECT_TRUE(output.events[0].is_downstream);
  EXPECT_FALSE(output.events[1].is_downstream);
}

TEST(BrowserActuatorInternalsMojomTraitsTest, ActiveSessionHasNoEndTime) {
  SessionSnapshot input;
  input.session_id = "active";
  input.start_wall_time = base::Time::FromMillisecondsSinceUnixEpoch(500'000);

  SessionSnapshot output;
  output.end_wall_time = base::Time::Now();
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SessionSummary>(
      input, output));
  EXPECT_FALSE(output.end_wall_time.has_value());
  EXPECT_TRUE(output.events.empty());
}

TEST(BrowserActuatorInternalsMojomTraitsTest, CountsSaturateAtUint32Max) {
  constexpr size_t kHuge =
      static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1;
  if constexpr (sizeof(size_t) <= sizeof(uint32_t)) {
    GTEST_SKIP() << "size_t cannot exceed uint32_t on this platform.";
  }
  SessionSnapshot input;
  input.total_downstream_messages = kHuge;
  input.total_upstream_messages = kHuge;
  input.total_events = kHuge;

  SessionSnapshot output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SessionSummary>(
      input, output));
  EXPECT_EQ(output.total_downstream_messages,
            std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(output.total_upstream_messages,
            std::numeric_limits<uint32_t>::max());
  EXPECT_EQ(output.total_events, std::numeric_limits<uint32_t>::max());
}

}  // namespace
}  // namespace browser_actuator
