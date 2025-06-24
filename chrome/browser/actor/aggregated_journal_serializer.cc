// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal_serializer.h"

#include "base/containers/span.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/common/builtin_clock.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/perfetto/tracing_service_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/screenshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace actor {

namespace {

uint64_t NowInNanoseconds() {
  return (base::Time::Now() - base::Time::UnixEpoch()).InNanoseconds();
}

}  // namespace

AggregatedJournalSerializer::AggregatedJournalSerializer(
    AggregatedJournal& journal)
    : journal_(journal.GetSafeRef()) {}

void AggregatedJournalSerializer::InitImpl() {
  WriteTracePreamble();
  journal_->AddObserver(this);
}

void AggregatedJournalSerializer::WriteTracePreamble() {
  // Write initial message in protobuf.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> init_msg;
    init_msg->set_trusted_packet_sequence_id(sequence_id_++);
    auto* clock_snapshot = init_msg->set_clock_snapshot();
    clock_snapshot->set_primary_trace_clock(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* clock = clock_snapshot->add_clocks();
    clock->set_clock_id(perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    clock->set_timestamp(NowInNanoseconds());

    auto* trace_config = init_msg->set_trace_config();
    auto* data_source = trace_config->add_data_sources();
    auto* source_config = data_source->set_config();
    source_config->set_name("track_event");
    source_config->set_target_buffer(0);
    perfetto::protos::gen::TrackEventConfig track_event_config;
    track_event_config.add_enabled_categories("*");
    source_config->set_track_event_config_raw(
        track_event_config.SerializeAsString());
    WriteTracePacket(init_msg.SerializeAsArray());
  }
  // Write tracing started.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* service_event = msg->set_service_event();
    service_event->set_tracing_started(true);
    WriteTracePacket(msg.SerializeAsArray());
  }
  // Write tracting active.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* service_event = msg->set_service_event();
    service_event->set_all_data_sources_started(true);
    WriteTracePacket(msg.SerializeAsArray());
  }
}

AggregatedJournalSerializer::~AggregatedJournalSerializer() {
  journal_->RemoveObserver(this);
}

void AggregatedJournalSerializer::WillAddJournalEntry(
    const AggregatedJournal::Entry& entry) {
  protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
  msg->set_trusted_packet_sequence_id(sequence_id_++);
  msg->set_timestamp(
      (entry.data->timestamp - base::Time::UnixEpoch()).InNanoseconds());
  msg->set_timestamp_clock_id(perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
  auto* track_event = msg->set_track_event();
  perfetto::protos::pbzero::TrackEvent_Type pb_type =
      perfetto::protos::pbzero::TrackEvent::TYPE_UNSPECIFIED;
  switch (entry.data->type) {
    case mojom::JournalEntryType::kBegin:
      pb_type = perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN;
      break;
    case mojom::JournalEntryType::kEnd:
      pb_type = perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_END;
      break;
    case mojom::JournalEntryType::kInstant:
      pb_type = perfetto::protos::pbzero::TrackEvent::TYPE_INSTANT;
      break;
  }
  track_event->set_type(pb_type);
  track_event->set_name(entry.data->event);
  // TODO(dtapuska): We likely want to set the track UUID to be the task.
  // track_event->set_track_uuid(entry.data->task_id);

  // For Perfetto to read screenshots we need to have the category as
  // "android_screenshot". See
  // https://github.com/google/perfetto/blob/891351c7233523c01dc0e58ac8650df47fad9ab5/src/trace_processor/perfetto_sql/stdlib/android/screenshots.sql#L37
  track_event->add_categories(
      entry.jpg_screenshot.has_value() ? "android_screenshot" : "actor");
  auto* annotation = track_event->add_debug_annotations();
  if (!entry.data->details.empty()) {
    annotation->set_name(
        pb_type == perfetto::protos::pbzero::TrackEvent::TYPE_SLICE_BEGIN
            ? "begin_details"
            : "details");
    annotation->set_string_value(entry.data->details);
  }

  if (entry.jpg_screenshot.has_value()) {
    auto* screenshot = track_event->set_screenshot();
    screenshot->set_jpg_image(entry.jpg_screenshot->data(),
                              entry.jpg_screenshot->size());
  }

  WriteTracePacket(msg.SerializeAsArray());
}

}  // namespace actor
