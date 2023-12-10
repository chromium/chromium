// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Use TestTraceProcessor to load a perfetto trace and run queries on the trace.
// Documentation on how to use the trace processor and write queries can be
// found here: https://perfetto.dev/docs/analysis/trace-processor.
// TODO(b/224531105): Implement EXTRACT_ARGS to return multiple args to simplify
// queries.

#ifndef BASE_TEST_TEST_TRACE_PROCESSOR_H_
#define BASE_TEST_TEST_TRACE_PROCESSOR_H_

#include "base/test/test_trace_processor_impl.h"
#include "base/test/trace_test_utils.h"
#include "base/types/expected.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY) && !BUILDFLAG(IS_WIN)
#define TEST_TRACE_PROCESSOR_ENABLED
#endif

namespace base::test {

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

using perfetto::protos::gen::TraceConfig;

TraceConfig DefaultTraceConfig(const StringPiece& category_filter_string,
                               bool privacy_filtering);

// Use TestTraceProcessor to record Perfetto traces in unit and browser tests.
// This API can be used to start and stop traces, run SQL queries on the trace
// and write expectations against the query result.
//
// Example:
//
//   TestTraceProcessor test_trace_processor;
//   test_trace_processor.StartTrace();
//
//   /* do stuff */
//
//   absl::Status status = test_trace_processor.StopAndParseTrace();
//   ASSERT_TRUE(status.ok()) << status.message();
//
//   std::string query = "YOUR QUERY";
//   auto result = test_trace_processor.RunQuery(query);
//
//   ASSERT_TRUE(result.has_value()) << result.message();
//   EXPECT_THAT(result.value(), /* your expectations */);

class TestTraceProcessor {
 public:
  using QueryResult = std::vector<std::vector<std::string>>;

  TestTraceProcessor();
  ~TestTraceProcessor();

  void StartTrace(const StringPiece& category_filter_string,
                  bool privacy_filtering = false);
  void StartTrace(
      const TraceConfig& config,
      perfetto::BackendType backend = perfetto::kUnspecifiedBackend);

  absl::Status StopAndParseTrace();

  base::expected<QueryResult, std::string> RunQuery(const std::string& query);

 private:
  TestTraceProcessorImpl test_trace_processor_;
  std::unique_ptr<perfetto::TracingSession> session_;
};

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace base::test

#endif  // BASE_TEST_TEST_TRACE_PROCESSOR_H_
