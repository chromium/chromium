// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"

#include "base/path_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const char kAppId[] = "123";
const char kAppUrl[] = "https://example.com/";
const char kAppKey[] = "apps";
const char kAppTitle[] = "Title";
const char kAppTitle2[] = "Title2";
const char kTitleKey[] = "name";
const char kIconKey[] = "icon";
const char kLaunchUrlKey[] = "launch_url";
const char kIconPath[] = "chrome/test/data/load_image/image.png";
const char kIconUrl[] = "/load_image/image.png";
const char kIconUrl2[] = "/load_image/fail_image.png";
const char kLastIconUrlKey[] = "last_icon_url";
const char kLaunchUrl[] = "https://example.com/launch";

base::FilePath GetFullPathToImage() {
  base::FilePath test_data_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  return test_data_dir.Append(kIconPath);
}

}  // namespace

class WebKioskAppDataTest : public InProcessBrowserTest,
                            public KioskAppDataDelegate {
 public:
  void WaitForAppDataChange(int count) {
    if (change_count_ >= count)
      return;
    waited_count_ = count;
    waiter_ = std::make_unique<base::RunLoop>();
    waiter_->Run();
  }

  void SetCached(bool installed) {
    const std::string app_key = std::string(kAppKey) + '.' + kAppId;
    auto app_dict = std::make_unique<base::DictionaryValue>();

    app_dict->SetString(app_key + '.' + std::string(kTitleKey), kAppTitle);
    app_dict->SetString(app_key + '.' + std::string(kIconKey),
                        GetFullPathToImage().value());
    if (installed)
      app_dict->SetString(app_key + '.' + std::string(kLaunchUrlKey),
                          kLaunchUrl);
    g_browser_process->local_state()->Set(
        WebKioskAppManager::kWebKioskDictionaryName, *app_dict);
  }

 private:
  // KioskAppDataDelegate:
  void GetKioskAppIconCacheDir(base::FilePath* cache_dir) override {
    base::FilePath user_data_dir;
    bool has_dir =
        base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    DCHECK(has_dir);
    *cache_dir = user_data_dir;
  }

  void OnKioskAppDataChanged(const std::string& app_id) override {
    change_count_++;
    if (change_count_ >= waited_count_ && waiter_)
      waiter_->Quit();
  }

  void OnKioskAppDataLoadFailure(const std::string& app_id) override {}

  void OnExternalCacheDamaged(const std::string& app_id) override {}

  // std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<base::RunLoop> waiter_;
  int change_count_ = 0;
  int waited_count_ = 0;
};

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, NoIconCached) {
  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           std::string(), /*icon_url*/ GURL());
  app_data.LoadFromCache();
  // The app will stay in the INIT state if there is nothing to be loaded from
  // cache.
  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kInit);
  EXPECT_EQ(app_data.name(), kAppUrl);
  EXPECT_TRUE(app_data.icon().isNull());
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, LoadCachedIcon) {
  SetCached(/*installed = */ false);
  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           std::string(), /*icon_url*/ GURL());
  app_data.LoadFromCache();
  app_data.LoadIcon();
  WaitForAppDataChange(2);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoaded);
  EXPECT_EQ(app_data.name(), kAppTitle);
  EXPECT_FALSE(app_data.icon().isNull());
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, PRE_DownloadedIconPersists) {
  // Start test server.
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(test_server.Start());

  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           kAppTitle,
                           /*icon_url*/ test_server.GetURL(kIconUrl));
  app_data.LoadFromCache();
  app_data.LoadIcon();
  WaitForAppDataChange(1);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoaded);
  EXPECT_EQ(app_data.name(), kAppTitle);
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, DownloadedIconPersists) {
  // No test server is launched intentionaly to verify that we are using the
  // cached icon.
  // We should still find the correct icon url in order to not initiate a
  // redownload.
  const std::string* icon_url_string =
      g_browser_process->local_state()
          ->GetDictionary(WebKioskAppManager::kWebKioskDictionaryName)
          ->FindDictKey(KioskAppDataBase::kKeyApps)
          ->FindDictKey(kAppId)
          ->FindStringKey(kLastIconUrlKey);
  ASSERT_TRUE(icon_url_string);
  const GURL icon_url = GURL(*icon_url_string);

  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           kAppTitle2, /*icon_url=*/icon_url);
  app_data.LoadFromCache();
  // Icon is stored in cache.
  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoading);

  app_data.LoadIcon();
  WaitForAppDataChange(2);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoaded);
  // The title should not persist.
  EXPECT_EQ(app_data.name(), kAppTitle2);
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest,
                       PRE_RedownloadIconWhenDifferentUrl) {
  // Start test server.
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(test_server.Start());

  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           kAppTitle,
                           /*icon_url*/ test_server.GetURL(kIconUrl));
  app_data.LoadFromCache();
  app_data.LoadIcon();
  WaitForAppDataChange(1);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoaded);
  EXPECT_EQ(app_data.name(), kAppTitle);
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, RedownloadIconWhenDifferentUrl) {
  // Start test server.
  net::EmbeddedTestServer test_server;
  test_server.AddDefaultHandlers(GetChromeTestDataDir());
  ASSERT_TRUE(test_server.Start());

  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           kAppTitle2,
                           /*icon_url*/ test_server.GetURL(kIconUrl2));

  app_data.LoadFromCache();
  // No icon was loaded from cache because urls are different.
  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kInit);

  app_data.LoadIcon();
  WaitForAppDataChange(1);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kLoaded);
  EXPECT_EQ(app_data.name(), kAppTitle2);
}

IN_PROC_BROWSER_TEST_F(WebKioskAppDataTest, AlreadyInstalled) {
  SetCached(/*installed = */ true);
  WebKioskAppData app_data(this, kAppId, EmptyAccountId(), GURL(kAppUrl),
                           kAppTitle2, /*icon_url=*/GURL());
  app_data.LoadFromCache();
  app_data.LoadIcon();
  WaitForAppDataChange(2);

  EXPECT_EQ(app_data.status(), WebKioskAppData::Status::kInstalled);
  EXPECT_EQ(app_data.name(), kAppTitle);
}

}  // namespace ash
