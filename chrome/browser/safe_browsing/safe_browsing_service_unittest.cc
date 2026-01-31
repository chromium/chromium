// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/safe_browsing/chrome_ping_manager_factory.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/safe_browsing_pref_change_handler.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_service_interface.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"

using network::GetUploadData;
using testing::Return;
using testing::ReturnRef;

using WarningSurface = DownloadItemWarningData::WarningSurface;
using WarningAction = DownloadItemWarningData::WarningAction;
using WarningActionEvent = DownloadItemWarningData::WarningActionEvent;
using DownloadWarningAction =
    safe_browsing::ClientSafeBrowsingReportRequest::DownloadWarningAction;

namespace safe_browsing {

namespace {
const char kTestDownloadUrl[] = "https://example.com";
}

// Mock SafeBrowsingPrefChangeHandler.
class MockSafeBrowsingPrefChangeHandler
    : public safe_browsing::SafeBrowsingPrefChangeHandler {
 public:
  explicit MockSafeBrowsingPrefChangeHandler(Profile* profile)
      : SafeBrowsingPrefChangeHandler(profile) {}
  MOCK_METHOD(void,
              MaybeShowEnhancedProtectionSettingChangeNotification,
              (),
              (override));
};

class SafeBrowsingServiceTest : public testing::Test {
 public:
  SafeBrowsingServiceTest() {
    feature_list_.InitAndDisableFeature(
        safe_browsing::kExtendedReportingRemovePrefDependency);
  }

  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();

    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        GetSafeBrowsingServiceFactory());
    // TODO(crbug.com/41437292): Port consumers of the |sb_service_| to use
    // the interface in components/safe_browsing, and remove this cast.
    sb_service_ = static_cast<SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    auto ref_counted_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    sb_service_->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();

    profile_ = std::make_unique<TestingProfile>();
    profile2_ = std::make_unique<TestingProfile>();
  }

  void TearDown() override {
    sb_service_->SetURLLoaderFactoryForTesting(nullptr);
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  Profile* profile() { return profile_.get(); }

  void ResetAndReinitFeatures(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  void SetUpDownload() {
    content::DownloadItemUtils::AttachInfoForTesting(&download_item_, profile(),
                                                     /*web_contents=*/nullptr);
    EXPECT_CALL(download_item_, GetDangerType())
        .WillRepeatedly(Return(download::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST));
    EXPECT_CALL(download_item_, IsDangerous()).WillRepeatedly(Return(true));
    EXPECT_CALL(download_item_, GetURL())
        .WillRepeatedly(ReturnRef(download_url_));

    DownloadProtectionService::SetDownloadProtectionData(
        &download_item_, "download_token",
        ClientDownloadResponse::DANGEROUS_HOST,
        ClientDownloadResponse::TailoredVerdict());
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::BUBBLE_MAINPAGE, WarningAction::SHOWN);
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::BUBBLE_SUBPAGE, WarningAction::CLOSE);
    DownloadItemWarningData::AddWarningActionEvent(
        &download_item_, WarningSurface::DOWNLOADS_PAGE,
        WarningAction::DISCARD);
  }

  std::unique_ptr<ClientSafeBrowsingReportRequest> GetActualRequest(
      const network::ResourceRequest& request) {
    std::string request_string = GetUploadData(request);
    auto actual_request = std::make_unique<ClientSafeBrowsingReportRequest>();
    actual_request->ParseFromString(request_string);
    return actual_request;
  }

  void VerifyDownloadReportRequest(
      ClientSafeBrowsingReportRequest* actual_request) {
    EXPECT_EQ(actual_request->type(),
              ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED);
    EXPECT_EQ(actual_request->download_verdict(),
              ClientDownloadResponse::DANGEROUS_HOST);
    EXPECT_EQ(actual_request->url(), download_url_.spec());
    EXPECT_TRUE(actual_request->did_proceed());
    EXPECT_TRUE(actual_request->show_download_in_folder());
    EXPECT_EQ(actual_request->token(), "download_token");
  }

  void VerifyInteractionOccurrenceCount(
      const ClientSafeBrowsingReportRequest& report,
      const ClientSafeBrowsingReportRequest::PhishySiteInteraction::
          PhishySiteInteractionType& expected_interaction_type,
      const int& expected_occurrence_count) {
    // Find the interaction within the report by comparing
    // security_interstitial_interaction.
    for (auto interaction : report.phishy_site_interactions()) {
      if (interaction.phishy_site_interaction_type() ==
          expected_interaction_type) {
        EXPECT_EQ(interaction.occurrence_count(), expected_occurrence_count);
        break;
      }
    }
  }

  void VerifyPhishySiteReportRequest(
      ClientSafeBrowsingReportRequest* actual_request,
      GURL& expected_url,
      int expected_interaction_size,
      int expected_click_occurrences,
      int expected_key_occurrences,
      int expected_paste_occurrences) {
    EXPECT_EQ(actual_request->type(),
              ClientSafeBrowsingReportRequest::PHISHY_SITE_INTERACTIONS);
    EXPECT_EQ(actual_request->url(), expected_url.spec());
    EXPECT_EQ(actual_request->phishy_site_interactions_size(),
              expected_interaction_size);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_CLICK_EVENT,
        expected_click_occurrences);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_KEY_EVENT,
        expected_key_occurrences);
    VerifyInteractionOccurrenceCount(
        *actual_request,
        ClientSafeBrowsingReportRequest::PhishySiteInteraction::
            PHISHY_PASTE_EVENT,
        expected_paste_occurrences);
  }

  void VerifyDownloadWarningAction(
      const ClientSafeBrowsingReportRequest::DownloadWarningAction&
          warning_action,
      DownloadWarningAction::Surface surface,
      DownloadWarningAction::Action action,
      bool is_terminal_action,
      int64_t interval_msec) {
    EXPECT_EQ(warning_action.surface(), surface);
    EXPECT_EQ(warning_action.action(), action);
    EXPECT_EQ(warning_action.is_terminal_action(), is_terminal_action);
    EXPECT_EQ(warning_action.interval_msec(), interval_msec);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<TestingBrowserProcess> browser_process_;

  scoped_refptr<SafeBrowsingService> sb_service_;
  TestingProfile::Builder profile_builder_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfile> profile2_;
  raw_ptr<TestingProfile> otr_profile_;

  ::testing::NiceMock<download::MockDownloadItem> download_item_;
  GURL download_url_ = GURL(kTestDownloadUrl);

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList feature_list_;
  ChromePingManagerAllowerForTesting allow_ping_manager_;
};

