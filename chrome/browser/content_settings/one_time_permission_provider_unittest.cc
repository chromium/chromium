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
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/test/content_settings_test_utils.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_context_base.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

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
    content_settings::ContentSettingConstraints constraints;
    constraints.set_session_model(
        content_settings::mojom::SessionModel::ONE_TIME);
    return constraints;
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
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

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
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

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
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW), {},
      content_settings::PartitionKey::GetDefaultForTesting());

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
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->SetWebsiteSetting(
      other_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->OnAllTabsInBackgroundTimerExpired(
      url::Origin::Create(primary_url),
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kTimeout);

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
      base::Value(CONTENT_SETTING_ALLOW), one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_MIC, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

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
      base::Value(CONTENT_SETTING_ALLOW), one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_MIC, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->OnAllTabsInBackgroundTimerExpired(
      url::Origin::Create(primary_url),
      OneTimePermissionsTrackerObserver::BackgroundExpiryType::kTimeout);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_CAMERA, false));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, secondary_url,
                ContentSettingsType::MEDIASTREAM_MIC, false));
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
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(), one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

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

TEST_F(OneTimePermissionProviderTest, VerifyPermissionObserversNotified) {
  base::HistogramTester histograms;
  content_settings::MockObserver mock_observer;
  one_time_permission_provider_->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_, _, ContentSettingsType::GEOLOCATION));

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::GEOLOCATION, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());
}

class OneTimePermissionProviderExpiryTest
    : public OneTimePermissionProviderTest,
      public ::testing::WithParamInterface<bool> {
 public:
  OneTimePermissionProviderExpiryTest() {
    if (GetParam()) {
      feature_list_.InitWithFeatures(
          {content_settings::features::kActiveContentSettingExpiry}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {content_settings::features::kActiveContentSettingExpiry});
    }
  }
  OneTimePermissionProviderExpiryTest(
      const OneTimePermissionProviderExpiryTest&) = delete;
  OneTimePermissionProviderExpiryTest& operator=(
      const OneTimePermissionProviderExpiryTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(OneTimePermissionProviderTest, SuspendExpiresAllGrants) {
  base::HistogramTester histograms;
  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_CAMERA,
      base::Value(CONTENT_SETTING_ALLOW), one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, ContentSettingsPattern::Wildcard(),
      ContentSettingsType::MEDIASTREAM_MIC, base::Value(CONTENT_SETTING_ALLOW),
      one_time_constraints(),
      content_settings::PartitionKey::GetDefaultForTesting());

  one_time_permission_provider_->OnSuspend();

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
          permissions::OneTimePermissionEvent::EXPIRED_ON_SUSPEND),
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
          permissions::OneTimePermissionEvent::EXPIRED_ON_SUSPEND),
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OneTimePermissionProviderExpiryTest,
                         testing::Bool());

TEST_P(OneTimePermissionProviderExpiryTest, RenewContentSetting_Noop) {
  GURL primary_url("https://example.com/");
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("https://[*.]example.com");

  ContentSettingConstraints constraints = one_time_constraints();
  if (GetParam()) {
    constraints.set_lifetime(permissions::kOneTimePermissionMaximumLifetime);
  } else {
    constraints.set_lifetime(base::Days(2));
  }

  one_time_permission_provider_->SetWebsiteSetting(
      primary_pattern, primary_pattern, ContentSettingsType::GEOLOCATION,
      base::Value(CONTENT_SETTING_ALLOW), constraints,
      content_settings::PartitionKey::GetDefaultForTesting());

  RuleMetaData metadata;
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, primary_url,
                ContentSettingsType::GEOLOCATION,
                /*include_incognito=*/false, &metadata));

  if (GetParam()) {
    EXPECT_EQ(metadata.lifetime(),
              permissions::kOneTimePermissionMaximumLifetime);
    EXPECT_NE(metadata.expiration(), base::Time());
  }

  // The lifetime given by `constraints` is ignored.
  base::Time original_expiration = metadata.expiration();

  EXPECT_FALSE(one_time_permission_provider_->RenewContentSetting(
      primary_url, primary_url, ContentSettingsType::GEOLOCATION, std::nullopt,
      content_settings::PartitionKey::GetDefaultForTesting()));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            TestUtils::GetContentSetting(
                one_time_permission_provider_.get(), primary_url, primary_url,
                ContentSettingsType::GEOLOCATION,
                /*include_incognito=*/false, &metadata));

  if (GetParam()) {
    EXPECT_EQ(metadata.lifetime(),
              permissions::kOneTimePermissionMaximumLifetime);
    EXPECT_EQ(original_expiration, metadata.expiration());
  }
}
}  // namespace content_settings
