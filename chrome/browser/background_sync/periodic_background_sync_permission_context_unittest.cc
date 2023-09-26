// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
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

#if BUILDFLAG(IS_ANDROID)
  void InstallTwa(const GURL& url) { installed_twas_.insert(url); }
#endif

  // PeriodicBackgroundSyncPermissionContext overrides:
  bool IsPwaInstalled(const GURL& url) const override {
    return installed_pwas_.find(url) != installed_pwas_.end();
  }

#if BUILDFLAG(IS_ANDROID)
  bool IsTwaInstalled(const GURL& url) const override {
    return installed_twas_.find(url) != installed_twas_.end();
  }
#endif

  GURL GetDefaultSearchEngineUrl() const override {
    return default_search_engine_url_;
  }

  void set_default_search_engine_url(const GURL& default_search_engine_url) {
    default_search_engine_url_ = default_search_engine_url;
  }

 private:
  std::set<GURL> installed_pwas_;
#if BUILDFLAG(IS_ANDROID)
  std::set<GURL> installed_twas_;
#endif
  GURL default_search_engine_url_;
};

class PeriodicBackgroundSyncPermissionContextTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PeriodicBackgroundSyncPermissionContextTest(
      const PeriodicBackgroundSyncPermissionContextTest&) = delete;
  PeriodicBackgroundSyncPermissionContextTest& operator=(
      const PeriodicBackgroundSyncPermissionContextTest&) = delete;

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
      render_frame_host = web_contents()->GetPrimaryMainFrame();
    }

    auto permission_result = permission_context_->GetPermissionStatus(
        render_frame_host, /* requesting_origin= */ url,
        /* embedding_origin= */ url);
    return permissions::PermissionUtil::PermissionStatusToContentSetting(
        permission_result.status);
  }

  void SetBackgroundSyncContentSetting(const GURL& url,
                                       ContentSetting setting) {
    auto* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile());
    ASSERT_TRUE(host_content_settings_map);
    host_content_settings_map->SetContentSettingDefaultScope(
        /* primary_url= */ url, /* secondary_url= */ url,
        ContentSettingsType::BACKGROUND_SYNC, setting);
  }

  void InstallPwa(const GURL& url) { permission_context_->InstallPwa(url); }
#if BUILDFLAG(IS_ANDROID)
  void InstallTwa(const GURL& url) { permission_context_->InstallTwa(url); }
#endif

  void SetUpPwaAndContentSettings(const GURL& url) {
    InstallPwa(url);
    SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  }

  void SetDefaultSearchEngineUrl(const GURL& url) {
    permission_context_->set_default_search_engine_url(url);
  }

 private:
  std::unique_ptr<TestPeriodicBackgroundSyncPermissionContext>
      permission_context_;
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(PeriodicBackgroundSyncPermissionContextTest, Twa) {
  GURL url("https://example.com");

  // No TWA or PWA installed.
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_BLOCK);

  InstallTwa(url);
  EXPECT_EQ(GetPermissionStatus(url), CONTENT_SETTING_ALLOW);
}
#endif

TEST_F(PeriodicBackgroundSyncPermissionContextTest, DefaultSearchEngine) {
  GURL requesting_origin("https://example.com");

  // 1. Flag disabled (by default)
  SetDefaultSearchEngineUrl(GURL("https://example.com/foo?q=asdf"));
  EXPECT_EQ(GetPermissionStatus(requesting_origin), CONTENT_SETTING_BLOCK);
}

class PeriodicBackgroundSyncPermissionContextWithPermissionTest
    : public PeriodicBackgroundSyncPermissionContextTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kPeriodicSyncPermissionForDefaultSearchEngine};
};

TEST_F(PeriodicBackgroundSyncPermissionContextWithPermissionTest,
       DefaultSearchEngine) {
  GURL requesting_origin("https://example.com");
  SetDefaultSearchEngineUrl(GURL("https://example.com/foo?q=asdf"));

  // No default search engine.
  SetDefaultSearchEngineUrl(GURL());
  EXPECT_EQ(GetPermissionStatus(requesting_origin), CONTENT_SETTING_BLOCK);

  // Default search engine doesn't match.
  SetDefaultSearchEngineUrl(GURL("https://differentexample.com"));
  EXPECT_EQ(GetPermissionStatus(requesting_origin), CONTENT_SETTING_BLOCK);

  // Default search engine matches.
  SetDefaultSearchEngineUrl(GURL("https://example.com/foo?q=asdf"));
  EXPECT_EQ(GetPermissionStatus(requesting_origin), CONTENT_SETTING_ALLOW);

  // Default search engine matches but no BACKGROUND_SYNC permission.
  SetBackgroundSyncContentSetting(requesting_origin, CONTENT_SETTING_BLOCK);
  EXPECT_EQ(GetPermissionStatus(requesting_origin), CONTENT_SETTING_BLOCK);
}

}  // namespace
