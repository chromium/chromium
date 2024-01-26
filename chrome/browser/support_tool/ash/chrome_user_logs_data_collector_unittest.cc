// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/chrome_user_logs_data_collector.h"

#include <map>
#include <memory>
#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;

namespace {

struct FakeUserLog {
  // The path that the logs will reside under user's profile directory.
  base::FilePath path;
  // Name of the logs.
  std::string log_name;
  std::string log_contents;
  // The version of `log_contents` that are redacted from PII.
  std::string redacted_contents;
};

const char kFakeUserEmail[] = "fakeusername@example.com";
const char kFakeGaiaId[] = "gaia-id";

const FakeUserLog kFakeUserLogs[] = {
    {/*path=*/base::FilePath("log/chrome"), /*log_name=*/"chrome",
     /*log_contents=*/"Fake Chrome logs\nUser is fakeusername@example.com",
     /*redacted_contents=*/"Fake Chrome logs\nUser is (email: 1)"},
    {/*path=*/base::FilePath("log/chrome_00000000-000000"),
     /*log_name=*/"chrome_00000000-000000",
     /*log_contents=*/"Sample logs", /*redacted_contents=*/"Sample logs"},
    {/*path=*/base::FilePath("log/chrome_00000000-111111"),
     /*log_name=*/"chrome_00000000-111111",
     /*log_contents=*/"Sample logs with PII chrome://resources/f?user=bar",
     /*redacted_contents=*/"Sample logs with PII (URL: 1)"}};

const PIIMap kExpectedPIIMap = {
    {redaction::PIIType::kEmail, {"fakeusername@example.com"}},
    {redaction::PIIType::kURL, {"chrome://resources/f?user=bar"}}};

class ChromeUserLogsDataCollectorTest : public ::testing::Test {
 public:
  ChromeUserLogsDataCollectorTest() {
    std::unique_ptr<user_manager::FakeUserManager> fake_user_manager =
        std::make_unique<user_manager::FakeUserManager>();
    AccountId fake_user_account =
        AccountId::FromUserEmailGaiaId(kFakeUserEmail, kFakeGaiaId);
    fake_user_hash_ =
        user_manager::FakeUserManager::GetFakeUsernameHash(fake_user_account);
    // Add the fake user to `fake_user_manager` and make it primary user by
    // making user logged in.
    fake_user_manager->AddUser(fake_user_account);
    fake_user_manager->UserLoggedIn(fake_user_account, fake_user_hash_,
                                    /*browser_restart=*/false, false);

    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    // Set up task runner and container for RedactionTool. We will use when
    // calling CollectDataAndDetectPII() and ExportCollectedDataWithPII()
    // functions on SystemStateDataCollectorTest for testing.
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);
  }

  void SetUp() override {
    // Allow blocking for testing in this scope for temporary directory
    // creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    auto profile_manager_unique = std::make_unique<FakeProfileManager>(
        base::CreateUniqueTempDirectoryScopedToTest());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::move(profile_manager_unique));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  base::FilePath GetTempDirForOutput() { return temp_dir_.GetPath(); }

  // Writes `kFakeUserLogs` to the profile directory that belongs to
  // `fake_user_hash_`.
  void WriteFakeLogFiles() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath fake_user_profile_dir =
        ash::ProfileHelper::Get()->GetProfilePathByUserIdHash(fake_user_hash_);
    // Create the directory where logs should reside.
    ASSERT_TRUE(base::CreateDirectory(
        fake_user_profile_dir.Append(FILE_PATH_LITERAL("log"))));
    for (const FakeUserLog& fake_log : kFakeUserLogs) {
      ASSERT_TRUE(base::WriteFile(fake_user_profile_dir.Append(fake_log.path),
                                  fake_log.log_contents));
    }
  }

  // Traverses the files in `directory` and returns the contents of the files in
  // a map.
  std::map<std::string, std::string> ReadFileContentsToMap(
      base::FilePath directory) {
    base::ScopedAllowBlockingForTesting allow_blocking;
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

  content::BrowserTaskEnvironment task_environment_;
  std::string fake_user_hash_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

}  // namespace

TEST_F(ChromeUserLogsDataCollectorTest, CollectAndExportData) {
  // Initialize ChromeUserLogsDataCollector for testing.
  ChromeUserLogsDataCollector data_collector;

  // Write fake log files to fake user's profile directory for `data_collector`
  // to read.
  WriteFakeLogFiles();

  // Test data collection and PII detection.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  // Check if CollectDataAndDetectPII call returned an error.
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Check the PII map that `data_collector` detected.
  EXPECT_THAT(data_collector.GetDetectedPII(), ContainerEq(kExpectedPIIMap));

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

  // Prepare expected logs.
  std::map<std::string, std::string> expected_logs;
  for (const FakeUserLog& fake_log : kFakeUserLogs) {
    expected_logs.emplace(fake_log.log_name, fake_log.redacted_contents);
  }

  std::map<std::string, std::string> output_file_contents =
      ReadFileContentsToMap(output_dir);
  // Check the exported logs if they're as expected.
  EXPECT_THAT(output_file_contents, ContainerEq(expected_logs));
}