TEST_F(SafeBrowsingServiceTest, SendDownloadReport_Success) {
  base::HistogramTester histogram_tester;
  SetUpDownload();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyDownloadReportRequest(actual_request.get());
        ASSERT_EQ(actual_request->download_warning_actions().size(), 2);
        VerifyDownloadWarningAction(
            actual_request->download_warning_actions().Get(0),
            DownloadWarningAction::BUBBLE_SUBPAGE, DownloadWarningAction::CLOSE,
            /*is_terminal_action=*/false, /*interval_msec=*/0);
        VerifyDownloadWarningAction(
            actual_request->download_warning_actions().Get(1),
            DownloadWarningAction::DOWNLOADS_PAGE,
            DownloadWarningAction::DISCARD,
            /*is_terminal_action=*/true, /*interval_msec=*/0);
        EXPECT_TRUE(actual_request->has_warning_shown_timestamp_msec());
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  sb_service_->SendDownloadReport(
      &download_item_,
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_OPENED,
      /*did_proceed=*/true,
      /*show_download_in_folder=*/true);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSafeBrowsingReport.SendDownloadReportResult",
      PingManager::ReportThreatDetailsResult::SUCCESS, 1);
}

TEST_F(
    SafeBrowsingServiceTest,
    SendDownloadReport_NoDownloadWarningActionWhenExtendedReportingDisabled) {
  base::HistogramTester histogram_tester;
  SetUpDownload();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), false);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        EXPECT_TRUE(actual_request->download_warning_actions().empty());
        EXPECT_FALSE(actual_request->has_warning_shown_timestamp_msec());
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  sb_service_->SendDownloadReport(
      &download_item_,
      ClientSafeBrowsingReportRequest::DANGEROUS_DOWNLOAD_RECOVERY,
      /*did_proceed=*/true,
      /*show_download_in_folder=*/true);
  histogram_tester.ExpectUniqueSample(
      "SafeBrowsing.ClientSafeBrowsingReport.SendDownloadReportResult",
      PingManager::ReportThreatDetailsResult::SUCCESS, 1);
}

TEST_F(SafeBrowsingServiceTest,
       WhenUserIsInSPAndNotPolicyOrSIncognitoReturnsTrue) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest, WhenSPIsSetByPolicyReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Setting Standard protection through policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      safe_browsing::IsSafeBrowsingPolicyManaged(*profile_->GetPrefs()));
  EXPECT_FALSE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest, WhenEPIsSetByPolicyReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Setting Enhanced protection through policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(true));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      safe_browsing::IsSafeBrowsingPolicyManaged(*profile_->GetPrefs()));
  EXPECT_FALSE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest, WhenNoProtectionIsSetByPolicyReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Setting no protection by policy
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnabled, std::make_unique<base::Value>(false));
  profile_->GetTestingPrefService()->SetManagedPref(
      prefs::kSafeBrowsingEnhanced, std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      safe_browsing::IsSafeBrowsingPolicyManaged(*profile_->GetPrefs()));
  EXPECT_FALSE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest, WhenUserIsInEPNormallyReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Set enhanced protection normally
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  EXPECT_FALSE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest, WhenUserIsIncognitoReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Set profile to incognito
  otr_profile_ = std::move(profile_builder_.BuildIncognito(profile_.get()));
  EXPECT_FALSE(
      SafeBrowsingService::IsUserEligibleForESBPromo(otr_profile_.get()));
}

