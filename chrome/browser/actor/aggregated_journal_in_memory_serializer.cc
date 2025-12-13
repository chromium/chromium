// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/aggregated_journal_in_memory_serializer.h"

#include "base/compiler_specific.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"

namespace actor {

AggregatedJournalInMemorySerializer::AggregatedJournalInMemorySerializer(
    AggregatedJournal& journal,
    size_t max_bytes)
    : AggregatedJournalSerializer(journal), max_bytes_(max_bytes) {}

AggregatedJournalInMemorySerializer::~AggregatedJournalInMemorySerializer() =
    default;

void AggregatedJournalInMemorySerializer::Init() {
  InitImpl();
}

void AggregatedJournalInMemorySerializer::Clear() {
  buffer_list_.clear();
  total_size_ = 0;
  WriteTracePreamble();
}

void AggregatedJournalInMemorySerializer::WriteTracePacket(
    std::vector<uint8_t> message) {
  total_size_ += message.size();
  buffer_list_.push_back(std::move(message));
  while (total_size_ > max_bytes_) {
    total_size_ -= buffer_list_.begin()->size();
    buffer_list_.erase(buffer_list_.begin());
  }
}

size_t AggregatedJournalInMemorySerializer::ApproximateSnapshotSize() {
  return total_size_ +
         (perfetto::TracePacket::kMaxPreambleBytes * buffer_list_.size());
}

std::vector<uint8_t> AggregatedJournalInMemorySerializer::Snapshot() {
  size_t total_size = 0;
  std::vector<uint8_t> result_buffer;
  result_buffer.reserve(std::min(ApproximateSnapshotSize(), max_bytes_));
  for (const auto& buffer : buffer_list_) {
    if (total_size + buffer.size() + perfetto::TracePacket::kMaxPreambleBytes >
        max_bytes_) {
      break;
    }
    perfetto::TracePacket packet;
    packet.AddSlice(buffer.data(), buffer.size());
    auto [preamble, preamble_size] = packet.GetProtoPreamble();
    auto preamble_span = UNSAFE_TODO(
        base::span(reinterpret_cast<const uint8_t*>(preamble), preamble_size));
    result_buffer.insert(result_buffer.end(), preamble_span.begin(),
                         preamble_span.end());
    for (const perfetto::Slice& slice : packet.slices()) {
      auto data_span = UNSAFE_TODO(
          base::span(static_cast<const uint8_t*>(slice.start), slice.size));
      result_buffer.insert(result_buffer.end(), data_span.begin(),
                           data_span.end());
    }
  }
  return result_buffer;
}

}  // namespace actor
