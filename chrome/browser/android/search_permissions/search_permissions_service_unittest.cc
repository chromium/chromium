// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_result.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

const char kDSESettingKeyDeprecated[] = "dse_setting";

const char kGoogleURL[] = "https://www.google.com";
const char kGoogleAusURL[] = "https://www.google.com.au";
const char kGoogleHTTPURL[] = "http://www.google.com";
const char kExampleURL[] = "https://www.example.com";

url::Origin ToOrigin(const char* url) {
  return url::Origin::Create(GURL(url));
}

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

  void SetDSEChangedCallback(base::RepeatingClosure callback) override {
    dse_changed_callback_ = std::move(callback);
  }

  void ChangeDSEOrigin(const std::string& dse_origin) {
    set_dse_origin(dse_origin);
    dse_changed_callback_.Run();
  }

  void set_dse_origin(const std::string& dse_origin) {
    dse_origin_ = url::Origin::Create(GURL(dse_origin));
  }

 private:
  url::Origin dse_origin_;
  base::RepeatingClosure dse_changed_callback_;
};

}  // namespace

class SearchPermissionsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_.reset(new TestingProfile);

    ClearNotificationsChannels();

    auto test_delegate = std::make_unique<TestSearchEngineDelegate>();
    test_delegate_ = test_delegate.get();
    GetService()->SetSearchEngineDelegateForTest(std::move(test_delegate));
    ReinitializeService(true /* clear_pref */);
  }

  void TearDown() override {
    test_delegate_ = nullptr;

    ClearNotificationsChannels();

    profile_.reset();
  }

  void ClearNotificationsChannels() {
    // Because notification channel settings aren't tied to the profile, they
    // will persist across tests. We need to make sure they're reset here.
    SetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                      CONTENT_SETTING_DEFAULT);
    SetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS,
                      CONTENT_SETTING_DEFAULT);
    SetContentSetting(kGoogleHTTPURL, ContentSettingsType::NOTIFICATIONS,
                      CONTENT_SETTING_DEFAULT);
    SetContentSetting(kExampleURL, ContentSettingsType::NOTIFICATIONS,
                      CONTENT_SETTING_DEFAULT);
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

    GetService()->OnDSEChanged();
  }

  // Simulate setting the old preference to test migration.
  void SetOldPreference(bool setting) {
    base::DictionaryValue dict;
    dict.SetBoolean(kDSESettingKeyDeprecated, setting);
    profile()->GetPrefs()->Set(prefs::kDSEGeolocationSettingDeprecated, dict);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::BrowserTaskEnvironment task_environment_;

  // This is owned by the SearchPermissionsService which is owned by the
  // profile.
  TestSearchEngineDelegate* test_delegate_;
};

TEST_F(SearchPermissionsServiceTest, Initialization) {
  for (ContentSettingsType type :
       {ContentSettingsType::GEOLOCATION, ContentSettingsType::NOTIFICATIONS}) {
    // DSE setting initialized to true if the content setting is ALLOW.
    test_delegate()->ChangeDSEOrigin(kGoogleURL);
    SetContentSetting(kGoogleURL, type, CONTENT_SETTING_ALLOW);
    ReinitializeService(true /* clear_pref */);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(kGoogleURL, type));
    // Check that the correct value is restored when changing the DSE.
    test_delegate()->ChangeDSEOrigin(kExampleURL);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(kGoogleURL, type));

    // DSE setting initialized to true if the content setting is ASK.
    test_delegate()->ChangeDSEOrigin(kGoogleURL);
    SetContentSetting(kGoogleURL, type, CONTENT_SETTING_DEFAULT);
    EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleURL, type));
    ReinitializeService(true /* clear_pref */);
    EXPECT_EQ(CONTENT_SETTING_ALLOW, GetContentSetting(kGoogleURL, type));
    test_delegate()->ChangeDSEOrigin(kExampleURL);
    EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleURL, type));

    // DSE setting initialized to false if the content setting is BLOCK.
    test_delegate()->ChangeDSEOrigin(kGoogleURL);
    SetContentSetting(kGoogleURL, type, CONTENT_SETTING_BLOCK);
    ReinitializeService(true /* clear_pref */);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(kGoogleURL, type));
    test_delegate()->ChangeDSEOrigin(kExampleURL);
    EXPECT_EQ(CONTENT_SETTING_BLOCK, GetContentSetting(kGoogleURL, type));

    // Nothing happens if the pref is already set when the service is
    // initialized.
    test_delegate()->ChangeDSEOrigin(kGoogleURL);
    SetContentSetting(kGoogleURL, type, CONTENT_SETTING_DEFAULT);
    ReinitializeService(false /* clear_pref */);
    EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleURL, type));
  }

  // Check that the geolocation disclosure is reset.
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  ReinitializeService(true /* clear_pref */);
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  ReinitializeService(false /* clear_pref */);
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
}

