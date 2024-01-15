// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/system_logs_data_collector.h"

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;

namespace {

// The logs that `FakeDebugDaemonClient` returns and the redacted versions of
// them.
const std::map<std::string, std::string> kExpectedLogs = {
    {"Sample Log",
     // Redacted version of "Your email address is abc@abc.com"
     "Your email address is (email: 1)"}};

constexpr char kExpectedLogName[] = "Sample Log";

// The PII in the sample logs that `FakeDebugDaemonClient` returns in
// `GetFeedbackLogs()` call.
const PIIMap kExpectedPIIInFeedbackLogs = {
    {redaction::PIIType::kEmail, {"abc@abc.com"}}};

}  // namespace

class SystemLogsDataCollectorTest : public ::testing::Test {
 public:
  SystemLogsDataCollectorTest() {
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());
    // Set up task runner and container for RedactionTool. We will use when
    // calling CollectDataAndDetectPII() and ExportCollectedDataWithPII()
    // functions on SystemLogsDataCollectorTest for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  SystemLogsDataCollectorTest(const SystemLogsDataCollectorTest&) = delete;
  SystemLogsDataCollectorTest& operator=(const SystemLogsDataCollectorTest&) =
      delete;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ash::DebugDaemonClient::InitializeFake();
  }

  void TearDown() override {
    if (!temp_dir_.IsValid())
      return;
    EXPECT_TRUE(temp_dir_.Delete());

    ash::DebugDaemonClient::Shutdown();
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  // Traverses the files in `directory` and returns the contents of the files in
  // a map.
  std::map<std::string, std::string> ReadFileContentsToMap(
      base::FilePath directory) {
    std::map<std::string, std::string> contents;
    base::FileEnumerator file_enumerator(directory, /*recursive=*/false,
                                         base::FileEnumerator::FILES);
    for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
         path = file_enumerator.Next()) {
      std::string logs;
      EXPECT_TRUE(base::ReadFileToString(path, &logs));
      contents[path.BaseName().AsUTF8Unsafe()] = logs;
    }
    return contents;
  }

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

TEST_F(SystemLogsDataCollectorTest, CollectAndExportDataSuccess) {
  std::set<base::FilePath> requested_logs = {
      base::FilePath(FILE_PATH_LITERAL(kExpectedLogName))};
  // Initialize SystemLogsDataCollector for testing.
  SystemLogsDataCollector data_collector(requested_logs);

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  EXPECT_THAT(data_collector.GetDetectedPII(),
              ContainerEq(kExpectedPIIInFeedbackLogs));

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

  // Read the output file contents. SystemLogsDataCollector opens a
  // "var_log_files" file under target directory and writes the log
  // files on to it.
  std::map<std::string, std::string> output_file_contents =
      ReadFileContentsToMap(
          output_dir.Append(FILE_PATH_LITERAL("var_log_files")));

  EXPECT_THAT(output_file_contents, ContainerEq(kExpectedLogs));
}

TEST_F(SystemLogsDataCollectorTest, RequestedLogNotFound) {
  // Set `requested_logs` as a log that debugd won't return.
  std::set<base::FilePath> requested_logs = {
      base::FilePath(FILE_PATH_LITERAL("requested_log"))};
  // Initialize SystemLogsDataCollector for testing.
  SystemLogsDataCollector data_collector(requested_logs);

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error. It should return
  // an error because it won't be able to retrieve requested logs.
  SupportToolError error = test_future_collect_data.Get().value();
  EXPECT_EQ(error.error_code, SupportToolErrorCode::kDataCollectorError);
  EXPECT_EQ(error.error_message,
            "SystemLogsDataCollector couldn't retrieve requested logs.");

  // Check PII removal and data export.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  base::FilePath output_dir = GetTempDirForOutput();
  // Export collected data to a directory and remove all PII from it.
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, test_future_export_data.GetCallback());
  // Check if ExportCollectedDataWithPII call returned an error.
  std::optional<SupportToolError> export_error = test_future_export_data.Get();
  EXPECT_EQ(export_error, std::nullopt);

  // Read the output file contents. SystemLogsDataCollector opens a
  // "var_log_files" file under target directory and writes the log
  // files on to it.
  std::map<std::string, std::string> output_file_contents =
      ReadFileContentsToMap(
          output_dir.Append(FILE_PATH_LITERAL("var_log_files")));
  // `output_file_contents` should be empty because none of the requested files
  // are found.
  EXPECT_TRUE(output_file_contents.empty());
}
