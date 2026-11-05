// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/request_desktop_site_web_contents_observer_android.h"

#include <cstdint>
#include <memory>

#include "base/android/device_info.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/view_android.h"
#include "url/gurl.h"

namespace {

constexpr char kTestUrl[] = "https://example.com/";
constexpr char kDesktopUrl[] = "https://desktop.example.com/";

// Values matching `rds_web_contents_observer::UserAgentRequestType`
constexpr int64_t kUkmUserAgentRequestTypeDesktop = 0;

class RequestDesktopSiteWebContentsObserverAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  RequestDesktopSiteWebContentsObserverAndroidTest() = default;
  ~RequestDesktopSiteWebContentsObserverAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    host_content_settings_map_ =
        HostContentSettingsMapFactory::GetForProfile(profile());
    RequestDesktopSiteWebContentsObserverAndroid::CreateForWebContents(
        web_contents());
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void TearDown() override {
    test_ukm_recorder_.reset();
    host_content_settings_map_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetDefaultContentSetting(ContentSetting setting) {
    host_content_settings_map_->SetDefaultContentSetting(
        ContentSettingsType::REQUEST_DESKTOP_SITE, setting);
  }

  void SetContentSettingForUrl(const GURL& url, ContentSetting setting) {
    host_content_settings_map_->SetContentSettingDefaultScope(
        url, url, ContentSettingsType::REQUEST_DESKTOP_SITE, setting);
  }

  void SetWindowSettingEnabled(bool enabled) {
    profile()->GetPrefs()->SetBoolean(prefs::kDesktopSiteWindowSettingEnabled,
                                      enabled);
  }

  void SetWebContentsWidth(int width_dp) {
    web_contents()->GetNativeView()->SetLayoutForTesting(0, 0, width_dp, 800);
  }

  bool CommitRendererInitiatedNavigation(const GURL& url) {
    auto simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());
    simulator->Commit();
    return web_contents()
        ->GetController()
        .GetLastCommittedEntry()
        ->GetIsOverridingUserAgent();
  }