TEST_F(SafeBrowsingServiceTest, WhenUserIsInNoProtectionNormallyoReturnsFalse) {
  // Default scenario: User is in standard protection normally
  EXPECT_TRUE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
  // Set no protection normally
  safe_browsing::SetSafeBrowsingState(
      profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);
  EXPECT_FALSE(SafeBrowsingService::IsUserEligibleForESBPromo(profile()));
}

TEST_F(SafeBrowsingServiceTest,
       SaveExtendedReportingPrefValueOnProfileAddedFeatureFlagEnabled) {
  ResetAndReinitFeatures(
      {safe_browsing::kExtendedReportingRemovePrefDependency}, {});
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);
  sb_service_->OnProfileAdded(profile());
  // Since the user enabled Extended Reporting, the preference value used to
  // record the state of Extended Reporting before its deprecation should be set
  // to true.
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated));
}

TEST_F(SafeBrowsingServiceTest,
       SaveExtendedReportingPrefValueOnProfileAddedFeatureFlagDisabled) {
  // SetUp:
  //   * disable kExtendedReportingRemovePrefDependency
  //   * Setup SBER enabled and
  //   kSafeBrowsingScoutReportingEnabledWhenDeprecated true
  ResetAndReinitFeatures(
      {}, {safe_browsing::kExtendedReportingRemovePrefDependency});
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);

  // Simulate that kSafeBrowsingScoutReportingEnabledWhenDeprecated was set to
  // true previously.
  profile_->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated, true);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated));

  // Add the profile to trigger the function.
  sb_service_->OnProfileAdded(profile());

  // The value of the pref should be reverted to false because the feature is
  // disabled now.
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kSafeBrowsingScoutReportingEnabledWhenDeprecated));
}

TEST_F(SafeBrowsingServiceTest, TestMinAllowedTimeForReferrerChains) {
  sb_service_->OnProfileAdded(profile());
  // If real-time URL lookups are disabled, referrer chains are not allowed.
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            base::Time::Max());

  // Enable ESB and check the timestamp is updated.
  auto time_esb_enabled = base::Time::Now();
  SetEnhancedProtectionPrefForTests(profile_->GetPrefs(), true);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_esb_enabled);
  // Enable MBB and check the timestamp hasn't changed, since ESB is already
  // enabled.
  task_environment_.FastForwardBy(base::Seconds(1));
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_esb_enabled);
  // Disable MBB and check the timestamp hasn't changed, since ESB is still
  // enabled.
  task_environment_.FastForwardBy(base::Seconds(1));
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_esb_enabled);
  // Disable ESB and confirm the timestamp is the max time (referrer chains are
  // not allowed).
  SetEnhancedProtectionPrefForTests(profile_->GetPrefs(), false);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            base::Time::Max());

  // Enable MBB and check the timestamp is updated.
  task_environment_.FastForwardBy(base::Seconds(1));
  auto time_mbb_enabled = base::Time::Now();
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_mbb_enabled);
  // Enable ESB and check the timestamp hasn't changed, since MBB is already
  // enabled.
  task_environment_.FastForwardBy(base::Seconds(1));
  SetEnhancedProtectionPrefForTests(profile_->GetPrefs(), true);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_mbb_enabled);
  // Disable ESB and check the timestamp hasn't changed, since MBB is still
  // enabled.
  task_environment_.FastForwardBy(base::Seconds(1));
  SetEnhancedProtectionPrefForTests(profile_->GetPrefs(), false);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_mbb_enabled);
  // Disable MBB and confirm the timestamp is the max time (referrer chains are
  // not allowed).
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            base::Time::Max());
}

TEST_F(SafeBrowsingServiceTest,
       TestMinAllowedTimeForReferrerChains_EsbEnabledOnStartup) {
  SetEnhancedProtectionPrefForTests(profile_->GetPrefs(), true);
  sb_service_->OnProfileAdded(profile());
  auto time_profile_added = base::Time::Now();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_profile_added);
}

