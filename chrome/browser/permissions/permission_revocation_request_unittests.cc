// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_revocation_request.h"

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/notifications_permission_revocation_config.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"

class PermissionRevocationRequestTestBase : public testing::Test {
 public:
  using Outcome = PermissionRevocationRequest::Outcome;
  using SiteReputation = CrowdDenyPreloadData::SiteReputation;

  PermissionRevocationRequestTestBase() = default;

  PermissionRevocationRequestTestBase(
      const PermissionRevocationRequestTestBase&) = delete;
  PermissionRevocationRequestTestBase& operator=(
      const PermissionRevocationRequestTestBase&) = delete;

  ~PermissionRevocationRequestTestBase() override = default;

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
    fake_database_manager_->RemoveAllBlocklistedUrls();
  }

  void AddToPreloadDataBlocklist(
      const GURL& origin,
      chrome_browser_crowd_deny::
          SiteReputation_NotificationUserExperienceQuality reputation_type,
      bool has_warning) {
    SiteReputation reputation;
    reputation.set_notification_ux_quality(reputation_type);
    reputation.set_warning_only(has_warning);
    testing_preload_data_.SetOriginReputation(url::Origin::Create(origin),
                                              std::move(reputation));
  }

  void QueryAndExpectDecisionForUrl(const GURL& origin,
                                    Outcome expected_result) {
    base::MockOnceCallback<void(Outcome)> mock_callback_receiver;
    base::RunLoop run_loop;
    auto permission_revocation = std::make_unique<PermissionRevocationRequest>(
        testing_profile_.get(), origin, mock_callback_receiver.Get());
    EXPECT_CALL(mock_callback_receiver, Run(expected_result))
        .WillOnce(
            testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

  void SetPermission(const GURL& origin, const ContentSetting value) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(testing_profile_.get());
    host_content_settings_map->SetContentSettingDefaultScope(
        origin, GURL(), ContentSettingsType::NOTIFICATIONS, value);
  }

  void VerifyNotificationsPermission(const GURL& origin,
                                     const ContentSetting value) {
    HostContentSettingsMap* host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(testing_profile_.get());

    ContentSetting result = host_content_settings_map->GetContentSetting(
        origin, GURL(), ContentSettingsType::NOTIFICATIONS);

    EXPECT_EQ(value, result);
  }

  TestingProfile* GetTestingProfile() { return testing_profile_.get(); }

 protected:
  // This needs to be declared before |task_environment_|, so that it will be
  // destroyed after |task_environment_|. This avoids tsan race errors with
  // tasks running on other threads checking if features are enabled while
  // |feature_list_| is being destroyed.
  base::test::ScopedFeatureList feature_list_;

 private:
  base::ScopedTempDir profile_dir_;
  content::BrowserTaskEnvironment task_environment_;
  testing::ScopedCrowdDenyPreloadDataOverride testing_preload_data_;
  std::unique_ptr<TestingProfile> testing_profile_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;
};

class PermissionRevocationRequestTest
    : public PermissionRevocationRequestTestBase {
 public:
  PermissionRevocationRequestTest() {
    feature_list_.InitAndEnableFeature(
        features::kAbusiveNotificationPermissionRevocation);
  }

  ~PermissionRevocationRequestTest() override = default;
};

TEST_F(PermissionRevocationRequestTest, OriginIsNotOnBlockingLists) {
  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
}

TEST_F(PermissionRevocationRequestTest, SafeBrowsingTest) {
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
  EXPECT_FALSE(PermissionRevocationRequest::HasPreviouslyRevokedPermission(
      GetTestingProfile(), origin_to_revoke));

  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ASK);
  EXPECT_TRUE(PermissionRevocationRequest::HasPreviouslyRevokedPermission(
      GetTestingProfile(), origin_to_revoke));
}