  bool CommitBrowserInitiatedNavigation(const GURL& url) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    simulator->Commit();
    return web_contents()
        ->GetController()
        .GetLastCommittedEntry()
        ->GetIsOverridingUserAgent();
  }

  void ExpectUkmEntry(int64_t expected_user_agent_type) {
    auto entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::Android_NonDefaultRdsPageLoad::kEntryName);
    ASSERT_EQ(1u, entries.size());
    test_ukm_recorder_->ExpectEntryMetric(
        entries[0],
        ukm::builders::Android_NonDefaultRdsPageLoad::kUserAgentTypeName,
        expected_user_agent_type);
  }

  void ExpectNoUkmEntry() {
    auto entries = test_ukm_recorder_->GetEntriesByName(
        ukm::builders::Android_NonDefaultRdsPageLoad::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest, CreateForWebContents) {
  EXPECT_NE(nullptr,
            RequestDesktopSiteWebContentsObserverAndroid::FromWebContents(
                web_contents()));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       DefaultGlobalSettingBlock) {
  // By default, REQUEST_DESKTOP_SITE is blocked (mobile UA).
  EXPECT_FALSE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
  ExpectNoUkmEntry();
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       DefaultGlobalSettingAllow) {
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
  // UKM should not be recorded for global settings.
  ExpectNoUkmEntry();
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       SiteExceptionAllowOverridesDefaultBlock) {
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  SetContentSettingForUrl(GURL(kDesktopUrl), CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kDesktopUrl)));
  ExpectUkmEntry(kUkmUserAgentRequestTypeDesktop);
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       SiteExceptionBlockOverridesDefaultAllow) {
  static constexpr char kMobileUrl[] = "https://mobile.example.com/";
  static constexpr int64_t kUkmUserAgentRequestTypeMobile = 1;

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  SetContentSettingForUrl(GURL(kMobileUrl), CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(CommitRendererInitiatedNavigation(GURL(kMobileUrl)));
  ExpectUkmEntry(kUkmUserAgentRequestTypeMobile);
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       BrowserInitiatedNavigationDoesNotOverrideUA) {
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);

  // Observer only overrides UA for renderer-initiated navigations.
  EXPECT_FALSE(CommitBrowserInitiatedNavigation(GURL(kTestUrl)));
  ExpectNoUkmEntry();
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       SubframeNavigationDoesNotOverrideUA) {
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  SetContentSettingForUrl(GURL(kDesktopUrl), CONTENT_SETTING_ALLOW);

  // First commit the primary main frame with site exception (ALLOW).
  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kDesktopUrl)));
  ExpectUkmEntry(kUkmUserAgentRequestTypeDesktop);

  // Reset UKM recorder to verify that subsequent subframe navigations do not
  // record UKM even when targeting a URL with a site-specific RDS exception.
  test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

  // Append a child frame.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("child_frame");
  ASSERT_TRUE(subframe);

  // Use a MockNavigationHandle for a subframe navigation to verify that
  // SetIsOverridingUserAgent is never called.
  testing::NiceMock<content::MockNavigationHandle> subframe_handle(
      GURL(kDesktopUrl), subframe);
  EXPECT_CALL(subframe_handle, SetIsOverridingUserAgent).Times(0);

  auto* observer =
      RequestDesktopSiteWebContentsObserverAndroid::FromWebContents(
          web_contents());
  observer->DidStartNavigation(&subframe_handle);

  // No UKM entry recorded for the subframe.
  ExpectNoUkmEntry();
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       CommandLineSwitchAlwaysRequestsDesktopSite) {
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kRequestDesktopSites);

  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       WindowSettingEnabled_NarrowWindowDowngradesDesktopMode) {
  if (base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }

  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  SetWindowSettingEnabled(true);

  // Narrow window width (less than kAndroidMinimumTabletWidthDp = 600 dp).
  SetWebContentsWidth(content::kAndroidMinimumTabletWidthDp - 100);

  EXPECT_FALSE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       WindowSettingEnabled_WideWindowRetainsDesktopMode) {
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  SetWindowSettingEnabled(true);

  // Wide window width (greater than or equal to kAndroidMinimumTabletWidthDp).
  SetWebContentsWidth(content::kAndroidMinimumTabletWidthDp + 200);

  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       WindowSettingEnabled_SiteExceptionDoesNotDowngrade) {
  SetDefaultContentSetting(CONTENT_SETTING_BLOCK);
  SetContentSettingForUrl(GURL(kDesktopUrl), CONTENT_SETTING_ALLOW);
  SetWindowSettingEnabled(true);

  // Narrow window width: window setting only applies to global settings,
  // so site-specific exception should not be downgraded.
  SetWebContentsWidth(content::kAndroidMinimumTabletWidthDp - 100);

  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kDesktopUrl)));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       WindowSettingEnabled_CommandLineSwitchDoesNotDowngrade) {
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  SetWindowSettingEnabled(true);

  // Narrow window width.
  SetWebContentsWidth(content::kAndroidMinimumTabletWidthDp - 100);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(
      switches::kRequestDesktopSites);

  // Command-line flag takes precedence over window setting downgrade.
  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
}

TEST_F(RequestDesktopSiteWebContentsObserverAndroidTest,
       WindowSettingDisabled_NarrowWindowRetainsDesktopMode) {
  SetDefaultContentSetting(CONTENT_SETTING_ALLOW);
  SetWindowSettingEnabled(false);

  // Narrow window width.
  SetWebContentsWidth(content::kAndroidMinimumTabletWidthDp - 100);

  // Window setting is disabled, so desktop mode is retained.
  EXPECT_TRUE(CommitRendererInitiatedNavigation(GURL(kTestUrl)));
}

}  // namespace
