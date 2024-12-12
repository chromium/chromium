// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_test_utils.h"

#include "base/trace_event/trace_log.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_config.h"

namespace base {
namespace test {

TracingEnvironment::TracingEnvironment() {
  trace_event::TraceLog::GetInstance()->ResetForTesting();
}

TracingEnvironment::~TracingEnvironment() {
  perfetto::Tracing::ResetForTesting();
}

// static
perfetto::protos::gen::TraceConfig TracingEnvironment::GetDefaultTraceConfig() {
  perfetto::protos::gen::TraceConfig trace_config;
  auto* buffer_config = trace_config.add_buffers();
  buffer_config->set_size_kb(32 * 1024);
  auto* data_source = trace_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("track_event");
  source_config->set_target_buffer(0);
  return trace_config;
}

}  // namespace test
}  // namespace base
