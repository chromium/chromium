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
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
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
  std::u16string GetDSEName() override {
    if (dse_origin_.host().find("google") != std::string::npos)
      return u"Google";

    return u"Example";
  }

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
    ClearContentSettings(ContentSettingsType::NOTIFICATIONS);

    auto test_delegate = std::make_unique<TestSearchEngineDelegate>();
    test_delegate_ = test_delegate.get();
    GetService()->SetSearchEngineDelegateForTest(std::move(test_delegate));
    ReinitializeService(true /* clear_pref */);
  }

  void TearDown() override {
    test_delegate_ = nullptr;

    // Because notification channel settings aren't tied to the profile, they
    // will persist across tests. We need to make sure they're reset here.
    ClearContentSettings(ContentSettingsType::NOTIFICATIONS);

    profile_.reset();
  }

  void ClearContentSettings(ContentSettingsType type) {
    SetContentSetting(kGoogleURL, type, CONTENT_SETTING_DEFAULT);
  }

  TestingProfile* profile() { return profile_.get(); }

  TestSearchEngineDelegate* test_delegate() { return test_delegate_; }

  SearchPermissionsService* GetService() {
    return SearchPermissionsService::Factory::GetForBrowserContext(profile());
  }

  void SetContentSetting(const std::string& origin_string,
                         ContentSettingsType type,
                         ContentSetting setting) {
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
    hcsm->SetContentSettingDefaultScope(url, url, type,
                                        CONTENT_SETTING_DEFAULT);
    hcsm->SetContentSettingDefaultScope(url, url, type, setting);
  }

  ContentSetting GetContentSetting(const std::string& origin_string,
                                   ContentSettingsType type) {
    GURL url(origin_string);
    return HostContentSettingsMapFactory::GetForProfile(profile())
        ->GetContentSetting(url, url, type);
  }

  // Simulates the initialization that happens when recreating the service. If
  // |clear_pref| is true, then it simulates the first time the service is ever
  // created.
  void ReinitializeService(bool clear_pref) {
    if (clear_pref) {
      profile()->GetPrefs()->ClearPref(prefs::kDSEPermissionsSettings);
      profile()->GetPrefs()->ClearPref(prefs::kDSEWasDisabledByPolicy);
    }

    GetService()->InitializeSettingsIfNeeded();
  }

  void SetDSEPref(ContentSetting setting) {
    GetService()->SetDSEPrefForTesting(setting, setting);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

  // This is owned by the SearchPermissionsService which is owned by the
  // profile.
  raw_ptr<TestSearchEngineDelegate> test_delegate_;
};

// As soon as the SearchPermissionsService is initialized, the DSE permissions
// are reverted.
TEST_F(SearchPermissionsServiceTest, DSEPermissionsAreReverted) {
  constexpr struct {
    ContentSetting stored_pref_setting;
    ContentSetting current_setting;
    ContentSetting expected_setting_after_autogrant_reverted;
  } kTests[] = {
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW, CONTENT_SETTING_ASK},
  };

  for (const auto& test : kTests) {
    for (const auto type : {ContentSettingsType::NOTIFICATIONS,
                            ContentSettingsType::GEOLOCATION}) {
      ClearContentSettings(type);
      SetDSEPref(test.stored_pref_setting);
      SetContentSetting(kGoogleURL, type, test.current_setting);

      // Initialize DSE and verify the expected setting.
      ReinitializeService(false /* clear_pref */);
      EXPECT_EQ(test.expected_setting_after_autogrant_reverted,
                GetContentSetting(kGoogleURL, type));
    }
  }
}

