// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/file_system_access_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/file_system_access/chrome_file_system_access_permission_context.h"
#include "chrome/browser/file_system_access/file_system_access_permission_context_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockFileSystemAccessPermissionContext
    : public ChromeFileSystemAccessPermissionContext {
 public:
  explicit MockFileSystemAccessPermissionContext(
      content::BrowserContext* context)
      : ChromeFileSystemAccessPermissionContext(context), num_calls_(0) {}
  ~MockFileSystemAccessPermissionContext() override = default;
  MockFileSystemAccessPermissionContext(
      const MockFileSystemAccessPermissionContext&) = delete;
  MockFileSystemAccessPermissionContext& operator=(
      const MockFileSystemAccessPermissionContext&) = delete;

  size_t num_calls() { return num_calls_; }
  // ChromeFileSystemAccessPermissionContext:
  void NavigatedAwayFromOrigin(const url::Origin& origin) override {
    num_calls_++;
  }

 private:
  size_t num_calls_;
};

}  // anonymous namespace

class FileSystemAccessTabHelperPrerenderingBrowserTest
    : public InProcessBrowserTest {
 public:
  FileSystemAccessTabHelperPrerenderingBrowserTest()
      : prerender_test_helper_(base::BindRepeating(
            &FileSystemAccessTabHelperPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~FileSystemAccessTabHelperPrerenderingBrowserTest() override = default;
  FileSystemAccessTabHelperPrerenderingBrowserTest(
      const FileSystemAccessTabHelperPrerenderingBrowserTest&) = delete;
  FileSystemAccessTabHelperPrerenderingBrowserTest& operator=(
      const FileSystemAccessTabHelperPrerenderingBrowserTest&) = delete;

  void SetUp() override {
    prerender_test_helper_.RegisterServerRequestMonitor(embedded_test_server());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Clear the permission context since setting the testing factory will
    // destroy the current context outside of the normal shutdown sequence.
    content::SetFileSystemAccessPermissionContext(browser()->profile(),
                                                  nullptr);
    FileSystemAccessPermissionContextFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            browser()->profile(),
            base::BindRepeating(
                &FileSystemAccessTabHelperPrerenderingBrowserTest::
                    BuildMockFileSystemAccessPermissionContext,
                base::Unretained(this)));

    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_test_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  size_t service_num_calls() { return mock_service_->num_calls(); }

 private:
  std::unique_ptr<KeyedService> BuildMockFileSystemAccessPermissionContext(
      content::BrowserContext* context) {
    std::unique_ptr<MockFileSystemAccessPermissionContext> service =
        std::make_unique<MockFileSystemAccessPermissionContext>(
            browser()->profile());
    mock_service_ = service.get();
    return std::move(service);
  }

  raw_ptr<MockFileSystemAccessPermissionContext, DanglingUntriaged>
      mock_service_;
  content::test::PrerenderTestHelper prerender_test_helper_;
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessTabHelperPrerenderingBrowserTest,
                       NavigatedAwayFromOrigin) {
  // Initial navigation
  GURL initial_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);
  EXPECT_EQ(1U, service_num_calls());

  // Should be checking that prerendering to a site with a different
  // origin does not trigger a call to NavigatedAwayFromOrigin() on the
  // FileSystemAccessPermissionContext service. However, prerender is restricted
  // to same origin navigations. Revisit when cross-origin prerender is
  // supported.
  GURL prerender_url =
      embedded_test_server()->GetURL("example.com", "/title1.html");
  prerender_helper().AddPrerender(prerender_url);
  EXPECT_EQ(1U, service_num_calls());

  // Activate prerendered page
  prerender_helper().NavigatePrimaryPage(prerender_url);
  // This should be 2 if we were activating a cross-site prerendered page.
  EXPECT_EQ(1U, service_num_calls());

  // Cross-navigation should trigger a call to NavigatedAwayFromOrigin() on the
  // FileSystemAccessPermissionContext service.
  GURL url = embedded_test_server()->GetURL("example2.com", "/title2.html");
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), url), nullptr);
  EXPECT_EQ(2U, service_num_calls());
}
