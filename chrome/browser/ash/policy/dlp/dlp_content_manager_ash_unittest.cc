// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager_ash.h"

#include <memory>
#include <optional>
#include <string_view>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/mock_dlp_warn_notifier.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;

namespace policy {

namespace {

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";
constexpr char kSrcPattern[] = "example";
constexpr char kRuleName[] = "ruleName";
constexpr char kRuleId[] = "obfuscatedId";
const std::u16string kApplicationName = u"application";
const DlpRulesManager::RuleMetadata kRuleMetadata(kRuleName, kRuleId);

const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenReported(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintingRestricted(
    DlpContentRestriction::kPrint,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kScreenShareRestricted(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kBlock);

const DlpContentRestrictionSet kPrintingWarned(DlpContentRestriction::kPrint,
                                               DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenshotWarned(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kWarn);
const DlpContentRestrictionSet kScreenShareWarned(
    DlpContentRestriction::kScreenShare,
    DlpRulesManager::Level::kWarn);

const DlpContentRestrictionSet kScreenshotReported(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kReport);

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kNonEmptyRestrictionSet = kScreenshotRestricted;

class MockPrivacyScreenHelper : public ash::PrivacyScreenDlpHelper {
 public:
  MOCK_METHOD(bool, IsSupported, (), (const, override));
  MOCK_METHOD(void, SetEnforced, (bool enforced), (override));
};

}  // namespace

using MockWarningCallback =
    testing::StrictMock<base::MockCallback<WarningCallback>>;

class DlpContentManagerAshTest : public testing::Test {
 public:
  DlpContentManagerAshTest(const DlpContentManagerAshTest&) = delete;
  DlpContentManagerAshTest& operator=(const DlpContentManagerAshTest&) = delete;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
        Profile::FromBrowserContext(context));
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

  MockDlpWarnNotifier* CreateAndSetDlpWarnNotifier(bool should_proceed) {
    std::unique_ptr<MockDlpWarnNotifier> wrapper =
        std::make_unique<MockDlpWarnNotifier>(should_proceed);
    MockDlpWarnNotifier* mock_dlp_warn_notifier = wrapper.get();
    helper_.SetWarnNotifierForTesting(std::move(wrapper));
    return mock_dlp_warn_notifier;
  }

  // Checks that there is an expected number of blocked/not blocked and
  // warned/not warned data points. Number of not blocked and not warned data
  // points is the difference between |total_count| and |blocked_count| and
  // |warned_count| respectfully.
  void VerifyHistogramCounts(int blocked_count,
                             int warned_count,
                             int total_count,
                             std::string blocked_suffix,
                             std::string warned_suffix) {
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + blocked_suffix, true,
        blocked_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + blocked_suffix, false,
        total_count - blocked_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + warned_suffix, true,
        warned_count);
    histogram_tester_.ExpectBucketCount(
        data_controls::GetDlpHistogramPrefix() + warned_suffix, false,
        total_count - warned_count);
  }

 protected:
  DlpContentManagerAshTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_.get())) {}
  ~DlpContentManagerAshTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContents() {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    // `DlpContentTabHelper` is responsible for clearing a destroyed
    // `WebContents` from `DlpContentManager`'s list of `WebContents`;
    // instantiate it here to make sure that cleanup happens even in unit
    // tests.
    DlpContentTabHelper::MaybeCreateForWebContents(web_contents.get());
    CHECK(DlpContentTabHelper::FromWebContents(web_contents.get()));
    return web_contents;
  }

  void SetUp() override {
    testing::Test::SetUp();

    ASSERT_TRUE(profile_manager_.SetUp());
    LoginFakeUser();
    SetReportQueueForReportingManager();
    SetupDlpRulesManager();

    EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
        .WillRepeatedly(::testing::Return(true));
  }

  void TearDown() override {
    testing::Test::TearDown();

    helper_.ResetWarnNotifierForTesting();
  }

