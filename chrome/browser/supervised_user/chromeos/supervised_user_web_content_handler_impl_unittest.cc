// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_web_content_handler_impl.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/supervised_user/chromeos/mock_large_icon_service.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_favicon_request_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
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
  SupervisedUserWebContentHandlerImplTest() {
    TestingProfile::Builder builder;
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(builder);
  }

  SupervisedUserWebContentHandlerImplTest(
      const SupervisedUserWebContentHandlerImplTest&) = delete;
  SupervisedUserWebContentHandlerImplTest& operator=(
      const SupervisedUserWebContentHandlerImplTest&) = delete;

  ~SupervisedUserWebContentHandlerImplTest() override = default;

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockLargeIconService& large_icon_service() { return large_icon_service_; }
  TestingProfile* GetProfilePtr() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  MockLargeIconService large_icon_service_;
};

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalApprovedChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()));

  auto result = crosapi::mojom::ParentAccessResult::NewApproved(
      crosapi::mojom::ParentAccessApprovedResult::New(
          "TEST_TOKEN", base::Time::FromSecondsSinceUnixEpoch(123456UL)));

  // Capture approval start time and forward clock by the fake approval
  // duration.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(), content::FrameTreeNodeId(),
      /*interstitial_navigation_id=*/0);

  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kApproved, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalDeclinedChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()))
      .Times(0);

  auto result = crosapi::mojom::ParentAccessResult::NewDeclined(
      crosapi::mojom::ParentAccessDeclinedResult::New());

  // Capture approval start time and forward clock by the fake approval
  // duration.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(), content::FrameTreeNodeId(),
      /*interstitial_navigation_id=*/0);

  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      1);
  histogram_tester.ExpectTimeBucketCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalCanceledChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()))
      .Times(0);

  auto result = crosapi::mojom::ParentAccessResult::NewCanceled(
      crosapi::mojom::ParentAccessCanceledResult::New());

  // Capture approval start time and forward clock by the fake approval
  // duration.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(), content::FrameTreeNodeId(),
      /*interstitial_navigation_id=*/0);

  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  // Check that the approval duration was NOT recorded for canceled request.
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      0);
  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kCanceled, 1);
}

TEST_F(SupervisedUserWebContentHandlerImplTest,
       LocalWebApprovalErrorChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()))
      .Times(0);

  auto result = crosapi::mojom::ParentAccessResult::NewError(
      crosapi::mojom::ParentAccessErrorResult::New(
          crosapi::mojom::ParentAccessErrorResult::Type::kUnknown));

  // Capture approval start time and forward clock by the fake approval
  // duration.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(GetProfilePtr()));
  SupervisedUserWebContentHandlerImpl web_content_handler(
      web_contents.get(), url, large_icon_service(), content::FrameTreeNodeId(),
      /*interstitial_navigation_id=*/0);

  web_content_handler.OnLocalApprovalRequestCompleted(
      supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  // Check that the approval duration was NOT recorded on error.
  histogram_tester.ExpectTotalCount(
      supervised_user::WebContentHandler::
          GetLocalApprovalDurationMillisecondsHistogram(),
      0);
  histogram_tester.ExpectUniqueSample(
      supervised_user::WebContentHandler::GetLocalApprovalResultHistogram(),
      supervised_user::WebContentHandler::LocalApprovalResult::kError, 1);
}
