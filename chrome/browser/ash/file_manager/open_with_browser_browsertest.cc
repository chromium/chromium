// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/open_with_browser.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/common/features.h"

namespace file_manager::util {

namespace {

// Returns full test file path to the given |file_name|.
base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

}  // namespace

using base::test::RunClosure;
using testing::_;

// Profile type to test. Provided to OpenWithBrowserBrowserTest via
// profile_type().
enum class TestProfileType {
  kRegular,
  kIncognito,
  kGuest,
};

struct TestCase {
  explicit TestCase(const TestProfileType profile_type)
      : profile_type(profile_type) {}

  // Show the startup browser. Navigating to a URL should work whether a browser
  // window is already opened or not. Provided to OpenWithBrowserBrowserTest via
  // startup_browser().
  TestCase& WithStartupBrowser() {
    startup_browser = true;
    return *this;
  }

  TestProfileType profile_type;
  bool startup_browser = false;
};

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  std::string result;
  switch (test.param.profile_type) {
    case TestProfileType::kRegular:
      result = "Regular";
      break;
    case TestProfileType::kIncognito:
      result = "Incognito";
      break;
    case TestProfileType::kGuest:
      result = "Guest";
      break;
  }

  if (test.param.startup_browser) {
    result += "_WithStartupBrowser";
  }

  return result;
}

class OpenWithBrowserBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<TestCase> {
 public:
  OpenWithBrowserBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (profile_type() == TestProfileType::kGuest) {
      ConfigureCommandLineForGuestMode(command_line);
    } else if (profile_type() == TestProfileType::kIncognito) {
      command_line->AppendSwitch(::switches::kIncognito);
    }
    if (!startup_browser()) {
      command_line->AppendSwitch(::switches::kNoStartupWindow);
    }
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  TestProfileType profile_type() const { return GetParam().profile_type; }

  bool startup_browser() const { return GetParam().startup_browser; }

  Profile* profile() const {
    if (browser()) {
      return browser()->profile();
    }
    return ProfileManager::GetActiveUserProfile();
  }

 protected:
  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeExternal, path);
  }

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://file-manager");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(OpenWithBrowserBrowserTest, OpenTextFile) {
  // For a given txt file, generate its FileSystemURL.
  const base::FilePath test_file_path = GetTestFilePath("text.txt");
  storage::FileSystemURL test_file_url = PathToFileSystemURL(test_file_path);

  // file: URL of the test file, as opened in the browser.
  GURL page_url = net::FilePathToFileURL(test_file_url.path());
  content::TestNavigationObserver navigation_observer(page_url);
  navigation_observer.StartWatchingNewWebContents();
  OpenFileWithAppOrBrowser(profile(), test_file_url, "view-in-browser");
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OpenWithBrowserBrowserTest,
    ::testing::Values(
        TestCase(TestProfileType::kRegular),
        TestCase(TestProfileType::kRegular).WithStartupBrowser(),
        TestCase(TestProfileType::kIncognito),
        TestCase(TestProfileType::kIncognito).WithStartupBrowser(),
        TestCase(TestProfileType::kGuest),
        TestCase(TestProfileType::kGuest).WithStartupBrowser()),
    &PostTestCaseName);

struct HostedAppTestCase {
  const std::string name;
  const std::string app_id;
  const std::string file_name;
};

std::string AppendTestCaseName(
    const ::testing::TestParamInfo<HostedAppTestCase>& test) {
  return test.param.name;
}