TEST_F(SafeBrowsingServiceTest,
       TestMinAllowedTimeForReferrerChains_MbbEnabledOnStartup) {
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sb_service_->OnProfileAdded(profile());
  auto time_profile_added = base::Time::Now();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_profile_added);
}

TEST_F(SafeBrowsingServiceTest,
       TestMinAllowedTimeForReferrerChains_MultipleProfiles) {
  profile_->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  sb_service_->OnProfileAdded(profile());
  auto time_original_profile_enabled = base::Time::Now();
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_original_profile_enabled);

  auto second_profile = std::make_unique<TestingProfile>();
  second_profile->GetPrefs()->SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  // Checking a profile not currently tracked is not expected, but if it
  // happens, it should not cause a crash.
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(
                second_profile.get()),
            base::Time::Max());
  // Confirm the original profile still returns the original time and the second
  // profile does not.
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_original_profile_enabled);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(
                second_profile.get()),
            base::Time::Max());

  // Now add the second profile and again confirm the two profile return
  // different time stamps.
  task_environment_.FastForwardBy(base::Seconds(1));
  sb_service_->OnProfileAdded(second_profile.get());
  auto time_second_profile_enabled = base::Time::Now();
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            time_original_profile_enabled);
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(
                second_profile.get()),
            time_second_profile_enabled);

  // Triggering profile destruction should reset the timestamp back to the max
  // timestamp.
  sb_service_->OnProfileWillBeDestroyed(profile());
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            base::Time::Max());
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(
                second_profile.get()),
            time_second_profile_enabled);
  sb_service_->OnProfileWillBeDestroyed(second_profile.get());
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(profile()),
            base::Time::Max());
  EXPECT_EQ(sb_service_->GetMinAllowedTimestampForReferrerChains(
                second_profile.get()),
            base::Time::Max());
}

TEST_F(SafeBrowsingServiceTest, EnhancedProtectionPrefChange_SingleProfile) {
  // 1. Create a single profile (using the fixture's profile).
  Profile* profile1 = profile();

  // 2. Create a mock SafeBrowsingPrefChangeHandler.
  auto mock_handler1 =
      std::make_unique<::testing::NiceMock<MockSafeBrowsingPrefChangeHandler>>(
          profile1);

  // 3. Set the expectation: The mock should be called once.
  EXPECT_CALL(*mock_handler1,
              MaybeShowEnhancedProtectionSettingChangeNotification());

  // 4. Add the mock handler to the map.
  sb_service_->pref_change_handlers_map_[profile1] = std::move(mock_handler1);

  // 5. Call the method under test.
  sb_service_->EnhancedProtectionPrefChange(profile1);
}

TEST_F(SafeBrowsingServiceTest,
       EnhancedProtectionPrefChange_SupportsMultipleProfiles) {
  // 1. Create multiple profiles.
  Profile* profile1 = profile();
  Profile* profile2_ptr = profile2_.get();  // Store the pointer before moving

  // 2. Create mock SafeBrowsingPrefChangeHandlers for each profile.
  auto mock_handler1 =
      std::make_unique<::testing::NiceMock<MockSafeBrowsingPrefChangeHandler>>(
          profile1);
  auto mock_handler2 =
      std::make_unique<::testing::NiceMock<MockSafeBrowsingPrefChangeHandler>>(
          profile2_ptr);

  // 3. Set expectations: Each mock should be called once.
  EXPECT_CALL(*mock_handler1.get(),
              MaybeShowEnhancedProtectionSettingChangeNotification());
  EXPECT_CALL(*mock_handler2.get(),
              MaybeShowEnhancedProtectionSettingChangeNotification());

  // 4. Add the mock handlers to the map, associating them with their profiles.
  sb_service_->pref_change_handlers_map_[profile1] = std::move(mock_handler1);
  sb_service_->pref_change_handlers_map_[profile2_ptr] =
      std::move(mock_handler2);

  // 5. Call the method under test for each profile.
  sb_service_->EnhancedProtectionPrefChange(profile1);
  sb_service_->EnhancedProtectionPrefChange(profile2_ptr);
}

