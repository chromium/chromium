// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/pii_types.h"
#include "components/feedback/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::ContainerEq;

// Test data that will be collected by TestLogSource.
const char kTestData1[] =
    "Collected data for testing:\nWill contain some PII sensitive info to test "
    "functionality.\nSome IP addresss as PII here: 0.255.255.255, "
    "::ffff:cb0c:10ea\n";
const char kTestData2[] =
    "More data for testing for this log source:\nFor example some URL address "
    "that could be visited by user is "
    "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x and "
    "this will be considered as PII.\n";

// The PII sensitive data that the test data contains.
const PIIMap kPIIInTestData = {
    {feedback::PIIType::kIPAddress, {"0.255.255.255", "::ffff:cb0c:10ea"}},
    {feedback::PIIType::kURL,
     {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x"}}};

namespace {

class TestLogSource : public system_logs::SystemLogsSource {
 public:
  TestLogSource() : SystemLogsSource("Test Log Source") {}

  TestLogSource(const TestLogSource&) = delete;
  TestLogSource& operator=(const TestLogSource&) = delete;

  ~TestLogSource() override = default;

  // SystemLogsSource override.
  void Fetch(system_logs::SysLogsSourceCallback callback) override {
    std::unique_ptr<system_logs::SystemLogsResponse> response =
        std::make_unique<system_logs::SystemLogsResponse>();
    // Add some test data to the response. We add two entries to make sure all
    // entries are collected by the corresponding DataCollector instance.
    response->emplace("test-log-source-1", kTestData1);
    response->emplace("test-log-source-2", kTestData2);
    std::move(callback).Run(std::move(response));
  }
};

}  // namespace

class SystemLogSourceDataCollectorAdaptorTest : public ::testing::Test {
 public:
  SystemLogSourceDataCollectorAdaptorTest() {
    // Set up task runner and container for RedactionTool. We will use these
    // when creating the SystemLogSourceDataCollector instance for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<feedback::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  SystemLogSourceDataCollectorAdaptorTest(
      const SystemLogSourceDataCollectorAdaptorTest&) = delete;
  SystemLogSourceDataCollectorAdaptorTest& operator=(
      const SystemLogSourceDataCollectorAdaptorTest&) = delete;

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<feedback::RedactionToolContainer> redaction_tool_container_;
};

TEST_F(SystemLogSourceDataCollectorAdaptorTest, CollectDataAndDetectPII) {
  // Initialize SystemLogSourceDataCollector for testing.
  SystemLogSourceDataCollectorAdaptor data_collector(
      "System Log Source Data Collector for testing",
      std::make_unique<TestLogSource>());
  base::test::TestFuture<absl::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  absl::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, absl::nullopt);
  PIIMap detected_pii = data_collector.GetDetectedPII();
  EXPECT_THAT(detected_pii, ContainerEq(kPIIInTestData));
}
