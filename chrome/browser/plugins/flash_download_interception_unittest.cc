// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include <memory>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationHandle;
using content::NavigationThrottle;

class FlashDownloadInterceptionTest : public ChromeRenderViewHostTestHarness {
 public:
  FlashDownloadInterceptionTest() : source_url_("https://source-url.com") {}

  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }

  bool ShouldStopFlashDownloadAction(const std::string& target_url) {
    return FlashDownloadInterception::ShouldStopFlashDownloadAction(
        host_content_settings_map(), source_url_, GURL(target_url), true);
  }

  void SetFlashContentSetting(ContentSetting setting) {
    host_content_settings_map()->SetContentSettingDefaultScope(
        source_url_, source_url_, ContentSettingsType::PLUGINS, std::string(),
        setting);
  }

 private:
  const GURL source_url_;
};

TEST_F(FlashDownloadInterceptionTest, DownloadUrlVariations) {
  const char* const flash_intercept_urls[] = {
      "https://get.adobe.com/flashplayer/",
      "http://get2.adobe.com/flashplayer/",
      "http://get.adobe.com/flash",
      "http://get.adobe.com/fr/flashplayer/",
      "http://get.adobe.com/flashplayer",
      "http://macromedia.com/go/getflashplayer",
      "http://adobe.com/go/getflashplayer",
      "http://adobe.com/go/CA-H-GET-FLASH",
      "http://adobe.com/go/DE_CH-H-M-A2",
      "http://adobe.com/go/gntray_dl_getflashplayer_jp",
      "http://www.adobe.com/shockwave/download/download.cgi?"
      "P1_Prod_Version=ShockwaveFlash",
  };

  for (auto* url : flash_intercept_urls) {
    EXPECT_TRUE(ShouldStopFlashDownloadAction(url))
        << "Should have intercepted: " << url;
  }

  const char* const flash_no_intercept_urls[] = {
      "https://www.examplefoo.com",
      "http://examplefoo.com/get.adobe.com/flashplayer",
      "http://ww.macromedia.com/go/getflashplayer",
      "http://wwwxmacromedia.com/go/getflashplayer",
      "http://www.adobe.com/software/flash/about/",
      "http://www.adobe.com/products/flashplayer.html",
      "http://www.adobe.com/products/flashruntimes.html",
      "http://www.adobe.com/go/flash",
      // Don't intercept URLs containing just "fp" without a matching prefix.
      "http://www.adobe.com/go/non-matching-prefix-fp",
      // Don't match text within the query or fragment.
      "http://www.adobe.com/go/non-matching?foo=flashplayer",
      "http://www.adobe.com/go/non-matching#!foo=flashplayer",
      "http://www.adobe.com/shockwave/download/download.cgi?"
      "P1_Prod_Version=SomethingElse",
  };

  for (auto* url : flash_no_intercept_urls) {
    EXPECT_FALSE(ShouldStopFlashDownloadAction(url))
        << "Should not have intercepted: " << url;
  }

  // Don't intercept navigations occurring on the flash download page.
  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("https://get.adobe.com/flashplayer/"),
      GURL("https://get.adobe.com/flashplayer/"), true));
}

TEST_F(FlashDownloadInterceptionTest, NavigationThrottleCancelsNavigation) {
  // Set the source URL to an HTTP source.
  NavigateAndCommit(GURL("http://example.com"));

  content::TestNavigationThrottleInserter throttle_inserter(
      web_contents(),
      base::BindRepeating(&FlashDownloadInterception::MaybeCreateThrottleFor));

  std::unique_ptr<content::NavigationSimulator> simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://get.adobe.com/flashplayer"), main_rfh());
  simulator->Commit();
  EXPECT_EQ(content::NavigationThrottle::CANCEL_AND_IGNORE,
            simulator->GetLastThrottleCheckResult());
}

TEST_F(FlashDownloadInterceptionTest, OnlyInterceptOnDetectContentSetting) {
  // Default Setting (which is DETECT)
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));

  // No intercept on ALLOW.
  SetFlashContentSetting(CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));

  // Intercept on both explicit DETECT and BLOCK.
  SetFlashContentSetting(CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));
  SetFlashContentSetting(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));
}
