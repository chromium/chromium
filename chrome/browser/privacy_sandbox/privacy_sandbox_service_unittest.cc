// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

#include "base/test/gtest_util.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/federated_learning/floc_id_provider.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/federated_learning/floc_internals.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/federated_learning/features/features.h"
#include "components/federated_learning/floc_id.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_test_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/federated_learning/floc.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

namespace {

class MockFlocIdProvider : public federated_learning::FlocIdProvider {
 public:
  blink::mojom::InterestCohortPtr GetInterestCohortForJsApi(
      const GURL& url,
      const absl::optional<url::Origin>& top_frame_origin) const override {
    return blink::mojom::InterestCohort::New();
  }
  MOCK_METHOD(federated_learning::mojom::WebUIFlocStatusPtr,
              GetFlocStatusForWebUi,
              (),
              (const, override));
  MOCK_METHOD(void, MaybeRecordFlocToUkm, (ukm::SourceId), (override));
  MOCK_METHOD(base::Time, GetApproximateNextComputeTime, (), (const, override));
};

}  // namespace

class PrivacySandboxServiceTest : public testing::Test {
 public:
  PrivacySandboxServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    InitializePrefsBeforeStart();

    privacy_sandbox_service_ = std::make_unique<PrivacySandboxService>(
        PrivacySandboxSettingsFactory::GetForProfile(profile()),
        CookieSettingsFactory::GetForProfile(profile()).get(),
        profile()->GetPrefs(), policy_service(), sync_service(),
        identity_test_env()->identity_manager(), mock_floc_id_provider());
  }

  virtual void InitializePrefsBeforeStart() {}

  TestingProfile* profile() { return &profile_; }
  PrivacySandboxService* privacy_sandbox_service() {
    return privacy_sandbox_service_.get();
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
  syncer::TestSyncService* sync_service() { return &sync_service_; }
  policy::MockPolicyService* policy_service() { return &mock_policy_service_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }
  MockFlocIdProvider* mock_floc_id_provider() {
    return &mock_floc_id_provider_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  testing::NiceMock<policy::MockPolicyService> mock_policy_service_;

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  syncer::TestSyncService sync_service_;
  MockFlocIdProvider mock_floc_id_provider_;

  std::unique_ptr<PrivacySandboxService> privacy_sandbox_service_;
};

TEST_F(PrivacySandboxServiceTest, GetFlocDescriptionForDisplay) {
  // Check that the returned FLoC description correctly takes into account the
  // time between FLoC recomputes.
  std::map<std::string, std::u16string> param_to_expected_string = {
      {"1h", l10n_util::GetPluralStringFUTF16(
                 IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 0)},
      {"23h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 0)},
      {"24h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 1)},
      {"25h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 1)},
      {"60h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 3)},
      {"167h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 7)},
      {"168h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_DESCRIPTION, 7)}};

  for (const auto& param_expected : param_to_expected_string) {
    feature_list()->InitAndEnableFeatureWithParameters(
        federated_learning::kFederatedLearningOfCohorts,
        {{"update_interval", param_expected.first}});
    EXPECT_EQ(param_expected.second,
              privacy_sandbox_service()->GetFlocDescriptionForDisplay());
    feature_list()->Reset();
  }
}

TEST_F(PrivacySandboxServiceTest, GetFlocIdForDisplay) {
  // Check that the cohort identifier is correctly converted to a string when
  // available.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  federated_learning::FlocId floc_id = federated_learning::FlocId::CreateValid(
      123456, base::Time(), base::Time::Now(),
      /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());

  EXPECT_EQ(std::u16string(u"123456"),
            privacy_sandbox_service()->GetFlocIdForDisplay());

  // If the FLoC preference, the Sandbox Preference, or the feature is disabled,
  // or the FLoC ID is invalid, the invalid string should be returned.
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_service()->GetFlocIdForDisplay());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_service()->GetFlocIdForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_service()->GetFlocIdForDisplay());

  floc_id.UpdateStatusAndSaveToPrefs(
      profile()->GetTestingPrefService(),
      federated_learning::FlocId::Status::kInvalidReset);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_INVALID),
            privacy_sandbox_service()->GetFlocIdForDisplay());
}

TEST_F(PrivacySandboxServiceTest, GetFlocIdNextUpdateForDisplay) {
  // Check that date FLoC will be next updated is returned when available.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);

  std::map<base::TimeDelta, std::u16string> offsets_to_expected_string = {
      {base::Hours(23), l10n_util::GetPluralStringFUTF16(
                            IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 0)},
      {base::Hours(25), l10n_util::GetPluralStringFUTF16(
                            IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 1)},
      {base::Days(2), l10n_util::GetPluralStringFUTF16(
                          IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 2)},
      {base::Hours(60), l10n_util::GetPluralStringFUTF16(
                            IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 3)},
      {base::Hours(167),  // 1 hour less than 7 days.
       l10n_util::GetPluralStringFUTF16(
           IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE, 7)}};

  for (const auto& offset_expected : offsets_to_expected_string) {
    EXPECT_CALL(*mock_floc_id_provider(), GetApproximateNextComputeTime)
        .WillOnce(testing::Return(base::Time::Now() + offset_expected.first));
    EXPECT_EQ(offset_expected.second,
              privacy_sandbox_service()->GetFlocIdNextUpdateForDisplay(
                  base::Time::Now()));
    testing::Mock::VerifyAndClearExpectations(mock_floc_id_provider());
  }

  // Check that disabling FLoC is also reflected in the returned string.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_CALL(*mock_floc_id_provider(), GetApproximateNextComputeTime).Times(0);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_FLOC_TIME_TO_NEXT_COMPUTE_INVALID),
            privacy_sandbox_service()->GetFlocIdNextUpdateForDisplay(
                base::Time::Now()));
  testing::Mock::VerifyAndClearExpectations(mock_floc_id_provider());

  // Disabling the FLoC feature should also invalidate the next compute time.
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  testing::Mock::VerifyAndClearExpectations(mock_floc_id_provider());
}

TEST_F(PrivacySandboxServiceTest, GetFlocResetExplanationForDisplay) {
  // Check that the string description indicating what happens when the user
  // resets the FLoC ID updates appropriately based on the feature parameter.
  std::map<std::string, std::u16string> param_to_expected_string = {
      {"1h", l10n_util::GetPluralStringFUTF16(
                 IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 0)},
      {"23h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 0)},
      {"24h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 1)},
      {"25h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 1)},
      {"60h", l10n_util::GetPluralStringFUTF16(
                  IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 3)},
      {"167h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 7)},
      {"168h", l10n_util::GetPluralStringFUTF16(
                   IDS_PRIVACY_SANDBOX_FLOC_RESET_EXPLANATION, 7)}};

  for (const auto& param_expected : param_to_expected_string) {
    feature_list()->InitAndEnableFeatureWithParameters(
        federated_learning::kFederatedLearningOfCohorts,
        {{"update_interval", param_expected.first}});
    EXPECT_EQ(param_expected.second,
              privacy_sandbox_service()->GetFlocResetExplanationForDisplay());
    feature_list()->Reset();
  }
}

TEST_F(PrivacySandboxServiceTest, GetFlocStatusForDisplay) {
  // Check the status of the user's FLoC is correctly returned. This depends
  // on whether the FLoC origin trial feature is enabled, and whether the user
  // has FLoC enabled.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_ACTIVE),
            privacy_sandbox_service()->GetFlocStatusForDisplay());

  // The Privacy Sandbox APIs pref & FLoC pref should disable the trial when
  // either is disabled.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE),
      privacy_sandbox_service()->GetFlocStatusForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_FLOC_STATUS_NOT_ACTIVE),
      privacy_sandbox_service()->GetFlocStatusForDisplay());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_FLOC_STATUS_ELIGIBLE_NOT_ACTIVE),
            privacy_sandbox_service()->GetFlocStatusForDisplay());
}

TEST_F(PrivacySandboxServiceTest, IsFlocIdResettable) {
  // Check that if FLoC is functional the FLoC ID is resettable, regardless of
  // whether the FLoC ID is currently valid.
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  federated_learning::FlocId floc_id = federated_learning::FlocId::CreateValid(
      123456, base::Time(), base::Time::Now(),
      /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocIdResettable());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {}, {blink::features::kInterestCohortAPIOriginTrial});
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocIdResettable());

  feature_list()->Reset();
  feature_list()->InitWithFeatures(
      {blink::features::kInterestCohortAPIOriginTrial}, {});
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocIdResettable());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocIdResettable());

  floc_id.UpdateStatusAndSaveToPrefs(
      profile()->GetTestingPrefService(),
      federated_learning::FlocId::Status::kInvalidReset);
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocIdResettable());
}

