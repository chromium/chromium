// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/signin_data_collector.h"

#include <cstdio>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/support_tool/data_collector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const std::set<redaction::PIIType> kExpectedPIITypes = {
    redaction::PIIType::kEmail, redaction::PIIType::kURL,
    redaction::PIIType::kGaiaID};

void ReadExportedFile(base::Value::Dict* signin, base::FilePath file_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_contents));
  std::optional<base::Value> dict_value = base::JSONReader::Read(file_contents);
  ASSERT_TRUE(dict_value);
  *signin = std::move(dict_value->GetDict());
}

std::set<redaction::PIIType> GetPIITypes(const PIIMap& pii_map) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::set<redaction::PIIType> pii_types;
  for (const auto& pii_map_entry : pii_map) {
    pii_types.insert(pii_map_entry.first);
  }
  return pii_types;
}

class SigninDataCollectorBrowserTestAsh
    : public MixinBasedInProcessBrowserTest {
 public:
  SigninDataCollectorBrowserTestAsh() = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    task_runner_for_redaction_tool_ =
        base::ThreadPool::CreateSequencedTaskRunner({});
    redaction_tool_container_ =
        base::MakeRefCounted<redaction::RedactionToolContainer>(
            task_runner_for_redaction_tool_, nullptr);

    logged_in_user_mixin_.LogInUser();
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    // Allow blocking for temporary directory creation.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void TearDownInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};

  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_for_redaction_tool_;
  scoped_refptr<redaction::RedactionToolContainer> redaction_tool_container_;
};

}  // namespace

// We test the status in detail for only Ash in
// SigninDataCollectorBrowserTestAsh.CollectPolicyStatus because the Mixins
// for logged-in user only exists for Ash.
IN_PROC_BROWSER_TEST_F(SigninDataCollectorBrowserTestAsh, CollectSigninStatus) {
  // SigninDataCollector for testing.
  SigninDataCollector data_collector(ProfileManager::GetActiveUserProfile());

  // Collect policies and assert no error returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  std::optional<SupportToolError> error = test_future_collect_data.Get();

  EXPECT_EQ(error, std::nullopt);

  // Check the returned map of detected PII inside the collected data to see if
  // it contains the PII types we expect.
  PIIMap pii_map = data_collector.GetDetectedPII();
  EXPECT_EQ(GetPIITypes(pii_map), kExpectedPIITypes);

  // Export the collected data and make sure no error is returned.
  base::FilePath output_path = temp_dir_.GetPath();
  auto output_file = output_path.Append(FILE_PATH_LITERAL("signin.json"));

  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/task_runner_for_redaction_tool_,
      /*redaction_tool_container=*/redaction_tool_container_,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  EXPECT_EQ(error, std::nullopt);

  // Review the file contents.
  base::Value::Dict json_result;
  ASSERT_NO_FATAL_FAILURE(ReadExportedFile(&json_result, output_file));
  EXPECT_FALSE(json_result.empty());
}

IN_PROC_BROWSER_TEST_F(SigninDataCollectorBrowserTestAsh, FailInIncognitoMode) {
  // Create incognito browser for testing.
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      true));

  // `SigninDataCollector` for testing.
  SigninDataCollector data_collector(incognito_browser->profile());

  // Attempt to collect sign-in data and verify that an error is returned.
  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_collect_data;
  data_collector.CollectDataAndDetectPII(test_future_collect_data.GetCallback(),
                                         task_runner_for_redaction_tool_,
                                         redaction_tool_container_);
  std::optional<SupportToolError> error = test_future_collect_data.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(
      error->error_message,
      "SigninDataCollector can't work without profile or in incognito mode.");
  EXPECT_EQ(error->error_code, SupportToolErrorCode::kDataCollectorError);

  // Attempt to export the collected data and verify that error is returned.
  base::FilePath output_path = temp_dir_.GetPath();
  auto output_file = output_path.Append(FILE_PATH_LITERAL("signin.json"));

  base::test::TestFuture<std::optional<SupportToolError>>
      test_future_export_data;
  data_collector.ExportCollectedDataWithPII(
      /*pii_types_to_keep=*/{}, output_path,
      /*task_runner_for_redaction_tool=*/task_runner_for_redaction_tool_,
      /*redaction_tool_container=*/redaction_tool_container_,
      test_future_export_data.GetCallback());
  error = test_future_export_data.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ(error->error_message,
            "SigninDataCollector: Status is empty. Can't export empty status.");
  EXPECT_EQ(error->error_code, SupportToolErrorCode::kDataCollectorError);
}
