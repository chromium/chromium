// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/typed_macros.h"

#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/trace_test_utils.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/typed_macros_embedder_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_mojo_event_info.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"

#include "base/tracing/perfetto_platform.h"

namespace base {
namespace trace_event {

namespace {

std::unique_ptr<perfetto::TracingSession> g_tracing_session;

void EnableTrace(bool filter_debug_annotations = false) {
  g_tracing_session = perfetto::Tracing::NewTrace();
  auto config = test::TracingEnvironment::GetDefaultTraceConfig();
  if (filter_debug_annotations) {
    for (auto& data_source : *config.mutable_data_sources()) {
      perfetto::protos::gen::TrackEventConfig track_event_config;
      track_event_config.set_filter_debug_annotations(true);
      data_source.mutable_config()->set_track_event_config_raw(
          track_event_config.SerializeAsString());
    }
  }
  g_tracing_session->Setup(config);
  g_tracing_session->StartBlocking();
}


void CancelTrace() {
  g_tracing_session.reset();
}

struct TestTrackEvent;
struct TestTracePacket;
TestTrackEvent* g_test_track_event;
TestTracePacket* g_test_trace_packet;

struct TestTrackEvent : public TrackEventHandle::CompletionListener {
 public:
  TestTrackEvent() {
    CHECK_EQ(g_test_track_event, nullptr)
        << "Another instance of TestTrackEvent is already active";
    g_test_track_event = this;
  }

  ~TestTrackEvent() override { g_test_track_event = nullptr; }

  void OnTrackEventCompleted() override {
    EXPECT_FALSE(event_completed);
    event_completed = true;
  }

  protozero::HeapBuffered<perfetto::protos::pbzero::TrackEvent> event;
  perfetto::internal::TrackEventIncrementalState incremental_state;
  bool prepare_called = false;
  bool event_completed = false;
};

struct TestTracePacket : public TracePacketHandle::CompletionListener {
 public:
  TestTracePacket() {
    CHECK_EQ(g_test_trace_packet, nullptr)
        << "Another instance of TestTracePacket is already active";
    g_test_trace_packet = this;
  }

  ~TestTracePacket() override { g_test_trace_packet = nullptr; }

  void OnTracePacketCompleted() override {
    EXPECT_FALSE(packet_completed);
    packet_completed = true;
  }

  protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> packet;
  bool prepare_called = false;
  bool packet_completed = false;
  bool emit_empty_called = false;
};

TrackEventHandle PrepareTrackEvent(bool filter_debug_annotations) {
  CHECK_NE(g_test_track_event, nullptr) << "TestTrackEvent not set yet";
  g_test_track_event->prepare_called = true;
  return TrackEventHandle(g_test_track_event->event.get(),
                          &g_test_track_event->incremental_state,
                          g_test_track_event, filter_debug_annotations);
}

TrackEventHandle PrepareTrackEventWithDebugAnnotations(TraceEvent*) {
  return PrepareTrackEvent(/*filter_debug_annotations=*/false);
}

TrackEventHandle PrepareTrackEventFilterDebugAnnotations(TraceEvent*) {
  return PrepareTrackEvent(/*filter_debug_annotations=*/true);
}

TracePacketHandle PrepareTracePacket() {
  CHECK_NE(g_test_track_event, nullptr) << "TestTracePacket not set yet";
  g_test_trace_packet->prepare_called = true;
  return TracePacketHandle(TracePacketHandle::PerfettoPacketHandle(
                               g_test_trace_packet->packet.get()),
                           g_test_trace_packet);
}

void EmitEmptyPacket() {
  CHECK_NE(g_test_track_event, nullptr) << "TestTracePacket not set yet";
  g_test_trace_packet->emit_empty_called = true;
}

class TypedTraceEventTest : public testing::Test {
 public:
  TypedTraceEventTest() {
    perfetto::internal::TrackRegistry::InitializeInstance();
    EnableTypedTraceEvents(&PrepareTrackEventWithDebugAnnotations,
                           &PrepareTracePacket, &EmitEmptyPacket);
  }

  ~TypedTraceEventTest() override { ResetTypedTraceEventsForTesting(); }

