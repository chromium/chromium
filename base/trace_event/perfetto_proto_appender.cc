// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/perfetto_proto_appender.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "third_party/perfetto/include/perfetto/protozero/contiguous_memory_range.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/debug_annotation.pbzero.h"

namespace base::trace_event {

PerfettoProtoAppender::PerfettoProtoAppender(
    perfetto::protos::pbzero::DebugAnnotation* proto)
    : annotation_proto_(proto) {}

PerfettoProtoAppender::~PerfettoProtoAppender() = default;

void PerfettoProtoAppender::AddBuffer(uint8_t* begin, uint8_t* end) {
  ranges_.push_back({.begin = begin, .end = end});
}

size_t PerfettoProtoAppender::Finalize(uint32_t field_id) {
  return annotation_proto_->AppendScatteredBytes(field_id, ranges_.data(),
                                                 ranges_.size());
}

}  // namespace base::trace_event
