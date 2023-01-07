// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/platform_verification_chromeos.h"

#include <memory>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/permission_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

GURL GetTestURL() {
  return GURL("https://test.example.com");
}

class PlatformVerificationChromeOSTest
    : public ChromeRenderViewHostTestHarness {
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    profile()->SetPermissionControllerDelegate(
        permissions::GetPermissionControllerDelegate(profile()));
    NavigateAndCommit(GetTestURL());
  }
};

// Tests the basic success case.
TEST_F(PlatformVerificationChromeOSTest, Success) {
  EXPECT_TRUE(platform_verification::PerformBrowserChecks(
      web_contents()->GetPrimaryMainFrame()));
}

TEST_F(PlatformVerificationChromeOSTest, BadURL) {
  GURL url("badurl");
  NavigateAndCommit(url);
  EXPECT_FALSE(platform_verification::PerformBrowserChecks(
      web_contents()->GetPrimaryMainFrame()));
}

TEST_F(PlatformVerificationChromeOSTest, IncognitoProfile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(
          profile()->GetOffTheRecordProfile(Profile::OTRProfileID::PrimaryID(),
                                            /*create_if_needed=*/true),
          /*site_instance=*/nullptr);
  content::WebContentsTester* tester =
      content::WebContentsTester::For(web_contents.get());
  tester->NavigateAndCommit(GetTestURL());
  EXPECT_FALSE(platform_verification::PerformBrowserChecks(
      web_contents.get()->GetPrimaryMainFrame()));
}

TEST_F(PlatformVerificationChromeOSTest, ContentSettings) {
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ASSERT_TRUE(host_content_settings_map);
  host_content_settings_map->SetContentSettingDefaultScope(
      /*primary_url=*/GetTestURL(), /*secondary_url=*/GetTestURL(),
      ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER, CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(platform_verification::PerformBrowserChecks(
      web_contents()->GetPrimaryMainFrame()));
}

}  // namespace