TEST_F(PrivacySandboxServiceTest, UserResetFlocID) {
  // Check that the PrivacySandboxSettings is informed, and the appropriate
  // actions are logged, in response to a user resetting the floc id.
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile());
  EXPECT_EQ(base::Time(), privacy_sandbox_settings->FlocDataAccessibleSince());

  privacy_sandbox_test_util::MockPrivacySandboxObserver observer;
  privacy_sandbox_settings->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(true)).Times(2);

  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_service()->ResetFlocId(/*user_initiated=*/true);

  EXPECT_NE(base::Time(), privacy_sandbox_settings->FlocDataAccessibleSince());
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));

  privacy_sandbox_service()->ResetFlocId(/*user_initiated=*/false);
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.ResetFloc"));
}

TEST_F(PrivacySandboxServiceTest, IsFlocPrefEnabled) {
  // IsFlocPrefEnabled should directly reflect the state of the FLoC pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocPrefEnabled());

  // The Privacy Sandbox APIs pref should not impact the return value.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  EXPECT_TRUE(privacy_sandbox_service()->IsFlocPrefEnabled());

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  EXPECT_FALSE(privacy_sandbox_service()->IsFlocPrefEnabled());
}

TEST_F(PrivacySandboxServiceTest, SetFlocPrefEnabled) {
  // The FLoc pref should always be updated by this function, regardless of
  // other Sandbox State.
  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  privacy_sandbox_service()->SetFlocPrefEnabled(false);
  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));

  // Disabling the sandbox shouldn't prevent the pref from being updated. This
  // state is not directly allowable by the UI, but the state itself is valid
  // as far as the PrivacySandboxService service is concerned.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  privacy_sandbox_service()->SetFlocPrefEnabled(true);
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxFlocEnabled));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocEnabled"));
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "Settings.PrivacySandbox.FlocDisabled"));
}