  void FlushTrace() {
    TrackEvent::Flush();
    g_tracing_session->StopBlocking();
    std::vector<char> serialized_data = g_tracing_session->ReadTraceBlocking();
    perfetto::protos::Trace trace;
    EXPECT_TRUE(
        trace.ParseFromArray(serialized_data.data(), serialized_data.size()));
    for (const auto& packet : trace.packet()) {
      if (packet.has_track_event()) {
        std::string serialized_event = packet.track_event().SerializeAsString();
        event_.prepare_called = true;
        event_.event->AppendRawProtoBytes(serialized_event.data(),
                                          serialized_event.size());
        event_.OnTrackEventCompleted();

        std::string serialized_interned_data =
            packet.interned_data().SerializeAsString();
        event_.incremental_state.serialized_interned_data->AppendRawProtoBytes(
            serialized_interned_data.data(), serialized_interned_data.size());

        std::string serialized_packet = packet.SerializeAsString();
        packet_.prepare_called = true;
        packet_.packet->AppendRawProtoBytes(serialized_packet.data(),
                                            serialized_packet.size());
        packet_.OnTracePacketCompleted();
        break;
      }
    }
  }

  perfetto::protos::TrackEvent ParseTrackEvent() {
    FlushTrace();
    auto serialized_data = event_.event.SerializeAsArray();
    perfetto::protos::TrackEvent track_event;
    EXPECT_TRUE(track_event.ParseFromArray(serialized_data.data(),
                                           serialized_data.size()));
    return track_event;
  }

