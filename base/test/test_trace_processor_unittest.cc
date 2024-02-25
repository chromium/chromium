// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"

#include "base/test/task_environment.h"
#include "base/test/trace_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

namespace base::test {

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

class TestTraceProcessorTest : public ::testing::Test {
 private:
  base::test::TracingEnvironment tracing_environment_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(TestTraceProcessorTest, Basic) {
  perfetto::Tracing::Initialize(perfetto::TracingInitArgs());
  TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace(
      base::test::DefaultTraceConfig("", /*privacy_filtering=*/false));

  auto status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();
}

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace base::test
