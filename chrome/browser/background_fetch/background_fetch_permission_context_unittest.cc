// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_permission_context.h"

#include <string>

#include "base/macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BackgroundFetchPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  BackgroundFetchPermissionContextTest() = default;

  ~BackgroundFetchPermissionContextTest() override = default;

  ContentSetting GetPermissonStatus(
      const GURL& url,
      BackgroundFetchPermissionContext* permission_context,
      bool with_frame) {
    content::RenderFrameHost* render_frame_host = nullptr;

    if (with_frame) {
      content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
      render_frame_host = web_contents()->GetMainFrame();
    }

    auto permission_result = permission_context->GetPermissionStatus(
        render_frame_host, url /* requesting_origin */,
        url /* embedding_origin */);
    return permission_result.content_setting;
  }

  void SetContentSetting(const GURL& url,
                         ContentSettingsType content_type,
                         ContentSetting setting) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        url /* primary_url*/, url /* secondary_url*/, content_type,
        std::string() /* resource_identifier */, setting);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchPermissionContextTest);
};

// Test that Background Fetch permission is "allow" by default, when queried
// from a top level frame.
TEST_F(BackgroundFetchPermissionContextTest, TestOutcomeAllowWithFrame) {
  GURL url("https://example.com");

  BackgroundFetchPermissionContext permission_context(profile());
  EXPECT_EQ(GetPermissonStatus(url, &permission_context, /*with_frame =*/true),
            CONTENT_SETTING_ALLOW);
}

// Test that Background Fetch permission is "allow" when queried from a worker
// context, if the Automatic Downloads content setting is set to
// CONTENT_SETTING_ALLOW.
TEST_F(BackgroundFetchPermissionContextTest, TestOutcomeAllowWithoutFrame) {
  GURL url("https://example.com");
  SetContentSetting(url, ContentSettingsType::AUTOMATIC_DOWNLOADS,
                    CONTENT_SETTING_ALLOW);

  BackgroundFetchPermissionContext permission_context(profile());

  EXPECT_EQ(GetPermissonStatus(url, &permission_context, /*with_frame =*/false),
            CONTENT_SETTING_ALLOW);
}

// Test that Background Fetch permission is "deny" when queried from a worker
// context, if the Automatic Downloads content settings is set to
// CONTENT_SETTING_BLOCK.
TEST_F(BackgroundFetchPermissionContextTest, TestOutcomeDenyWithoutFrame) {
  GURL url("https://example.com");
  SetContentSetting(url, ContentSettingsType::AUTOMATIC_DOWNLOADS,
                    CONTENT_SETTING_BLOCK);

  BackgroundFetchPermissionContext permission_context(profile());
  EXPECT_EQ(GetPermissonStatus(url, &permission_context, /*with_frame =*/false),
            CONTENT_SETTING_BLOCK);
}

// Test that Background Fetch permission is "prompt" when queried from a worker
// context, if the Automatic Downloads content setting is CONTENT_SETTING_ASK.
TEST_F(BackgroundFetchPermissionContextTest, TestOutcomePromptWithoutFrame) {
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  ASSERT_TRUE(host_content_settings_map);

  GURL url("https://example.com");
  SetContentSetting(url, ContentSettingsType::AUTOMATIC_DOWNLOADS,
                    CONTENT_SETTING_ASK);

  BackgroundFetchPermissionContext permission_context(profile());

  EXPECT_EQ(GetPermissonStatus(url, &permission_context, /*with_frame =*/false),
            CONTENT_SETTING_ASK);
}

}  // namespace