 protected:
  test::TracingEnvironment tracing_environment_;
  TestTrackEvent event_;
  TestTracePacket packet_;
};

}  // namespace

TEST_F(TypedTraceEventTest, CallbackExecutedWhenTracingEnabled) {
  EnableTrace();

  TRACE_EVENT("cat", "Name", [&](perfetto::EventContext ctx) {
    perfetto::protos::pbzero::LogMessage* log = ctx.event()->set_log_message();
    log->set_body_iid(1);
  });
  FlushTrace();

  EXPECT_TRUE(event_.prepare_called);
  EXPECT_FALSE(event_.event.empty());
  EXPECT_TRUE(event_.event_completed);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, CallbackNotExecutedWhenTracingDisabled) {
  TRACE_EVENT("cat", "Name", [this](perfetto::EventContext ctx) {
    EXPECT_EQ(ctx.event(), event_.event.get());
    perfetto::protos::pbzero::LogMessage* log = ctx.event()->set_log_message();
    log->set_body_iid(1);
  });

  EXPECT_FALSE(event_.prepare_called);
  EXPECT_TRUE(event_.event.empty());
  EXPECT_FALSE(event_.event_completed);
}

TEST_F(TypedTraceEventTest, DescriptorPacketWrittenForEventWithTrack) {
  EnableTrace();

  TRACE_EVENT("cat", "Name", perfetto::Track(1234));

  FlushTrace();
  EXPECT_TRUE(event_.prepare_called);
  EXPECT_FALSE(event_.event.empty());
  EXPECT_TRUE(event_.event_completed);

  EXPECT_TRUE(packet_.prepare_called);
  EXPECT_FALSE(packet_.packet.empty());
  EXPECT_TRUE(packet_.packet_completed);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InternedData) {
  EnableTrace();
  const TraceSourceLocation location("TestFunction", "test.cc", 123);
  size_t iid = 0;

  TRACE_EVENT("cat", "Name", [&location, &iid](perfetto::EventContext ctx) {
    auto* log = ctx.event()->set_log_message();
    iid = InternedSourceLocation::Get(&ctx, location);
    EXPECT_NE(0u, iid);
    log->set_body_iid(iid);

    size_t iid2 = InternedSourceLocation::Get(&ctx, location);
    EXPECT_EQ(iid, iid2);

    TraceSourceLocation location2("TestFunction2", "test.cc", 456);
    size_t iid3 = InternedSourceLocation::Get(&ctx, location2);
    EXPECT_NE(0u, iid3);
    EXPECT_NE(iid, iid3);

    // Test getting an interned ID directly from a base::Location object, the
    // only attributes that are compared are function_name, file_name and
    // line_number.
    const void* dummy_pc = &iid;
    base::Location base_location = base::Location::CreateForTesting(
        "TestFunction", "test.cc", 123, dummy_pc);
    TraceSourceLocation location3(base_location);
    size_t iid4 = InternedSourceLocation::Get(&ctx, location3);
    EXPECT_EQ(iid, iid4);
  });
  FlushTrace();

  auto serialized_data =
      event_.incremental_state.serialized_interned_data.SerializeAsArray();
  perfetto::protos::InternedData interned_data;
  EXPECT_TRUE(interned_data.ParseFromArray(serialized_data.data(),
                                           serialized_data.size()));
  EXPECT_EQ(2, interned_data.source_locations_size());
  auto interned_loc = interned_data.source_locations()[0];
  EXPECT_EQ(iid, interned_loc.iid());
  EXPECT_EQ("TestFunction", interned_loc.function_name());
  EXPECT_EQ("test.cc", interned_loc.file_name());
  interned_loc = interned_data.source_locations()[1];
  EXPECT_EQ("TestFunction2", interned_loc.function_name());
  EXPECT_EQ("test.cc", interned_loc.file_name());

  // Make sure the in-memory interning index persists between trace events by
  // recording another event.
  event_.incremental_state.serialized_interned_data.Reset();
  event_.event_completed = false;

  TRACE_EVENT("cat", "Name", [&location](perfetto::EventContext ctx) {
    auto* log = ctx.event()->set_log_message();
    size_t iid = InternedSourceLocation::Get(&ctx, location);
    EXPECT_NE(0u, iid);
    log->set_body_iid(iid);
  });
  FlushTrace();

  // No new data should have been interned the second time around.
  EXPECT_EQ(
      "",
      event_.incremental_state.serialized_interned_data.SerializeAsString());

  CancelTrace();
}

// TODO: crbug/334063999 - The test is disabled due to flakiness.
TEST_F(TypedTraceEventTest, DISABLED_InstantThreadEvent) {
  EnableTrace();

  TRACE_EVENT_INSTANT("cat", "ThreadEvent", [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_FALSE(track_event.has_track_uuid());

  CancelTrace();
}

// TODO: crbug/334063999 - The test is disabled due to flakiness.
TEST_F(TypedTraceEventTest, DISABLED_InstantProcessEvent) {
  EnableTrace();

  TRACE_EVENT_INSTANT("cat", "ProcessEvent", perfetto::ProcessTrack::Current(),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::ProcessTrack::Current().uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantGlobalEvent) {
  EnableTrace();

  TRACE_EVENT_INSTANT("cat", "GlobalEvent", perfetto::Track::Global(1234),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::Track::Global(1234).uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantGlobalDefaultEvent) {
  EnableTrace();

  TRACE_EVENT_INSTANT("cat", "GlobalDefaultEvent", perfetto::Track::Global(0),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::Track::Global(0).uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, BeginEventOnDefaultTrackDoesNotWriteTrackUuid) {
  EnableTrace();

  TRACE_EVENT_BEGIN("cat", "Name");
  auto begin_event = ParseTrackEvent();
  EXPECT_FALSE(begin_event.has_track_uuid());

  CancelTrace();
}

TEST_F(TypedTraceEventTest, EndEventOnDefaultTrackDoesNotWriteTrackUuid) {
  EnableTrace();

  TRACE_EVENT_END("cat");
  auto end_event = ParseTrackEvent();
  EXPECT_FALSE(end_event.has_track_uuid());

  CancelTrace();
}

// In the client library, EmitEmptyPacket() isn't called and the packet
// disappears from the trace. This functionality is instead tested in Perfetto's
// API integration tests. We just verify that the macro builds correctly here
// when building with the client library.
#define MAYBE_EmptyEvent DISABLED_EmptyEvent
TEST_F(TypedTraceEventTest, MAYBE_EmptyEvent) {
  EnableTrace();

  EXPECT_FALSE(g_test_trace_packet->emit_empty_called);
  EXPECT_TRUE(g_test_trace_packet->emit_empty_called);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, ChromeMojoInterfaceTag) {
  EnableTrace();

  TRACE_EVENT("cat", "Name", [](perfetto::EventContext ctx) {
    auto* info = ctx.event()->set_chrome_mojo_event_info();
    if (!ctx.ShouldFilterDebugAnnotations()) {
      info->set_mojo_interface_tag("MojoInterface");
    }
  });
  auto track_event = ParseTrackEvent();
  // Be default, debug annotations are enabled in the tests. So
  // mojo_interface_tag should be emitted.
  EXPECT_EQ(track_event.chrome_mojo_event_info().mojo_interface_tag(),
            "MojoInterface");

  CancelTrace();
}

class TypedTraceEventFilterDebugAnnotationsTest : public TypedTraceEventTest {
 public:
  TypedTraceEventFilterDebugAnnotationsTest() {
    EnableTypedTraceEvents(&PrepareTrackEventFilterDebugAnnotations,
                           &PrepareTracePacket, &EmitEmptyPacket);
  }
};

TEST_F(TypedTraceEventFilterDebugAnnotationsTest, ChromeMojoInterfaceTag) {
  EnableTrace(/*filter_debug_annotations=*/true);

  TRACE_EVENT("cat", "Name", [](perfetto::EventContext ctx) {
    auto* info = ctx.event()->set_chrome_mojo_event_info();
    if (!ctx.ShouldFilterDebugAnnotations()) {
      info->set_mojo_interface_tag("MojoInterface");
    }
  });
  auto track_event = ParseTrackEvent();
  // Debug annotations are disabled. So mojo_interface_tag should not be
  // emitted.
  EXPECT_FALSE(track_event.chrome_mojo_event_info().has_mojo_interface_tag());

  CancelTrace();
}

}  // namespace trace_event
}  // namespace base
