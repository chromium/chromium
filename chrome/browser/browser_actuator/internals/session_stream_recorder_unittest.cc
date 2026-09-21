// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/session_stream_recorder.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/browser_actuator/internal/proto/transport_messages.pb.h"
#include "components/browser_actuator/public/common.h"
#include "components/browser_actuator/public/transport_session.h"
#include "components/sharing_message/proto/actuator_downstream_message.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

class FakeTransportSession : public TransportSession {
 public:
  explicit FakeTransportSession(std::string_view session_id)
      : session_id_(session_id) {}
  ~FakeTransportSession() override = default;

  std::string_view GetSessionId() const override { return session_id_; }
  base::expected<void, SendUpstreamMessageError> SendUpstreamMessage(
      PayloadType payload_type,
      const google::protobuf::MessageLite& message) override {
    return {};
  }
  void OnMessage(PayloadType payload_type,
                 const google::protobuf::MessageLite& message) override {}

 private:
  std::string session_id_;
};

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
  downstream.set_sequence_number(5);

  recorder.RecordDownstreamMessage(downstream);

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 1u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 1u);

  const auto& entry = recorder.entries().front();
  EXPECT_FALSE(entry.timestamp.is_null());
  ASSERT_TRUE(std::holds_alternative<ActuatorDownstreamMessage>(entry.message));
  EXPECT_EQ(
      std::get<ActuatorDownstreamMessage>(entry.message).sequence_number(), 5);
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

  // A control payload is recorded as a downstream message carrying the payload
  // type it was dispatched under, with its bytes stored verbatim.
  ControlCommand command;
  command.mutable_close_session();
  recorder.OnMessage(PayloadType::kControl, command.SerializeAsString());

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

  // An experimental triggering payload is distinguished by its payload type
  // rather than by inspecting the bytes.
  recorder.OnMessage(PayloadType::kExperimentalTriggering, "payload-bytes");

  ASSERT_EQ(recorder.entries().size(), 2u);
  const auto& triggering_entry =
      std::get<ActuatorDownstreamMessage>(recorder.entries()[1].message);
  ASSERT_EQ(triggering_entry.typed_payloads_size(), 1);
  EXPECT_EQ(triggering_entry.typed_payloads(0).payload_type(),
            ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  EXPECT_EQ(triggering_entry.typed_payloads(0).proto_payload().value(),
            "payload-bytes");

  // An unspecified payload type is recorded as such. The recorder never
  // discards a payload: an entry the internals page cannot classify is still
  // more useful than a gap in the log.
  recorder.OnMessage(PayloadType::kUnspecified, "");

  EXPECT_EQ(recorder.metadata().total_downstream_messages, 3u);
  EXPECT_EQ(recorder.metadata().total_upstream_messages, 0u);
  ASSERT_EQ(recorder.entries().size(), 3u);
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(recorder.entries()[2].message)
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
  EXPECT_FALSE(recorder.metadata().end_time.has_value());

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);
  EXPECT_TRUE(recorder.metadata().end_time.has_value());
  base::TimeTicks first_end_time = *recorder.metadata().end_time;

  recorder.MarkSessionClosed();
  EXPECT_FALSE(recorder.metadata().is_active);
  EXPECT_EQ(*recorder.metadata().end_time, first_end_time);
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
  EXPECT_FALSE(saved_metadata.is_active);
  EXPECT_TRUE(saved_metadata.end_time.has_value());
  EXPECT_EQ(saved_metadata.total_downstream_messages, 1u);
  EXPECT_EQ(saved_metadata.total_upstream_messages, 0u);
  ASSERT_EQ(saved_entries.size(), 1u);
  EXPECT_EQ(std::get<ActuatorDownstreamMessage>(saved_entries[0].message)
                .sequence_number(),
            7);
}