TEST_F(PrivacySandboxServiceTest, OnPrivacySandboxPrefChanged) {
  // When either the main Privacy Sandbox pref, or the FLoC pref, are changed
  // the FLoC ID should be reset. This will be propagated to the settings
  // instance, which should then notify observers.
  privacy_sandbox_test_util::MockPrivacySandboxObserver
      mock_privacy_sandbox_observer;
  PrivacySandboxSettingsFactory::GetForProfile(profile())->AddObserver(
      &mock_privacy_sandbox_observer);
  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));

  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);

  EXPECT_CALL(mock_privacy_sandbox_observer,
              OnFlocDataAccessibleSinceUpdated(/*reset_compute_timer=*/true));
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxApisEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&mock_privacy_sandbox_observer);
}

class PrivacySandboxServiceTestReconciliationBlocked
    : public PrivacySandboxServiceTest {
 public:
  void InitializePrefsBeforeStart() override {
    // Set the reconciled preference to true here, so when the service is
    // created prior to each test case running, it does not attempt to reconcile
    // the preferences. Tests must call ResetReconciledPref before testing to
    // reset the preference to it's default value.
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxPreferencesReconciled,
        std::make_unique<base::Value>(true));
  }

  void ResetReconciledPref() {
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kPrivacySandboxPreferencesReconciled,
        std::make_unique<base::Value>(false));
  }
};

