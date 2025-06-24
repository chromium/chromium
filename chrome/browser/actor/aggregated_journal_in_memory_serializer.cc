// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"

#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"

namespace actor {

AggregatedJournalInMemorySerializer::AggregatedJournalInMemorySerializer(
    AggregatedJournal& journal)
    : AggregatedJournalSerializer(journal) {}

AggregatedJournalInMemorySerializer::~AggregatedJournalInMemorySerializer() =
    default;

void AggregatedJournalInMemorySerializer::Init() {
  InitImpl();
}

void AggregatedJournalInMemorySerializer::Clear() {
  buffer_list_.clear();
  WriteTracePreamble();
}

void AggregatedJournalInMemorySerializer::WriteTracePacket(
    std::vector<uint8_t> message) {
  buffer_list_.push_back(std::move(message));
}

size_t AggregatedJournalInMemorySerializer::ApproximateSnapshotSize() {
  size_t total_size = 0;
  for (const auto& buffer : buffer_list_) {
    total_size += buffer.size();
    total_size += perfetto::TracePacket::kMaxPreambleBytes;
  }
  return total_size;
}

std::vector<uint8_t> AggregatedJournalInMemorySerializer::Snapshot(
    size_t max_bytes) {
  size_t total_size = 0;
  std::vector<uint8_t> result_buffer;
  result_buffer.reserve(std::min(ApproximateSnapshotSize(), max_bytes));
  for (const auto& buffer : buffer_list_) {
    if (total_size + buffer.size() + perfetto::TracePacket::kMaxPreambleBytes >
        max_bytes) {
      break;
    }
    perfetto::TracePacket packet;
    packet.AddSlice(buffer.data(), buffer.size());
    auto [preamble, preamble_size] = packet.GetProtoPreamble();
    auto preamble_span =
        base::span(reinterpret_cast<const uint8_t*>(preamble), preamble_size);
    result_buffer.insert(result_buffer.end(), preamble_span.begin(),
                         preamble_span.end());
    for (const perfetto::Slice& slice : packet.slices()) {
      auto data_span =
          base::span(static_cast<const uint8_t*>(slice.start), slice.size);
      result_buffer.insert(result_buffer.end(), data_span.begin(),
                           data_span.end());
    }
  }
  return result_buffer;
}

}  // namespace actor
