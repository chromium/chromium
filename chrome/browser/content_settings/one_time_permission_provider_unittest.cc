// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/one_time_permission_provider.h"
#include <memory>
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content_settings {

class OneTimePermissionProviderTest : public testing::Test {
 public:
  OneTimePermissionProviderTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Ensure all content settings are initialized.
    ContentSettingsRegistry::GetInstance();
    feature_list_.InitAndEnableFeature(
        permissions::features::kOneTimePermission);
  }

  void SetUp() override {
    tracker_ = std::make_unique<OneTimePermissionsTracker>();
    one_time_permission_provider_ =
        std::make_unique<OneTimePermissionProvider>(tracker_.get());
  }

  void TearDown() override {
    one_time_permission_provider_->ShutdownOnUIThread();
    one_time_permission_provider_
        .reset();  // required because destructor may destroy tracker_ first
  }

  void FastForwardTime(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 protected:
  content_settings::ContentSettingConstraints one_time_constraints() {
    return content_settings::ContentSettingConstraints{
        .session_model = content_settings::SessionModel::OneTime};
  }

  GURL primary_url = GURL("http://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromURLNoWildcard(primary_url);

  GURL other_url = GURL("http://other.com");
  ContentSettingsPattern other_pattern =
      ContentSettingsPattern::FromURLNoWildcard(other_url);

  GURL secondary_url = GURL("*");

  std::unique_ptr<OneTimePermissionProvider> one_time_permission_provider_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<OneTimePermissionsTracker> tracker_;
};

TEST_F(OneTimePermissionProviderTest, SetAndGetContentSetting) {
  base::HistogramTester histograms;
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      1);
}

TEST_F(OneTimePermissionProviderTest,
       SetAndGetContentSettingWithoutOneTimeCapabilityDoesNotAllow) {
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::NOTIFICATIONS, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::NOTIFICATIONS, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::NOTIFICATIONS, false));
}

TEST_F(OneTimePermissionProviderTest,
       SetAndGetContentSettingWithoutOneTimeConstraintsDoeNotAllow) {
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW), {});

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));
}

TEST_F(OneTimePermissionProviderTest,
       AllTabsInBackgroundExpiryRevokesGeolocation) {
  base::HistogramTester histograms;
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  one_time_permission_provider_->SetWebsiteSetting(
      other_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  one_time_permission_provider_->OnAllTabsInBackgroundTimerExpired(
      url::Origin::Create(primary_url));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), other_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  // We granted to two distinct origins
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      2);

  // Only one origin was in the background and should have been expired
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND),
      1);
}

TEST_F(OneTimePermissionProviderTest, CaptureExpiryRevokesPermissions) {
  base::HistogramTester histograms;
  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::Value(CONTENT_SETTING_ALLOW), one_time_constraints());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_MIC, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  one_time_permission_provider_->OnCapturingVideoExpired(
      url::Origin::Create(primary_url));
  one_time_permission_provider_->OnCapturingAudioExpired(
      url::Origin::Create(primary_url));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_CAMERA, false));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), other_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_MIC, false));

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      2);
  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      2);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_CAMERA),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND),
      1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      1);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::MEDIASTREAM_MIC),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND),
      1);
}

TEST_F(OneTimePermissionProviderTest,
       AllTabsInBackgroundExpiryDoesNotRevokeCamMic) {
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_CAMERA, false));
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_MIC, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::Value(CONTENT_SETTING_ALLOW), one_time_constraints());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_MIC, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  one_time_permission_provider_->OnAllTabsInBackgroundTimerExpired(
      url::Origin::Create(primary_url));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_CAMERA, false));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_MIC, false));
}

TEST_F(OneTimePermissionProviderTest, EnsureOneDayExpiry) {
  base::HistogramTester histograms;
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  FastForwardTime(base::Days(1));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  // Only a grant sample should be recorded. 1-day expiry can be computed from
  // #grants - #other buckets
  histograms.ExpectUniqueSample(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      1);
}

TEST_F(OneTimePermissionProviderTest, ManualRevocationUmaTest) {
  base::HistogramTester histograms;
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::GEOLOCATION, false));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ASK), {});

  histograms.ExpectTotalCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      2);
  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::GRANTED_ONE_TIME),
      1);

  histograms.ExpectBucketCount(
      permissions::PermissionUmaUtil::GetOneTimePermissionEventHistogram(
          ContentSettingsType::GEOLOCATION),
      static_cast<base::HistogramBase::Sample>(
          permissions::OneTimePermissionEvent::REVOKED_MANUALLY),
      1);
}
}  // namespace content_settings
