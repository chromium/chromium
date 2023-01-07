// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "net/base/network_delegate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)

class ChromeNetworkDelegateBrowserTest : public InProcessBrowserTest {
 public:
  ChromeNetworkDelegateBrowserTest(const ChromeNetworkDelegateBrowserTest&) =
      delete;
  ChromeNetworkDelegateBrowserTest& operator=(
      const ChromeNetworkDelegateBrowserTest&) = delete;

 protected:
  ChromeNetworkDelegateBrowserTest() {}

  void SetUpInProcessBrowserTestFixture() override {
    // Access to all files via file: scheme is allowed on browser
    // tests. Bring back the production behaviors.
    ChromeNetworkDelegate::EnableAccessToAllFilesForTesting(false);
  }

  void SetUpOnMainThread() override {
    base::FilePath temp_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDirUnderPath(temp_dir));
  }

  base::ScopedTempDir scoped_temp_dir_;
};

// Ensure that access to a test file, that is not in an accessible location,
// via file: scheme is rejected with ERR_ACCESS_DENIED.
IN_PROC_BROWSER_TEST_F(ChromeNetworkDelegateBrowserTest, AccessToFile) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath test_file = test_dir.AppendASCII("empty.html");
  ASSERT_FALSE(
      ChromeNetworkDelegate::IsAccessAllowed(test_file, base::FilePath()));

  GURL url = net::FilePathToFileURL(test_file);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(net::ERR_ACCESS_DENIED, observer.last_net_error_code());
}

// Ensure that access to a symbolic link, that is in an accessible location,
// to a test file, that isn't, via file: scheme is rejected with
// ERR_ACCESS_DENIED.
IN_PROC_BROWSER_TEST_F(ChromeNetworkDelegateBrowserTest, AccessToSymlink) {
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath test_file = test_dir.AppendASCII("empty.html");
  ASSERT_FALSE(
      ChromeNetworkDelegate::IsAccessAllowed(test_file, base::FilePath()));

  base::FilePath temp_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &temp_dir));
  base::FilePath symlink = scoped_temp_dir_.GetPath().AppendASCII("symlink");
  ASSERT_TRUE(base::CreateSymbolicLink(test_file, symlink));
  ASSERT_TRUE(
      ChromeNetworkDelegate::IsAccessAllowed(symlink, base::FilePath()));

  GURL url = net::FilePathToFileURL(symlink);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(web_contents);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ(net::ERR_ACCESS_DENIED, observer.last_net_error_code());
}

#endif  // BUILDFLAG(IS_CHROMEOS)
