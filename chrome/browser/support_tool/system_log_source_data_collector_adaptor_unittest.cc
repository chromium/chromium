// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;

struct TestData {
  // Name of the data in the log source and the file that the logs will be
  // exported into.
  std::string data_source_name;
  // The logs that will be collected.
  std::string test_logs;
  // The version of the logs that has PII sensitive data redacted.
  std::string test_logs_pii_redacted;
};

const TestData kTestData[] = {
    {/*data_source_name=*/"test-log-source-ip",
     /*test_logs=*/
     "Collected data for testing:\n"
     "Will contain some PII sensitive info to test functionality.\n"
     "Some IP addresss as PII here: 0.255.255.255, ::ffff:cb0c:10ea\n",
     /*test_logs_pii_redacted=*/
     "Collected data for testing:\n"
     "Will contain some PII sensitive info to test functionality.\n"
     "Some IP addresss as PII here: (0.0.0.0/8: 1), (IPv6: 1)\n"},
    {/*data_source_name=*/"test-log-source-url",
     /*test_logs=*/
     "More data for testing for this log source:\n"
     "For example some URL address that could be visited by user\n"
     "is chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x "
     "and this will be considered as PII.\n",
     /*test_logs_pii_redacted=*/
     "More data for testing for this log source:\n"
     "For example some URL address that could be visited by user\n"
     "is (URL: 1) and this will be considered as PII.\n"},
};

// The PII sensitive data that the test data contains.
const PIIMap kPIIInTestData = {
    {redaction::PIIType::kIPAddress, {"0.255.255.255", "::ffff:cb0c:10ea"}},
    {redaction::PIIType::kURL,
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
    for (const auto& data : kTestData) {
      response->emplace(data.data_source_name, data.test_logs);
    }
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
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  SystemLogSourceDataCollectorAdaptorTest(
      const SystemLogSourceDataCollectorAdaptorTest&) = delete;
  SystemLogSourceDataCollectorAdaptorTest& operator=(
      const SystemLogSourceDataCollectorAdaptorTest&) = delete;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());
  }

  // Traverses the files in `directory` and returns the contents of the files in
  // a map.
  std::map<base::FilePath, std::string> ReadFileContentsToMap(
      base::FilePath directory) {
    std::map<base::FilePath, std::string> contents;
    base::FileEnumerator file_enumerator(directory, /*recursive=*/false,
                                         base::FileEnumerator::FILES);
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      std::string logs;
      EXPECT_TRUE(base::ReadFileToString(path, &logs));
      contents[path] = logs;
    }
    return contents;
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

TEST_F(SystemLogSourceDataCollectorAdaptorTest, CollectAndExportData) {
  // Initialize SystemLogSourceDataCollector for testing.
  SystemLogSourceDataCollectorAdaptor data_collector(
      "System Log Source Data Collector for testing",
      std::make_unique<TestLogSource>());

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);
  PIIMap detected_pii = data_collector.GetDetectedPII();
  EXPECT_THAT(detected_pii, ContainerEq(kPIIInTestData));

  // Check PII removal and data export.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  base::FilePath output_dir = GetTempDirForOutput();
  // Export collected data to a directory and remove all PII from it.
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, test_future_export_data.GetCallback());
  // Check if ExportCollectedDataWithPII call returned an error.
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);
  // Read the output file.
  std::map<base::FilePath, std::string> result_contents =
      ReadFileContentsToMap(output_dir);

  std::map<base::FilePath, std::string> expected_contents;
  for (const auto& data : kTestData) {
    expected_contents[output_dir.AppendASCII(data.data_source_name)
                          .AddExtension(FILE_PATH_LITERAL(".log"))] =
        data.test_logs_pii_redacted;
  }
  EXPECT_THAT(result_contents, ContainerEq(expected_contents));
}
