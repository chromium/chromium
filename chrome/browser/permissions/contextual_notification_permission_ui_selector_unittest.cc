// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/contextual_notification_permission_ui_selector.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/permissions/crowd_deny.pb.h"
#include "chrome/browser/permissions/crowd_deny_fake_safe_browsing_database_manager.h"
#include "chrome/browser/permissions/crowd_deny_preload_data.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_config.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using QuietUiReason = ContextualNotificationPermissionUiSelector::QuietUiReason;
using WarningReason = ContextualNotificationPermissionUiSelector::WarningReason;
using Decision = ContextualNotificationPermissionUiSelector::Decision;

constexpr char kTestDomainUnknown[] = "unknown.com";
constexpr char kTestDomainAcceptable[] = "acceptable.com";
constexpr char kTestDomainSpammy[] = "spammy.com";
constexpr char kTestDomainSpammyWarn[] = "warn-spammy.com";
constexpr char kTestDomainAbusivePrompts[] = "abusive_prompts.com";
constexpr char kTestDomainAbusivePromptsWarn[] = "warn_prompts.com";

constexpr char kTestOriginNoData[] = "https://nodata.com/";
constexpr char kTestOriginUnknown[] = "https://unknown.com/";
constexpr char kTestOriginAcceptable[] = "https://acceptable.com/";
constexpr char kTestOriginSpammy[] = "https://spammy.com/";
constexpr char kTestOriginSpammyWarn[] = "https://warn-spammy.com/";
constexpr char kTestOriginAbusivePrompts[] = "https://abusive-prompts.com/";
constexpr char kTestOriginSubDomainOfAbusivePrompts[] =
    "https://b.abusive-prompts.com/";
constexpr char kTestOriginAbusivePromptsWarn[] =
    "https://warn-abusive-prompts.com/";
constexpr char kTestOriginAbusiveContent[] = "https://abusive-content.com/";
constexpr char kTestOriginAbusiveContentWarn[] =
    "https://warn-abusive-content.com/";
constexpr char kTestOriginDisruptive[] = "https://disruptive.com/";

constexpr const char* kAllTestingOrigins[] = {
    kTestOriginNoData,
    kTestOriginUnknown,
    kTestOriginAcceptable,
    kTestOriginSpammy,
    kTestOriginSpammyWarn,
    kTestOriginAbusivePrompts,
    kTestOriginSubDomainOfAbusivePrompts,
    kTestOriginAbusivePromptsWarn,
    kTestOriginAbusiveContent,
    kTestOriginAbusiveContentWarn,
    kTestOriginDisruptive};

}  // namespace

class ContextualNotificationPermissionUiSelectorTest : public testing::Test {
 public:
  ContextualNotificationPermissionUiSelectorTest()
      : testing_profile_(std::make_unique<TestingProfile>()) {}

  ContextualNotificationPermissionUiSelectorTest(
      const ContextualNotificationPermissionUiSelectorTest&) = delete;
  ContextualNotificationPermissionUiSelectorTest& operator=(
      const ContextualNotificationPermissionUiSelectorTest&) = delete;

  ~ContextualNotificationPermissionUiSelectorTest() override = default;

 protected:
  void SetUp() override {
    testing::Test::SetUp();

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

  void SetQuietUiEnabledInPrefs(bool enabled) {
    testing_profile_->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, enabled);
  }

