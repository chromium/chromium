// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/mall/url_constants.h"
#include "ash/webui/print_preview_cros/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class MallAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  MallAppIntegrationTest() {
    features_.InitWithFeatures(
        {chromeos::features::kCrosMall, chromeos::features::kCrosMallSwa},
        /*disabled_features=*/{});
  }

  std::string GetMallEmbedUrl(content::WebContents* contents) {
    // Poll to wait for an iframe to be embedded on the page, and resolve with
    // the 'src' attribute.
    constexpr char kScript[] = R"js(
      new Promise((resolve, reject) => {
        let intervalId = setInterval(() => {
          if (document.querySelector("iframe")) {
            clearInterval(intervalId);
            resolve(document.querySelector("iframe").src);
          }
        }, 50);
      });
    )js";

    return content::EvalJs(contents, kScript).ExtractString();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that the Mall app installs and launches correctly.
IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, MallApp) {
  const GURL url{ash::kChromeUIMallUrl};
  EXPECT_NO_FATAL_FAILURE(
      ExpectSystemWebAppValid(ash::SystemWebAppType::MALL, url,
                              /*title=*/"Get Apps and Games"));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, EmbedMallWithContext) {
  WaitForTestSystemAppInstall();

  content::WebContents* contents = LaunchApp(ash::SystemWebAppType::MALL);

  EXPECT_THAT(GetMallEmbedUrl(contents),
              testing::StartsWith("https://discover.apps.chrome/?context="));
}

IN_PROC_BROWSER_TEST_P(MallAppIntegrationTest, EmbedMallWithDeepLink) {
  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::MALL);
  params.override_url = GURL("chrome://mall/apps/list");

  content::WebContents* contents = LaunchApp(std::move(params));

  EXPECT_THAT(
      GetMallEmbedUrl(contents),
      testing::StartsWith("https://discover.apps.chrome/apps/list?context="));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    MallAppIntegrationTest);

}  // namespace
