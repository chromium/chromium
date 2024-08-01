// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

struct EncodingTestData {
  const char* file_name;
  const char* encoding_name;
};

const EncodingTestData kEncodingTestDatas[] = {
  { "Big5.html", "Big5" },
  { "EUC-JP.html", "EUC-JP" },
  { "gb18030.html", "gb18030" },
  { "iso-8859-1.html", "windows-1252" },
  { "ISO-8859-2.html", "ISO-8859-2" },
  { "ISO-8859-4.html", "ISO-8859-4" },
  { "ISO-8859-5.html", "ISO-8859-5" },
  { "ISO-8859-6.html", "ISO-8859-6" },
  { "ISO-8859-7.html", "ISO-8859-7" },
  { "ISO-8859-8.html", "ISO-8859-8" },
  { "ISO-8859-13.html", "ISO-8859-13" },
  { "ISO-8859-15.html", "ISO-8859-15" },
  { "KOI8-R.html", "KOI8-R" },
  { "KOI8-U.html", "KOI8-U" },
  { "macintosh.html", "macintosh" },
  { "Shift-JIS.html", "Shift_JIS" },
  { "US-ASCII.html", "windows-1252" },  // http://crbug.com/15801
  { "UTF-8.html", "UTF-8" },
  { "UTF-16LE.html", "UTF-16LE" },
  { "windows-874.html", "windows-874" },
  { "EUC-KR.html", "EUC-KR" },
  { "windows-1250.html", "windows-1250" },
  { "windows-1251.html", "windows-1251" },
  { "windows-1252.html", "windows-1252" },
  { "windows-1253.html", "windows-1253" },
  { "windows-1254.html", "windows-1254" },
  { "windows-1255.html", "windows-1255" },
  { "windows-1256.html", "windows-1256" },
  { "windows-1257.html", "windows-1257" },
  { "windows-1258.html", "windows-1258" }
};

}  // namespace

static const base::FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("encoding_tests");

class BrowserEncodingTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<EncodingTestData> {
 protected:
  BrowserEncodingTest() {}

  // Saves the current page and verifies that the output matches the expected
  // result.
  void SaveAndCompare(const char* filename_to_write,
                      const base::FilePath& expected,
                      const GURL& url) {
    // Dump the page, the content of dump page should be identical to the
    // expected result file.
    base::FilePath full_file_name = save_dir_.AppendASCII(filename_to_write);
    // We save the page as way of complete HTML file, which requires a directory
    // name to save sub resources in it. Although this test file does not have
    // sub resources, but the directory name is still required.
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    content::SavePackageFinishedObserver observer(
        browser()->profile()->GetDownloadManager(), loop_runner->QuitClosure());
    browser()->tab_strip_model()->GetActiveWebContents()->SavePage(
        full_file_name, temp_sub_resource_dir_,
        content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);
    loop_runner->Run();

    base::FilePath expected_file_name = ui_test_utils::GetTestFilePath(
        base::FilePath(kTestDir), expected);

    std::string actual_contents;
    std::string expected_contents;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::ReadFileToString(full_file_name, &actual_contents));
      ASSERT_TRUE(
          base::ReadFileToString(expected_file_name, &expected_contents));
    }

    // Add "Mark of the Web" path with source URL.
    expected_contents = base::StringPrintfNonConstexpr(
        expected_contents.c_str(), url.spec().length(), url.spec().c_str());

    EXPECT_EQ(expected_contents, actual_contents);
  }

  void SetUpOnMainThread() override {
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    save_dir_ = temp_dir_.GetPath();
    temp_sub_resource_dir_ = save_dir_.AppendASCII("sub_resource_files");
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath save_dir_;
  base::FilePath temp_sub_resource_dir_;
};

// TODO(jnd): 1. Some encodings are missing here. It'll be added later. See
// http://crbug.com/13306.
// 2. Add more files with multiple encoding name variants for each canonical
// encoding name). Webkit layout tests cover some, but testing in the UI test is
// also necessary.
IN_PROC_BROWSER_TEST_P(BrowserEncodingTest, TestEncodingAliasMapping) {
  const char* const kAliasTestDir = "alias_mapping";

  base::FilePath test_dir_path = base::FilePath(kTestDir).AppendASCII(
      kAliasTestDir);
  base::FilePath test_file_path(test_dir_path);
  test_file_path = test_file_path.AppendASCII(
      GetParam().file_name);

  GURL url =
      embedded_test_server()->GetURL("/" + test_file_path.MaybeAsASCII());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(GetParam().encoding_name,
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetEncoding());
}