TEST(SessionStreamRecorderFactoryTest, FactoryIdAndPayloadTypes) {
  SessionStreamRecorderFactory factory;

  EXPECT_EQ(factory.GetFactoryId(), FactoryId::kSessionStreamRecorder);
  EXPECT_THAT(factory.GetSupportedPayloadTypes(),
              ::testing::UnorderedElementsAre(
                  PayloadType::kControl, PayloadType::kExperimentalTriggering));
}

TEST(SessionStreamRecorderFactoryTest, CreatesRecorderOnNewSession) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_xyz");

  std::unique_ptr<TransportHandler> handler = factory.OnNewSession(&session);
  ASSERT_NE(handler, nullptr);

  auto* recorder = static_cast<SessionStreamRecorder*>(handler.get());
  EXPECT_EQ(recorder->session_id(), "session_xyz");
  EXPECT_TRUE(recorder->metadata().is_active);
  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 1u);
}

TEST(SessionStreamRecorderFactoryTest, ReturnsNullptrOnNullSession) {
  SessionStreamRecorderFactory factory;
  EXPECT_EQ(factory.OnNewSession(nullptr), nullptr);
  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 0u);
}

TEST(SessionStreamRecorderFactoryTest, ExportAllSessionsAsJson) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session1("session_1");
  FakeTransportSession session2("session_2");

  std::unique_ptr<TransportHandler> h1 = factory.OnNewSession(&session1);
  std::unique_ptr<TransportHandler> h2 = factory.OnNewSession(&session2);

  auto* r1 = static_cast<SessionStreamRecorder*>(h1.get());
  auto* r2 = static_cast<SessionStreamRecorder*>(h2.get());

  ActuatorDownstreamMessage d1;
  d1.set_session_id("session_1");
  d1.set_sequence_number(1);
  d1.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  r1->RecordDownstreamMessage(d1);

  ActuatorUpstreamMessage u1;
  u1.set_session_id("session_1");
  u1.set_client_sequence_number(2);
  r1->RecordUpstreamMessage(u1);

  ActuatorDownstreamMessage d2;
  d2.set_session_id("session_2");
  d2.set_sequence_number(5);
  d2.add_typed_payloads()->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING);
  r2->RecordDownstreamMessage(d2);

  ActuatorUpstreamMessage u2;
  u2.set_session_id("session_2");
  u2.set_client_sequence_number(10);
  u2.set_responding_to_sequence_number(5);
  r2->RecordUpstreamMessage(u2);
  r2->MarkSessionClosed();

  std::string json_str = factory.ExportAllSessionsAsJson();
  ASSERT_FALSE(json_str.empty());

  std::optional<base::DictValue> parsed_json =
      base::JSONReader::ReadDict(json_str, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed_json.has_value());

  const base::ListValue* sessions = parsed_json->FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 2u);

  const base::DictValue* s1_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s1_dict, nullptr);
  EXPECT_EQ(*s1_dict->FindString("session_id"), "session_1");
  EXPECT_TRUE(*s1_dict->FindBool("is_active"));
  EXPECT_EQ(s1_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s1_dict->FindInt("total_upstream_messages"), 1);
  const base::ListValue* s1_events = s1_dict->FindList("events");
  ASSERT_NE(s1_events, nullptr);
  ASSERT_EQ(s1_events->size(), 2u);

  // The message body is serialized by the generated `ToValue()`, which emits
  // every proto field. int64 fields are emitted as strings, as base::Value has
  // no int64 type.
  const base::DictValue* s1_e0 = (*s1_events)[0].GetIfDict();
  ASSERT_NE(s1_e0, nullptr);
  EXPECT_EQ(*s1_e0->FindString("direction"), "Downstream");
  const base::DictValue* s1_m0 = s1_e0->FindDict("message");
  ASSERT_NE(s1_m0, nullptr);
  EXPECT_EQ(*s1_m0->FindString("session_id"), "session_1");
  EXPECT_EQ(*s1_m0->FindString("sequence_number"), "1");
  const base::ListValue* s1_m0_payloads = s1_m0->FindList("typed_payloads");
  ASSERT_NE(s1_m0_payloads, nullptr);
  ASSERT_EQ(s1_m0_payloads->size(), 1u);
  EXPECT_EQ(*(*s1_m0_payloads)[0].GetIfDict()->FindString("payload_type"),
            "ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND");

  const base::DictValue* s1_e1 = (*s1_events)[1].GetIfDict();
  ASSERT_NE(s1_e1, nullptr);
  EXPECT_EQ(*s1_e1->FindString("direction"), "Upstream");
  const base::DictValue* s1_m1 = s1_e1->FindDict("message");
  ASSERT_NE(s1_m1, nullptr);
  EXPECT_EQ(*s1_m1->FindString("client_sequence_number"), "2");
  // Unset optional fields are omitted entirely.
  EXPECT_EQ(s1_m1->Find("responding_to_sequence_number"), nullptr);

  const base::DictValue* s2_dict = (*sessions)[1].GetIfDict();
  ASSERT_NE(s2_dict, nullptr);
  EXPECT_EQ(*s2_dict->FindString("session_id"), "session_2");
  EXPECT_FALSE(*s2_dict->FindBool("is_active"));
  EXPECT_EQ(s2_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s2_dict->FindInt("total_upstream_messages"), 1);
  const base::ListValue* s2_events = s2_dict->FindList("events");
  ASSERT_NE(s2_events, nullptr);
  ASSERT_EQ(s2_events->size(), 2u);
  const base::DictValue* s2_e0 = (*s2_events)[0].GetIfDict();
  ASSERT_NE(s2_e0, nullptr);
  EXPECT_EQ(*s2_e0->FindString("direction"), "Downstream");
  const base::DictValue* s2_m0 = s2_e0->FindDict("message");
  ASSERT_NE(s2_m0, nullptr);
  EXPECT_EQ(*s2_m0->FindString("sequence_number"), "5");
  const base::ListValue* s2_m0_payloads = s2_m0->FindList("typed_payloads");
  ASSERT_NE(s2_m0_payloads, nullptr);
  ASSERT_EQ(s2_m0_payloads->size(), 1u);
  EXPECT_EQ(*(*s2_m0_payloads)[0].GetIfDict()->FindString("payload_type"),
            "ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_EXPERIMENTAL_TRIGGERING");

  const base::DictValue* s2_e1 = (*s2_events)[1].GetIfDict();
  ASSERT_NE(s2_e1, nullptr);
  EXPECT_EQ(*s2_e1->FindString("direction"), "Upstream");
  const base::DictValue* s2_m1 = s2_e1->FindDict("message");
  ASSERT_NE(s2_m1, nullptr);
  EXPECT_EQ(*s2_m1->FindString("client_sequence_number"), "10");
  EXPECT_EQ(*s2_m1->FindString("responding_to_sequence_number"), "5");
}

