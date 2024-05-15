// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_file_system_access_permission_context.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace {
enum class MethodName : int {
  kShowOpenFilePicker = 0,
  kShowSaveFilePicker = 1,
  kShowDirectoryPicker = 2,
};

constexpr char kShowOpenFilePickerScript[] = R"(
      (async () => {
        let [e] = await self.showOpenFilePicker();
        return e.name;
      })()
    )";
constexpr char kShowSaveFilePickerScript[] = R"(
      (async () => {
        let e = await self.showSaveFilePicker();
        return e.name;
      })()
    )";
constexpr char kShowDirectoryPickerScript[] = R"(
      (async () => {
        let e = await self.showDirectoryPicker();
        return e.name;
      })()
    )";

constexpr char kUserGestureErrorMessage[] =
    "Must be handling a user gesture to show a file picker.";

constexpr char kTestServerOrigin[] = "http://127.0.0.1";
constexpr char kOtherOrigin[] = "https://other-origin";
}  // namespace

class FileSystemAccessPickerRequiresUserGestureTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<MethodName> {
 public:
  FileSystemAccessPickerRequiresUserGestureTest()
      : api_method_name_(GetParam()) {}
  ~FileSystemAccessPickerRequiresUserGestureTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SetupPolicyProvider();

    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    SetupTestFileOrDirectory();
    SetupEmbeddedTestServer();
    SetupFakePermissionContext();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    ui::SelectFileDialog::SetFactory(nullptr);
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void SetupPolicyProvider() {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

  void SetupEmbeddedTestServer() {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetupFakePermissionContext() {
    // Required to bypass permission prompt for directory picker.
    content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                  &permission_context_);
  }

  void SetupTestFileOrDirectory() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = (api_method_name_ == MethodName::kShowDirectoryPicker)
                     ? CreateTestDir()
                     : CreateTestFile();

    // File/Directory will automatically be selected by picker.
    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{file_path_}));
  }

  base::FilePath CreateTestFile() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &result));
    EXPECT_TRUE(base::WriteFile(result, "test_file"));
    return result;
  }

  base::FilePath CreateTestDir() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath result;
    EXPECT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir_.GetPath(), FILE_PATH_LITERAL("test_dir"), &result));
    return result;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  std::string GetScript() {
    switch (api_method_name_) {
      case MethodName::kShowOpenFilePicker:
        return kShowOpenFilePickerScript;
      case MethodName::kShowSaveFilePicker:
        return kShowSaveFilePickerScript;
      case MethodName::kShowDirectoryPicker:
        return kShowDirectoryPickerScript;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  void SetPolicy(const std::string& allowlisted_origin) {
    policy::PolicyMap policy_map;
    base::Value::List allowed_origins;
    allowed_origins.Append(base::Value(allowlisted_origin));
    policy_map.Set(
        policy::key::kFileOrDirectoryPickerWithoutGestureAllowedForOrigins,
        policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
        policy::POLICY_SOURCE_PLATFORM, base::Value(std::move(allowed_origins)),
        nullptr);
    policy_provider_.UpdateChromePolicy(policy_map);
    base::RunLoop().RunUntilIdle();
  }

  void NavigateToPage() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/title1.html")));
  }

 protected:
  const MethodName api_method_name_;
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  content::FakeFileSystemAccessPermissionContext permission_context_;
};

IN_PROC_BROWSER_TEST_P(FileSystemAccessPickerRequiresUserGestureTest,
                       SuceedsWithGestureWithoutPolicy) {
  NavigateToPage();

  EXPECT_EQ(file_path_.BaseName().AsUTF8Unsafe(),
            EvalJs(GetWebContents(), GetScript()));
}

IN_PROC_BROWSER_TEST_P(FileSystemAccessPickerRequiresUserGestureTest,
                       FailsWithoutGestureOrPolicy) {
  NavigateToPage();

  auto result = EvalJs(GetWebContents(), GetScript(),
                       content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_TRUE(result.error.find(kUserGestureErrorMessage) != std::string::npos)
      << result.error;
}

IN_PROC_BROWSER_TEST_P(FileSystemAccessPickerRequiresUserGestureTest,
                       SuceedsWithPolicyWithoutGesture) {
  SetPolicy(kTestServerOrigin);
  NavigateToPage();

  EXPECT_EQ(file_path_.BaseName().AsUTF8Unsafe(),
            EvalJs(GetWebContents(), GetScript(),
                   content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_P(FileSystemAccessPickerRequiresUserGestureTest,
                       SuceedsWithPolicyAndGesture) {
  SetPolicy(kTestServerOrigin);
  NavigateToPage();

  EXPECT_EQ(file_path_.BaseName().AsUTF8Unsafe(),
            EvalJs(GetWebContents(), GetScript()));
}

IN_PROC_BROWSER_TEST_P(FileSystemAccessPickerRequiresUserGestureTest,
                       FailsWithOtherPolicyWithoutGesture) {
  SetPolicy(kOtherOrigin);
  NavigateToPage();

  auto result = EvalJs(GetWebContents(), GetScript(),
                       content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE);
  EXPECT_TRUE(result.error.find(kUserGestureErrorMessage) != std::string::npos)
      << result.error;
}

INSTANTIATE_TEST_SUITE_P(All,
                         FileSystemAccessPickerRequiresUserGestureTest,
                         testing::Values(MethodName::kShowOpenFilePicker,
                                         MethodName::kShowSaveFilePicker,
                                         MethodName::kShowDirectoryPicker));
