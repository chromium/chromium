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
#include "components/permissions/permission_actions_history.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#endif

namespace {

class MockPeriodicBackgroundSyncPermissionContext
    : public PeriodicBackgroundSyncPermissionContext {
 public:
  explicit MockPeriodicBackgroundSyncPermissionContext(Profile* profile)
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

  MOCK_METHOD(void,
              OnContentSettingChanged,
              (const ContentSettingsPattern& primary_pattern,
               const ContentSettingsPattern& secondary_pattern,
               ContentSettingsTypeSet content_type_set),
              (override));

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
    mock_permission_context_ =
        std::make_unique<MockPeriodicBackgroundSyncPermissionContext>(
            profile());
#if !BUILDFLAG(IS_ANDROID)
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
#endif  // !BUILDFLAG(IS_ANDROID)
  }

  void TearDown() override {
    // The destructor for |mock_permission_context_| needs a valid thread
    // bundle.
    mock_permission_context_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  ContentSetting GetPermissionStatus(const GURL& url, bool with_frame = false) {
    content::RenderFrameHost* render_frame_host = nullptr;

    if (with_frame) {
      content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);
      render_frame_host = web_contents()->GetPrimaryMainFrame();
    }

    auto permission_result = mock_permission_context_->GetPermissionStatus(
        content::PermissionDescriptorUtil::
            CreatePermissionDescriptorForPermissionType(
                blink::PermissionType::PERIODIC_BACKGROUND_SYNC),
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

  void InstallPwa(const GURL& url) {
    mock_permission_context_->InstallPwa(url);
  }
#if BUILDFLAG(IS_ANDROID)
  void InstallTwa(const GURL& url) {
    mock_permission_context_->InstallTwa(url);
  }
#endif

  void SetUpPwaAndContentSettings(const GURL& url) {
    InstallPwa(url);
    SetBackgroundSyncContentSetting(url, CONTENT_SETTING_ALLOW);
  }

  void SetDefaultSearchEngineUrl(const GURL& url) {
    mock_permission_context_->set_default_search_engine_url(url);
  }

  std::unique_ptr<MockPeriodicBackgroundSyncPermissionContext>
      mock_permission_context_;
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
#else  // !BUILDFLAG(IS_ANDROID)
TEST_F(PeriodicBackgroundSyncPermissionContextTest, OnWebAppInstalled) {
  GURL url("https://example.com");
  // Both both `OnWebAppInstalled` and `OnWebAppInstalledWithOsHooks`
  // can be called. So there might be more than 1 times.
  EXPECT_CALL(*mock_permission_context_,
              OnContentSettingChanged(
                  ContentSettingsPattern::FromURL(url),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsTypeSet(
                      ContentSettingsType::PERIODIC_BACKGROUND_SYNC)))
      .Times(testing::AtLeast(1));

  web_app::test::InstallDummyWebApp(profile(), "Test App", url);
}

TEST_F(PeriodicBackgroundSyncPermissionContextTest, OnWebAppUninstalled) {
  GURL url("https://example.com");
  const webapps::AppId app_id =
      web_app::test::InstallDummyWebApp(profile(), "Test App", url);

  EXPECT_CALL(*mock_permission_context_,
              OnContentSettingChanged(
                  ContentSettingsPattern::FromURL(url),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsTypeSet(
                      ContentSettingsType::PERIODIC_BACKGROUND_SYNC)))
      .Times(1);

  web_app::test::UninstallWebApp(profile(), app_id);
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