class SafeBrowsingServiceAntiPhishingTelemetryTest
    : public SafeBrowsingServiceTest {
 protected:
  PhishySiteInteractionMap SetUpPhishyInteractionMap(
      int expected_click_occurrences,
      int expected_key_occurrences,
      int expected_paste_occurrences) {
    PhishySiteInteractionMap new_map;
    if (expected_click_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_CLICK_EVENT,
          PhishyPageInteractionDetails(
              expected_click_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    if (expected_key_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_KEY_EVENT,
          PhishyPageInteractionDetails(
              expected_key_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    if (expected_paste_occurrences > 0) {
      new_map.insert_or_assign(
          ClientSafeBrowsingReportRequest::PhishySiteInteraction::
              PHISHY_PASTE_EVENT,
          PhishyPageInteractionDetails(
              expected_paste_occurrences,
              base::Time::Now().InMillisecondsSinceUnixEpoch(),
              base::Time::Now().InMillisecondsSinceUnixEpoch()));
    }
    return new_map;
  }
};

TEST_F(SafeBrowsingServiceAntiPhishingTelemetryTest,
       SendPhishyInteractionsReport_Success) {
  const int kExpectedClickEventCount = 5;
  const int kExpectedKeyEventCount = 2;
  const int kExpectedPasteEventCount = 0;
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);
  GURL test_url("http://phishing.com");
  GURL test_page_url("http://page_url.com");
  PhishySiteInteractionMap test_map = SetUpPhishyInteractionMap(
      kExpectedClickEventCount, kExpectedKeyEventCount,
      kExpectedPasteEventCount);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyPhishySiteReportRequest(
            actual_request.get(), test_url, 2, kExpectedClickEventCount,
            kExpectedKeyEventCount, kExpectedPasteEventCount);
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));

  EXPECT_TRUE(sb_service_->SendPhishyInteractionsReport(
      profile(), test_url, test_page_url, test_map));
}

TEST_F(SafeBrowsingServiceAntiPhishingTelemetryTest,
       WhenUserIsIncognitoDontSendReport) {
  // Set profile to incognito
  otr_profile_ = std::move(profile_builder_.BuildIncognito(profile_.get()));

  const int kExpectedClickEventCount = 5;
  const int kExpectedKeyEventCount = 2;
  const int kExpectedPasteEventCount = 0;
  SetExtendedReportingPrefForTests(otr_profile_->GetPrefs(), true);
  GURL test_url("http://phishing.com");
  GURL test_page_url("http://page_url.com");
  PhishySiteInteractionMap test_map = SetUpPhishyInteractionMap(
      kExpectedClickEventCount, kExpectedKeyEventCount,
      kExpectedPasteEventCount);

  EXPECT_FALSE(sb_service_->SendPhishyInteractionsReport(
      otr_profile_.get(), test_url, test_page_url, test_map));
}

class SendNotificationsAcceptedTest : public SafeBrowsingServiceTest {
 public:
  void EnableNotificationsAcceptedFeature() {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kCreateNotificationsAcceptedClientSafeBrowsingReports);
  }

  void DisableNotificationsAcceptedFeature() {
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kCreateNotificationsAcceptedClientSafeBrowsingReports);
  }

  std::unique_ptr<ClientSafeBrowsingReportRequest> GetActualRequest(
      const network::ResourceRequest& request) {
    std::string request_string = GetUploadData(request);
    auto actual_request = std::make_unique<ClientSafeBrowsingReportRequest>();
    actual_request->ParseFromString(request_string);
    return actual_request;
  }

  void VerifyNotificationAcceptedReportRequest(
      ClientSafeBrowsingReportRequest* actual_request,
      GURL& expected_url,
      GURL& expected_page_url,
      GURL& expected_permission_prompt_origin,
      int64_t expected_prompt_duration) {
    EXPECT_EQ(
        actual_request->type(),
        ClientSafeBrowsingReportRequest::NOTIFICATION_PERMISSION_ACCEPTED);
    EXPECT_EQ(actual_request->url(), expected_url.spec());
    EXPECT_EQ(actual_request->page_url(), expected_page_url.spec());
    EXPECT_EQ(actual_request->permission_prompt_info().origin(),
              expected_permission_prompt_origin.spec());
    EXPECT_EQ(actual_request->permission_prompt_info().display_duration_sec(),
              expected_prompt_duration);
  }

  void SetUrlIsAllowlistedForTesting() {
    sb_service_->SetUrlIsAllowlistedForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SendNotificationsAcceptedTest, SendReportForAllowlistedURL) {
  EnableNotificationsAcceptedFeature();
  SetUrlIsAllowlistedForTesting();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);
  GURL notification_url1("http://www.notification1.com/");
  GURL notification_url2("http://www.notification2.com/");
  GURL notification_url3("http://www.notification3.com/");
  base::TimeDelta display_duration = base::Seconds(10);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  bool request_validated = false;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyNotificationAcceptedReportRequest(
            actual_request.get(), notification_url1, notification_url2,
            notification_url3, display_duration.InSeconds());
        request_validated = true;
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  EXPECT_TRUE(sb_service_->MaybeSendNotificationsAcceptedReport(
      nullptr, profile(), notification_url1, notification_url2,
      notification_url3, display_duration));
  EXPECT_TRUE(request_validated);
}

