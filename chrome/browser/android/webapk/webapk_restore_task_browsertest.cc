// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/android/webapk/webapk_restore_manager.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {

namespace {

sync_pb::WebApkSpecifics CreateWebApkSpecifics(const std::string& url) {
  sync_pb::WebApkSpecifics web_apk;
  web_apk.set_manifest_id(url);
  web_apk.set_start_url(url);

  return web_apk;
}
}  // namespace

class WebApkRestoreTaskBrowserTest : public PlatformBrowserTest {
 public:
  WebApkRestoreTaskBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }
};

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, CreateAndRunTasks) {
  base::RunLoop run_loop;

  std::unique_ptr<WebApkRestoreWebContentsManager> web_contents_manager =
      std::make_unique<WebApkRestoreWebContentsManager>(profile());
  web_contents_manager->EnsureWebContentsCreated(
      WebApkRestoreManager::PassKeyForTesting());

  GURL test_url = embedded_test_server()->GetURL("/manifest_test_page.html");

  WebApkRestoreTask task(WebApkRestoreManager::PassKeyForTesting(),
                         CreateWebApkSpecifics(test_url.spec()));

  task.Start(web_contents_manager.get(),
             base::BindLambdaForTesting(
                 [&run_loop](const GURL&) { run_loop.Quit(); }));

  run_loop.Run();

  EXPECT_EQ(test_url,
            web_contents_manager->web_contents()->GetLastCommittedURL());
}

}  // namespace webapk