TEST(SessionStreamRecorderFactoryTest, RetainsDestroyedSessionsForDump) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_temp");

  {
    std::unique_ptr<TransportHandler> handler = factory.OnNewSession(&session);
    auto* recorder = static_cast<SessionStreamRecorder*>(handler.get());
    ActuatorDownstreamMessage downstream;
    downstream.set_session_id("session_temp");
    downstream.set_sequence_number(42);
    recorder->RecordDownstreamMessage(downstream);
    // Handler destroyed at end of scope
  }

  EXPECT_EQ(factory.GetActiveRecordersCountForTesting(), 0u);

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* sessions = dump.FindList("sessions");
  ASSERT_NE(sessions, nullptr);
  ASSERT_EQ(sessions->size(), 1u);
  const base::DictValue* s_dict = (*sessions)[0].GetIfDict();
  ASSERT_NE(s_dict, nullptr);
  EXPECT_EQ(*s_dict->FindString("session_id"), "session_temp");
  EXPECT_FALSE(*s_dict->FindBool("is_active"));
  EXPECT_NE(s_dict->FindString("start_wall_time"), nullptr);
  EXPECT_TRUE(s_dict->FindDouble("end_time_ticks").has_value());
  EXPECT_TRUE(s_dict->FindDouble("duration_ms").has_value());
  EXPECT_EQ(s_dict->FindInt("total_downstream_messages"), 1);
  EXPECT_EQ(s_dict->FindInt("total_upstream_messages"), 0);
  const base::ListValue* events = s_dict->FindList("events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->size(), 1u);
  const base::DictValue* e0 = (*events)[0].GetIfDict();
  ASSERT_NE(e0, nullptr);
  EXPECT_EQ(*e0->FindString("direction"), "Downstream");
  const base::DictValue* message = e0->FindDict("message");
  ASSERT_NE(message, nullptr);
  EXPECT_EQ(*message->FindString("sequence_number"), "42");
}