TEST_F(SearchPermissionsServiceTest, InitializationInconsistent) {
  // Test initialization when the stored pref has become inconsistent with the
  // current DSE.
  test_delegate()->ChangeDSEOrigin(kGoogleURL);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));

  test_delegate()->set_dse_origin(kGoogleAusURL);
  ReinitializeService(false /* clear_pref */);

  // The settings for the previous DSE should be restored when the service is
  // started.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));

  // The settings should be transferred to the new DSE.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(SearchPermissionsServiceTest, Incognito) {
  // Service isn't constructed for Incognito profile.
  Profile* incognito_profile = profile()->GetPrimaryOTRProfile();
  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(
          incognito_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(SearchPermissionsServiceTest, NonPrimaryOffTheRecord) {
  // Service isn't constructed for non-primary OTR profiles.
  Profile* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID("Test::SearchPermissions"));
  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(SearchPermissionsServiceTest, Migration) {
  // When location was previously allowed for the DSE, it should be carried
  // over.
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  SetOldPreference(true /* setting */);
  ReinitializeService(true /* clear_pref */);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  // If location was previously blocked for the DSE, it should be carried over.
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_DEFAULT);
  SetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_DEFAULT);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  SetOldPreference(false /* setting */);
  ReinitializeService(true /* clear_pref */);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  // Notifications should be unaffected.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  // Changing DSE should cause the setting to go back to ASK for Google.
  test_delegate()->ChangeDSEOrigin(kExampleURL);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  // Check that migrating the pref causes it to be deleted.
  SetOldPreference(false /* setting */);
  ReinitializeService(true /* clear_pref */);
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(
      prefs::kDSEGeolocationSettingDeprecated));

  // Check that the disclosure won't be reset if we migrate a pref.
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  SetOldPreference(false /* setting */);
  ReinitializeService(true /* clear_pref */);
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
}

TEST_F(SearchPermissionsServiceTest, IsPermissionControlledByDSE) {
  // True for origin that matches the CCTLD and meets all requirements.
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_TRUE(GetService()->IsPermissionControlledByDSE(
      ContentSettingsType::NOTIFICATIONS, ToOrigin(kGoogleURL)));

  // False for different origin.
  EXPECT_FALSE(GetService()->IsPermissionControlledByDSE(
      ContentSettingsType::GEOLOCATION, ToOrigin(kGoogleAusURL)));

  // False for http origin.
  test_delegate()->ChangeDSEOrigin(kGoogleHTTPURL);
  EXPECT_FALSE(GetService()->IsPermissionControlledByDSE(
      ContentSettingsType::NOTIFICATIONS, ToOrigin(kGoogleHTTPURL)));

  // True even for non-Google search engines.
  test_delegate()->ChangeDSEOrigin(kExampleURL);
  EXPECT_TRUE(GetService()->IsPermissionControlledByDSE(
      ContentSettingsType::GEOLOCATION, ToOrigin(kExampleURL)));

  // False for permissions not controlled by the DSE.
  EXPECT_FALSE(GetService()->IsPermissionControlledByDSE(
      ContentSettingsType::COOKIES, ToOrigin(kExampleURL)));
}

TEST_F(SearchPermissionsServiceTest, DSEChanges) {
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));

  // Change to google.com.au. Settings for google.com should revert and settings
  // for google.com.au should be set to allow.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // Set the content setting for google.com to block for notifications. When we
  // change back to google.com, the setting should still be blocked.
  SetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // Now set the notification setting for google.com.au to ALLOW. When we change
  // to google.com.au notifications should still be blocked. The google.com
  // notifications setting should remain blocked.
  SetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // Now changing back to google.com, the google.com.au notifications setting
  // should be reset to ask (we reset it because of the conflict previously).
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // The google.com setting was block before it became the DSE and it remains
  // block. Now, if it's toggled to allow while it's still the DSE, we should
  // reset it back to ask once it is no longer the DSE.
  SetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(SearchPermissionsServiceTest, DSEChangesWithEnterprisePolicy) {
  test_delegate()->ChangeDSEOrigin(kGoogleURL);

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  // Set a policy value for the geolocation setting.
  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  // Change DSE.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);

  // The enterprise policy should still be in effect.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));

  // When the enterprise policy goes away, the setting should revert to ALLOW
  // for the current DSE and ASK for the previous one.
  prefs->RemoveManagedPref(prefs::kManagedDefaultGeolocationSetting);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));

  // Simulate the user setting google.com to BLOCK.
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);

  // Put an ALLOW enterprise policy in place.
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_ALLOW));
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));

  // Now change the DSE back to google.com. The enterprise setting should still
  // be in effect so it should be ALLOW.
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  // Remove the enterprise policy. google.com should go back to blocked.
  // google.com.au should be ASK.
  prefs->RemoveManagedPref(prefs::kManagedDefaultGeolocationSetting);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
}