TEST_F(PrivacySandboxServiceTestReconciliationBlocked, ReconciliationOutcome) {
  // Check that reconciling preferences has the appropriate outcome based on
  // the current user cookie settings.
  ResetReconciledPref();

  // Blocking 3P cookies should disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking all cookies should disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Blocking cookies via content setting exceptions, now matter how broad,
  // should not disable.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/
      {{"[*.]com", "*", ContentSetting::CONTENT_SETTING_BLOCK}},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // If the user has already expressed control over the privacy sandbox, it
  // should not be disabled.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Allowing cookies should leave the sandbox enabled.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));

  // Reconciliation should not enable the privacy sandbox.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});
  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxManuallyControlled,
      std::make_unique<base::Value>(false));

  privacy_sandbox_service()->ReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationNoSync) {
  // Check that if the user is not syncing preferences, reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  auto registered_types =
      sync_service()->GetUserSettings()->GetRegisteredSelectableTypes();
  registered_types.Remove(syncer::UserSelectableType::kPreferences);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, registered_types);

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationSyncComplete) {
  // Check that if sync has completed a cycle that reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetNonEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationPersistentSyncError) {
  // Check that if sync has a persistent error that reconciliation occurs
  // immediately.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       ImmediateReconciliationNoDisable) {
  // Check that if the local settings would not disable the privacy sandbox
  // that reconciliation runs.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncSuccess) {
  // Check that a sync service which has not yet started delays reconciliation
  // until it has completed a sync cycle.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent error has occurred.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A transient sync startup state should not result in reconciliation.
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::START_DEFERRED);
  sync_service()->FireStateChanged();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A state update after an unrecoverable error should result in
  // reconciliation.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR);
  sync_service()->FireStateChanged();

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationIdentityFailure) {
  // Check that a sync service which has not yet started delays reconciliation
  // until a persistent identity error has occurred.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // An account becoming available should not result in reconciliation.
  identity_test_env()->MakePrimaryAccountAvailable("test@test.com",
                                                   signin::ConsentLevel::kSync);

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A successful update to refresh tokens should not result in reconciliation.
  identity_test_env()->SetRefreshTokenForPrimaryAccount();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A persistent authentication error for a non-primary account should not
  // result in reconciliation.
  auto non_primary_account =
      identity_test_env()->MakeAccountAvailable("unrelated@unrelated.com");
  identity_test_env()->SetRefreshTokenForAccount(
      non_primary_account.account_id);
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      non_primary_account.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // A persistent authentication error for the primary account should result
  // in reconciliation.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_test_env()->identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSync),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       DelayedReconciliationSyncIssueThenManaged) {
  // Check that if before an initial sync issue is resolved, the cookie settings
  // are disabled by policy, that reconciliation does not run until the policy
  // is removed.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Apply a management state that is disabling cookies. This should result
  // in the policy service being observed when the sync issue is resolved.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  EXPECT_CALL(*policy_service(), AddObserver(policy::POLICY_DOMAIN_CHROME,
                                             privacy_sandbox_service()))
      .Times(1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  // Removing the management state and firing the policy update listener should
  // result in reconciliation running.
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  // The HostContentSettingsMap & PrefService are inspected directly, and not
  // the PolicyMap provided here. The associated browser tests confirm that this
  // is a valid approach.
  privacy_sandbox_service()->OnPolicyUpdated(
      policy::PolicyNamespace(), policy::PolicyMap(), policy::PolicyMap());

  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       NoReconciliationAlreadyRun) {
  // Reconciliation should not run if it is recorded as already occurring.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  profile()->GetTestingPrefService()->SetUserPref(
      prefs::kPrivacySandboxPreferencesReconciled,
      std::make_unique<base::Value>(true));

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  // If run, reconciliation would have disabled the sandbox.
  EXPECT_TRUE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxApisEnabled));
}

TEST_F(PrivacySandboxServiceTestReconciliationBlocked,
       MetricsLoggingOccursCorrectly) {
  base::HistogramTester histograms;
  const std::string histogram_name = "Settings.PrivacySandbox.Enabled";
  ResetReconciledPref();

  // The histogram should start off empty.
  histograms.ExpectTotalCount(histogram_name, 0);

  // For buckets that do not explicitly mention FLoC, it is assumed to be on,
  // or its state is irrelevant, i.e. overridden by the Privacy Sandbox pref.
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, true);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 1);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledAllowAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 2);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlock3P),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 3);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledBlockAll),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 4);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledAllowAll),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 5);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlock3P),
      1);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::PrivacySandboxService::
                           SettingsPrivacySandboxEnabled::kPSDisabledBlockAll),
      1);

  // Verify that delayed reconciliation still logs properly.
  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  sync_service()->SetEmptyLastCycleSnapshot();

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  EXPECT_FALSE(profile()->GetTestingPrefService()->GetBoolean(
      prefs::kPrivacySandboxPreferencesReconciled));

  histograms.ExpectTotalCount(histogram_name, 6);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      1);

  sync_service()->SetNonEmptyLastCycleSnapshot();
  sync_service()->FireSyncCycleCompleted();

  histograms.ExpectTotalCount(histogram_name, 7);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledBlockAll),
      2);

  ResetReconciledPref();
  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/false,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 8);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSDisabledPolicyBlockAll),
      1);

  // Disable FLoC and test the buckets that reflect a disabled FLoC state.
  ResetReconciledPref();
  profile()->GetTestingPrefService()->SetBoolean(
      prefs::kPrivacySandboxFlocEnabled, false);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/false,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 9);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledAllowAll),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_ALLOW,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 10);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlock3P),
      1);

  privacy_sandbox_test_util::SetupTestState(
      prefs(), host_content_settings_map(),
      /*privacy_sandbox_enabled=*/true,
      /*block_third_party_cookies=*/true,
      /*default_cookie_setting=*/ContentSetting::CONTENT_SETTING_BLOCK,
      /*user_cookie_exceptions=*/{},
      /*managed_cookie_setting=*/privacy_sandbox_test_util::kNoSetting,
      /*managed_cookie_exceptions=*/{});

  privacy_sandbox_service()->MaybeReconcilePrivacySandboxPref();

  histograms.ExpectTotalCount(histogram_name, 11);
  histograms.ExpectBucketCount(
      histogram_name,
      static_cast<int>(PrivacySandboxService::SettingsPrivacySandboxEnabled::
                           kPSEnabledFlocDisabledBlockAll),
      1);
}
