// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"

namespace base::test {

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TestTraceProcessor::TestTraceProcessor() = default;
TestTraceProcessor::~TestTraceProcessor() = default;

void TestTraceProcessor::StartTrace(const StringPiece& category_filter_string) {
  session_ = perfetto::Tracing::NewTrace();
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
  session_->Setup(config);
  // Some tests run the tracing service on the main thread and StartBlocking()
  // can deadlock so use a RunLoop instead.
  base::RunLoop run_loop;
  session_->SetOnStartCallback([&run_loop]() { run_loop.QuitWhenIdle(); });
  session_->Start();
  run_loop.Run();
}

absl::Status TestTraceProcessor::StopAndParseTrace() {
  base::TrackEvent::Flush();
  session_->StopBlocking();
  std::vector<char> trace = session_->ReadTraceBlocking();
  return test_trace_processor_.ParseTrace(trace);
}

base::expected<TestTraceProcessor::QueryResult, std::string>
TestTraceProcessor::RunQuery(const std::string& query) {
  auto result = test_trace_processor_.ExecuteQuery(query);
  if (absl::holds_alternative<std::string>(result)) {
    return base::unexpected(absl::get<std::string>(result));
  }
  return base::ok(absl::get<TestTraceProcessorImpl::QueryResult>(result));
}

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace base::test
