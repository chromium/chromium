// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/android/supervised_user_web_content_handler_impl.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#include "chrome/test/base/testing_profile.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
class MockSupervisedUserSettingsService
    : public supervised_user::SupervisedUserSettingsService {
 public:
  MOCK_METHOD1(RecordLocalWebsiteApproval, void(const std::string& host));
};
}  // namespace

// TODO(b/273692421): Extend unit test scope of all the methods in
// SupervisedUserWebContentHandlerImpl.

class SupervisedUserWebContentHandlerImplTest : public ::testing::Test {
 public:
  SupervisedUserWebContentHandlerImplTest() = default;

  SupervisedUserWebContentHandlerImplTest(
      const SupervisedUserWebContentHandlerImplTest&) = delete;
  SupervisedUserWebContentHandlerImplTest& operator=(
      const SupervisedUserWebContentHandlerImplTest&) = delete;

  ~SupervisedUserWebContentHandlerImplTest() override = default;

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }
  TestingProfile* GetProfilePtr() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
};

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalDurationHistogramRejectionTest) {
  base::HistogramTester histogram_tester;

  GURL url("http://www.example.com");
  base::TimeTicks start_time = base::TimeTicks::Now();
  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;

  // Receive a request rejected by the parent with a total duration of 1 minute.
  // Check that duration metric is recorded.
  base::TimeDelta elapsed_time = base::Minutes(1);
  task_environment().FastForwardBy(elapsed_time);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler =
      SupervisedUserWebContentHandlerImpl(web_contents.get(),
                                          content::FrameTreeNodeId(),
                                          /*interstitial_navigation_id=*/0);

  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kRejected);

  histogram_tester.ExpectBucketCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      elapsed_time, 1);
}

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalDurationHistogramCancellationTest) {
  base::HistogramTester histogram_tester;

  GURL url("http://www.example.com");
  base::TimeTicks start_time = base::TimeTicks::Now();
  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;

  base::TimeDelta elapsed_time = base::Minutes(5);
  task_environment().FastForwardBy(elapsed_time);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler =
      SupervisedUserWebContentHandlerImpl(web_contents.get(),
                                          content::FrameTreeNodeId(),
                                          /*interstitial_navigation_id=*/0);

  // Receive a request canceled by the parent.
  // Check that no duration metric is recorded for incomplete requests.
  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kIncomplete);

  histogram_tester.ExpectBucketCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kCanceled, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      0);
}

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalDurationHistogramApprovalTest) {
  base::HistogramTester histogram_tester;

  GURL url("http://www.example.com");
  base::TimeTicks start_time = base::TimeTicks::Now();
  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;

  base::TimeDelta elapsed_time = base::Minutes(5);
  task_environment().FastForwardBy(elapsed_time);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler =
      SupervisedUserWebContentHandlerImpl(web_contents.get(),
                                          content::FrameTreeNodeId(),
                                          /*interstitial_navigation_id=*/0);

  // Receive a request accepted by the parent with a total duration of 5
  // minutes. Check that duration metric is recorded.
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()));
  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kApproved);

  histogram_tester.ExpectBucketCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kApproved, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(), 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      elapsed_time, 1);
}
