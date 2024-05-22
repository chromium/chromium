// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kInitialUrl[] = "/run_async_code_on_worker.html";
constexpr char kSuccessMessage[] = "success";
constexpr char kSecurityErrorMessage[] =
    "SecurityError: Storage directory access is denied.";

}  // namespace

class FileSystemFileHandleBrowserTest : public InProcessBrowserTest {
 public:
  FileSystemFileHandleBrowserTest() = default;
  ~FileSystemFileHandleBrowserTest() override = default;

  FileSystemFileHandleBrowserTest(const FileSystemFileHandleBrowserTest&) =
      delete;
  FileSystemFileHandleBrowserTest& operator=(
      const FileSystemFileHandleBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void ConfigureCookieSetting(const GURL& url, ContentSetting setting) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetCookieSetting(url, setting);
  }

  // Initialize a file handle via the File System Access API.
  void InitializeFileHandle() {
    std::string script = R"(runOnWorkerAndWaitForResult(`
            const root = await navigator.storage.getDirectory();
            fileSystemFileHandle =
                await root.getFileHandle('draft.txt', { create: true });
        `);)";
    std::ignore = EvalJs(GetWebContents(), script);
  }
};

IN_PROC_BROWSER_TEST_F(FileSystemFileHandleBrowserTest, StorageAccessBlocked) {
  GURL initial_url = embedded_test_server()->GetURL(kInitialUrl);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  ConfigureCookieSetting(initial_url, CONTENT_SETTING_ALLOW);
  InitializeFileHandle();

  // Block the storage access by blocking the cookie setting. This will prevent
  // `createSyncAccessHandle()` to succeed.
  ConfigureCookieSetting(initial_url, CONTENT_SETTING_BLOCK);

  // Try to create a sync access handle from the file handle
  // `fileSystemFileHandle` created from InitializeFileHandle(). This
  // should fail as the storage access is blocked.
  std::string script = R"(
        runOnWorkerAndWaitForResult(`
            try {
                syncAccessHandle =
                    await fileSystemFileHandle.createSyncAccessHandle();
            } catch(e) {
                return e.toString();
            }
            return "success";
        `);
    )";

  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemFileHandleBrowserTest,
                       StorageAccessBlockedWithMultipleCall) {
  GURL initial_url = embedded_test_server()->GetURL(kInitialUrl);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  ConfigureCookieSetting(initial_url, CONTENT_SETTING_ALLOW);
  InitializeFileHandle();

  // Block the storage access by blocking the cookie setting. This will prevent
  // `createSyncAccessHandle()` to succeed.
  ConfigureCookieSetting(initial_url, CONTENT_SETTING_BLOCK);

  // Try to create a sync access handle from the file handle
  // `fileSystemFileHandle` created from InitializeFileHandle(). This
  // should fail as the storage access is blocked.
  std::string script = R"(
        runOnWorkerAndWaitForResult(`
            try {
                syncAccessHandle =
                    await fileSystemFileHandle.createSyncAccessHandle();
            } catch(e) {
                return e.toString();
            }
            return "success";
        `);
    )";

  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);

  // The content setting is still blocked and the cache will be hit.
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSecurityErrorMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemFileHandleBrowserTest, StorageAccessAllowed) {
  GURL initial_url = embedded_test_server()->GetURL(kInitialUrl);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  ConfigureCookieSetting(initial_url, CONTENT_SETTING_ALLOW);
  InitializeFileHandle();

  // Try to create a sync access handle from the file handle
  // `fileSystemFileHandle` created from InitializeFileHandle(). This
  // should succeed as the storage access is allowed.
  std::string script = R"(
        runOnWorkerAndWaitForResult(`
            try {
                syncAccessHandle =
                    await fileSystemFileHandle.createSyncAccessHandle();
                syncAccessHandle.close();
            } catch(e) {
                return e.toString();
            }
            return "success";
        `);
    )";

  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemFileHandleBrowserTest,
                       StorageAccessAllowedWithMultipleCall) {
  GURL initial_url = embedded_test_server()->GetURL(kInitialUrl);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  ConfigureCookieSetting(initial_url, CONTENT_SETTING_ALLOW);
  InitializeFileHandle();

  // Try to create a sync access handle from the file handle
  // `fileSystemFileHandle` created from InitializeFileHandle(). This
  // should succeed as the storage access is allowed.
  std::string script = R"(
        runOnWorkerAndWaitForResult(`
            try {
                syncAccessHandle =
                    await fileSystemFileHandle.createSyncAccessHandle();
                syncAccessHandle.close();
            } catch(e) {
                return e.toString();
            }
            return "success";
        `);
    )";

  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);

  // The content setting is still allowed and the cache will be hit.
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);
}

IN_PROC_BROWSER_TEST_F(FileSystemFileHandleBrowserTest,
                       StateChangeFromAllowToBlock) {
  GURL initial_url = embedded_test_server()->GetURL(kInitialUrl);
  ASSERT_NE(ui_test_utils::NavigateToURL(browser(), initial_url), nullptr);

  ConfigureCookieSetting(initial_url, CONTENT_SETTING_ALLOW);
  InitializeFileHandle();

  // Try to create a sync access handle from the file handle
  // `fileSystemFileHandle` created from InitializeFileHandle(). This
  // should succeed as the storage access is allowed.
  std::string script = R"(
        runOnWorkerAndWaitForResult(`
            try {
                syncAccessHandle =
                    await fileSystemFileHandle.createSyncAccessHandle();
                syncAccessHandle.close();
            } catch(e) {
                return e.toString();
            }
            return "success";
        `);
    )";

  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);

  // Block the storage access by blocking the cookie setting. This will prevent
  // `createSyncAccessHandle()` to succeed.
  ConfigureCookieSetting(initial_url, CONTENT_SETTING_BLOCK);

  // The cache will hit. Therefore, the new status will be ignored.
  EXPECT_EQ(EvalJs(GetWebContents(), script), kSuccessMessage);
}
