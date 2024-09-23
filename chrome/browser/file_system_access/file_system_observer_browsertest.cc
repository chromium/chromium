// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/permission_util.h"
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

class TestFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit TestFileSystemAccessPermissionContext(
      content::BrowserContext* context)
      : ChromeFileSystemAccessPermissionContext(context) {}
  ~TestFileSystemAccessPermissionContext() override = default;

 private:
  base::WeakPtrFactory<TestFileSystemAccessPermissionContext> weak_factory_{
      this};
};

class FileSystemObserverTest : public InProcessBrowserTest {
 public:
  FileSystemObserverTest() = default;
  ~FileSystemObserverTest() override = default;

  FileSystemObserverTest(const FileSystemObserverTest&) = delete;
  FileSystemObserverTest& operator=(const FileSystemObserverTest&) = delete;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
#if BUILDFLAG(IS_WIN)
    // Convert path to long format to avoid mixing long and 8.3 formats in test.
    ASSERT_TRUE(temp_dir_.Set(base::MakeLongFilePath(temp_dir_.Take())));
#elif BUILDFLAG(IS_MAC)
    // Temporary files in Mac are created under /var/, which is a symlink that
    // resolves to /private/var/. Set `temp_dir_` directly to the resolved file
    // path, given that the expected FSEvents event paths are reported as
    // resolved paths.
    base::FilePath resolved_path =
        base::MakeAbsoluteFilePath(temp_dir_.GetPath());
    if (!resolved_path.empty()) {
      temp_dir_.Take();
      ASSERT_TRUE(temp_dir_.Set(resolved_path));
    }
#endif
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

  base::FilePath CreateDirectoryToBePicked() {
    base::FilePath dir_path;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      EXPECT_TRUE(base::CreateTemporaryDirInDir(
          temp_dir_.GetPath(), FILE_PATH_LITERAL("test"), &dir_path));
    }

    ui::SelectFileDialog::SetFactory(
        std::make_unique<content::FakeSelectFileDialogFactory>(
            std::vector<base::FilePath>{dir_path}));
    EXPECT_TRUE(NavigateToURL(GetWebContents(), test_url_));
    return dir_path;
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

  bool SupportsReportingModifiedPath() const {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_MAC)
    return true;
#else
    return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_MAC)
  }

  bool SupportsChangeInfo() const { return SupportsReportingModifiedPath(); }

 protected:
  base::ScopedTempDir temp_dir_;
  GURL test_url_;
};

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest, StorageAccessAllowed) {
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

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest, StorageAccessBlocked) {
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

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest,
                       StorageAccessChangeFromAllowedToBlocked) {
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

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest,
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

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest,
                       ErrorsAfterPermissionsAreRevoked) {
  auto file = CreateFileToBePicked();

  auto* browser_profile = browser()->profile();
  TestFileSystemAccessPermissionContext permission_context(browser_profile);
  content::SetFileSystemAccessPermissionContext(browser_profile,
                                                &permission_context);

  std::string setup_script = content::JsReplace(
      R"""((async () => {
        // Constants
        const actionTimeoutMs = $1;

        // Setup observer
        let records = [];
        function onChange(recs) {
          records.push(...recs.map(record => record.type));
        };
        self.observer = new FileSystemObserver(onChange);

        // Observe a file.
        const [file] = await self.showOpenFilePicker();
        await observer.observe(file);

        // Returns a promise that resolves after `actionTimeoutMs` to the list
        // of records observed by the observer.
        self.collectRecords = () => {
          const {promise, resolve} = Promise.withResolvers();
          setTimeout(() => {
            resolve([...records]);
            records = [];
          }, actionTimeoutMs);
          return promise;
        };
      })())""",
      base::Int64ToValue(TestTimeouts::action_timeout().InMilliseconds()));

  std::string get_results_script = "collectRecords()";

  EXPECT_TRUE(ExecJs(GetWebContents(), setup_script));

  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    ASSERT_TRUE(base::WriteFile(file, "content"));
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();
    const std::string expected_change_type =
        SupportsChangeInfo() ? "modified" : "unknown";

    // Expect that we received at least one "modified" event.
    ASSERT_THAT(records.GetList(), testing::Not(testing::IsEmpty()));
    EXPECT_THAT(records.GetList().front().GetString(),
                testing::StrEq(expected_change_type));
  }

  auto origin = url::Origin::Create(test_url_);
  permission_context.RevokeGrants(origin);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();

    // Expect that we received only one "errored" event.
    ASSERT_THAT(records.GetList(), testing::SizeIs(1));
    EXPECT_THAT(records.GetList().front().GetString(),
                testing::StrEq("errored"));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(file, "content v2"));
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();

    // Expect that no more events are received after it's errored.
    ASSERT_THAT(records.GetList(), testing::IsEmpty());
  }
}

IN_PROC_BROWSER_TEST_F(FileSystemObserverTest,
                       ErrorsAfterRevokeAllActiveGrants) {
  auto dir = CreateDirectoryToBePicked();

  auto* browser_profile = browser()->profile();
  TestFileSystemAccessPermissionContext permission_context(browser_profile);
  content::SetFileSystemAccessPermissionContext(browser_profile,
                                                &permission_context);

  FileSystemAccessPermissionRequestManager::FromWebContents(GetWebContents())
      ->set_auto_response_for_test(permissions::PermissionAction::GRANTED);

  std::string setup_script = content::JsReplace(
      R"""((async () => {
        // Constants
        const actionTimeoutMs = $1;

        // Setup observer
        let records = [];
        function onChange(recs) {
          records.push(...recs.map(record => record.type));
        };
        self.observer = new FileSystemObserver(onChange);

        // Observe a directory.
        const dir = await self.showDirectoryPicker();
        await observer.observe(dir, { recursive: false });

        // Returns a promise that resolves after `actionTimeoutMs` to the list
        // of records observed by the observer.
        self.collectRecords = () => {
          const {promise, resolve} = Promise.withResolvers();
          setTimeout(() => {
            resolve([...records]);
            records = [];
          }, actionTimeoutMs);
          return promise;
        };
      })())""",
      base::Int64ToValue(TestTimeouts::action_timeout().InMilliseconds()));

  EXPECT_TRUE(ExecJs(GetWebContents(), setup_script));

  base::FilePath file = dir.Append(FILE_PATH_LITERAL("file.txt"));
  std::string get_results_script = "collectRecords()";
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(file, "content"));
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();
    const std::string expected_change_type =
        SupportsChangeInfo() ? "appeared" : "unknown";

    // We expect to receive an "appeared" event when the file is created.
    ASSERT_THAT(records.GetList(), testing::Not(testing::IsEmpty()));
    EXPECT_THAT(records.GetList().front().GetString(),
                testing::StrEq(expected_change_type));
  }

  permission_context.RevokeAllActiveGrants();

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();

    // Expect that we received an "errored" event due to the active grants being
    // revoked.
    ASSERT_THAT(records.GetList(), testing::SizeIs(1));
    EXPECT_THAT(records.GetList().front().GetString(),
                testing::StrEq("errored"));
  }

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(file, "content v2"));
    auto records = EvalJs(GetWebContents(), get_results_script).ExtractList();

    // Expect that no more events are received after it's errored.
    ASSERT_THAT(records.GetList(), testing::IsEmpty());
  }
}