TEST_F(PermissionRevocationRequestTest, PreloadDataTest) {
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
                            SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  AddToPreloadDataBlocklist(abusive_prompts_origin_to_revoke,
                            SiteReputation::ABUSIVE_PROMPTS,
                            /*has_warning=*/false);
  AddToPreloadDataBlocklist(unsolicited_prompts_origin,
                            SiteReputation::UNSOLICITED_PROMPTS,
                            /*has_warning=*/false);
  AddToPreloadDataBlocklist(acceptable_origin, SiteReputation::ACCEPTABLE,
                            /*has_warning=*/false);
  AddToPreloadDataBlocklist(unknown_origin, SiteReputation::UNKNOWN,
                            /*has_warning=*/false);

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

TEST_F(PermissionRevocationRequestTest, PreloadDataAsyncTest) {
  auto* instance = CrowdDenyPreloadData::GetInstance();
  // From this point on CrowdDenyPreloadData is not usable for origins
  // verification.
  instance->set_is_ready_to_use_for_testing(false);

  const GURL abusive_content_origin_to_revoke =
      GURL("https://abusive-content.com/");
  base::MockOnceCallback<void(Outcome)> mock_callback_receiver_1;
  auto permission_revocation_1 = std::make_unique<PermissionRevocationRequest>(
      GetTestingProfile(), abusive_content_origin_to_revoke,
      mock_callback_receiver_1.Get());

  const GURL abusive_prompts_origin_to_revoke =
      GURL("https://abusive-prompts.com/");
  base::MockOnceCallback<void(Outcome)> mock_callback_receiver_2;
  auto permission_revocation_2 = std::make_unique<PermissionRevocationRequest>(
      GetTestingProfile(), abusive_prompts_origin_to_revoke,
      mock_callback_receiver_2.Get());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_callback_receiver_1,
              Run(Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE));

  EXPECT_CALL(mock_callback_receiver_2,
              Run(Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  SetPermission(abusive_content_origin_to_revoke, CONTENT_SETTING_ALLOW);
  SetPermission(abusive_prompts_origin_to_revoke, CONTENT_SETTING_ALLOW);

  AddToSafeBrowsingBlocklist(abusive_content_origin_to_revoke);
  AddToSafeBrowsingBlocklist(abusive_prompts_origin_to_revoke);

  // At this point CrowdDenyPreloadData will be reactivated by a new data set.
  AddToPreloadDataBlocklist(abusive_content_origin_to_revoke,
                            SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  AddToPreloadDataBlocklist(abusive_prompts_origin_to_revoke,
                            SiteReputation::ABUSIVE_PROMPTS,
                            /*has_warning=*/false);
  run_loop.Run();
}

TEST_F(PermissionRevocationRequestTest, PreloadDataAsyncHistogramTest) {
  base::HistogramTester histograms;
  // The Crowd Deny component is ready to use, there should be no
  // DelayedPushNotification recording.
  {
    const GURL origin_1 = GURL("https://not-abusive-origin-1.com/");
    SetPermission(origin_1, CONTENT_SETTING_ALLOW);
    base::MockOnceCallback<void(Outcome)> mock_callback_receiver_1;
    AddToPreloadDataBlocklist(origin_1, SiteReputation::ACCEPTABLE,
                              /*has_warning=*/false);
    auto permission_revocation_1 =
        std::make_unique<PermissionRevocationRequest>(
            GetTestingProfile(), origin_1, mock_callback_receiver_1.Get());

    EXPECT_CALL(mock_callback_receiver_1, Run(Outcome::PERMISSION_NOT_REVOKED));
    base::RunLoop().RunUntilIdle();
    histograms.ExpectTotalCount(
        "Permissions.CrowdDeny.PreloadData.DelayedPushNotification", 0);
  }

  auto* crowd_deny = CrowdDenyPreloadData::GetInstance();
  // From this point on CrowdDenyPreloadData is not usable for origin
  // verification. DelayedPushNotification should be tracked for non-abusive
  // origins.
  crowd_deny->set_is_ready_to_use_for_testing(false);

  const GURL origin_1 = GURL("https://not-abusive-origin-1.com/");
  SetPermission(origin_1, CONTENT_SETTING_ALLOW);
  base::MockOnceCallback<void(Outcome)> mock_callback_receiver_1;
  auto permission_revocation_1 = std::make_unique<PermissionRevocationRequest>(
      GetTestingProfile(), origin_1, mock_callback_receiver_1.Get());
  EXPECT_CALL(mock_callback_receiver_1, Run(Outcome::PERMISSION_NOT_REVOKED));

  const GURL origin_2 = GURL("https://not-abusive-origin-2.com/");
  SetPermission(origin_2, CONTENT_SETTING_ALLOW);
  base::MockOnceCallback<void(Outcome)> mock_callback_receiver_2;
  auto permission_revocation_2 = std::make_unique<PermissionRevocationRequest>(
      GetTestingProfile(), origin_2, mock_callback_receiver_2.Get());
  EXPECT_CALL(mock_callback_receiver_2, Run(Outcome::PERMISSION_NOT_REVOKED));

  const GURL abusive_origin = GURL("https://abusive-origin.com/");
  SetPermission(abusive_origin, CONTENT_SETTING_ALLOW);
  AddToSafeBrowsingBlocklist(abusive_origin);
  base::MockOnceCallback<void(Outcome)> mock_callback_receiver_3;
  auto permission_revocation_3 = std::make_unique<PermissionRevocationRequest>(
      GetTestingProfile(), abusive_origin, mock_callback_receiver_3.Get());
  EXPECT_CALL(mock_callback_receiver_3,
              Run(Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, crowd_deny->get_pending_origins_queue_size_for_testing());
  // Set Crowd Deny reputation only for abusive origin. Non-abusive origins will
  // have default reputation.
  AddToPreloadDataBlocklist(abusive_origin, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  base::RunLoop().RunUntilIdle();

  histograms.ExpectTotalCount(
      "Permissions.CrowdDeny.PreloadData.DelayedPushNotification", 2);
}

TEST_F(PermissionRevocationRequestTest, PreloadDataTestWithWarning) {
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
                            SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/true);
  AddToPreloadDataBlocklist(abusive_prompts_origin_to_revoke,
                            SiteReputation::ABUSIVE_PROMPTS,
                            /*has_warning=*/true);
  AddToPreloadDataBlocklist(unsolicited_prompts_origin,
                            SiteReputation::UNSOLICITED_PROMPTS,
                            /*has_warning=*/true);
  AddToPreloadDataBlocklist(acceptable_origin, SiteReputation::ACCEPTABLE,
                            /*has_warning=*/true);
  AddToPreloadDataBlocklist(unknown_origin, SiteReputation::UNKNOWN,
                            /*has_warning=*/true);

  // The origins are on CrowdDeny blocking lists, but not on SafeBrowsing.
  for (auto origin : origins)
    QueryAndExpectDecisionForUrl(origin, Outcome::PERMISSION_NOT_REVOKED);

  for (auto origin : origins)
    AddToSafeBrowsingBlocklist(origin);

  // The warning is enabled for all origin, permission should not be revoked.
  for (auto origin : origins)
    QueryAndExpectDecisionForUrl(origin, Outcome::PERMISSION_NOT_REVOKED);
}

TEST_F(PermissionRevocationRequestTest, ExemptAbusiveOriginTest) {
  const GURL origin_to_exempt = GURL("https://origin-allow.com/");
  const GURL origin_to_revoke = GURL("https://origin.com/");

  PermissionRevocationRequest::ExemptOriginFromFutureRevocations(
      GetTestingProfile(), origin_to_exempt);

  SetPermission(origin_to_exempt, CONTENT_SETTING_ALLOW);

  AddToPreloadDataBlocklist(origin_to_exempt, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  AddToSafeBrowsingBlocklist(origin_to_exempt);

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  AddToSafeBrowsingBlocklist(origin_to_revoke);

  // The origin added to the exempt list will not be revoked.
  QueryAndExpectDecisionForUrl(origin_to_exempt,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ASK);
}

TEST_F(PermissionRevocationRequestTest, SafeBrowsingDisabledTest) {
  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);

  AddToSafeBrowsingBlocklist(origin_to_revoke);
  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_REVOKED_DUE_TO_ABUSE);

  GetTestingProfile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                              false);

  // Permission should not be revoked because Safe Browsing is disabled.
  const GURL origin_to_not_revoke = GURL("https://origin-not_revoked.com/");

  SetPermission(origin_to_not_revoke, CONTENT_SETTING_ALLOW);

  AddToSafeBrowsingBlocklist(origin_to_not_revoke);
  AddToPreloadDataBlocklist(origin_to_not_revoke,
                            SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);

  QueryAndExpectDecisionForUrl(origin_to_not_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_not_revoke, CONTENT_SETTING_ALLOW);
}

class PermissionRevocationRequestDisabledTest
    : public PermissionRevocationRequestTestBase {
 public:
  PermissionRevocationRequestDisabledTest() {
    feature_list_.InitAndDisableFeature(
        features::kAbusiveNotificationPermissionRevocation);
  }
  ~PermissionRevocationRequestDisabledTest() override = default;
};

TEST_F(PermissionRevocationRequestDisabledTest,
       PermissionRevocationFeatureDisabled) {
  const GURL origin_to_revoke = GURL("https://origin.com/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  AddToPreloadDataBlocklist(origin_to_revoke, SiteReputation::ABUSIVE_CONTENT,
                            /*has_warning=*/false);
  AddToSafeBrowsingBlocklist(origin_to_revoke);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
}

class PermissionDisruptiveRevocationEnabledTest
    : public PermissionRevocationRequestTestBase {
 public:
  PermissionDisruptiveRevocationEnabledTest() {
    feature_list_.InitWithFeatures(
        {features::kDisruptiveNotificationPermissionRevocation},
        {features::kAbusiveNotificationPermissionRevocation});
  }

  ~PermissionDisruptiveRevocationEnabledTest() override = default;
};

TEST_F(PermissionDisruptiveRevocationEnabledTest,
       PermissionDisruptiveRevocationEnabled) {
  const GURL origin_to_revoke = GURL("https://origin.test/");

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
  EXPECT_FALSE(PermissionRevocationRequest::HasPreviouslyRevokedPermission(
      GetTestingProfile(), origin_to_revoke));

  AddToPreloadDataBlocklist(origin_to_revoke,
                            SiteReputation::DISRUPTIVE_BEHAVIOR,
                            /*has_warning=*/false);
  QueryAndExpectDecisionForUrl(
      origin_to_revoke, Outcome::PERMISSION_REVOKED_DUE_TO_DISRUPTIVE_BEHAVIOR);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ASK);
  EXPECT_TRUE(PermissionRevocationRequest::HasPreviouslyRevokedPermission(
      GetTestingProfile(), origin_to_revoke));
}

class PermissionDisruptiveRevocationDisabledTest
    : public PermissionRevocationRequestTestBase {
 public:
  PermissionDisruptiveRevocationDisabledTest() {
    feature_list_.InitWithFeatures(
        {features::kAbusiveNotificationPermissionRevocation},
        {features::kDisruptiveNotificationPermissionRevocation});
  }

  ~PermissionDisruptiveRevocationDisabledTest() override = default;
};

TEST_F(PermissionDisruptiveRevocationDisabledTest,
       PermissionDisruptiveRevocationDisabled) {
  const GURL origin_to_revoke = GURL("https://origin.test/");

  SetPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
  AddToSafeBrowsingBlocklist(origin_to_revoke);
  AddToPreloadDataBlocklist(origin_to_revoke,
                            SiteReputation::DISRUPTIVE_BEHAVIOR,
                            /*has_warning=*/false);
  QueryAndExpectDecisionForUrl(origin_to_revoke,
                               Outcome::PERMISSION_NOT_REVOKED);
  VerifyNotificationsPermission(origin_to_revoke, CONTENT_SETTING_ALLOW);
}