TEST_F(SendNotificationsAcceptedTest,
       DontSendReportForNonExtendedReportingUser) {
  EnableNotificationsAcceptedFeature();
  SetUrlIsAllowlistedForTesting();
  GURL notification_url1("http://www.notification1.com/");
  GURL notification_url2("http://www.notification2.com/");
  GURL notification_url3("http://www.notification3.com/");
  base::TimeDelta display_duration = base::Seconds(10);
  EXPECT_FALSE(sb_service_->MaybeSendNotificationsAcceptedReport(
      nullptr, profile(), notification_url1, notification_url2,
      notification_url3, display_duration));
}

TEST_F(SendNotificationsAcceptedTest, DontSendReportWhenFeatureIsNotEnabled) {
  DisableNotificationsAcceptedFeature();
  SetUrlIsAllowlistedForTesting();
  SetExtendedReportingPrefForTests(profile_->GetPrefs(), true);
  GURL notification_url1("http://www.notification1.com/");
  GURL notification_url2("http://www.notification2.com/");
  GURL notification_url3("http://www.notification3.com/");
  base::TimeDelta display_duration = base::Seconds(10);

  auto* ping_manager =
      ChromePingManagerFactory::GetForBrowserContext(profile());
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        std::unique_ptr<ClientSafeBrowsingReportRequest> actual_request =
            GetActualRequest(request);
        VerifyNotificationAcceptedReportRequest(
            actual_request.get(), notification_url1, notification_url2,
            notification_url3, display_duration.InSeconds());
      }));
  ping_manager->SetURLLoaderFactoryForTesting(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  EXPECT_FALSE(sb_service_->MaybeSendNotificationsAcceptedReport(
      nullptr, profile(), notification_url1, notification_url2,
      notification_url3, display_duration));
}

TEST_F(SendNotificationsAcceptedTest, DontSendReportWhenUserIsIncognito) {
  // Set profile to incognito
  otr_profile_ = std::move(profile_builder_.BuildIncognito(profile_.get()));
  SetExtendedReportingPrefForTests(otr_profile_->GetPrefs(), true);
  EnableNotificationsAcceptedFeature();
  SetUrlIsAllowlistedForTesting();

  GURL notification_url1("http://www.notification1.com/");
  GURL notification_url2("http://www.notification2.com/");
  GURL notification_url3("http://www.notification3.com/");
  base::TimeDelta display_duration = base::Seconds(10);
  EXPECT_FALSE(sb_service_->MaybeSendNotificationsAcceptedReport(
      nullptr, otr_profile_.get(), notification_url1, notification_url2,
      notification_url3, display_duration));
}