INSTANTIATE_TEST_SUITE_P(EncodingAliases,
                         BrowserEncodingTest,
                         testing::ValuesIn(kEncodingTestDatas));

// The following encodings are excluded from the auto-detection test because
// it's a known issue that the current encoding detector does not detect them:
// ISO-8859-4
// ISO-8859-13
// KOI8-U
// macintosh
// windows-874
// windows-1252
// windows-1253
// windows-1257
// windows-1258

IN_PROC_BROWSER_TEST_F(BrowserEncodingTest, TestEncodingAutoDetect) {
  struct EncodingAutoDetectTestData {
    const char* test_file_name;   // File name of test data.
    const char* expected_result;  // File name of expected results.
    const char* expected_encoding;   // expected encoding.
  };
  const EncodingAutoDetectTestData kTestDatas[] = {
      { "Big5_with_no_encoding_specified.html",
        "expected_Big5_saved_from_no_encoding_specified.html",
        "Big5" },
      { "GBK_with_no_encoding_specified.html",
        "expected_GBK_saved_from_no_encoding_specified.html",
        "GBK" },
      { "iso-8859-1_with_no_encoding_specified.html",
        "expected_iso-8859-1_saved_from_no_encoding_specified.html",
        "windows-1252" },
      { "ISO-8859-5_with_no_encoding_specified.html",
        "expected_ISO-8859-5_saved_from_no_encoding_specified.html",
        "ISO-8859-5" },
      { "ISO-8859-6_with_no_encoding_specified.html",
        "expected_ISO-8859-6_saved_from_no_encoding_specified.html",
        "ISO-8859-6" },
      { "ISO-8859-7_with_no_encoding_specified.html",
        "expected_ISO-8859-7_saved_from_no_encoding_specified.html",
        "ISO-8859-7" },
      { "ISO-8859-8-I_with_no_encoding_specified.html",
        "expected_ISO-8859-8-I_saved_from_no_encoding_specified.html",
        "windows-1255" },
      { "KOI8-R_with_no_encoding_specified.html",
        "expected_KOI8-R_saved_from_no_encoding_specified.html",
        "KOI8-R" },
      { "Shift-JIS_with_no_encoding_specified.html",
        "expected_Shift-JIS_saved_from_no_encoding_specified.html",
        "Shift_JIS" },
      { "EUC-KR_with_no_encoding_specified.html",
        "expected_EUC-KR_saved_from_no_encoding_specified.html",
        "EUC-KR" },
      { "windows-1251_with_no_encoding_specified.html",
        "expected_windows-1251_saved_from_no_encoding_specified.html",
        "windows-1251" },
      { "windows-1254_with_no_encoding_specified.html",
        "expected_windows-1254_saved_from_no_encoding_specified.html",
        "windows-1254" },
      { "windows-1255_with_no_encoding_specified.html",
        "expected_windows-1255_saved_from_no_encoding_specified.html",
        "windows-1255" },
      { "windows-1256_with_no_encoding_specified.html",
        "expected_windows-1256_saved_from_no_encoding_specified.html",
        "windows-1256" }
    };
  const char* const kAutoDetectDir = "auto_detect";
  // Directory of the files of expected results.
  const char* const kExpectedResultDir = "expected_results";

  base::FilePath test_dir_path =
      base::FilePath(kTestDir).AppendASCII(kAutoDetectDir);

  // Set the default charset to one of encodings not supported by the current
  // auto-detector (Please refer to the above comments) to make sure we
  // incorrectly decode the page. Now we use ISO-8859-4.
  browser()->profile()->GetPrefs()->SetString(prefs::kDefaultCharset,
                                              "ISO-8859-4");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  for (size_t i = 0; i < std::size(kTestDatas); ++i) {
    SCOPED_TRACE(i);
    base::FilePath test_file_path(test_dir_path);
    test_file_path = test_file_path.AppendASCII(kTestDatas[i].test_file_name);
    GURL url =
        embedded_test_server()->GetURL("/" + test_file_path.MaybeAsASCII());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Get the encoding of page. It should return the real encoding now.
    EXPECT_EQ(kTestDatas[i].expected_encoding, web_contents->GetEncoding());

    // Dump the page, the content of dump page should be equal with our expect
    // result file.
    base::FilePath expected_result_file_name =
        base::FilePath().AppendASCII(kAutoDetectDir).
        AppendASCII(kExpectedResultDir).
        AppendASCII(kTestDatas[i].expected_result);
    SaveAndCompare(kTestDatas[i].test_file_name, expected_result_file_name,
                   url);
  }
}