  void SetReportQueueForReportingManager() {
    auto report_queue = std::unique_ptr<::reporting::MockReportQueue,
                                        base::OnTaskRunnerDeleter>(
        new ::reporting::MockReportQueue(),
        base::OnTaskRunnerDeleter(
            base::ThreadPool::CreateSequencedTaskRunner({})));
    EXPECT_CALL(*report_queue.get(), AddRecord)
        .WillRepeatedly(
            [this](std::string_view record, ::reporting::Priority priority,
                   ::reporting::ReportQueue::EnqueueCallback callback) {
              DlpPolicyEvent event;
              event.ParseFromString(std::string(record));
              // Don't use this code in a multithreaded env as it can course
              // concurrency issues with the events in the vector.
              events_.push_back(event);
            });
    helper_.GetReportingManager()->SetReportQueueForTest(
        std::move(report_queue));
  }

  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpContentManagerAshTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  DlpContentManagerAsh* GetManager() {
    return static_cast<DlpContentManagerAsh*>(helper_.GetContentManager());
  }

  TestingProfile* profile() { return profile_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<::reporting::ReportingClient::TestEnvironment>
      test_reporting_ = ::reporting::ReportingClient::TestEnvironment::
          CreateWithStorageModule(
              base::MakeRefCounted<::reporting::test::TestStorageModule>());
  DlpContentManagerTestHelper helper_;
  base::HistogramTester histogram_tester_;
  std::vector<DlpPolicyEvent> events_;
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> mock_rules_manager_ = nullptr;
  MockPrivacyScreenHelper mock_privacy_screen_helper_;

 private:
  void LoginFakeUser() {
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);

    profile_ = profile_manager_.CreateTestingProfile(account_id.GetUserEmail());
    profile_->SetIsNewProfile(true);

    user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id, false /*is_affiliated*/, user_manager::UserType::kRegular,
        profile_);
    user_manager_->LoginUser(account_id, true /*set_profile_created_flag*/);

    EXPECT_EQ(ProfileManager::GetActiveUserProfile(), profile_);
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged> user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(DlpContentManagerAshTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, ConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, ConfidentialDataVisibilityChanged) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest,
       TwoWebContentsVisibilityAndConfidentialityChanged) {
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  // WebContents 1 becomes confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents2->WasHidden();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  // WebContents 1 becomes non-confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  // WebContents 2 becomes confidential.
  helper_.ChangeConfidentiality(web_contents2.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  web_contents2->WasShown();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents1.get());
  helper_.DestroyWebContents(web_contents2.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, PrivacyScreenEnforcement) {
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _, _))
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(src_pattern)));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(true)).Times(1);
  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      false, 0);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          src_pattern, DlpRulesManager::Restriction::kPrivacyScreen, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(false)).Times(1);
  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      false, 1);
  EXPECT_EQ(events_.size(), 1u);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(true)).Times(1);
  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      true, 2);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      false, 1);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          src_pattern, DlpRulesManager::Restriction::kPrivacyScreen, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper_);
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(false)).Times(1);
  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      true, 2);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      false, 2);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshTest, PrivacyScreenReported) {
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _, _))
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(src_pattern)));

  // Privacy screen should never be enforced.
  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(true));
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenReported);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          src_pattern, DlpRulesManager::Restriction::kPrivacyScreen, kRuleName,
          kRuleId, DlpRulesManager::Level::kReport)));

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  EXPECT_EQ(events_.size(), 1u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          src_pattern, DlpRulesManager::Restriction::kPrivacyScreen, kRuleName,
          kRuleId, DlpRulesManager::Level::kReport)));

  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      true, 0);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrivacyScreenEnforcedUMA,
      false, 0);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerAshTest,
       PrivacyScreenNotEnforcedAndReportedOnUnsupportedDevice) {
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));

  EXPECT_CALL(mock_privacy_screen_helper_, IsSupported())
      .WillRepeatedly(::testing::Return(false));

  // Privacy screen should never be enforced or reported.
  EXPECT_CALL(mock_privacy_screen_helper_, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);
  EXPECT_EQ(events_.size(), 0u);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  EXPECT_EQ(events_.size(), 0u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(events_.size(), 0u);

  helper_.DestroyWebContents(web_contents.get());
}