  void LoadTestPreloadData() {
    using SiteReputation = CrowdDenyPreloadData::SiteReputation;

    SiteReputation reputation_unknown;
    reputation_unknown.set_domain(kTestDomainUnknown);
    reputation_unknown.set_notification_ux_quality(SiteReputation::UNKNOWN);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginUnknown)),
        std::move(reputation_unknown));

    SiteReputation reputation_acceptable;
    reputation_acceptable.set_domain(kTestDomainAcceptable);
    reputation_acceptable.set_notification_ux_quality(
        SiteReputation::ACCEPTABLE);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAcceptable)),
        std::move(reputation_acceptable));

    SiteReputation reputation_spammy;
    reputation_spammy.set_domain(kTestDomainSpammy);
    reputation_spammy.set_notification_ux_quality(
        SiteReputation::UNSOLICITED_PROMPTS);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginSpammy)),
        std::move(reputation_spammy));

    SiteReputation reputation_spammy_warn;
    reputation_spammy_warn.set_domain(kTestDomainSpammyWarn);
    reputation_spammy_warn.set_notification_ux_quality(
        SiteReputation::UNSOLICITED_PROMPTS);
    reputation_spammy_warn.set_warning_only(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginSpammyWarn)),
        std::move(reputation_spammy_warn));

    SiteReputation reputation_abusive;
    reputation_abusive.set_domain(kTestDomainAbusivePrompts);
    reputation_abusive.set_notification_ux_quality(
        SiteReputation::ABUSIVE_PROMPTS);
    reputation_abusive.set_include_subdomains(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusivePrompts)),
        std::move(reputation_abusive));

    SiteReputation reputation_abusive_warn;
    reputation_abusive_warn.set_domain(kTestDomainAbusivePromptsWarn);
    reputation_abusive_warn.set_notification_ux_quality(
        SiteReputation::ABUSIVE_PROMPTS);
    reputation_abusive_warn.set_warning_only(true);
    reputation_abusive_warn.set_include_subdomains(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusivePromptsWarn)),
        std::move(reputation_abusive_warn));

    SiteReputation reputation_abusive_content;
    reputation_abusive_content.set_notification_ux_quality(
        SiteReputation::ABUSIVE_CONTENT);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusiveContent)),
        std::move(reputation_abusive_content));

    SiteReputation reputation_abusive_content_warn;
    reputation_abusive_content_warn.set_notification_ux_quality(
        SiteReputation::ABUSIVE_CONTENT);
    reputation_abusive_content_warn.set_warning_only(true);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginAbusiveContentWarn)),
        std::move(reputation_abusive_content_warn));

    SiteReputation reputation_disruptive;
    reputation_disruptive.set_notification_ux_quality(
        SiteReputation::DISRUPTIVE_BEHAVIOR);
    testing_preload_data_.SetOriginReputation(
        url::Origin::Create(GURL(kTestOriginDisruptive)),
        std::move(reputation_disruptive));
  }

  void AddUrlToFakeApiAbuseBlocklist(const GURL& url) {
    safe_browsing::ThreatMetadata test_metadata;
    test_metadata.api_permissions.emplace("NOTIFICATIONS");
    fake_database_manager_->SetSimulatedMetadataForUrl(url, test_metadata);
  }

  void LoadTestSafeBrowsingBlocklist() {
    // For simplicity, Safe Browsing will simulate a match for all testing
    // origins, tests can clear the fake results to simulate a miss.
    for (const char* origin : kAllTestingOrigins) {
      AddUrlToFakeApiAbuseBlocklist(GURL(origin));
    }
  }

  void ClearSafeBrowsingBlocklist() {
    fake_database_manager_->RemoveAllBlocklistedUrls();
  }

  void QueryAndExpectDecisionForUrl(
      const GURL& origin,
      std::optional<QuietUiReason> quiet_ui_reason,
      std::optional<WarningReason> warning_reason) {
    permissions::MockPermissionRequest mock_request(
        origin, permissions::RequestType::kNotifications);
    base::MockCallback<
        ContextualNotificationPermissionUiSelector::DecisionMadeCallback>
        mock_callback;
    Decision actual_decison(std::nullopt, std::nullopt);
    EXPECT_CALL(mock_callback, Run)
        .WillRepeatedly(testing::SaveArg<0>(&actual_decison));
    contextual_selector_.SelectUiToUse(&mock_request, mock_callback.Get());
    task_environment_.RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(&mock_callback);
    EXPECT_EQ(quiet_ui_reason, actual_decison.quiet_ui_reason);
    EXPECT_EQ(warning_reason, actual_decison.warning_reason);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  testing::ScopedCrowdDenyPreloadDataOverride testing_preload_data_;
  scoped_refptr<CrowdDenyFakeSafeBrowsingDatabaseManager>
      fake_database_manager_;
  std::unique_ptr<safe_browsing::TestSafeBrowsingServiceFactory>
      safe_browsing_factory_;

  ContextualNotificationPermissionUiSelector contextual_selector_;
};

