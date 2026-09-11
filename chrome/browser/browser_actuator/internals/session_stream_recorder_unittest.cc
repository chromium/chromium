// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/session_stream_recorder.h"

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/proto/actuator_downstream_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class TestObserver : public SessionStreamRecorder::Observer {
 public:
  void OnDownstreamMessage(base::TimeTicks timestamp,
                           const ActuatorDownstreamMessage& message) override {
    downstream_count++;
    last_downstream_seq = message.sequence_number();
  }

  void OnUpstreamMessage(base::TimeTicks timestamp,
                         const ActuatorUpstreamMessage& message) override {
    upstream_count++;
    last_upstream_seq = message.client_sequence_number();
  }

  size_t downstream_count = 0;
  size_t upstream_count = 0;
  int64_t last_downstream_seq = 0;
  int64_t last_upstream_seq = 0;
};

class DefaultObserver : public SessionStreamRecorder::Observer {};

TEST(SessionStreamRecorderTest, InitialState) {
  SessionStreamRecorder recorder("session_123");

  EXPECT_EQ(recorder.session_id(), "session_123");
  EXPECT_EQ(recorder.metadata().session_id, "session_123");
  EXPECT_TRUE(recorder.metadata().is_active);
  EXPECT_EQ(recorder.metadata().total_downstream_messages, 0u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  EXPECT_TRUE(recorder.entries().empty());
}

TEST(SessionStreamRecorderTest, RecordsDownstreamMessage) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_123");
  downstream.set_sequence_number(42);

  recorder.RecordDownstreamMessage(downstream);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 1u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries().front();
  EXPECT_FALSE(entry.timestamp.is_null());
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(entry.message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entry.message).sequence_number(), 42);
}

TEST(SessionStreamRecorderTest, RecordsUpstreamMessage) {
  SessionStreamRecorder recorder("session_123");

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_123");
  upstream.set_client_sequence_number(10);

  recorder.RecordUpstreamMessage(upstream);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 0u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries().front();
  EXPECT_FALSE(entry.timestamp.is_null());
  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(entry.message));
  EXPECT_EQ(
      std::get<ActuatorUpstreamMessage>(entry.message).client_sequence_number(),
      10);
}

TEST(SessionStreamRecorderTest, PreservesChronologicalOrderAcrossDirections) {
  SessionStreamRecorder recorder("session_123");

  ActuatorDownstreamMessage d1;
  d1.set_sequence_number(1);
  recorder.RecordDownstreamMessage(d1);

  ActuatorUpstreamMessage u1;
  u1.set_client_sequence_number(2);
  recorder.RecordUpstreamMessage(u1);

  ActuatorDownstreamMessage d2;
  d2.set_sequence_number(3);
  recorder.RecordDownstreamMessage(d2);

  const auto& entries = recorder.entries();
  ASSERT_EQ(entries.size(), 3u);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorDownstreamMessage>(entries[0].message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entries[0].message).sequence_number(),
      1);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorUpstreamMessage>(entries[1].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(entries[1].message)
                .client_sequence_number(),
            2);

  ASSERT_TRUE(
      std::holds_alternative<ActuatorDownstreamMessage>(entries[2].message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entries[2].message).sequence_number(),
      3);
}

TEST(SessionStreamRecorderTest, NotifiesObservers) {
  SessionStreamRecorder recorder("session_123");
  TestObserver observer;
  recorder.AddObserver(&observer);

  ActuatorDownstreamMessage d;
  d.set_sequence_number(1);
  recorder.RecordDownstreamMessage(d);

  EXPECT_EQ(observer.downstream_count, 1u);
  EXPECT_EQ(observer.last_downstream_seq, 1);

  ActuatorUpstreamMessage u;
  u.set_client_sequence_number(2);
  recorder.RecordUpstreamMessage(u);

  EXPECT_EQ(observer.upstream_count, 1u);
  EXPECT_EQ(observer.last_upstream_seq, 2);

  recorder.RemoveObserver(&observer);

  ActuatorDownstreamMessage d2;
  d2.set_sequence_number(3);
  recorder.RecordDownstreamMessage(d2);

  EXPECT_EQ(observer.downstream_count, 1u);
}

TEST(SessionStreamRecorderTest, ObserverDefaultMethodsDoNothing) {
  SessionStreamRecorder recorder("session_123");
  DefaultObserver observer;
  recorder.AddObserver(&observer);

  ActuatorDownstreamMessage downstream;
  recorder.RecordDownstreamMessage(downstream);

  ActuatorUpstreamMessage upstream;
  recorder.RecordUpstreamMessage(upstream);

  recorder.RemoveObserver(&observer);
}