TEST_F(DlpContentManagerAshTest, VideoCaptureReportDuringRecording) {
  const GURL kSrcUrl = GURL("https://example.com/");
  const GURL kGoogleUrl = GURL("https://google.com/");
  // Return |kSrcPattern| for reporting for both |kSrcUrl| and |kGoogleUrl|.
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  // Setup two web contents with different urls.
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  content::WebContentsTester::For(web_contents1.get())
      ->NavigateAndCommit(kSrcUrl);
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  content::WebContentsTester::For(web_contents2.get())
      ->NavigateAndCommit(kGoogleUrl);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);

  // WebContents 1 becomes confidential. No reporting expected.
  helper_.ChangeConfidentiality(web_contents1.get(), kScreenshotReported);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kScreenshotReported);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kScreenshotReported);
  EXPECT_TRUE(events_.empty());

  // Simulate starting video capture. Expect report event from WebContents 1.
  const ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();
  GetManager()->OnVideoCaptureStarted(area);
  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          web_contents1->GetLastCommittedURL().spec(),
          DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
          DlpRulesManager::Level::kReport)));

  // WebContents 2 becomes confidential. Expect report event from WebContents 2.
  helper_.ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  helper_.ChangeConfidentiality(web_contents2.get(), kScreenshotReported);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kScreenshotReported);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kScreenshotReported);
  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          web_contents2->GetLastCommittedURL().spec(),
          DlpRulesManager::Restriction::kScreenshot, kRuleName, kRuleId,
          DlpRulesManager::Level::kReport)));

  // Remove confidentiality for both web contents.
  helper_.ChangeConfidentiality(web_contents2.get(), kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
  EXPECT_EQ(events_.size(), 2u);

  // Both web contents become confidential. Expect no reporting event because
  // both urls were already reported during the current capture.
  helper_.ChangeConfidentiality(web_contents1.get(), kScreenshotReported);
  helper_.ChangeConfidentiality(web_contents2.get(), kScreenshotReported);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents1.get()),
            kScreenshotReported);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents2.get()),
            kScreenshotReported);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kScreenshotReported);
  EXPECT_EQ(events_.size(), 2u);

  GetManager()->CheckStoppedVideoCapture(base::DoNothing());
  EXPECT_EQ(events_.size(), 2u);

  // Remove confidentiality to avoid race condition in test case
  // deinitialization.
  helper_.ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  helper_.ChangeConfidentiality(web_contents2.get(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerAshTest, PrintingRestricted) {
  // Needs to be set because CheckPrintingRestriction() will show the blocked
  // notification.
  NotificationDisplayServiceTester display_service_tester(profile());
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(1);   // No restrictions.
  EXPECT_CALL(cb, Run(false)).Times(1);  // Block restriction.
  EXPECT_CALL(cb, Run(true)).Times(1);   // WebContents destroyed.

  // No restrictions are enforced for web_contents: allow.
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);

  // Block restriction is enforced for web_contents: block.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingRestricted);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, PrintingWarnedProceeded) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/true);
  // The warning should be shown only once.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(3)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  EXPECT_CALL(cb, Run(true)).Times(3);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingWarned);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());

  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId)));
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrintingWarnProceededUMA,
      true, 1);

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());

  EXPECT_EQ(events_.size(), 3u);
  EXPECT_THAT(
      events_[2],
      data_controls::IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId)));
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrintingWarnProceededUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrintingWarnSilentProceededUMA,
      true, 1);

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());

  EXPECT_EQ(events_.size(), 3u);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, PrintingWarnedCancelled) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/false);
  // If the user cancels, the warning can be shown again for the same contents.
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(2);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(false)).Times(2);  // Action canceled after the warning.
  EXPECT_CALL(cb, Run(true)).Times(1);   // WebContents destroyed.

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kPrintingWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingWarned);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrintingWarnProceededUMA,
      false, 1);

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kPrinting, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kPrintingWarnProceededUMA,
      false, 2);

  // Web contents are destroyed: allow, no dialog is shown.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckPrintingRestriction(web_contents.get(), rfh_id, cb.Get());
  EXPECT_EQ(events_.size(), 2u);
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kPrintingBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kPrintingWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, CaptureModeInitRestricted) {
  // Needs to be set because CheckCaptureModeInitRestriction() will show the
  // blocked notification.
  NotificationDisplayServiceTester display_service_tester(profile());
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(1);   // No restrictions enforced.
  EXPECT_CALL(cb, Run(false)).Times(1);  // Block restriction.
  EXPECT_CALL(cb, Run(true)).Times(1);   // WebContents destroyed.

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);

  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotRestricted);
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, CaptureModeInitWarnedContinued) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/true);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(2);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());

  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      true, 1);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      true, 1);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnSilentProceededUMA,
      true, 1);
  EXPECT_EQ(events_.size(), 1u);
}

