// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_task.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/android/webapk/webapk_restore_manager.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {

class TestWebContentsManager : public WebApkRestoreWebContentsManager {
 public:
  explicit TestWebContentsManager(Profile* profile,
                                  content::WebContents* web_contents)
      : WebApkRestoreWebContentsManager(profile),
        test_web_contents_(web_contents->GetWeakPtr()) {}
  ~TestWebContentsManager() override = default;

  content::WebContents* web_contents() override {
    return test_web_contents_.get();
  }

 private:
  const base::WeakPtr<content::WebContents> test_web_contents_;
};

class WebApkRestoreTaskBrowserTest : public AndroidBrowserTest {
 public:
  WebApkRestoreTaskBrowserTest() = default;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/banners");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  void OnTaskCompleted(base::OnceClosure done,
                       const GURL& expected_manifest_id,
                       const GURL& manifest_id,
                       webapps::WebApkInstallResult result) {
    EXPECT_EQ(webapps::WebApkInstallResult::SERVER_URL_INVALID, result);
    EXPECT_EQ(expected_manifest_id, manifest_id);
    std::move(done).Run();
  }

  std::unique_ptr<WebApkRestoreWebContentsManager> GetTestWebContentsManager() {
    return std::make_unique<TestWebContentsManager>(profile(), web_contents());
  }
};

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, CreateAndRunTasks) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  GURL test_url = embedded_test_server()->GetURL("/manifest_test_page.html");

  auto web_contents_manager = GetTestWebContentsManager();

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(),
      std::make_unique<webapps::ShortcutInfo>(test_url), base::Time());
  task.Start(base::BindOnce(&WebApkRestoreTaskBrowserTest::OnTaskCompleted,
                            base::Unretained(this), run_loop.QuitClosure(),
                            test_url));
  run_loop.Run();

  EXPECT_EQ(test_url,
            web_contents_manager->web_contents()->GetLastCommittedURL());

  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.FetchInstallableData.AppType", 1 /* WEBAPK */, 1);
  histogram_tester.ExpectUniqueSample("WebApk.Sync.Restore.InstallEvent",
                                      0 /* kNone */, 1);
  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.InstallResult.Fetched", 4 /* SERVER_URL_INVALID */,
      1);
  histogram_tester.ExpectTotalCount(
      "WebApk.Sync.Restore.InstallResult.Fallback", 0);
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, NoManifestPage) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  GURL test_url = embedded_test_server()->GetURL("/no_manifest_test_page.html");

  auto web_contents_manager = GetTestWebContentsManager();

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(),
      std::make_unique<webapps::ShortcutInfo>(test_url), base::Time());
  task.Start(base::BindOnce(&WebApkRestoreTaskBrowserTest::OnTaskCompleted,
                            base::Unretained(this), run_loop.QuitClosure(),
                            test_url));
  run_loop.Run();

  EXPECT_EQ(test_url,
            web_contents_manager->web_contents()->GetLastCommittedURL());

  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.FetchInstallableData.AppType", 2 /* SHORTCUT */, 1);
  histogram_tester.ExpectUniqueSample("WebApk.Sync.Restore.InstallEvent",
                                      3 /* kNotWebApkCompatible */, 1);
  histogram_tester.ExpectTotalCount("WebApk.Sync.Restore.InstallResult.Fetched",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.InstallResult.Fallback", 4 /* SERVER_URL_INVALID */,
      1);
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, StartUrlNotLoadable) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  GURL test_url = embedded_test_server()->GetURL("/not_exist.html");

  auto web_contents_manager = GetTestWebContentsManager();

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(),
      std::make_unique<webapps::ShortcutInfo>(test_url), base::Time());
  task.Start(base::BindOnce(&WebApkRestoreTaskBrowserTest::OnTaskCompleted,
                            base::Unretained(this), run_loop.QuitClosure(),
                            test_url));
  run_loop.Run();

  EXPECT_EQ(test_url,
            web_contents_manager->web_contents()->GetLastCommittedURL());

  histogram_tester.ExpectUniqueSample("WebApk.Sync.Restore.InstallEvent",
                                      1 /* kLoadUrl */, 1);
  histogram_tester.ExpectTotalCount("WebApk.Sync.Restore.InstallResult.Fetched",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.InstallResult.Fallback", 4 /* SERVER_URL_INVALID */,
      1);
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, ManifestIdMismatch) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  GURL test_url = embedded_test_server()->GetURL("/manifest_test_page.html");
  GURL test_manifest_id = GURL("https://example.com");

  auto test_shortcut_info = std::make_unique<webapps::ShortcutInfo>(test_url);
  test_shortcut_info->manifest_id = test_manifest_id;

  auto web_contents_manager = GetTestWebContentsManager();

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(), std::move(test_shortcut_info), base::Time());
  task.Start(base::BindOnce(&WebApkRestoreTaskBrowserTest::OnTaskCompleted,
                            base::Unretained(this), run_loop.QuitClosure(),
                            test_manifest_id));
  run_loop.Run();

  EXPECT_EQ(test_url,
            web_contents_manager->web_contents()->GetLastCommittedURL());

  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.FetchInstallableData.AppType", 1 /* WEBAPK */, 1);
  histogram_tester.ExpectUniqueSample("WebApk.Sync.Restore.InstallEvent",
                                      2 /* kManifestIdMismatch */, 1);
  histogram_tester.ExpectTotalCount("WebApk.Sync.Restore.InstallResult.Fetched",
                                    0);
  histogram_tester.ExpectUniqueSample(
      "WebApk.Sync.Restore.InstallResult.Fallback", 4 /* SERVER_URL_INVALID */,
      1);
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, DownloadIcon) {
  auto web_contents_manager = GetTestWebContentsManager();

  auto test_shortcut_info = std::make_unique<webapps::ShortcutInfo>(
      embedded_test_server()->GetURL("/manifest_test_page.html"));
  test_shortcut_info->best_primary_icon_url =
      embedded_test_server()->GetURL("/256x256-green.png");

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(), std::move(test_shortcut_info), base::Time());

  base::RunLoop run_loop;
  task.DownloadIcon(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(task.app_icon().drawsNothing());
  EXPECT_NE(task.app_icon().width(), 256);
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, DownloadIconNoIconUrl) {
  auto web_contents_manager = GetTestWebContentsManager();

  auto test_shortcut_info = std::make_unique<webapps::ShortcutInfo>(
      embedded_test_server()->GetURL("/manifest_test_page.html"));
  test_shortcut_info->best_primary_icon_url = GURL();

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(), std::move(test_shortcut_info), base::Time());

  base::RunLoop run_loop;
  task.DownloadIcon(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(task.app_icon().drawsNothing());
}

IN_PROC_BROWSER_TEST_F(WebApkRestoreTaskBrowserTest, DownloadIconBadIcon) {
  auto web_contents_manager = GetTestWebContentsManager();

  auto test_shortcut_info = std::make_unique<webapps::ShortcutInfo>(
      embedded_test_server()->GetURL("/manifest_test_page.html"));
  test_shortcut_info->best_primary_icon_url =
      embedded_test_server()->GetURL("/bad_icon.png");

  WebApkRestoreTask task(
      WebApkRestoreManager::PassKeyForTesting(),
      WebApkInstallServiceFactory::GetForBrowserContext(profile()),
      web_contents_manager.get(), std::move(test_shortcut_info), base::Time());

  base::RunLoop run_loop;
  task.DownloadIcon(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(task.app_icon().drawsNothing());
}

}  // namespace webapk
