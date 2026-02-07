// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/system_state_data_collector.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
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
const std::map<std::string, std::string> kExpectedDebugdLogs = {
    {// The data collector adds .txt extension to file names when creating the
     // file.
     "Sample Log.txt",
     // Redacted version of "Your email address is abc@abc.com"
     "Your email address is (email: 1)"}};

// The PII in the sample logs that `FakeDebugDaemonClient` returns in
// `GetFeedbackLogs()` call.
const PIIMap kExpectedPIIInFeedbackLogs = {
    {redaction::PIIType::kEmail, {"abc@abc.com"}}};

}  // namespace

class SystemStateDataCollectorTest : public ::testing::Test {
 public:
  SystemStateDataCollectorTest() {
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<user_manager::FakeUserManager>());
    // Set up task runner and container for RedactionTool. We will use when
    // calling CollectDataAndDetectPII() and ExportCollectedDataWithPII()
    // functions on SystemStateDataCollectorTest for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  SystemStateDataCollectorTest(const SystemStateDataCollectorTest&) = delete;
  SystemStateDataCollectorTest& operator=(const SystemStateDataCollectorTest&) =
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

TEST_F(SystemStateDataCollectorTest, CollectAndExportData) {
  // Initialize SystemStateDataCollector for testing.
  SystemStateDataCollector data_collector;

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

  // Read the output file contents. SystemStateDataCollector opens a
  // "chromeos_system_state" file under target directory and writes the log
  // files on to it.
  std::map<std::string, std::string> output_file_contents =
      ReadFileContentsToMap(
          output_dir.Append(FILE_PATH_LITERAL("chromeos_system_state")));

  std::map<std::string, std::string> expected_logs;
  // SystemStateDataCollector will include all the logs that's returned from
  // debugd.
  for (const auto& [log_name, log_content] : kExpectedDebugdLogs) {
    expected_logs.emplace(log_name, log_content);
  }

  // SystemStateDataCollector will include the extra logs listed.
  for (const auto& extra_log : SystemStateDataCollector::GetExtraLogNames()) {
    expected_logs.emplace(
        // SystemStateDataCollector adds .txt extension to file names.
        base::StringPrintf("%s.txt", extra_log.c_str()),
        // The output format FakeDebugDaemonClient returns in.
        base::StringPrintf("%s: response from GetLog", extra_log.c_str()));
  }

  EXPECT_THAT(output_file_contents, ContainerEq(expected_logs));
}