TEST_F(DlpContentManagerAshTest, CaptureModeInitWarnedCancelled) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/false);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(2);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(false)).Times(2);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 1);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckCaptureModeInitRestriction(cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kCaptureModeInitBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kCaptureModeInitWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 2);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
}

TEST_F(DlpContentManagerAshTest, ScreenshotRestricted) {
  // Needs to be set because CheckScreenshotRestriction() will show the blocked
  // notification.
  NotificationDisplayServiceTester display_service_tester(profile());
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(1);   // No restrictions enforced.
  EXPECT_CALL(cb, Run(false)).Times(1);  // Block restriction.
  EXPECT_CALL(cb, Run(true)).Times(1);   // WebContents destroyed.

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);

  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotRestricted);
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, ScreenshotWarnedContinued) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/true);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  EXPECT_CALL(cb, Run(true)).Times(2);

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      true, 1);
  EXPECT_EQ(events_.size(), 1u);
}

TEST_F(DlpContentManagerAshTest, ScreenshotWarnedCancelled) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/false);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(2);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  EXPECT_CALL(cb, Run(false)).Times(2);

  ScreenshotArea area = ScreenshotArea::CreateForAllRootWindows();
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenshotWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenshotWarned);
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 1);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckScreenshotRestriction(area, cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenshotBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenshotWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenshotWarnProceededUMA,
      false, 2);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenshot, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
}

TEST_F(DlpContentManagerAshTest, ScreenShareRestricted) {
  // Needs to be set because CheckScreenShareRestriction() will show the blocked
  // notification.
  NotificationDisplayServiceTester display_service_tester(profile());
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  testing::InSequence s;
  EXPECT_CALL(cb, Run(true)).Times(1);   // No restrictions enforced.
  EXPECT_CALL(cb, Run(false)).Times(1);  // Block restriction.
  EXPECT_CALL(cb, Run(true)).Times(1);   // WebContents destroyed.

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/0,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  helper_.ChangeConfidentiality(web_contents.get(), kScreenShareRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenShareRestricted);
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenShare, kRuleName,
          kRuleId, DlpRulesManager::Level::kBlock)));

  // Web contents are destroyed: allow.
  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/1, /*warned_count=*/0,
      /*total_count=*/3,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
}

TEST_F(DlpContentManagerAshTest, ScreenShareWarnedContinued) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/true);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(1)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  EXPECT_CALL(cb, Run(true)).Times(2);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));

  // Warn restriction is enforced: allow and remember that the user proceeded.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenShareWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenShareWarned);
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenShare, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: allow based on cached user's response - no dialog is shown.
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      true, 1);
  EXPECT_EQ(events_.size(), 1u);
}

TEST_F(DlpContentManagerAshTest, ScreenShareWarnedCancelled) {
  MockDlpWarnNotifier* mock_dlp_warn_notifier =
      CreateAndSetDlpWarnNotifier(/*should_proceed=*/false);
  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(2);

  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern)
      .Times(2)
      .WillRepeatedly(testing::DoAll(::testing::SetArgPointee<3>(kRuleMetadata),
                                     ::testing::Return(kSrcPattern)));

  MockWarningCallback cb;
  EXPECT_CALL(cb, Run(false)).Times(2);

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  const content::DesktopMediaID media_id(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(
          web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
          web_contents->GetPrimaryMainFrame()->GetRoutingID()));

  // Warn restriction is enforced: reject since the user canceled.
  helper_.ChangeConfidentiality(web_contents.get(), kScreenShareWarned);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kScreenShareWarned);
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/1,
      /*total_count=*/1,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      false, 1);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenShare, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));

  // Check again: since the user previously cancelled, dialog is shown again.
  GetManager()->CheckScreenShareRestriction(media_id, kApplicationName,
                                            cb.Get());
  VerifyHistogramCounts(
      /*blocked_count=*/0, /*warned_count=*/2,
      /*total_count=*/2,
      /*blocked_suffix=*/data_controls::dlp::kScreenShareBlockedUMA,
      /*warned_suffix=*/data_controls::dlp::kScreenShareWarnedUMA);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kScreenShareWarnProceededUMA,
      false, 2);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kSrcPattern, DlpRulesManager::Restriction::kScreenShare, kRuleName,
          kRuleId, DlpRulesManager::Level::kWarn)));
}

TEST_F(DlpContentManagerAshTest, OnWindowRestrictionChanged) {}

}  // namespace policy
