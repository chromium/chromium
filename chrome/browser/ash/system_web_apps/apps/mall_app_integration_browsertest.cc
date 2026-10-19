// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/mall/url_constants.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "chromeos/ash/components/system_web_apps/system_web_app_type.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

class MallAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  std::string GetMallEmbedUrl(content::WebContents* contents) {
    // Poll to wait for an iframe to be embedded on the page, and resolve with
    // the 'src' attribute.
    constexpr char kScript[] = R"js(
      new Promise((resolve, reject) => {
        let intervalId = setInterval(() => {
          const src = document.querySelector("iframe")?.src;
          if (src) {
            clearInterval(intervalId);
            resolve(src);
          }
        }, 50);
      });
    )js";

    return content::EvalJs(contents, kScript).ExtractString();
  }
};

// Test that the Mall app installs and launches correctly.
IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, MallApp) {
  const GURL url{ash::kChromeUIMallUrl};
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(ash::SystemWebAppType::MALL,
                                                  url,
                                                  /*title=*/"Apps & games"));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, EmbedMallWithContext) {
  WaitForTestSystemAppInstall();

  content::WebContents* contents = LaunchApp(ash::SystemWebAppType::MALL);

  EXPECT_THAT(
      GetMallEmbedUrl(contents),
      testing::StartsWith(
          "https://discover.apps.chrome/?origin=chrome%3A%2F%2Fmall&context="));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, EmbedMallWithDeepLink) {
  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::MALL);
  params.override_url = GURL("chrome://mall/apps/list");

  content::WebContents* contents = LaunchApp(std::move(params));

  EXPECT_THAT(GetMallEmbedUrl(contents),
              testing::StartsWith("https://discover.apps.chrome/apps/"
                                  "list?origin=chrome%3A%2F%2Fmall&context="));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, OfflineMessageEncoding) {
  ScopedBrowserLocale browser_locale("ja");
  base::ScopedAllowBlockingForTesting allow_blocking;
  ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("ja");

  WaitForTestSystemAppInstall();

  content::WebContents* contents = LaunchApp(ash::SystemWebAppType::MALL);

  EXPECT_EQ(contents->GetEncoding(), "UTF-8");
  EXPECT_EQ(content::EvalJs(contents, "document.characterSet"), "UTF-8");
  EXPECT_EQ(content::EvalJs(contents, "document.documentElement.lang"), "ja");

  // Tests don't have connection to the outside internet so the offline message
  // will be shown.
  EXPECT_EQ(
      content::EvalJs(contents,
                      "document.getElementById('offline-message').textContent"),
      "オフラインです");

  ui::ResourceBundle::GetSharedInstance().ReloadLocaleResources("en-US");
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MallAppIntegrationTest);

}  // namespace
