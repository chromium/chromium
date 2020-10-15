// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/interned_args_helper.h"

#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

namespace base {
namespace trace_event {

//  static
void InternedSourceLocation::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const TraceSourceLocation& location) {
  auto* msg = interned_data->add_source_locations();
  msg->set_iid(iid);
  if (location.file_name != nullptr)
    msg->set_file_name(location.file_name);
  if (location.function_name != nullptr)
    msg->set_function_name(location.function_name);
  // TODO(ssid): Add line number once it is whitelisted in internal proto.
  // TODO(ssid): Add program counter to the proto fields when
  // !BUILDFLAG(ENABLE_LOCATION_SOURCE).
  // TODO(http://crbug.com760702) remove file name and just pass the program
  // counter to the heap profiler macro.
  // TODO(ssid): Consider writing the program counter of the current task
  // (from the callback function pointer) instead of location that posted the
  // task.
}

// static
void InternedLogMessage::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const std::string& log_message) {
  auto* msg = interned_data->add_log_message_body();
  msg->set_iid(iid);
  msg->set_body(log_message);
}

}  // namespace trace_event
}  // namespace base
