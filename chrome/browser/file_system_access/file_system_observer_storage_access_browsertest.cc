// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/file_system_chooser_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace {

#define GET_FILE                                    \
  "const [file] = await self.showOpenFilePicker();" \
  "self.entry = file;"

#define TRY_CATCH_OBSERVE_FILE                         \
  "async function onChange(records, observer) {"       \
  "  numRecords += records.length;"                    \
  "};"                                                 \
  "const observer = new FileSystemObserver(onChange);" \
  "try {"                                              \
  "  await observer.observe(self.entry);"              \
  "} catch (e) {"                                      \
  "  return e.toString();"                             \
  "}"                                                  \
  "return 'success';"

constexpr char kSuccessMessage[] = "success";
constexpr char kSecurityErrorMessage[] =
    "SecurityError: Storage directory access is denied.";

}  // namespace

class FileSystemObserverStorageAccessTest : public InProcessBrowserTest {
 public:
  FileSystemObserverStorageAccessTest() = default;
  ~FileSystemObserverStorageAccessTest() override = default;

  FileSystemObserverStorageAccessTest(
      const FileSystemObserverStorageAccessTest&) = delete;
  FileSystemObserverStorageAccessTest& operator=(
      const FileSystemObserverStorageAccessTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(temp_dir_.Set(base::MakeLongFilePath(temp_dir_.Take())));
#endif  // BUILDFLAG(IS_WIN)
    ASSERT_TRUE(embedded_test_server()->Start());
    test_url_ = embedded_test_server()->GetURL("/title1.html");

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
    ui::SelectFileDialog::SetFactory(nullptr);
    InProcessBrowserTest::TearDown();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::FilePath CreateFileToBePicked() {
    base::FilePath file_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(
          base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &file_path));
      EXPECT_TRUE(base::WriteFile(file_path, "observe me"));
    }

    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{file_path}));
    EXPECT_TRUE(NavigateToURL(GetWebContents(), test_url_));
    return file_path;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable experimental web platform features to enable read/write access.
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "FileSystemObserver");
  }

  void ConfigureCookieSetting(const GURL& url, ContentSetting setting) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetCookieSetting(url, setting);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;
};

IN_PROC_BROWSER_TEST_F(FileSystemObserverStorageAccessTest,
                       StorageAccessAllowed) {
  CreateFileToBePicked();
  ConfigureCookieSetting(test_url_, CONTENT_SETTING_ALLOW);

  // Start observing the file.
  std::string script =
      // clang-format off
      "(async () => {"
         GET_FILE
         TRY_CATCH_OBSERVE_FILE
      "})()";
  // clang-format on
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemObserverStorageAccessTest,
                       StorageAccessBlocked) {
  CreateFileToBePicked();
  ConfigureCookieSetting(test_url_, CONTENT_SETTING_ALLOW);

  // Pick a file to observe.
  std::string script =
      // clang-format off
      "(async () => {"
         GET_FILE
      "})()";
  // clang-format on
  EXPECT_TRUE(ExecJs(GetWebContents(), script));

  ConfigureCookieSetting(test_url_, CONTENT_SETTING_BLOCK);
  // Attempt to observe the file. This should fail as the storage access is
  // blocked.
  script =
      // clang-format off
      "(async () => {"
        TRY_CATCH_OBSERVE_FILE
      "})()";
  // clang-format on
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemObserverStorageAccessTest,
                       StateChangeFromAllowedToBlocked) {
  CreateFileToBePicked();
  ConfigureCookieSetting(test_url_, CONTENT_SETTING_ALLOW);

  // Start observing the file.
  std::string script =
      // clang-format off
      "(async () => {"
        GET_FILE
        TRY_CATCH_OBSERVE_FILE
      "})()";
  // clang-format on
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);

  ConfigureCookieSetting(test_url_, CONTENT_SETTING_BLOCK);

  // The cached value will be used. So, the new state will be ignored.
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemObserverStorageAccessTest,
                       StorageAccessChangeFromBlockedToAllowed) {
  CreateFileToBePicked();
  ConfigureCookieSetting(test_url_, CONTENT_SETTING_ALLOW);

  // Pick a file to observe.
  std::string script =
      // clang-format off
      "(async () => {"
         GET_FILE
      "})()";
  // clang-format on
  EXPECT_TRUE(ExecJs(GetWebContents(), script));

  ConfigureCookieSetting(test_url_, CONTENT_SETTING_BLOCK);
  // Attempt to observe the file. This should fail as the storage access is
  // blocked.
  script =
      // clang-format off
      "(async () => {"
        TRY_CATCH_OBSERVE_FILE
      "})()";
  // clang-format on
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);

  ConfigureCookieSetting(test_url_, CONTENT_SETTING_ALLOW);

  // The cached value will be used. So, the new state will be ignored.
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);
}
