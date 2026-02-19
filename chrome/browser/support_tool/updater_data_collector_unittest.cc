// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/updater_data_collector.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/updater/updater_scope.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::UnorderedElementsAre;

namespace {

// Test data containing PII.
constexpr char kLogFileContent[] =
    "This is a log file with an IP address: 192.168.0.1 and a URL: "
    "http://example.com";
constexpr char kRedactedLogFileContent[] =
    "This is a log file with an IP address: (192.168.0.0/16: 1) and a URL: "
    "(URL: 1)";

constexpr char kPrefsFileContent[] =
    "{\"email\": \"test@example.com\", \"ip\": \"10.0.0.1\"}";
constexpr char kHistoryFileContent[] =
    "{\"event\": \"update\", \"user\": \"pii@example.com\"}\n"
    "{\"event\": \"install\", \"ip\": \"1.2.3.4\"}";

}  // namespace

class UpdaterDataCollectorTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Creates a fake updater install directory for the given scope with some
  // test files.
  base::FilePath CreateFakeUpdaterInstall(updater::UpdaterScope scope,
                                          bool create_files = true) {
    base::FilePath install_dir = temp_dir_.GetPath().AppendASCII(
        scope == updater::UpdaterScope::kSystem ? "System" : "User");
    EXPECT_TRUE(base::CreateDirectory(install_dir));

    if (create_files) {
      EXPECT_TRUE(base::WriteFile(install_dir.AppendASCII("updater.log"),
                                  kLogFileContent));
      EXPECT_TRUE(base::WriteFile(install_dir.AppendASCII("prefs.json"),
                                  kPrefsFileContent));
      EXPECT_TRUE(
          base::WriteFile(install_dir.AppendASCII("updater_history.jsonl"),
                          kHistoryFileContent));
      EXPECT_TRUE(
          base::WriteFile(install_dir.AppendASCII("updater_history.jsonl.old"),
                          kHistoryFileContent));
    }
    return install_dir;
  }

  void VerifyExportedData(const base::FilePath& output_dir,
                          updater::UpdaterScope scope,
                          bool expect_files = true) {
    base::FilePath scope_dir = output_dir.AppendASCII("updater").AppendASCII(
        scope == updater::UpdaterScope::kSystem ? "system" : "user");
    EXPECT_TRUE(base::PathExists(scope_dir));

    base::FilePath log_path = scope_dir.AppendASCII("updater.log");
    base::FilePath prefs_path = scope_dir.AppendASCII("prefs.json");
    base::FilePath history_path =
        scope_dir.AppendASCII("updater_history.jsonl");
    base::FilePath old_history_path =
        scope_dir.AppendASCII("updater_history.jsonl.old");

    if (expect_files) {
      std::string log_contents;
      EXPECT_TRUE(base::ReadFileToString(log_path, &log_contents));
      EXPECT_EQ(log_contents, kRedactedLogFileContent);

      std::string prefs_contents;
      EXPECT_TRUE(base::ReadFileToString(prefs_path, &prefs_contents));
      EXPECT_EQ(prefs_contents, kPrefsFileContent);

      std::string history_contents;
      EXPECT_TRUE(base::ReadFileToString(history_path, &history_contents));
      EXPECT_EQ(history_contents, kHistoryFileContent);

      std::string old_history_contents;
      EXPECT_TRUE(base::ReadFileToString(old_history_path, &history_contents));
      EXPECT_EQ(history_contents, kHistoryFileContent);
    } else {
      EXPECT_FALSE(base::PathExists(log_path));
      EXPECT_FALSE(base::PathExists(prefs_path));
      EXPECT_FALSE(base::PathExists(history_path));
      EXPECT_FALSE(base::PathExists(old_history_path));
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_ =
      base::MakeRefCounted<redaction::RedactionToolContainer>(
          task_runner_for_redaction_tool_);
};

TEST_F(UpdaterDataCollectorTest, CollectAndExportData) {
  base::FilePath user_dir =
      CreateFakeUpdaterInstall(updater::UpdaterScope::kUser);
  base::FilePath system_dir =
      CreateFakeUpdaterInstall(updater::UpdaterScope::kSystem);

  UpdaterDataCollector collector(user_dir, system_dir);

  base::test::TestFuture<std::optional<SupportToolError>> collect_future;
  collector.CollectDataAndDetectPII(collect_future.GetCallback(),
                                    task_runner_for_redaction_tool_,
                                    redaction_tool_container_);
  ASSERT_EQ(collect_future.Get(), std::nullopt);

  EXPECT_THAT(collector.GetDetectedPII(),
              UnorderedElementsAre(Key(redaction::PIIType::kIPAddress),
                                   Key(redaction::PIIType::kURL)));

  base::test::TestFuture<std::optional<SupportToolError>> export_future;
  base::FilePath output_dir = temp_dir_.GetPath().AppendASCII("exported_logs");
  collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, export_future.GetCallback());
  ASSERT_EQ(export_future.Get(), std::nullopt);

  VerifyExportedData(output_dir, updater::UpdaterScope::kUser);
  VerifyExportedData(output_dir, updater::UpdaterScope::kSystem);
}