TEST(SessionStreamRecorderTest, RecordsIncomingMessagesViaOnMessage) {
  SessionStreamRecorder recorder("session_123");

  // 1. Unpacked ControlCommand arriving via OnMessage is recorded as a
  // downstream message with typed payload.
  ControlCommand command;
  command.mutable_close_session();
  recorder.OnMessage(command);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 1u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries()[0];
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(entry.message));
  const auto& downstream = std::get<ActuatorDownstreamMessage>(entry.message);
  EXPECT_EQ(downstream.session_id(), "session_123");
  ASSERT_EQ(downstream.typed_payloads_size(), 1);
  EXPECT_EQ(downstream.typed_payloads(0).payload_type(),
            ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);

  ControlCommand parsed_command;
  ASSERT_TRUE(parsed_command.ParseFromString(
      downstream.typed_payloads(0).proto_payload().value()));
  EXPECT_TRUE(parsed_command.has_close_session());

  // 2. Direct ActuatorDownstreamMessage arriving via OnMessage is also
  // recorded.
  ActuatorDownstreamMessage full_downstream;
  full_downstream.set_session_id("session_123");
  full_downstream.set_sequence_number(42);
  recorder.OnMessage(full_downstream);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 2u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 2u);
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(
      recorder.entries()[1].message));
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(recorder.entries()[1].message)
                .sequence_number(),
            42);

  // 3. Direct ActuatorUpstreamMessage arriving via OnMessage is also
  // recorded.
  ActuatorUpstreamMessage full_upstream;
  full_upstream.set_session_id("session_123");
  full_upstream.set_client_sequence_number(99);
  recorder.OnMessage(full_upstream);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 2u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 3u);
  ASSERT_TRUE(std::holds_alternative<ActuatorUpstreamMessage>(
      recorder.entries()[2].message));
  EXPECT_EQ(std::get<ActuatorUpstreamMessage>(recorder.entries()[2].message)
                .client_sequence_number(),
            99);

  // 4. Unrecognized payload arriving via OnMessage is recorded with
  // ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED.
  WatchSessionsRequest unrecognized_message;
  recorder.OnMessage(unrecognized_message);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 3u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 1u);
  ASSERT_EQ(recorder.entries().size(), 4u);
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(
      recorder.entries()[3].message));
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(recorder.entries()[3].message)
                .typed_payloads(0)
                .payload_type(),
            ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_UNSPECIFIED);
}

TEST(SessionStreamRecorderTest, GetWeakPtr) {
  SessionStreamRecorder recorder("session_123");
  auto weak_ptr = recorder.GetWeakPtr();
  EXPECT_TRUE(weak_ptr);
  EXPECT_EQ(weak_ptr.get(), &recorder);
}

TEST(SessionStreamRecorderTest, MarkSessionClosed) {
  SessionStreamRecorder recorder("session_123");
  EXPECT_TRUE(recorder.metadata().is_active);

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);
}

TEST(SessionStreamRecorderTest, InvokesDestructionCallback) {
  bool callback_invoked = false;
  SessionMetadata saved_metadata;
  std::vector<SessionStreamRecorder::Entry> saved_entries;

  {
    SessionStreamRecorder recorder("session_123");
    recorder.SetDestructionCallback(base::BindOnce(
        [](bool* invoked, SessionMetadata* saved_meta,
           std::vector<SessionStreamRecorder::Entry>* saved_ents,
           const SessionMetadata& metadata,
           std::vector<SessionStreamRecorder::Entry> entries) {
          *invoked = true;
          *saved_meta = metadata;
          *saved_ents = std::move(entries);
        },
        base::Unretained(&callback_invoked), base::Unretained(&saved_metadata),
        base::Unretained(&saved_entries)));

    ActuatorDownstreamMessage downstream;
    downstream.set_session_id("session_123");
    downstream.set_sequence_number(7);
    recorder.RecordDownstreamMessage(downstream);
  }

  EXPECT_TRUE(callback_invoked);
  EXPECT_EQ(saved_metadata.session_id, "session_123");
  EXPECT_EQ(saved_metadata.total_downstream_messages, 1u);
  EXPECT_EQ(saved_metadata.total_upstream_messages, 0u);
  ASSERT_EQ(saved_entries.size(), 1u);
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(saved_entries[0].message)
                .sequence_number(),
            7);
}

}  // namespace
}  // namespace browser_actuator
