// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_content_manager.h"

#include <memory>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/ash/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Mock;

namespace policy {

namespace {

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kNonEmptyRestrictionSet = kScreenshotRestricted;
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrivacyScreenReported(
    DlpContentRestriction::kPrivacyScreen,
    DlpRulesManager::Level::kReport);
const DlpContentRestrictionSet kPrintingRestricted(
    DlpContentRestriction::kPrint,
    DlpRulesManager::Level::kBlock);
const DlpContentRestrictionSet kPrintingWarning(DlpContentRestriction::kPrint,
                                                DlpRulesManager::Level::kWarn);

class MockPrivacyScreenHelper : public ash::PrivacyScreenDlpHelper {
 public:
  MOCK_METHOD1(SetEnforced, void(bool));
};

}  // namespace

class DlpContentManagerTest : public testing::Test {
 public:
  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    mock_rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
  }

 protected:
  DlpContentManagerTest()
      : profile_(std::make_unique<TestingProfile>()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(base::WrapUnique(user_manager_)) {}
  DlpContentManagerTest(const DlpContentManagerTest&) = delete;
  DlpContentManagerTest& operator=(const DlpContentManagerTest&) = delete;
  ~DlpContentManagerTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  void SetReportQueueForReportingManager() {
    auto report_queue = std::make_unique<reporting::MockReportQueue>();
    EXPECT_CALL(*report_queue.get(), AddRecord)
        .WillRepeatedly(
            [this](base::StringPiece record, reporting::Priority priority,
                   reporting::ReportQueue::EnqueueCallback callback) {
              DlpPolicyEvent event;
              event.ParseFromString(std::string(record));
              // Don't use this code in a multithreaded env as it can course
              // concurrency issues with the events in the vector.
              events_.push_back(event);
            });
    helper_.GetReportingManager()->GetReportQueueSetter().Run(
        std::move(report_queue));
  }

  void SetupDlpRulesManager() {
    DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&DlpContentManagerTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(DlpRulesManagerFactory::GetForPrimaryProfile());
  }

  void LoginFakeUser() {
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, false /*is_affiliated*/,
            user_manager::USER_TYPE_REGULAR, profile());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                false /* browser_restart */,
                                false /* is_child */);
  }

  DlpContentManager* GetManager() { return helper_.GetContentManager(); }

  TestingProfile* profile() { return profile_.get(); }

  DlpContentManagerTestHelper helper_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::vector<DlpPolicyEvent> events_;
  MockDlpRulesManager* mock_rules_manager_ = nullptr;

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  ash::FakeChromeUserManager* user_manager_;
  user_manager::ScopedUserManager scoped_user_manager_;
};

TEST_F(DlpContentManagerTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(GetManager()->GetOnScreenPresentRestrictions(),
            kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, ConfidentialDataShown) {
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

TEST_F(DlpContentManagerTest, ConfidentialDataVisibilityChanged) {
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

TEST_F(DlpContentManagerTest,
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

TEST_F(DlpContentManagerTest, PrivacyScreenEnforcement) {
  LoginFakeUser();
  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));

  MockPrivacyScreenHelper mock_privacy_screen_helper;
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(true)).Times(1);
  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenEnforced);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 0);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);
  EXPECT_EQ(events_.size(), 1u);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(true)).Times(1);
  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kBlock)));

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 2);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerTest, PrivacyScreenReported) {
  LoginFakeUser();
  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .WillRepeatedly(::testing::Return(src_pattern));

  // Privacy screen should never be enforced.
  MockPrivacyScreenHelper mock_privacy_screen_helper;
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(testing::_)).Times(0);
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();

  helper_.ChangeConfidentiality(web_contents.get(), kPrivacyScreenReported);
  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kReport)));

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  EXPECT_EQ(events_.size(), 1u);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(events_.size(), 2u);
  EXPECT_THAT(events_[1],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrivacyScreen,
                  DlpRulesManager::Level::kReport)));

  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 0);
  EXPECT_EQ(events_.size(), 2u);
}

TEST_F(DlpContentManagerTest, PrintingRestricted) {
  LoginFakeUser();

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);

  EXPECT_FALSE(GetManager()->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  SetReportQueueForReportingManager();
  SetupDlpRulesManager();
  const std::string src_pattern("example.com");
  EXPECT_CALL(*mock_rules_manager_, GetSourceUrlPattern(_, _, _))
      .Times(1)
      .WillOnce(::testing::Return(src_pattern));

  helper_.ChangeConfidentiality(web_contents.get(), kPrintingRestricted);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingRestricted);
  EXPECT_TRUE(GetManager()->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  EXPECT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0],
              IsDlpPolicyEvent(CreateDlpPolicyEvent(
                  src_pattern, DlpRulesManager::Restriction::kPrinting,
                  DlpRulesManager::Level::kBlock)));

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(GetManager()->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 2);
}

TEST_F(DlpContentManagerTest, PrintingWarning) {
  LoginFakeUser();

  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);

  EXPECT_FALSE(GetManager()->ShouldWarnBeforePrinting(web_contents.get()));

  SetupDlpRulesManager();

  helper_.ChangeConfidentiality(web_contents.get(), kPrintingWarning);
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kPrintingWarning);
  EXPECT_TRUE(GetManager()->ShouldWarnBeforePrinting(web_contents.get()));
  EXPECT_FALSE(GetManager()->IsPrintingRestricted(web_contents.get()));

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(GetManager()->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(GetManager()->ShouldWarnBeforePrinting(web_contents.get()));
}

}  // namespace policy
