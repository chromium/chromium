// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal_serializer.h"

#include "base/containers/span.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/tracing/common/system_profile_metadata_recorder.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/common/builtin_clock.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/trace_config.pbzero.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/clock_snapshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/perfetto/tracing_service_event.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/screenshot.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
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
  observed_task_ids_.clear();
  observed_track_ids_.clear();
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

  // Record the system info in the actor journal.
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    tracing::RecordSystemProfileMetadata(msg->set_chrome_events());
    WriteTracePacket(msg.SerializeAsArray());
  }
}

AggregatedJournalSerializer::~AggregatedJournalSerializer() {
  journal_->RemoveObserver(this);
}

void AggregatedJournalSerializer::WillAddJournalEntry(
    const AggregatedJournal::Entry& entry) {
  ObservedTaskId(entry.data->task_id);
  ObservedTrackId(entry.data->track_uuid, entry.data->task_id,
                  entry.data->event);

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
  track_event->set_track_uuid(entry.data->track_uuid);

  // For Perfetto to read screenshots we need to have the category as
  // "android_screenshot". See
  // https://github.com/google/perfetto/blob/891351c7233523c01dc0e58ac8650df47fad9ab5/src/trace_processor/perfetto_sql/stdlib/android/screenshots.sql#L37
  track_event->add_categories(
      entry.screenshot.has_value() ? "android_screenshot" : "actor");

  for (auto& details_entry : entry.data->details) {
    auto* annotation = track_event->add_debug_annotations();
    annotation->set_name(details_entry->key);
    annotation->set_string_value(details_entry->value);
  }

  // If we have an annontated page content we encode it into screenshot
  // descriptor for now. TODO(dtapuska): annotation->set_proto_value
  // wasn't working because it didn't know about the encoded protobuf
  // type in the chrome_intelligence_proto_features.AnnotatedPageContent type.
  if (entry.annotated_page_content.has_value()) {
    auto* screenshot = track_event->set_screenshot();
    screenshot->set_jpg_image(entry.annotated_page_content->data(),
                              entry.annotated_page_content->size());
  }

  if (entry.screenshot.has_value()) {
    auto* screenshot = track_event->set_screenshot();
    // Despite being named jpg_image this field will support any image/* payload
    screenshot->set_jpg_image(entry.screenshot->data(),
                              entry.screenshot->size());
  }

  if (!entry.url.empty()) {
    auto* annotation = track_event->add_debug_annotations();
    annotation->set_name("url");
    annotation->set_string_value(entry.url);
  }

  WriteTracePacket(msg.SerializeAsArray());
}

void AggregatedJournalSerializer::ObservedTaskId(TaskId task_id) {
  if (task_id.value() == 0 || observed_task_ids_.contains(task_id)) {
    return;
  }

  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* track_descriptor = msg->set_track_descriptor();
    track_descriptor->set_uuid(task_id.value());
    track_descriptor->set_name("Actor Task Default");
    auto* process_descriptor = track_descriptor->set_process();
    process_descriptor->set_pid(task_id.value());
    WriteTracePacket(msg.SerializeAsArray());
  }
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* track_descriptor = msg->set_track_descriptor();
    track_descriptor->set_uuid(MakeFrontEndTrackUUID(task_id));
    track_descriptor->set_parent_uuid(task_id.value());
    track_descriptor->set_name("Front End");
    WriteTracePacket(msg.SerializeAsArray());
  }
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* track_descriptor = msg->set_track_descriptor();
    track_descriptor->set_uuid(MakeBrowserTrackUUID(task_id));
    track_descriptor->set_parent_uuid(task_id.value());
    track_descriptor->set_name("Browser");
    WriteTracePacket(msg.SerializeAsArray());
  }
  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* track_descriptor = msg->set_track_descriptor();
    track_descriptor->set_uuid(MakeRendererTrackUUID(task_id));
    track_descriptor->set_parent_uuid(task_id.value());
    track_descriptor->set_name("Renderer");
    WriteTracePacket(msg.SerializeAsArray());
  }
  observed_task_ids_.insert(task_id);
  observed_track_ids_.insert(task_id.value());
  observed_track_ids_.insert(MakeFrontEndTrackUUID(task_id));
  observed_track_ids_.insert(MakeBrowserTrackUUID(task_id));
  observed_track_ids_.insert(MakeRendererTrackUUID(task_id));
}

void AggregatedJournalSerializer::ObservedTrackId(
    uint64_t track_uuid,
    TaskId task_id,
    const std::string& event_name) {
  if (track_uuid == 0 || task_id.value() == 0 ||
      observed_track_ids_.contains(track_uuid)) {
    return;
  }

  {
    protozero::HeapBuffered<perfetto::protos::pbzero::TracePacket> msg;
    msg->set_trusted_packet_sequence_id(sequence_id_++);
    msg->set_timestamp(NowInNanoseconds());
    msg->set_timestamp_clock_id(
        perfetto::protos::pbzero::BUILTIN_CLOCK_REALTIME);
    auto* track_descriptor = msg->set_track_descriptor();

    // For any dynamic track, associate them with the browser track and
    // the name of the first event causing that track.
    // We may need to adjust this if the Renderer or front end start
    // producing dynamic tracks.
    track_descriptor->set_uuid(track_uuid);
    track_descriptor->set_parent_uuid(MakeBrowserTrackUUID(task_id));
    track_descriptor->set_name(event_name);
    WriteTracePacket(msg.SerializeAsArray());
  }
  observed_track_ids_.insert(track_uuid);
}

}  // namespace actor
