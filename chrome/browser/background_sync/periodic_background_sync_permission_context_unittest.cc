// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestPeriodicBackgroundSyncPermissionContext
    : public PeriodicBackgroundSyncPermissionContext {
 public:
  explicit TestPeriodicBackgroundSyncPermissionContext(Profile* profile)
      : PeriodicBackgroundSyncPermissionContext(profile) {}

  void InstallPwa(const GURL& url) { installed_pwas_.insert(url); }

#if defined(OS_ANDROID)
  void InstallTwa(const GURL& url) { installed_twas_.insert(url); }
#endif

  // PeriodicBackgroundSyncPermissionContext overrides:
  bool IsPwaInstalled(const GURL& url) const override {
    return installed_pwas_.find(url) != installed_pwas_.end();
  }

#if defined(OS_ANDROID)
  bool IsTwaInstalled(const GURL& url) const override {
    return installed_twas_.find(url) != installed_twas_.end();
  }
#endif

 private:
  std::set<GURL> installed_pwas_;
#if defined(OS_ANDROID)
  std::set<GURL> installed_twas_;
#endif
};

class PeriodicBackgroundSyncPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  PeriodicBackgroundSyncPermissionContextTest() = default;
  ~PeriodicBackgroundSyncPermissionContextTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    permission_context_ =
        std::make_unique<TestPeriodicBackgroundSyncPermissionContext>(
            profile());
  }

  void TearDown() override {
    // The destructor for |permission_context_| needs a valid thread bundle.
    permission_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ContentSetting GetPermissionStatus(const GURL& url, bool with_frame = false) {
    content::RenderFrameHost* render_frame_host = nullptr;

    if (with_frame) {
      content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
      render_frame_host = web_contents()->GetMainFrame();
    }

    auto permission_result = permission_context_->GetPermissionStatus(
        render_frame_host, /* requesting_origin= */ url,
        /* embedding_origin= */ url);
    return permission_result.content_setting;
  }

  void SetBackgroundSyncContentSetting(const GURL& url,
                                       ContentSetting setting) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        /* primary_url= */ url, /* secondary_url= */ url,
        ContentSettingsType::BACKGROUND_SYNC,
        /* resource_identifier= */ std::string(), setting);
  }

  void InstallPwa(const GURL& url) { permission_context_->InstallPwa(url); }
#if defined(OS_ANDROID)
  void InstallTwa(const GURL& url) { permission_context_->InstallTwa(url); }
#endif

  void SetUpPwaAndContentSettings(const GURL& url) {
    InstallPwa(url);
    SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  }

 private:
  std::unique_ptr<TestPeriodicBackgroundSyncPermissionContext>
      permission_context_;
  DISALLOW_COPY_AND_ASSIGN(PeriodicBackgroundSyncPermissionContextTest);
};

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyWhenFeatureDisabled) {
  EXPECT_EQ(GetPermissionStatus(GURL("https://example.com")),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DenyForInsecureOrigin) {
  GURL url("http://example.com");
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ false),
            CONTENT_SETTING_BLOCK);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithFrame) {
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ true),
            CONTENT_SETTING_ALLOW);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, AllowWithoutFrame) {
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);

  EXPECT_EQ(GetPermissionStatus(url, /* with_frame= */ false),
            CONTENT_SETTING_ALLOW);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DesktopPwa) {
  GURL url("https://example.com");
  SetUpPwaAndContentSettings(url);

  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_ALLOW);

  // Disable one-shot Background Sync.
  SetBackgroundSyncContentSetting(url, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_BLOCK);
}

#if defined(OS_ANDROID)
TEST_F(PeriodicBackgroundSyncPermissionContextTest, Twa) {
  GURL url("https://example.com");

  // No TWA or PWA installed.
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_BLOCK);

  InstallTwa(url);
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_ALLOW);
}
#endif

}  // namespace
