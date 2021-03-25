// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"

#include <memory>

#include "ash/public/cpp/privacy_screen_dlp_helper.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/reporting/client/mock_report_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy {

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kScreenshotRestricted(
    DlpContentRestriction::kScreenshot);
const DlpContentRestrictionSet kNonEmptyRestrictionSet = kScreenshotRestricted;
const DlpContentRestrictionSet kPrivacyScreenEnforced(
    DlpContentRestriction::kPrivacyScreen);
const DlpContentRestrictionSet kPrintingRestricted(
    DlpContentRestriction::kPrint);

class MockPrivacyScreenHelper : public ash::PrivacyScreenDlpHelper {
 public:
  MOCK_METHOD1(SetEnforced, void(bool));
};

}  // namespace

class DlpContentManagerTest : public testing::Test {
 protected:
  DlpContentManagerTest() { manager_ = DlpContentManager::Get(); }
  DlpContentManagerTest(const DlpContentManagerTest&) = delete;
  DlpContentManagerTest& operator=(const DlpContentManagerTest&) = delete;
  ~DlpContentManagerTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    SetUpDlpReportingManager();
  }

  void SetUpDlpReportingManager() {
    profile_ = std::make_unique<TestingProfile>();
    reporting_manager_ = new DlpReportingManager();

    DlpReportingManager::SetDlpReportingManagerForTesting(reporting_manager_);
    reporting_manager_->report_queue_ =
        std::make_unique<reporting::MockReportQueue>();
  }

  reporting::MockReportQueue* mock_report_queue() const {
    reporting::MockReportQueue* queue =
        static_cast<reporting::MockReportQueue*>(
            reporting_manager_->report_queue_.get());
    DCHECK(queue);
    return queue;
  }
  std::unique_ptr<content::WebContents> CreateWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                             nullptr);
  }

  DlpContentManagerTestHelper helper_;
  // This points to the DlpContentManager object which is created in the
  // constructor of |helper_|.
  DlpContentManager* manager_ = nullptr;
  DlpReportingManager* reporting_manager_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpContentManagerTest, NoConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, ConfidentialDataShown) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, ConfidentialDataVisibilityChanged) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  helper_.ChangeConfidentiality(web_contents.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest,
       TwoWebContentsVisibilityAndConfidentialityChanged) {
  std::unique_ptr<content::WebContents> web_contents1 = CreateWebContents();
  std::unique_ptr<content::WebContents> web_contents2 = CreateWebContents();
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  // WebContents 1 becomes confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  web_contents2->WasHidden();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  // WebContents 1 becomes non-confidential.
  helper_.ChangeConfidentiality(web_contents1.get(), kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  // WebContents 2 becomes confidential.
  helper_.ChangeConfidentiality(web_contents2.get(), kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);

  web_contents2->WasShown();
  helper_.ChangeVisibility(web_contents2.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kNonEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(),
            kNonEmptyRestrictionSet);

  helper_.DestroyWebContents(web_contents1.get());
  helper_.DestroyWebContents(web_contents2.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents1.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents2.get()),
            kEmptyRestrictionSet);
  EXPECT_EQ(manager_->GetOnScreenPresentRestrictions(), kEmptyRestrictionSet);
}

TEST_F(DlpContentManagerTest, PrivacyScreenEnforcement) {
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

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  web_contents->WasHidden();
  helper_.ChangeVisibility(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(true)).Times(1);
  web_contents->WasShown();
  helper_.ChangeVisibility(web_contents.get());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 1);

  testing::Mock::VerifyAndClearExpectations(&mock_privacy_screen_helper);
  EXPECT_CALL(mock_privacy_screen_helper, SetEnforced(false)).Times(1);
  helper_.DestroyWebContents(web_contents.get());
  task_environment_.FastForwardBy(helper_.GetPrivacyScreenOffDelay());
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, true, 2);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrivacyScreenEnforcedUMA, false, 2);
}

MATCHER_P(MatchEvents, expected, "DlpPolicyEvent equals") {
  DCHECK(expected);
  DlpPolicyEvent event;
  std::string event_str = (std::string)arg;
  event.ParseFromString(event_str);
  bool result = true;
  if (event.destination().url() != expected->destination().url()) {
    *result_listener << "destination urls are not equal "
                     << event.destination().url()
                     << " != " << expected->destination().url();
    result = false;
  }
  if (event.destination().component() != expected->destination().component()) {
    *result_listener << "destination component is not equal "
                     << event.destination().component()
                     << " != " << expected->destination().component();
    result = false;
  }
  if (event.source().url() != expected->source().url()) {
    *result_listener << "source urls are not equal " << event.source().url()
                     << " != " << expected->source().url();
    result = false;
  }
  if (event.restriction() != expected->restriction()) {
    *result_listener << "restrictions are not equal " << event.restriction()
                     << " != " << expected->restriction();
    result = false;
  }
  if (event.mode() != expected->mode()) {
    *result_listener << "modes are not equal " << event.mode()
                     << " != " << expected->mode();
    result = false;
  }
  return result;
}

TEST_F(DlpContentManagerTest, PrintingRestricted) {
  std::unique_ptr<content::WebContents> web_contents = CreateWebContents();
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  DlpPolicyEvent* event_expected =
      CreateDlpPolicyEvent(web_contents.get(), DlpRulesManager::Level::kBlock,
                           DlpRulesManager::Restriction::kPrinting);
  EXPECT_CALL(*mock_report_queue(),
              AddRecord(MatchEvents(event_expected), _, _))
      .Times(1);

  EXPECT_FALSE(manager_->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 0);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  helper_.ChangeConfidentiality(web_contents.get(), kPrintingRestricted);
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kPrintingRestricted);
  EXPECT_TRUE(manager_->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 1);

  helper_.DestroyWebContents(web_contents.get());
  EXPECT_EQ(manager_->GetConfidentialRestrictions(web_contents.get()),
            kEmptyRestrictionSet);
  EXPECT_FALSE(manager_->IsPrintingRestricted(web_contents.get()));
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, true, 1);
  histogram_tester_.ExpectBucketCount(
      GetDlpHistogramPrefix() + dlp::kPrintingBlockedUMA, false, 2);
}
}  // namespace policy