// With all the field trials enabled, test all combinations of:
//   (a) quiet UI being enabled/disabled in prefs, and
//   (b) positive/negative Safe Browsing verdicts.
// The ContextualNotificationPermissionUiSelector should only take into account
// the Safe Browsing verdict and ignore the user pref.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       PrefAndSafeBrowsingCombinations) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "true"},
         {Config::kEnableAbusiveRequestBlocking, "true"},
         {Config::kEnableAbusiveRequestWarning, "true"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "true"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "true"},
         {Config::kCrowdDenyHoldBackChance, "0.0"}}},
       {features::kDisruptiveNotificationPermissionRevocation, {}}},
      {});

  LoadTestPreloadData();

  for (const bool quiet_ui_enabled_in_prefs : {false, true}) {
    SetQuietUiEnabledInPrefs(quiet_ui_enabled_in_prefs);
    ClearSafeBrowsingBlocklist();

    SCOPED_TRACE(quiet_ui_enabled_in_prefs ? "Quiet UI enabled in prefs"
                                           : "Quiet UI disabled in prefs");
    SCOPED_TRACE("Safe Browsing verdicts negative");

    for (const auto* origin_string : kAllTestingOrigins) {
      SCOPED_TRACE(origin_string);
      QueryAndExpectDecisionForUrl(GURL(origin_string), Decision::UseNormalUi(),
                                   Decision::ShowNoWarning());
    }
  }

  for (const bool quiet_ui_enabled_in_prefs : {false, true}) {
    SetQuietUiEnabledInPrefs(quiet_ui_enabled_in_prefs);
    LoadTestSafeBrowsingBlocklist();

    const struct {
      const char* origin_string;
      std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
      std::optional<WarningReason> expected_warning_reason =
          Decision::ShowNoWarning();
    } kTestCases[] = {
        {kTestOriginNoData},
        {kTestOriginUnknown},
        {kTestOriginAcceptable},
        {kTestOriginSpammy, QuietUiReason::kTriggeredByCrowdDeny},
        {kTestOriginSpammyWarn},
        {kTestOriginAbusivePrompts,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginSubDomainOfAbusivePrompts,
         QuietUiReason::kTriggeredDueToAbusiveRequests},
        {kTestOriginAbusivePromptsWarn, Decision::UseNormalUi(),
         WarningReason::kAbusiveRequests},
        {kTestOriginAbusiveContent,
         QuietUiReason::kTriggeredDueToAbusiveContent},
        {kTestOriginAbusiveContentWarn, Decision::UseNormalUi(),
         WarningReason::kAbusiveContent},
        {kTestOriginDisruptive,
         QuietUiReason::kTriggeredDueToDisruptiveBehavior},
    };

    SCOPED_TRACE(quiet_ui_enabled_in_prefs ? "Quiet UI enabled in prefs"
                                           : "Quiet UI disabled in prefs");
    SCOPED_TRACE("Safe Browsing verdicts positive");

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_string);
      QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                   test.expected_ui_reason,
                                   test.expected_warning_reason);
    }
  }
}

TEST_F(ContextualNotificationPermissionUiSelectorTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kQuietNotificationPrompts,
           features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto* origin_string : kAllTestingOrigins) {
    SCOPED_TRACE(origin_string);
    QueryAndExpectDecisionForUrl(GURL(origin_string), Decision::UseNormalUi(),
                                 Decision::ShowNoWarning());
  }
}

// The feature is enabled but no triggers are enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest, AllTriggersDisabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  SetQuietUiEnabledInPrefs(true);
  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto* origin_string : kAllTestingOrigins) {
    SCOPED_TRACE(origin_string);
    QueryAndExpectDecisionForUrl(GURL(origin_string), Decision::UseNormalUi(),
                                 Decision::ShowNoWarning());
  }
}

// The feature is enabled but only the `crowd deny` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest, OnlyCrowdDenyEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "true"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"},
         {Config::kCrowdDenyHoldBackChance, "0.0"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy, QuietUiReason::kTriggeredByCrowdDeny},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts},
      {kTestOriginSubDomainOfAbusivePrompts},
      {kTestOriginAbusivePromptsWarn},
      {kTestOriginAbusiveContent},
      {kTestOriginAbusiveContentWarn},
      {kTestOriginDisruptive},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

// The feature is enabled but only the `abusive content` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyAbusiveContentBlockingEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;

  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "true"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts},
      {kTestOriginSubDomainOfAbusivePrompts},
      {kTestOriginAbusivePromptsWarn},
      {kTestOriginAbusiveContent, QuietUiReason::kTriggeredDueToAbusiveContent},
      {kTestOriginAbusiveContentWarn},
      {kTestOriginDisruptive},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

// The feature is enabled but only the `abusive content` warning is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyAbusiveContentWarningsEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "true"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts},
      {kTestOriginSubDomainOfAbusivePrompts},
      {kTestOriginAbusivePromptsWarn},
      {kTestOriginAbusiveContent},
      {kTestOriginAbusiveContentWarn, Decision::UseNormalUi(),
       WarningReason::kAbusiveContent},
      {kTestOriginDisruptive},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

// The feature is enabled but only the `abusive prompts` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyAbusivePromptBlockingEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "true"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts,
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      {kTestOriginSubDomainOfAbusivePrompts,
       QuietUiReason::kTriggeredDueToAbusiveRequests},
      {kTestOriginAbusivePromptsWarn},
      {kTestOriginAbusiveContent},
      {kTestOriginAbusiveContentWarn},
      {kTestOriginDisruptive},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

