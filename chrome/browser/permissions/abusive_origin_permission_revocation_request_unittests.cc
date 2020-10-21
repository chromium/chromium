// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/abusive_origin_permission_revocation_request.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/permissions/abusive_origin_notifications_permission_revocation_config.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/browser_task_environment.h"

class AbusiveOriginPermissionRevocationRequestTest : public testing::Test {
 public:
  using Outcome = AbusiveOriginPermissionRevocationRequest::Outcome;
  using SiteReputation = CrowdDenyPreloadData::SiteReputation;

  AbusiveOriginPermissionRevocationRequestTest() = default;

  ~AbusiveOriginPermissionRevocationRequestTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();

    DCHECK(profile_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(profile_dir_.GetPath());
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    testing_profile_ = profile_builder.Build();

    fake_database_manager_ =
        base::MakeRefCounted<CrowdDenyFakeSafeBrowsingDatabaseManager>();
    safe_browsing_factory_ =
        std::make_unique<safe_browsing::TestSafeBrowsingServiceFactory>();
    safe_browsing_factory_->SetTestDatabaseManager(
        fake_database_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
        safe_browsing_factory_->CreateSafeBrowsingService());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);
    testing::Test::TearDown();
  }

  void AddToSafeBrowsingBlocklist(const GURL& url) {
    safe_browsing::ThreatMetadata test_metadata;
    test_metadata.api_permissions.emplace("NOTIFICATIONS");
    fake_database_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

  void ClearSafeBrowsingBlocklist() {
    fake_database_manager_->RemoveAllBlacklistedUrls();
  }

  void AddToPreloadDataBlocklist(
      const GURL& origin,
      chrome_browser_crowd_deny::
          SiteReputation_NotificationUserExperienceQuality reputation_type) {
    SiteReputation reputation;
    reputation.set_notification_ux_quality(reputation_type);
    testing_preload_data_.SetOriginReputation(url::Origin::Create(origin),
                                              std::move(reputation));
  }

  void QueryAndExpectDecisionForUrl(const GURL& origin,
                                    Outcome expected_result) {
    base::MockOnceCallback<void(Outcome)> mock_callback_receiver;
    permission_revocation_ =
        std::make_unique<AbusiveOriginPermissionRevocationRequest>(
            testing_profile_.get(), origin, mock_callback_receiver.Get());
    EXPECT_CALL(mock_callback_receiver, Run(expected_result));
    task_environment_.RunUntilIdle();
    permission_revocation_.reset();
  }

  void SetPermission(const GURL& origin, const ContentSetting value) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(testing_profile_.get());
    host_content_settings_map->SetContentSettingDefaultScope(
        origin, GURL(), ContentSettingsType::NOTIFICATIONS, std::string(),
        value);
  }

  void VerifyNotificationsPermission(const GURL& origin,
                                     const ContentSetting value) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(testing_profile_.get());

    ContentSetting result = host_content_settings_map->GetContentSetting(
        origin, GURL(), ContentSettingsType::NOTIFICATIONS, std::string());

    EXPECT_EQ(value, result);
  }

  TestingProfile* GetTestingProfile() { return testing_profile_.get(); }

 private:
  base::ScopedTempDir profile_dir_;
  content::BrowserTaskEnvironment task_environment_;
  testing::ScopedCrowdDenyPreloadDataOverride testing_preload_data_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<AbusiveOriginPermissionRevocationRequest>
      permission_revocation_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;

  DISALLOW_COPY_AND_ASSIGN(AbusiveOriginPermissionRevocationRequestTest);
};

TEST_F(AbusiveOriginPermissionRevocationRequestTest,
       PermissionRevocationFeatureDisabled) {
  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
}

TEST_F(AbusiveOriginPermissionRevocationRequestTest,
       OriginIsNotOnBlockingLists) {
  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAbusiveNotificationPermissionRevocation);

  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
}