TEST_F(UpdaterDataCollectorTest, EmptyDirectories) {
  base::FilePath user_dir = CreateFakeUpdaterInstall(
      updater::UpdaterScope::kUser, /*create_files=*/false);
  base::FilePath system_dir = CreateFakeUpdaterInstall(
      updater::UpdaterScope::kSystem, /*create_files=*/false);

  UpdaterDataCollector collector(user_dir, system_dir);

  base::test::TestFuture<std::optional<SupportToolError>> collect_future;
  collector.CollectDataAndDetectPII(collect_future.GetCallback(),
                                    task_runner_for_redaction_tool_,
                                    redaction_tool_container_);
  ASSERT_EQ(collect_future.Get(), std::nullopt);
  EXPECT_THAT(collector.GetDetectedPII(), IsEmpty());

  base::test::TestFuture<std::optional<SupportToolError>> export_future;
  base::FilePath output_dir = temp_dir_.GetPath().AppendASCII("exported_logs");
  collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, export_future.GetCallback());
  ASSERT_EQ(export_future.Get(), std::nullopt);

  VerifyExportedData(output_dir, updater::UpdaterScope::kUser,
                     /*expect_files=*/false);
  VerifyExportedData(output_dir, updater::UpdaterScope::kSystem,
                     /*expect_files=*/false);
}

TEST_F(UpdaterDataCollectorTest, MissingDirectories) {
  base::FilePath user_dir = temp_dir_.GetPath().AppendASCII("NonExistentUser");
  base::FilePath system_dir =
      temp_dir_.GetPath().AppendASCII("NonExistentSystem");

  UpdaterDataCollector collector(user_dir, system_dir);

  base::test::TestFuture<std::optional<SupportToolError>> collect_future;
  collector.CollectDataAndDetectPII(collect_future.GetCallback(),
                                    task_runner_for_redaction_tool_,
                                    redaction_tool_container_);
  ASSERT_EQ(collect_future.Get(), std::nullopt);
  EXPECT_THAT(collector.GetDetectedPII(), IsEmpty());

  base::test::TestFuture<std::optional<SupportToolError>> export_future;
  base::FilePath output_dir = temp_dir_.GetPath().AppendASCII("exported_logs");
  collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, export_future.GetCallback());
  ASSERT_EQ(export_future.Get(), std::nullopt);

  VerifyExportedData(output_dir, updater::UpdaterScope::kUser,
                     /*expect_files=*/false);
  VerifyExportedData(output_dir, updater::UpdaterScope::kSystem,
                     /*expect_files=*/false);
}

TEST_F(UpdaterDataCollectorTest, SubsetOfFiles) {
  base::FilePath user_dir = CreateFakeUpdaterInstall(
      updater::UpdaterScope::kUser, /*create_files=*/false);
  // Only write updater.log
  EXPECT_TRUE(
      base::WriteFile(user_dir.AppendASCII("updater.log"), kLogFileContent));

  base::FilePath system_dir = CreateFakeUpdaterInstall(
      updater::UpdaterScope::kSystem, /*create_files=*/false);
  // Only write prefs.json
  EXPECT_TRUE(
      base::WriteFile(system_dir.AppendASCII("prefs.json"), kPrefsFileContent));

  UpdaterDataCollector collector(user_dir, system_dir);

  base::test::TestFuture<std::optional<SupportToolError>> collect_future;
  collector.CollectDataAndDetectPII(collect_future.GetCallback(),
                                    task_runner_for_redaction_tool_,
                                    redaction_tool_container_);
  ASSERT_EQ(collect_future.Get(), std::nullopt);

  EXPECT_THAT(collector.GetDetectedPII(),
              UnorderedElementsAre(Key(redaction::PIIType::kIPAddress),
                                   Key(redaction::PIIType::kURL)));

  base::test::TestFuture<std::optional<SupportToolError>> export_future;
  base::FilePath output_dir = temp_dir_.GetPath().AppendASCII("exported_logs");
  collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_dir, task_runner_for_redaction_tool_,
      redaction_tool_container_, export_future.GetCallback());
  ASSERT_EQ(export_future.Get(), std::nullopt);

  // Check user dir
  base::FilePath output_user_dir =
      output_dir.AppendASCII("updater").AppendASCII("user");
  EXPECT_TRUE(base::PathExists(output_user_dir.AppendASCII("updater.log")));
  EXPECT_FALSE(base::PathExists(output_user_dir.AppendASCII("prefs.json")));

  // Check system dir
  base::FilePath output_system_dir =
      output_dir.AppendASCII("updater").AppendASCII("system");
  EXPECT_FALSE(base::PathExists(output_system_dir.AppendASCII("updater.log")));
  EXPECT_TRUE(base::PathExists(output_system_dir.AppendASCII("prefs.json")));
}