// Tests permission revert in the scenario in which the permission is disabled
// by default but the DSE origin is allowed.
TEST_F(SearchPermissionsServiceTest, PermissionDisabledByDefault) {
  constexpr struct {
    ContentSetting stored_pref_setting;
    ContentSetting expected_setting;
  } kTests[] = {
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK},
  };
  HostContentSettingsMap* hcsm =
      HostContentSettingsMapFactory::GetForProfile(profile());

  for (const auto& test : kTests) {
    for (const auto type : {ContentSettingsType::GEOLOCATION,
                            ContentSettingsType::NOTIFICATIONS}) {
      ClearContentSettings(type);
      hcsm->SetDefaultContentSetting(type, CONTENT_SETTING_BLOCK);
      SetContentSetting(kGoogleURL, type, CONTENT_SETTING_ALLOW);
      SetDSEPref(test.stored_pref_setting);

      EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(kGoogleURL, type));

      // After revert the DSE origin should now have the expected setting.
      ReinitializeService(false /* clear_pref */);
      EXPECT_EQ(test.expected_setting, GetContentSetting(kGoogleURL, type));
    }
  }
}

// Test that the appropriate UMA metrics have been recorded when the DSE is
// disabled.
TEST_F(SearchPermissionsServiceTest,
       MetricsAndPrefsAreRecordedWhenAutoDSEPermissionReverted) {
  constexpr struct {
    ContentSetting initial_setting;
    ContentSetting updated_setting;
    permissions::AutoDSEPermissionRevertTransition expected_transition;
  } kTests[] = {
      {CONTENT_SETTING_ASK, CONTENT_SETTING_ALLOW,
       permissions::AutoDSEPermissionRevertTransition::NO_DECISION_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_ALLOW,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_ALLOW,
       permissions::AutoDSEPermissionRevertTransition::CONFLICT_ASK},
      {CONTENT_SETTING_ASK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ASK},
      {CONTENT_SETTING_ALLOW, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_ALLOW},
      {CONTENT_SETTING_BLOCK, CONTENT_SETTING_BLOCK,
       permissions::AutoDSEPermissionRevertTransition::PRESERVE_BLOCK_BLOCK},
  };

  for (const auto& test : kTests) {
    for (const auto& type : {ContentSettingsType::NOTIFICATIONS,
                             ContentSettingsType::GEOLOCATION}) {
      // Notifications can not be set to ASK on Android as notification channels
      // explicitly rely on the state being only BLOCK/ALLOW/DEFAULT.
      if (test.initial_setting == CONTENT_SETTING_ASK &&
          type == ContentSettingsType::NOTIFICATIONS) {
        continue;
      }

      // Simulate an initial autogranted permission state.
      ClearContentSettings(type);
      SetDSEPref(test.initial_setting);
      SetContentSetting(kGoogleURL, type, test.updated_setting);

      // Initialize the service which should revert the autogranted permissions.
      {
        base::HistogramTester histograms;
        ReinitializeService(false /* clear_pref */);

        // Test that the expected samples are recorded in histograms.
        for (auto sample = static_cast<int>(
                 permissions::AutoDSEPermissionRevertTransition::
                     NO_DECISION_ASK);
             sample <
             static_cast<int>(
                 permissions::AutoDSEPermissionRevertTransition::kMaxValue);
             ++sample) {
          std::string histogram =
              "Permissions.DSE.AutoPermissionRevertTransition.";
          histogram += type == ContentSettingsType::NOTIFICATIONS
                           ? "Notifications"
                           : "Geolocation";
          histograms.ExpectBucketCount(
              histogram, sample,
              static_cast<int>(test.expected_transition) == sample ? 1 : 0);
        }
      }
    }
  }
}

// Records DSE origin settings whenever the service is initialized.
TEST_F(SearchPermissionsServiceTest, DSEEffectiveSettingMetric) {
  base::HistogramTester histograms;
  ClearContentSettings(ContentSettingsType::NOTIFICATIONS);
  ClearContentSettings(ContentSettingsType::GEOLOCATION);

  ReinitializeService(true /* clear_pref */);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_ASK, 1);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Geolocation",
                               CONTENT_SETTING_ASK, 1);

  SetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_ALLOW);

  ReinitializeService(false /* clear_pref */);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_ASK, 1);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Notifications",
                               CONTENT_SETTING_BLOCK, 1);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Geolocation",
                               CONTENT_SETTING_ASK, 1);
  histograms.ExpectBucketCount("Permissions.DSE.EffectiveSetting.Geolocation",
                               CONTENT_SETTING_ALLOW, 1);
}
