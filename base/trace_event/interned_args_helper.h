// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_INTERNED_ARGS_HELPER_H_
#define BASE_TRACE_EVENT_INTERNED_ARGS_HELPER_H_

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"

namespace base {
namespace trace_event {

// TrackEventInternedDataIndex expects the same data structure to be used for
// all interned fields with the same field number. We can't use base::Location
// for log event's location since base::Location uses program counter based
// location.
struct BASE_EXPORT TraceSourceLocation {
  const char* function_name = nullptr;
  const char* file_name = nullptr;
  size_t line_number = 0;

  TraceSourceLocation() = default;
  TraceSourceLocation(const char* function_name,
                      const char* file_name,
                      size_t line_number)
      : function_name(function_name),
        file_name(file_name),
        line_number(line_number) {}
  // Construct a new source location from an existing base::Location, the only
  // attributes that are read are |function_name|, |file_name| and
  // |line_number|.
  explicit TraceSourceLocation(const base::Location& location)
      : function_name(location.function_name()),
        file_name(location.file_name()),
        line_number(location.line_number()) {}

  bool operator==(const TraceSourceLocation& other) const {
    return file_name == other.file_name &&
           function_name == other.function_name &&
           line_number == other.line_number;
  }
};

}  // namespace trace_event
}  // namespace base

namespace std {

template <>
struct hash<base::trace_event::TraceSourceLocation> {
  std::size_t operator()(
      const base::trace_event::TraceSourceLocation& loc) const {
    static_assert(sizeof(base::trace_event::TraceSourceLocation) ==
                      2 * sizeof(const char*) + sizeof(size_t),
                  "Padding will cause uninitialized memory to be hashed.");
    return base::FastHash(base::as_bytes(base::make_span(&loc, 1)));
  }
};

}  // namespace std

namespace base {
namespace trace_event {

struct BASE_EXPORT InternedSourceLocation
    : public perfetto::TrackEventInternedDataIndex<
          InternedSourceLocation,
          perfetto::protos::pbzero::InternedData::kSourceLocationsFieldNumber,
          TraceSourceLocation> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const TraceSourceLocation& location);
};

struct BASE_EXPORT InternedLogMessage
    : public perfetto::TrackEventInternedDataIndex<
          InternedLogMessage,
          perfetto::protos::pbzero::InternedData::kLogMessageBodyFieldNumber,
          std::string> {
  static void Add(perfetto::protos::pbzero::InternedData* interned_data,
                  size_t iid,
                  const std::string& log_message);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_INTERNED_ARGS_HELPER_H_