class SafeBrowsingServiceMigrationTest : public testing::Test {
 public:
  void SetUp() override {
    browser_process_ = TestingBrowserProcess::GetGlobal();

    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(
        GetSafeBrowsingServiceFactory());
    // TODO(crbug.com/41437292): Port consumers of the |sb_service_| to use
    // the interface in components/safe_browsing, and remove this cast.
    sb_service_ = static_cast<SafeBrowsingService*>(
        safe_browsing::SafeBrowsingService::CreateSafeBrowsingService());
    auto ref_counted_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    sb_service_->SetURLLoaderFactoryForTesting(ref_counted_url_loader_factory);
    browser_process_->SetSafeBrowsingService(sb_service_.get());
    sb_service_->Initialize();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    sb_service_->SetURLLoaderFactoryForTesting(nullptr);
    browser_process_->safe_browsing_service()->ShutDown();
    browser_process_->SetSafeBrowsingService(nullptr);
    safe_browsing::SafeBrowsingServiceInterface::RegisterFactory(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  void MigrateUserToEnhancedBundleIfNeeded(Profile* profile) {
    // MigrateUserToEnhancedBundle will be queued by OnProfileAdded.
    RunOnProfileAddedAndWaitForTasks(profile);
  }

  void MigrateUserToAutomaticJavaScriptBlockingIfNeeded(Profile* profile) {
    // MigrateUserToAutomaticJavaScriptBlocking will be queued by
    // OnProfileAdded.
    RunOnProfileAddedAndWaitForTasks(profile);
  }

  void RunOnProfileAddedAndWaitForTasks(Profile* profile) {
    base::RunLoop run_loop;
    sb_service_->SetAddProfileTasksCompletedClosureForTesting(
        run_loop.QuitClosure());
    sb_service_->OnProfileAdded(profile);
    // Call OnProfileWillBeDestroyed() so that the migration functions can be
    // called for the same profile multiple times in a single test.
    sb_service_->OnProfileWillBeDestroyed(profile);
    run_loop.Run();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  raw_ptr<TestingBrowserProcess> browser_process_;

  scoped_refptr<SafeBrowsingService> sb_service_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
};

class SafeBrowsingServiceEnhancedSecurityBundleMigrationTest
    : public SafeBrowsingServiceMigrationTest {
 public:
  SafeBrowsingServiceEnhancedSecurityBundleMigrationTest() {
    feature_list_.InitAndEnableFeature(kMigrateEnhancedSbUserToEnhancedBundle);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SafeBrowsingServiceEnhancedSecurityBundleMigrationTest,
       MigrationOccursOnce) {
  auto profile = std::make_unique<TestingProfile>();
  PrefService* prefs = profile->GetPrefs();

  {
    SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
    SetSafeBrowsingState(prefs, SafeBrowsingState::ENHANCED_PROTECTION);
    MigrateUserToEnhancedBundleIfNeeded(profile.get());
    EXPECT_EQ(SecuritySettingsBundleSetting::ENHANCED,
              GetSecurityBundleSetting(*prefs));
  }

  {
    SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
    SetSafeBrowsingState(prefs, SafeBrowsingState::ENHANCED_PROTECTION);
    MigrateUserToEnhancedBundleIfNeeded(profile.get());
    EXPECT_EQ(SecuritySettingsBundleSetting::STANDARD,
              GetSecurityBundleSetting(*prefs));
  }
}

TEST_F(SafeBrowsingServiceEnhancedSecurityBundleMigrationTest,
       SetsMigrationToastStateToPending) {
  auto profile = std::make_unique<TestingProfile>();
  PrefService* prefs = profile->GetPrefs();

  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
  SetSafeBrowsingState(prefs, SafeBrowsingState::ENHANCED_PROTECTION);
  MigrateUserToEnhancedBundleIfNeeded(profile.get());
  EXPECT_EQ(
      static_cast<int>(SecuritySettingsBundleToastState::kPending),
      prefs->GetInteger(prefs::kSecuritySettingsBundleMigrationToastState));
}

TEST_F(SafeBrowsingServiceEnhancedSecurityBundleMigrationTest,
       MigrateOnlyOnLaunchAfterFeatureEnabled) {
  auto profile = std::make_unique<TestingProfile>();
  PrefService* prefs = profile->GetPrefs();
  {
    // Migration should not occur because the user has standard safe-browsing.
    SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
    SetSafeBrowsingState(prefs, SafeBrowsingState::STANDARD_PROTECTION);
    MigrateUserToEnhancedBundleIfNeeded(profile.get());
    EXPECT_EQ(SecuritySettingsBundleSetting::STANDARD,
              GetSecurityBundleSetting(*prefs));
  }

  {
    // Migration should not occur because this is not the first browser-launch
    // after the migration-feature was enabled.
    SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
    SetSafeBrowsingState(prefs, SafeBrowsingState::ENHANCED_PROTECTION);
    MigrateUserToEnhancedBundleIfNeeded(profile.get());
    EXPECT_EQ(SecuritySettingsBundleSetting::STANDARD,
              GetSecurityBundleSetting(*prefs));
  }
}

TEST_F(SafeBrowsingServiceEnhancedSecurityBundleMigrationTest,
       DontMigrateIfUsingStandardSafeBrowsing) {
  auto profile = std::make_unique<TestingProfile>();
  PrefService* prefs = profile->GetPrefs();
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
  SetSafeBrowsingState(prefs, SafeBrowsingState::STANDARD_PROTECTION);
  MigrateUserToEnhancedBundleIfNeeded(profile.get());
  EXPECT_EQ(SecuritySettingsBundleSetting::STANDARD,
            GetSecurityBundleSetting(*prefs));
}

class SafeBrowsingServiceEnhancedSecurityBundleMigrationDisabledTest
    : public SafeBrowsingServiceEnhancedSecurityBundleMigrationTest {
 public:
  SafeBrowsingServiceEnhancedSecurityBundleMigrationDisabledTest() = default;
  ~SafeBrowsingServiceEnhancedSecurityBundleMigrationDisabledTest() override =
      default;

  void SetUp() override {
    SafeBrowsingServiceEnhancedSecurityBundleMigrationTest::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        kMigrateEnhancedSbUserToEnhancedBundle);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SafeBrowsingServiceEnhancedSecurityBundleMigrationDisabledTest,
       DontMigrateIfFeatureIsDisabled) {
  auto profile = std::make_unique<TestingProfile>();
  PrefService* prefs = profile->GetPrefs();
  SetSecurityBundleSetting(*prefs, SecuritySettingsBundleSetting::STANDARD);
  SetSafeBrowsingState(prefs, SafeBrowsingState::ENHANCED_PROTECTION);
  MigrateUserToEnhancedBundleIfNeeded(profile.get());
  EXPECT_EQ(SecuritySettingsBundleSetting::STANDARD,
            GetSecurityBundleSetting(*prefs));
}

// Base test fixture for JsOptimizerSettingMigration tests. Provides
// convenience functions for setting and checking the state of the JavaScript
// optimizer setting.
class JsOptimizerSettingMigrationBaseTest
    : public SafeBrowsingServiceMigrationTest {
 public:
  void SetUp() override {
    SafeBrowsingServiceMigrationTest::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    SetJsOptimizerSetting(
        content_settings::JavascriptOptimizerSetting::kAllowed);
    profile()->GetPrefs()->SetBoolean(
        prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites, false);
  }

  TestingProfile* profile() { return profile_.get(); }

  void SetJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting setting) {
    content_settings::GeneratedJavascriptOptimizerPref pref(profile());
    base::Value value(static_cast<int>(setting));
    pref.SetPref(&value);
  }

  void ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting expected_setting) {
    EXPECT_EQ(
        site_protection::ComputeDefaultJavascriptOptimizerSetting(profile()),
        expected_setting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

class JsOptimizerSettingMigrationEnabledTest
    : public JsOptimizerSettingMigrationBaseTest {
 public:
  JsOptimizerSettingMigrationEnabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {content_settings::features::kBlockV8OptimizerOnUnfamiliarSitesSetting,
         /*disabled_features=*/kMigrateToBlockV8OptimizerOnUnfamiliarSites},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(JsOptimizerSettingMigrationEnabledTest,
       WhenUserHasDefaultSetting_UpdatesJsOptimizerSetting) {
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlockedForUnfamiliarSites);
}

TEST_F(JsOptimizerSettingMigrationEnabledTest,
       WhenUserHasDefaultSetting_RecordsThatMigrationHappened) {
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

TEST_F(JsOptimizerSettingMigrationEnabledTest,
       WhenUserHasDisabledSafeBrowsing_MigrationDoesNotHappen) {
  safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                      SafeBrowsingState::NO_SAFE_BROWSING);
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

TEST_F(
    JsOptimizerSettingMigrationEnabledTest,
    WhenUserHasDisabledSafeBrowsingThenEnablesSafeBrowsing_MigrationHappens) {
  safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                      SafeBrowsingState::NO_SAFE_BROWSING);
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  // Unfamiliar sites blocking will not be enabled.
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));

  // Now the user enables Safe Browsing, then restarts Chrome.
  safe_browsing::SetSafeBrowsingState(profile()->GetPrefs(),
                                      SafeBrowsingState::STANDARD_PROTECTION);
  // Simulate restart by re-running the migration code.
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());

  // Ensure that the profile is migrated.
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlockedForUnfamiliarSites);
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

TEST_F(JsOptimizerSettingMigrationEnabledTest, MigrationHappensOnlyOnce) {
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  // Expect that the user was migrated.
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlockedForUnfamiliarSites);
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));

  // Simulate the user setting "allow always".
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kAllowed);

  // Attempt migration again.
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());

  // But this time, setting is not changed.
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
  // But the profile is still marked as migrated.
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