TEST_F(SearchPermissionsServiceTest, DSEChangesAndDisclosure) {
  test_delegate()->ChangeDSEOrigin(kGoogleURL);
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  // Change to google.com.au. The disclosure should not be reset.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));

  // Now set to a non-google search. The disclosure should be reset.
  test_delegate()->ChangeDSEOrigin(kExampleURL);
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());

  // Go back to google.com.au. The disclosure should again be reset.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
}

TEST_F(SearchPermissionsServiceTest, Embargo) {
  test_delegate()->ChangeDSEOrigin(kGoogleURL);

  // Place another origin under embargo.
  GURL google_aus_url(kGoogleAusURL);
  permissions::PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlockerFactory::GetForProfile(profile());
  auto_blocker->RecordDismissAndEmbargo(
      google_aus_url, ContentSettingsType::GEOLOCATION, false);
  auto_blocker->RecordDismissAndEmbargo(
      google_aus_url, ContentSettingsType::GEOLOCATION, false);
  auto_blocker->RecordDismissAndEmbargo(
      google_aus_url, ContentSettingsType::GEOLOCATION, false);
  permissions::PermissionResult result = auto_blocker->GetEmbargoResult(
      GURL(kGoogleAusURL), ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(result.source,
            permissions::PermissionStatusSource::MULTIPLE_DISMISSALS);
  EXPECT_EQ(result.content_setting, CONTENT_SETTING_BLOCK);

  // Now change the DSE to this origin and make sure the embargo is cleared.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  result = auto_blocker->GetEmbargoResult(GURL(kGoogleAusURL),
                                          ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(result.source, permissions::PermissionStatusSource::UNSPECIFIED);
  EXPECT_EQ(result.content_setting, CONTENT_SETTING_ASK);
}

TEST_F(SearchPermissionsServiceTest, DSEChangedButDisabled) {
  SetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);

  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // DSE disabled by enterprise policy
  test_delegate()->ChangeDSEOrigin(std::string());

  // The settings should return to their original value.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // Now disable enterprise policy. The settings will be BLOCK because we don't
  // know what the user's previous DSE setting was.
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(SearchPermissionsServiceTest, DSEInitializedButDisabled) {
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // Set the DSE origin without calling OnDSEChanged.
  test_delegate()->set_dse_origin(std::string());

  ReinitializeService(/*clear_pref=*/false);

  // Settings should revert back to default.
  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  // The pref shouldn't exist anymore.
  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kDSEPermissionsSettings));

  // Firing the DSE changed event now should not do anything.
  test_delegate()->ChangeDSEOrigin(std::string());

  EXPECT_EQ(CONTENT_SETTING_ASK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ASK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));

  EXPECT_FALSE(
      profile()->GetPrefs()->HasPrefPath(prefs::kDSEPermissionsSettings));

  // Re-enabling the DSE origin should set the permissions to BLOCK for safety,
  // except for notifications where the user had manually granted permission.
  SetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_ALLOW);

  test_delegate()->set_dse_origin(kGoogleAusURL);
  ReinitializeService(/*clear_pref=*/false);

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
}

TEST_F(SearchPermissionsServiceTest, ResetDSEPermission) {
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  SetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);
  SetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);

  GetService()->ResetDSEPermission(ContentSettingsType::GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_BLOCK,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));

  GetService()->ResetDSEPermission(ContentSettingsType::NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
}

TEST_F(SearchPermissionsServiceTest, ResetDSEPermissions) {
  test_delegate()->ChangeDSEOrigin(kGoogleAusURL);
  SetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);
  SetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS,
                    CONTENT_SETTING_BLOCK);
  SetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION,
                    CONTENT_SETTING_BLOCK);

  GetService()->ResetDSEPermissions();
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetContentSetting(kGoogleAusURL, ContentSettingsType::GEOLOCATION));
  EXPECT_EQ(
      CONTENT_SETTING_ALLOW,
      GetContentSetting(kGoogleAusURL, ContentSettingsType::NOTIFICATIONS));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            GetContentSetting(kGoogleURL, ContentSettingsType::GEOLOCATION));
}