TEST_F(AbusiveOriginPermissionRevocationRequestTest, SafeBrowsingTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAbusiveNotificationPermissionRevocation);

  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  // The origin is not on any blocking lists. Notifications permission is not
  // revoked.
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);

  AddToSafeBrowsingBlocklist(origin_to_revoke);
  // Origin is not on CrowdDeny blocking lists.
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      AbusiveOriginPermissionRevocationRequest::HasPreviouslyRevokedPermission(
          GetTestingProfile(), origin_to_revoke));

  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ASK);
  EXPECT_TRUE(
      AbusiveOriginPermissionRevocationRequest::HasPreviouslyRevokedPermission(
          GetTestingProfile(), origin_to_revoke));
}

TEST_F(AbusiveOriginPermissionRevocationRequestTest, PreloadDataTest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAbusiveNotificationPermissionRevocation);

  const GURL abusive_content_origin_to_revoke =
      GURL("https://abusive-content.com/");
  const GURL abusive_prompts_origin_to_revoke =
      GURL("https://abusive-prompts.com/");
  const GURL unsolicited_prompts_origin =
      GURL("https://unsolicited-prompts.com/");
  const GURL acceptable_origin = GURL("https://acceptable-origin.com/");
  const GURL unknown_origin = GURL("https://unknown-origin.com/");

  auto origins = {abusive_content_origin_to_revoke,
                  abusive_prompts_origin_to_revoke, unsolicited_prompts_origin,
                  acceptable_origin, unknown_origin};

  for (auto origin : origins)
    SetPermission(origin, CONTENT_SETTING_ALLOW);

  // The origins are not on any blocking lists.
  for (auto origin : origins)
    QueryAndExpectDecisionForUrl(origin, Outcome::PERMISSION_NOT_REVOKED);

  AddToPreloadDataBlocklist(abusive_content_origin_to_revoke,
                            SiteReputation::ABUSIVE_CONTENT);
  AddToPreloadDataBlocklist(abusive_prompts_origin_to_revoke,
                            SiteReputation::ABUSIVE_PROMPTS);
  AddToPreloadDataBlocklist(unsolicited_prompts_origin,
                            SiteReputation::UNSOLICITED_PROMPTS);
  AddToPreloadDataBlocklist(acceptable_origin, SiteReputation::ACCEPTABLE);
  AddToPreloadDataBlocklist(unknown_origin, SiteReputation::UNKNOWN);

  // The origins are on CrowdDeny blocking lists, but not on SafeBrowsing.
  for (auto origin : origins)
    QueryAndExpectDecisionForUrl(origin, Outcome::PERMISSION_NOT_REVOKED);

  for (auto origin : origins)
    AddToSafeBrowsingBlocklist(origin);

  QueryAndExpectDecisionForUrl(abusive_content_origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  QueryAndExpectDecisionForUrl(abusive_prompts_origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  QueryAndExpectDecisionForUrl(unsolicited_prompts_origin,
                               Outcome::PERMISSION_NOT_REVOKED);
  QueryAndExpectDecisionForUrl(acceptable_origin,
                               Outcome::PERMISSION_NOT_REVOKED);
  QueryAndExpectDecisionForUrl(unknown_origin, Outcome::PERMISSION_NOT_REVOKED);
}

TEST_F(AbusiveOriginPermissionRevocationRequestTest, ExemptAbusiveOriginTest) {
  const GURL origin_to_exempt = GURL("https://origin-allow.com/");
  const GURL origin_to_revoke = GURL("https://origin.com/");

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAbusiveNotificationPermissionRevocation);

  AbusiveOriginPermissionRevocationRequest::ExemptOriginFromFutureRevocations(
      GetTestingProfile(), origin_to_exempt);

  SetPermission(origin_to_exempt, CONTENT_SETTING_ALLOW);

  AddToPreloadDataBlocklist(origin_to_exempt, SiteReputation::ABUSIVE_CONTENT);
  AddToSafeBrowsingBlocklist(origin_to_exempt);

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT);
  AddToSafeBrowsingBlocklist(origin_to_revoke);

  // The origin added to the exempt list will not be revoked.
  QueryAndExpectDecisionForUrl(origin_to_exempt,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ASK);
}