TEST_F(JsOptimizerSettingMigrationEnabledTest,
       WhenUserHasBlockSetting_DoesNotMigrateUser) {
  SetJsOptimizerSetting(content_settings::JavascriptOptimizerSetting::kBlocked);

  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kBlocked);

  // Verify that this profile was not migrated.
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

class JsOptimizerSettingMigrationDisabledTest
    : public JsOptimizerSettingMigrationBaseTest {
 public:
  JsOptimizerSettingMigrationDisabledTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            content_settings::features::
                kBlockV8OptimizerOnUnfamiliarSitesSetting,
        },
        /*disabled_features=*/
        {kMigrateToBlockV8OptimizerOnUnfamiliarSites});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(JsOptimizerSettingMigrationDisabledTest,
       WhenUserHasDefaultSetting_DoesNotMigrate) {
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

class JsOptimizerSettingUnavailableMigrationTest
    : public JsOptimizerSettingMigrationBaseTest {
 public:
  JsOptimizerSettingUnavailableMigrationTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {kMigrateToBlockV8OptimizerOnUnfamiliarSites},
        /*disabled_features=*/
        {
            content_settings::features::
                kBlockV8OptimizerOnUnfamiliarSitesSetting,
        });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(JsOptimizerSettingUnavailableMigrationTest, DoesNotMigrate) {
  MigrateUserToAutomaticJavaScriptBlockingIfNeeded(profile());
  ExpectJsOptimizerSetting(
      content_settings::JavascriptOptimizerSetting::kAllowed);
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kMigratedToJavascriptOptimizerBlockedForUnfamiliarSites));
}

}  // namespace safe_browsing
