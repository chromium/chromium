// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/typed_macros.h"

#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/trace_log.h"
#include "base/trace_event/typed_macros_embedder_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pb.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"

namespace base {
namespace trace_event {

namespace {

constexpr const char kRecordAllCategoryFilter[] = "*";

void CancelTraceAsync(WaitableEvent* flush_complete_event) {
  TraceLog::GetInstance()->CancelTracing(base::BindRepeating(
      [](WaitableEvent* complete_event,
         const scoped_refptr<base::RefCountedString>&, bool has_more_events) {
        if (!has_more_events)
          complete_event->Signal();
      },
      base::Unretained(flush_complete_event)));
}

void CancelTrace() {
  WaitableEvent flush_complete_event(WaitableEvent::ResetPolicy::AUTOMATIC,
                                     WaitableEvent::InitialState::NOT_SIGNALED);
  CancelTraceAsync(&flush_complete_event);
  flush_complete_event.Wait();
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
};

TrackEventHandle PrepareTrackEvent(TraceEvent*) {
  CHECK_NE(g_test_track_event, nullptr) << "TestTrackEvent not set yet";
  g_test_track_event->prepare_called = true;
  return TrackEventHandle(g_test_track_event->event.get(),
                          &g_test_track_event->incremental_state,
                          g_test_track_event);
}

TracePacketHandle PrepareTracePacket() {
  CHECK_NE(g_test_track_event, nullptr) << "TestTracePacket not set yet";
  g_test_trace_packet->prepare_called = true;
  return TracePacketHandle(TracePacketHandle::PerfettoPacketHandle(
                               g_test_trace_packet->packet.get()),
                           g_test_trace_packet);
}

class TypedTraceEventTest : public testing::Test {
 public:
  TypedTraceEventTest() {
    perfetto::internal::TrackRegistry::InitializeInstance();
    EnableTypedTraceEvents(&PrepareTrackEvent, &PrepareTracePacket);
  }

  ~TypedTraceEventTest() override { ResetTypedTraceEventsForTesting(); }

  perfetto::protos::TrackEvent ParseTrackEvent() {
    auto serialized_data = event_.event.SerializeAsArray();
    perfetto::protos::TrackEvent track_event;
    EXPECT_TRUE(track_event.ParseFromArray(serialized_data.data(),
                                           serialized_data.size()));
    return track_event;
  }

 protected:
  TestTrackEvent event_;
  TestTracePacket packet_;
};

}  // namespace

TEST_F(TypedTraceEventTest, CallbackExecutedWhenTracingEnabled) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT("cat", "Name", [this](perfetto::EventContext ctx) {
    EXPECT_EQ(ctx.event(), event_.event.get());
    perfetto::protos::pbzero::LogMessage* log = ctx.event()->set_log_message();
    log->set_body_iid(1);
  });

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
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT("cat", "Name", perfetto::Track(1234));

  EXPECT_TRUE(event_.prepare_called);
  EXPECT_FALSE(event_.event.empty());
  EXPECT_TRUE(event_.event_completed);

  EXPECT_TRUE(packet_.prepare_called);
  EXPECT_FALSE(packet_.packet.empty());
  EXPECT_TRUE(packet_.packet_completed);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InternedData) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);
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
    base::Location base_location =
        base::Location::Current("TestFunction", "test.cc", 123);
    TraceSourceLocation location3(base_location);
    size_t iid4 = InternedSourceLocation::Get(&ctx, location3);
    EXPECT_EQ(iid, iid4);
  });

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

  // No new data should have been interned the second time around.
  EXPECT_EQ(
      "",
      event_.incremental_state.serialized_interned_data.SerializeAsString());

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantThreadEvent) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_INSTANT("cat", "ThreadEvent", [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_FALSE(track_event.has_track_uuid());

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantProcessEvent) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_INSTANT("cat", "ProcessEvent", perfetto::ProcessTrack::Current(),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::ProcessTrack::Current().uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantGlobalEvent) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_INSTANT("cat", "GlobalEvent", perfetto::Track::Global(1234),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::Track::Global(1234).uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, InstantGlobalDefaultEvent) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_INSTANT("cat", "GlobalDefaultEvent", perfetto::Track::Global(0),
                      [](perfetto::EventContext) {});
  auto track_event = ParseTrackEvent();
  EXPECT_TRUE(track_event.has_track_uuid());
  EXPECT_EQ(track_event.track_uuid(), perfetto::Track::Global(0).uuid);

  CancelTrace();
}

TEST_F(TypedTraceEventTest, BeginEventOnDefaultTrackDoesNotWriteTrackUuid) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_BEGIN("cat", "Name");
  auto begin_event = ParseTrackEvent();
  EXPECT_FALSE(begin_event.has_track_uuid());

  CancelTrace();
}

TEST_F(TypedTraceEventTest, EndEventOnDefaultTrackDoesNotWriteTrackUuid) {
  TraceLog::GetInstance()->SetEnabled(TraceConfig(kRecordAllCategoryFilter, ""),
                                      TraceLog::RECORDING_MODE);

  TRACE_EVENT_END("cat");
  auto end_event = ParseTrackEvent();
  EXPECT_FALSE(end_event.has_track_uuid());

  CancelTrace();
}

}  // namespace trace_event
}  // namespace base