// The message body is serialized by the generated `ToValue()` rather than by a
// hand-written list of fields, so fields that no caller explicitly asked for
// still end up in the dump.
TEST(SessionStreamRecorderFactoryTest, ExportIncludesAllProtoFields) {
  SessionStreamRecorderFactory factory;
  FakeTransportSession session("session_1");
  std::unique_ptr<TransportHandler> handler = factory.OnNewSession(&session);
  auto* recorder = static_cast<SessionStreamRecorder*>(handler.get());

  ActuatorUpstreamMessage upstream;
  upstream.set_session_id("session_1");
  auto* capabilities = upstream.mutable_capabilities();
  capabilities->set_chrome_major_version_number(140);
  capabilities->add_supported_features("feature_a");
  recorder->RecordUpstreamMessage(upstream);

  ActuatorDownstreamMessage downstream;
  downstream.set_session_id("session_1");
  downstream.set_sequence_number(1);
  auto* typed_payload = downstream.add_typed_payloads();
  typed_payload->set_payload_type(
      ACTUATOR_DOWNSTREAM_PAYLOAD_TYPE_CONTROL_COMMAND);
  ControlCommand command;
  command.mutable_close_session();
  std::string serialized_command = command.SerializeAsString();
  typed_payload->mutable_proto_payload()->set_type_url(
      "type.googleapis.com/browser_actuator.ControlCommand");
  typed_payload->mutable_proto_payload()->set_value(serialized_command);
  recorder->RecordDownstreamMessage(downstream);

  base::DictValue dump = factory.ExportAllSessionsAsValue();
  const base::ListValue* events =
      (*dump.FindList("sessions"))[0].GetIfDict()->FindList("events");
  ASSERT_NE(events, nullptr);
  ASSERT_EQ(events->size(), 2u);

  const base::DictValue* upstream_message =
      (*events)[0].GetIfDict()->FindDict("message");
  ASSERT_NE(upstream_message, nullptr);
  const base::DictValue* dumped_capabilities =
      upstream_message->FindDict("capabilities");
  ASSERT_NE(dumped_capabilities, nullptr);
  EXPECT_EQ(*dumped_capabilities->FindString("chrome_major_version_number"),
            "140");
  const base::ListValue* features =
      dumped_capabilities->FindList("supported_features");
  ASSERT_NE(features, nullptr);
  ASSERT_EQ(features->size(), 1u);
  EXPECT_EQ((*features)[0].GetString(), "feature_a");

  const base::DictValue* downstream_message =
      (*events)[1].GetIfDict()->FindDict("message");
  ASSERT_NE(downstream_message, nullptr);
  const base::ListValue* dumped_payloads =
      downstream_message->FindList("typed_payloads");
  ASSERT_NE(dumped_payloads, nullptr);
  ASSERT_EQ(dumped_payloads->size(), 1u);
  const base::DictValue* dumped_proto_payload =
      (*dumped_payloads)[0].GetIfDict()->FindDict("proto_payload");
  ASSERT_NE(dumped_proto_payload, nullptr);
  EXPECT_EQ(*dumped_proto_payload->FindString("type_url"),
            "type.googleapis.com/browser_actuator.ControlCommand");
  EXPECT_EQ(*dumped_proto_payload->FindString("value"),
            base::Base64Encode(serialized_command));
}

}  // namespace
}  // namespace browser_actuator