// The feature is enabled but only the `abusive prompts` warning is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyAbusivePromptWarningsEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "true"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"}}}},
      {features::kDisruptiveNotificationPermissionRevocation});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts},
      {kTestOriginSubDomainOfAbusivePrompts},
      {kTestOriginAbusivePromptsWarn, Decision::UseNormalUi(),
       WarningReason::kAbusiveRequests},
      {kTestOriginAbusiveContent},
      {kTestOriginAbusiveContentWarn},
      {kTestOriginDisruptive},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

// The feature is enabled but only the `disruptive behavior` trigger is enabled.
TEST_F(ContextualNotificationPermissionUiSelectorTest,
       OnlyDisruptiveBehaviorRequestBlockingEnabled) {
  using Config = QuietNotificationPermissionUiConfig;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kQuietNotificationPrompts,
        {{Config::kEnableAdaptiveActivation, "true"},
         {Config::kEnableCrowdDenyTriggering, "false"},
         {Config::kEnableAbusiveRequestBlocking, "false"},
         {Config::kEnableAbusiveRequestWarning, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestBlocking, "false"},
         {Config::kEnableAbusiveContentTriggeredRequestWarning, "false"}}},
       {features::kDisruptiveNotificationPermissionRevocation, {}}},
      {});

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  const struct {
    const char* origin_string;
    std::optional<QuietUiReason> expected_ui_reason = Decision::UseNormalUi();
    std::optional<WarningReason> expected_warning_reason =
        Decision::ShowNoWarning();
  } kTestCases[] = {
      {kTestOriginSpammy},
      {kTestOriginSpammyWarn},
      {kTestOriginAbusivePrompts},
      {kTestOriginSubDomainOfAbusivePrompts},
      {kTestOriginAbusivePromptsWarn},
      {kTestOriginAbusiveContent},
      {kTestOriginAbusiveContentWarn},
      {kTestOriginDisruptive, QuietUiReason::kTriggeredDueToDisruptiveBehavior},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_string);
    QueryAndExpectDecisionForUrl(GURL(test.origin_string),
                                 test.expected_ui_reason,
                                 test.expected_warning_reason);
  }
}

TEST_F(ContextualNotificationPermissionUiSelectorTest,
       CrowdDenyHoldbackChance) {
  const struct {
    std::string holdback_chance;
    std::optional<QuietUiReason> expected_ui_reason;
    bool expected_histogram_bucket;
  } kTestCases[] = {
      // 100% chance to holdback, the UI used should be the normal UI.
      {"1.0", Decision::UseNormalUi(), true},
      // 0% chance to holdback, the UI used should be the quiet UI.
      {"0.0", QuietUiReason::kTriggeredByCrowdDeny},
  };

  LoadTestPreloadData();
  LoadTestSafeBrowsingBlocklist();

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.holdback_chance);

    using Config = QuietNotificationPermissionUiConfig;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeaturesAndParameters(
        {{features::kQuietNotificationPrompts,
          {{Config::kEnableAdaptiveActivation, "true"},
           {Config::kEnableAbusiveRequestBlocking, "true"},
           {Config::kEnableAbusiveRequestWarning, "true"},
           {Config::kEnableAbusiveContentTriggeredRequestBlocking, "true"},
           {Config::kEnableAbusiveContentTriggeredRequestWarning, "true"},
           {Config::kEnableCrowdDenyTriggering, "true"},
           {Config::kCrowdDenyHoldBackChance, test.holdback_chance}}},
         {features::kDisruptiveNotificationPermissionRevocation, {}}},
        {});

    base::HistogramTester histograms;
    QueryAndExpectDecisionForUrl(GURL(kTestOriginSpammy),
                                 test.expected_ui_reason,
                                 Decision::ShowNoWarning());

    // The hold-back should not apply to other per-site triggers.
    QueryAndExpectDecisionForUrl(GURL(kTestOriginAbusivePrompts),
                                 QuietUiReason::kTriggeredDueToAbusiveRequests,
                                 Decision::ShowNoWarning());
    QueryAndExpectDecisionForUrl(GURL(kTestOriginAbusivePromptsWarn),
                                 Decision::UseNormalUi(),
                                 WarningReason::kAbusiveRequests);

    QueryAndExpectDecisionForUrl(GURL(kTestOriginAbusiveContent),
                                 QuietUiReason::kTriggeredDueToAbusiveContent,
                                 Decision::ShowNoWarning());
    QueryAndExpectDecisionForUrl(GURL(kTestOriginAbusiveContentWarn),
                                 Decision::UseNormalUi(),
                                 WarningReason::kAbusiveContent);

    auto expected_bucket = static_cast<base::HistogramBase::Sample>(
        test.expected_histogram_bucket);
    histograms.ExpectBucketCount("Permissions.CrowdDeny.DidHoldbackQuietUi",
                                 expected_bucket, 1);
  }
}
