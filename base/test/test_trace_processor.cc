// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"

namespace base::test {

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
std::unique_ptr<perfetto::TracingSession> StartTrace(
    const StringPiece& category_filter_string) {
  std::unique_ptr<perfetto::TracingSession> session =
      perfetto::Tracing::NewTrace();
  perfetto::protos::gen::TraceConfig config =
      TracingEnvironment::GetDefaultTraceConfig();
  for (auto& data_source : *config.mutable_data_sources()) {
    perfetto::protos::gen::TrackEventConfig track_event_config;
    base::trace_event::TraceConfigCategoryFilter category_filter;
    category_filter.InitializeFromString(category_filter_string);
    for (const auto& included_category :
         category_filter.included_categories()) {
      track_event_config.add_enabled_categories(included_category);
    }
    for (const auto& disabled_category :
         category_filter.disabled_categories()) {
      track_event_config.add_enabled_categories(disabled_category);
    }
    for (const auto& excluded_category :
         category_filter.excluded_categories()) {
      track_event_config.add_disabled_categories(excluded_category);
    }
    data_source.mutable_config()->set_track_event_config_raw(
        track_event_config.SerializeAsString());
  }
  session->Setup(config);
  session->StartBlocking();
  return session;
}

std::vector<char> StopTrace(std::unique_ptr<perfetto::TracingSession> session) {
  base::TrackEvent::Flush();
  session->StopBlocking();
  return session->ReadTraceBlocking();
}

base::expected<TestTraceProcessorImpl::QueryResult, std::string> RunQuery(
    const std::string& query,
    const std::vector<char>& trace) {
  TestTraceProcessorImpl trace_processor;
  absl::Status status = trace_processor.ParseTrace(trace);
  if (!status.ok()) {
    return base::unexpected(std::string(status.message()));
  }
  return base::ok(trace_processor.ExecuteQuery(query));
}

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace base::test
