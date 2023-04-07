// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/file_manager/open_with_browser.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/profile_test_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"

namespace file_manager {
namespace util {

namespace {

// Returns full test file path to the given |file_name|.
base::FilePath GetTestFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

}  // namespace

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
  OpenWithBrowserBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, /*disabled_features=*/{ash::features::kLacrosPrimary});
  }

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
  OpenFileWithBrowser(profile(), test_file_url, "view-in-browser");
  navigation_observer.Wait();
  ASSERT_TRUE(navigation_observer.last_navigation_succeeded());
}

// Test to check that OpenNewTabForHostedOfficeFile() doesn't crash when passed
// an invalid URL.
IN_PROC_BROWSER_TEST_P(OpenWithBrowserBrowserTest,
                       InvalidUrlDoesNotCauseCrash) {
  // Create an empty, invalid URL.
  GURL invalid_url = GURL();
  ASSERT_FALSE(invalid_url.is_valid());

  OpenNewTabForHostedOfficeFile(invalid_url);
}

// Test to check that OpenNewTabForHostedOfficeFile() correctly adds a query
// parameter to the input office url and attempts to open the resulting url in
// the browser.
IN_PROC_BROWSER_TEST_P(OpenWithBrowserBrowserTest,
                       AddQueryParamToOfficeFileUrl) {
  const std::string& test_url =
      "https://docs.google.com/document/d/testurl/edit";
  GURL page_url = GURL(test_url);
  GURL page_url_with_query_param = GURL(test_url + "?cros_files=true");

  content::TestNavigationObserver navigation_observer(
      page_url_with_query_param);

  // Start watching for the opening of `page_url_with_query_param`
  navigation_observer.StartWatchingNewWebContents();
  OpenNewTabForHostedOfficeFile(page_url);
  navigation_observer.Wait();
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

}  // namespace util
}  // namespace file_manager
