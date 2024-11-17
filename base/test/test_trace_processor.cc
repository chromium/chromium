// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"

#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/test/chrome_track_event.descriptor.h"
#include "base/test/perfetto_sql_stdlib.h"
#include "base/trace_event/trace_log.h"
#include "third_party/perfetto/protos/perfetto/trace/extension_descriptor.pbzero.h"

namespace base::test {


namespace {
// Emitting the chrome_track_event.descriptor into the trace allows the trace
// processor to parse the arguments during ingestion of the trace events.
// This function emits the descriptor generated from
// base/tracing/protos/chrome_track_event.proto so we can use TestTraceProcessor
// to write tests based on new arguments/types added in the same patch.
void EmitChromeTrackEventDescriptor() {
  base::TrackEvent::Trace([&](base::TrackEvent::TraceContext ctx) {
    protozero::MessageHandle<perfetto::protos::pbzero::TracePacket> handle =
        ctx.NewTracePacket();
    auto* extension_descriptor = handle->BeginNestedMessage<protozero::Message>(
        perfetto::protos::pbzero::TracePacket::kExtensionDescriptorFieldNumber);
    extension_descriptor->AppendBytes(
        perfetto::protos::pbzero::ExtensionDescriptor::kExtensionSetFieldNumber,
        base::testing::kChromeTrackEventDescriptor.data(),
        base::testing::kChromeTrackEventDescriptor.size());
    handle->Finalize();
  });
}

std::string kChromeSqlModuleName = "chrome";
// A command-line switch to save the trace test trace processor generated to
// make debugging complex traces.
constexpr char kSaveTraceSwitch[] = "ttp-save-trace";

// Returns a vector of pairs of strings consisting of
// {include_key, sql_file_contents}. For example, the include key for
// `chrome/scroll_jank/utils.sql` is `chrome.scroll_jank.utils`.
// The output is used to override the Chrome SQL module in the trace processor.
TestTraceProcessorImpl::PerfettoSQLModule GetChromeStdlib() {
  std::vector<std::pair<std::string, std::string>> stdlib;
  for (const auto& file_to_sql :
       perfetto::trace_processor::chrome_stdlib::kFileToSql) {
    std::string include_key;
    base::ReplaceChars(file_to_sql.path, "/", ".", &include_key);
    if (include_key.ends_with(".sql")) {
      include_key.resize(include_key.size() - 4);
    }
    stdlib.emplace_back(kChromeSqlModuleName + "." + include_key,
                        file_to_sql.sql);
  }
  return stdlib;
}
}  // namespace

TraceConfig DefaultTraceConfig(std::string_view category_filter_string,
                               bool privacy_filtering) {
  TraceConfig trace_config;
  auto* buffer_config = trace_config.add_buffers();
  buffer_config->set_size_kb(4 * 1024);

  auto* data_source = trace_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("track_event");
  source_config->set_target_buffer(0);

  perfetto::protos::gen::TrackEventConfig track_event_config;
  base::trace_event::TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(category_filter_string);

  // If no categories are explicitly enabled, enable the default ones.
  // Otherwise only matching categories are enabled.
  if (category_filter.included_categories().empty()) {
    track_event_config.add_enabled_categories("*");
  } else {
    track_event_config.add_disabled_categories("*");
  }
  for (const auto& included_category : category_filter.included_categories()) {
    track_event_config.add_enabled_categories(included_category);
  }
  for (const auto& disabled_category : category_filter.disabled_categories()) {
    track_event_config.add_enabled_categories(disabled_category);
  }
  for (const auto& excluded_category : category_filter.excluded_categories()) {
    track_event_config.add_disabled_categories(excluded_category);
  }

  // This category is added by default to tracing sessions initiated via
  // command-line flags (see TraceConfig::ToPerfettoTrackEventConfigRaw),
  // so to adopt startup sessions correctly, we need to specify it too.
  track_event_config.add_enabled_categories("__metadata");

  if (privacy_filtering) {
    track_event_config.set_filter_debug_annotations(true);
    track_event_config.set_filter_dynamic_event_names(true);
  }

  source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());

  return trace_config;
}

TestTraceProcessor::TestTraceProcessor() {
  auto status = test_trace_processor_.OverrideSqlModule(kChromeSqlModuleName,
                                                        GetChromeStdlib());
  CHECK(status.ok());
}

TestTraceProcessor::~TestTraceProcessor() = default;

void TestTraceProcessor::StartTrace(std::string_view category_filter_string,
                                    bool privacy_filtering) {
  StartTrace(DefaultTraceConfig(category_filter_string, privacy_filtering));
}

void TestTraceProcessor::StartTrace(const TraceConfig& config,
                                    perfetto::BackendType backend) {
  // Try to guess the correct backend if it's unspecified. In unit tests
  // Perfetto is initialized by TraceLog, and only the in-process backend is
  // available. In browser tests multiple backend can be available, so we
  // explicitly specialize the custom backend to prevent tests from connecting
  // to a system backend.
  if (backend == perfetto::kUnspecifiedBackend) {
    if (base::trace_event::TraceLog::GetInstance()
            ->IsPerfettoInitializedByTraceLog()) {
      backend = perfetto::kInProcessBackend;
    } else {
      backend = perfetto::kCustomBackend;
    }
  }
  session_ = perfetto::Tracing::NewTrace(backend);
  session_->Setup(config);
  // Some tests run the tracing service on the main thread and StartBlocking()
  // can deadlock so use a RunLoop instead.
  base::RunLoop run_loop;
  session_->SetOnStartCallback([&run_loop] { run_loop.QuitWhenIdle(); });
  session_->Start();
  run_loop.Run();
}

absl::Status TestTraceProcessor::StopAndParseTrace() {
  EmitChromeTrackEventDescriptor();
  base::TrackEvent::Flush();
  session_->StopBlocking();
  std::vector<char> trace = session_->ReadTraceBlocking();

  if (CommandLine::ForCurrentProcess()->HasSwitch(kSaveTraceSwitch)) {
    ScopedAllowBlockingForTesting allow;
    WriteFile(base::FilePath::FromASCII("test.pftrace"), as_byte_span(trace));
  }

  return test_trace_processor_.ParseTrace(trace);
}

base::expected<TestTraceProcessor::QueryResult, std::string>
TestTraceProcessor::RunQuery(const std::string& query) {
  auto result_or_error = test_trace_processor_.ExecuteQuery(query);
  if (!result_or_error.ok()) {
    return base::unexpected(result_or_error.error());
  }
  return base::ok(result_or_error.result());
}


}  // namespace base::test
