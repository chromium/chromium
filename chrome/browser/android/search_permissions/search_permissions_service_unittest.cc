// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/permission_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/resolvers/permission_prompt_options.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

const char kGoogleURL[] = "https://www.google.com/";

// The test delegate is used to mock out search-engine related functionality.
class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  TestSearchEngineDelegate()
      : dse_origin_(url::Origin::Create(GURL(kGoogleURL))) {}

  url::Origin GetDSEOrigin() override { return dse_origin_; }

  void set_dse_origin(const std::string& dse_origin) {
    dse_origin_ = url::Origin::Create(GURL(dse_origin));
  }

 private:
  url::Origin dse_origin_;
};

}  // namespace

class SearchPermissionsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    // Because notification channel settings aren't tied to the profile,
    // they will persist across tests. We need to make sure they're clean
    // here.
    ClearPermissionSettings(ContentSettingsType::NOTIFICATIONS);

    auto test_delegate = std::make_unique<TestSearchEngineDelegate>();
    test_delegate_ = test_delegate.get();
    GetService()->SetSearchEngineDelegateForTest(std::move(test_delegate));
  }

  void TearDown() override {
    test_delegate_ = nullptr;

    // Because notification channel settings aren't tied to the profile, they
    // will persist across tests. We need to make sure they're reset here.
    ClearPermissionSettings(ContentSettingsType::NOTIFICATIONS);

    profile_.reset();
  }

  void ClearPermissionSettings(ContentSettingsType type) {
    SetPermissionSetting(kGoogleURL, type, std::nullopt);
  }

  TestingProfile* profile() { return profile_.get(); }

  TestSearchEngineDelegate* test_delegate() { return test_delegate_; }

  SearchPermissionsService* GetService() {
    return SearchPermissionsService::Factory::GetForBrowserContext(profile());
  }

  void SetPermissionSetting(const std::string& origin_string,
                            ContentSettingsType type,
                            std::optional<PermissionSetting> setting) {
    GURL url(origin_string);
    HostContentSettingsMap* hcsm =
        HostContentSettingsMapFactory::GetForProfile(profile());
    // Clear a setting before setting it. This is needed because in general
    // notifications settings can't be changed from ALLOW<->BLOCK on Android O+.
    // We need to change the setting from ALLOW->BLOCK in one case, where the
    // previous DSE had permission blocked but the new DSE we're changing to has
    // permission allowed. Thus this works around that restriction.
    // WARNING: This is a special case and in general notification settings
    // should never be changed between ALLOW<->BLOCK on Android. Do not copy
    // this code. Check with the notifications team if you need to do something
    // like this.
    hcsm->SetPermissionSettingDefaultScope(url, url, type, std::nullopt);
    if (setting) {
      hcsm->SetPermissionSettingDefaultScope(url, url, type, *setting);
    }
  }

  // Simulates the initialization that happens when recreating the service. If
  // |clear_pref| is true, then it simulates the first time the service is ever
  // created.
  void ReinitializeService() {
    GetService()->RecordEffectiveDSEOriginPermissions(profile_.get());
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

  // This is owned by the SearchPermissionsService which is owned by the
  // profile.
  raw_ptr<TestSearchEngineDelegate> test_delegate_;
};

// Records DSE origin settings whenever the service is initialized.
TEST_F(SearchPermissionsServiceTest, IsDseOrigin) {
  EXPECT_TRUE(GetService()->IsDseOrigin(url::Origin::Create(GURL(kGoogleURL))));
  EXPECT_FALSE(GetService()->IsDseOrigin(
      url::Origin::Create(GURL("https://example.com"))));
}

// Records DSE origin settings whenever the service is initialized.
TEST_F(SearchPermissionsServiceTest, DSEEffectiveSettingMetric) {
  base::HistogramTester histograms;
  ClearPermissionSettings(ContentSettingsType::NOTIFICATIONS);

  ReinitializeService();
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_ASK, 1);

  SetPermissionSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                       CONTENT_SETTING_BLOCK);

  ReinitializeService();
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_ASK, 1);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_BLOCK, 1);
}

TEST_F(SearchPermissionsServiceTest,
       DSEEffectiveSettingMetricApproximateGeolocation) {
  ClearPermissionSettings(ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
  {
    base::HistogramTester histograms;

    ReinitializeService();
    histograms.ExpectUniqueSample(
        "Permissions.DSE.EffectiveSetting.Geolocation", CONTENT_SETTING_ASK, 1);
  }

  struct {
    GeolocationSetting setting;
    ContentSetting expected_setting;
    std::optional<GeolocationAccuracy> expected_accuracy;
  } testCases[]{
      {
          {.approximate = PermissionOption::kDenied,
           .precise = PermissionOption::kDenied},
          ContentSetting::CONTENT_SETTING_BLOCK,
      },
      {
          {.approximate = PermissionOption::kAllowed,
           .precise = PermissionOption::kDenied},
          ContentSetting::CONTENT_SETTING_ALLOW,
          GeolocationAccuracy::kApproximate,
      },
      {
          {.approximate = PermissionOption::kAllowed,
           .precise = PermissionOption::kAsk},
          ContentSetting::CONTENT_SETTING_ALLOW,
          GeolocationAccuracy::kApproximate,
      },
      {
          {.approximate = PermissionOption::kAllowed,
           .precise = PermissionOption::kAllowed},
          ContentSetting::CONTENT_SETTING_ALLOW,
          GeolocationAccuracy::kPrecise,
      },
  };

  for (const auto& testCase : testCases) {
    base::HistogramTester histograms;
    SetPermissionSetting(kGoogleURL,
                         ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
                         testCase.setting);

    ReinitializeService();
    histograms.ExpectUniqueSample(
        "Permissions.DSE.EffectiveSetting.Geolocation",
        testCase.expected_setting, 1);
    if (testCase.expected_accuracy) {
      histograms.ExpectUniqueSample(
          "Permissions.DSE.EffectiveSetting.Geolocation.Accuracy",
          *testCase.expected_accuracy, 1);
    }
  }
}