class OpenHostedFileWithAppBrowserBaseTest : public InProcessBrowserTest {
 public:
  OpenHostedFileWithAppBrowserBaseTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kDesktopPWAsTabStrip},
        {features::kDesktopPWAsTabStripSettings});
  }
  ~OpenHostedFileWithAppBrowserBaseTest() override = default;

 protected:
  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeExternal, path);
  }

  Profile* profile() const {
    if (browser()) {
      return browser()->profile();
    }
    return ProfileManager::GetActiveUserProfile();
  }

  void SetUpAppInAppService(const std::string& app_id,
                            apps::Readiness readiness) {
    std::vector<apps::AppPtr> apps;
    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kWeb, app_id);
    app->app_id = app_id;
    app->readiness = readiness;
    apps.push_back(std::move(app));
    apps::AppServiceProxyFactory::GetForProfile(profile())->OnApps(
        std::move(apps), apps::AppType::kWeb,
        false /* should_notify_initialized */);
  }

  const storage::FileSystemURL CreateHostedFile(const std::string& file_name) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath test_file_path = temp_dir_.GetPath().Append(file_name);
    EXPECT_TRUE(base::WriteFile(
        test_file_path,
        base::StrCat({"{\"url\":\"", kTestHostedURL.spec(), "\"}"})));
    return PathToFileSystemURL(test_file_path);
  }

  void OpenURLAndExpectAppToBeOpened(
      const storage::FileSystemURL& test_file_url) {
    base::RunLoop run_loop;
    base::MockCallback<LaunchAppCallback> mock_callback;
    EXPECT_CALL(mock_callback, Run(_))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    OpenFileWithAppOrBrowser(profile(), test_file_url, "view-in-browser",
                             mock_callback.Get());
    run_loop.Run();
  }

  void OpenURLAndExpectBrowserToBeOpened(
      const storage::FileSystemURL& test_file_url) {
    content::TestNavigationObserver navigation_observer(kTestHostedURL);
    navigation_observer.StartWatchingNewWebContents();
    OpenFileWithAppOrBrowser(profile(), test_file_url, "view-in-browser");
    navigation_observer.Wait();
    EXPECT_EQ(navigation_observer.last_navigation_url(), kTestHostedURL);
  }

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://file-manager");
  base::ScopedTempDir temp_dir_;
  const GURL kTestHostedURL = GURL("https://docs.google.com/test-id");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class OpenHostedFileWithAppBrowserTest
    : public OpenHostedFileWithAppBrowserBaseTest,
      public ::testing::WithParamInterface<HostedAppTestCase> {};

IN_PROC_BROWSER_TEST_P(OpenHostedFileWithAppBrowserTest,
                       AppIsAvailableAndReady) {
  const HostedAppTestCase& test_case = GetParam();
  const storage::FileSystemURL test_file_url =
      CreateHostedFile(test_case.file_name);
  SetUpAppInAppService(test_case.app_id, apps::Readiness::kReady);
  OpenURLAndExpectAppToBeOpened(test_file_url);
}

IN_PROC_BROWSER_TEST_P(OpenHostedFileWithAppBrowserTest, AppIsNotInstalled) {
  const HostedAppTestCase& test_case = GetParam();
  const storage::FileSystemURL test_file_url =
      CreateHostedFile(test_case.file_name);
  OpenURLAndExpectBrowserToBeOpened(test_file_url);
}

IN_PROC_BROWSER_TEST_P(OpenHostedFileWithAppBrowserTest,
                       AppIsUninstalledByUser) {
  const HostedAppTestCase& test_case = GetParam();
  const storage::FileSystemURL test_file_url =
      CreateHostedFile(test_case.file_name);
  SetUpAppInAppService(test_case.app_id, apps::Readiness::kUninstalledByUser);
  OpenURLAndExpectBrowserToBeOpened(test_file_url);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OpenHostedFileWithAppBrowserTest,
    ::testing::Values(HostedAppTestCase{.name = "Docs",
                                        .app_id = web_app::kGoogleDocsAppId,
                                        .file_name = "doc.gdoc"},
                      HostedAppTestCase{.name = "Sheets",
                                        .app_id = web_app::kGoogleSheetsAppId,
                                        .file_name = "sheet.gsheet"},
                      HostedAppTestCase{.name = "Slides",
                                        .app_id = web_app::kGoogleSlidesAppId,
                                        .file_name = "slide.gslides"}),
    &AppendTestCaseName);

using OpenHostedFileWithoutAppBrowserTest =
    OpenHostedFileWithAppBrowserBaseTest;

IN_PROC_BROWSER_TEST_F(OpenHostedFileWithoutAppBrowserTest,
                       HostedDocWithoutApp) {
  const storage::FileSystemURL test_file_url = CreateHostedFile("form.gform");
  OpenURLAndExpectBrowserToBeOpened(test_file_url);
}

}  // namespace file_manager::util
