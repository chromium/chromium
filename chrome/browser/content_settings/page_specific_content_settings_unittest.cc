// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/browser/page_specific_content_settings.h"

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace content_settings {

class PageSpecificContentSettingsTest : public ChromeRenderViewHostTestHarness {
 public:
  PageSpecificContentSettingsTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    PageSpecificContentSettings::CreateForWebContents(
        web_contents(),
        std::make_unique<PageSpecificContentSettingsDelegate>(web_contents()));

    permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
        web_contents());
  }
};

TEST_F(PageSpecificContentSettingsTest, HistogramTest) {
  base::HistogramTester histograms;
  const GURL test_url("https://test.com/");
  const char kGeolocationHistogramName[] =
      "Permissions.Usage.ElapsedTimeSinceGrant.Geolocation";
  const char kMicrophoneHistogramName[] =
      "Permissions.Usage.ElapsedTimeSinceGrant.AudioCapture";
  const char kCameraHistogramName[] =
      "Permissions.Usage.ElapsedTimeSinceGrant.VideoCapture";
  NavigateAndCommit(test_url);
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(test_url, test_url,
                                     ContentSettingsType::GEOLOCATION,
                                     ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(test_url, test_url,
                                     ContentSettingsType::MEDIASTREAM_MIC,
                                     ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingDefaultScope(test_url, test_url,
                                     ContentSettingsType::MEDIASTREAM_CAMERA,
                                     ContentSetting::CONTENT_SETTING_ALLOW);

  task_environment()->FastForwardBy(base::Seconds(1));
  PageSpecificContentSettings* content_settings =
      PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  histograms.ExpectTotalCount(kGeolocationHistogramName, 0);
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  histograms.ExpectTotalCount(kGeolocationHistogramName, 1);
  EXPECT_THAT(histograms.GetAllSamples(kGeolocationHistogramName),
              testing::ElementsAre(base::Bucket(1, 1)));
  content_settings->OnContentAllowed(ContentSettingsType::GEOLOCATION);
  // Count should stay same even after multiple usage of permission
  histograms.ExpectTotalCount(kGeolocationHistogramName, 1);

  content_settings->OnContentAllowed(ContentSettingsType::NOTIFICATIONS);
  // Count should stay same even if a different permission is used
  histograms.ExpectTotalCount(kGeolocationHistogramName, 1);

  PageSpecificContentSettings::MicrophoneCameraState microphone_accessed{
      PageSpecificContentSettings::kMicrophoneAccessed,
      PageSpecificContentSettings::kCameraAccessed,
      PageSpecificContentSettings::kCameraBlocked,
  };

  histograms.ExpectTotalCount(kMicrophoneHistogramName, 0);
  content_settings->OnMediaStreamPermissionSet(test_url, microphone_accessed);
  histograms.ExpectTotalCount(kMicrophoneHistogramName, 1);
  EXPECT_THAT(histograms.GetAllSamples(kMicrophoneHistogramName),
              testing::ElementsAre(base::Bucket(1, 1)));
  const PageSpecificContentSettings::MicrophoneCameraState mic_camera_accessed{
      PageSpecificContentSettings::kMicrophoneAccessed,
      PageSpecificContentSettings::kCameraAccessed,
  };

  histograms.ExpectTotalCount(kCameraHistogramName, 0);
  content_settings->OnMediaStreamPermissionSet(test_url, mic_camera_accessed);
  histograms.ExpectTotalCount(kCameraHistogramName, 1);
  EXPECT_THAT(histograms.GetAllSamples(kCameraHistogramName),
              testing::ElementsAre(base::Bucket(1, 1)));
  content_settings->OnMediaStreamPermissionSet(test_url, mic_camera_accessed);
  // Count should stay same even after multiple usage of permission
  histograms.ExpectTotalCount(kMicrophoneHistogramName, 1);
  histograms.ExpectTotalCount(kCameraHistogramName, 1);

  // Count should stay same even if a different permission is used
  histograms.ExpectTotalCount(kMicrophoneHistogramName, 1);
  histograms.ExpectTotalCount(kCameraHistogramName, 1);
}

}  // namespace content_settings
