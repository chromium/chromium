// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/web_approvals_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/supervised_user/android/website_parent_approval.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

class AsyncResultHolder {
 public:
  AsyncResultHolder() = default;

  AsyncResultHolder(const AsyncResultHolder&) = delete;
  AsyncResultHolder& operator=(const AsyncResultHolder&) = delete;

  ~AsyncResultHolder() = default;

  bool GetResult() {
    run_loop_.Run();
    return result_;
  }

  void SetResult(bool result) {
    result_ = result;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  bool result_ = false;
};

// TODO(agawronska): Check if this can be a real mock.
// Mocks PermissionRequestCreator to test the async responses.
class MockPermissionRequestCreator : public PermissionRequestCreator {
 public:
  MockPermissionRequestCreator() = default;

  MockPermissionRequestCreator(const MockPermissionRequestCreator&) = delete;
  MockPermissionRequestCreator& operator=(const MockPermissionRequestCreator&) =
      delete;

  ~MockPermissionRequestCreator() override {}

  void set_enabled(bool enabled) { enabled_ = enabled; }

  const std::vector<GURL>& requested_urls() const { return requested_urls_; }

  void AnswerRequest(size_t index, bool result) {
    ASSERT_LT(index, requested_urls_.size());
    std::move(callbacks_[index]).Run(result);
    callbacks_.erase(callbacks_.begin() + index);
    requested_urls_.erase(requested_urls_.begin() + index);
  }

 private:
  // PermissionRequestCreator:
  bool IsEnabled() const override { return enabled_; }

  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override {
    ASSERT_TRUE(enabled_);
    requested_urls_.push_back(url_requested);
    callbacks_.push_back(std::move(callback));
  }

  bool enabled_ = false;
  std::vector<GURL> requested_urls_;
  std::vector<SuccessCallback> callbacks_;
};

class MockSupervisedUserSettingsService
    : public ::SupervisedUserSettingsService {
 public:
  MOCK_METHOD1(RecordLocalWebsiteApproval, void(const std::string& host));
};

}  // namespace

class WebApprovalsManagerTest : public ::testing::Test {
 protected:
  WebApprovalsManagerTest() = default;

  WebApprovalsManagerTest(const WebApprovalsManagerTest&) = delete;
  WebApprovalsManagerTest& operator=(const WebApprovalsManagerTest&) = delete;

  ~WebApprovalsManagerTest() override = default;

  WebApprovalsManager& web_approvals_manager() {
    return web_approvals_manager_;
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  void RequestRemoteApproval(const GURL& url,
                             AsyncResultHolder* result_holder) {
    web_approvals_manager_.RequestRemoteApproval(
        url, base::BindOnce(&AsyncResultHolder::SetResult,
                            base::Unretained(result_holder)));
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  WebApprovalsManager web_approvals_manager_;
};

TEST_F(WebApprovalsManagerTest, CreatePermissionRequest) {
  GURL url("http://www.example.com");

  // Without any permission request creators, it should be disabled, and any
  // AddURLAccessRequest() calls should fail.
  EXPECT_FALSE(web_approvals_manager().AreRemoteApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // TODO(agawronska): Check if that can be eliminated by using a mock.
  // Add a disabled permission request creator. This should not change anything.
  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  web_approvals_manager().AddRemoteApprovalRequestCreator(
      base::WrapUnique(creator));

  EXPECT_FALSE(web_approvals_manager().AreRemoteApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Enable the permission request creator. This should enable permission
  // requests and queue them up.
  creator->set_enabled(true);
  EXPECT_TRUE(web_approvals_manager().AreRemoteApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, false);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a second permission request creator.
  MockPermissionRequestCreator* creator_2 = new MockPermissionRequestCreator;
  creator_2->set_enabled(true);
  web_approvals_manager().AddRemoteApprovalRequestCreator(
      base::WrapUnique(creator_2));

  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator succeed. This should make the whole thing succeed.
    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    RequestRemoteApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator fail. This should fall back to the second one.
    creator->AnswerRequest(0, false);
    ASSERT_EQ(1u, creator_2->requested_urls().size());
    EXPECT_EQ(url.spec(), creator_2->requested_urls()[0].spec());

    // Make the second creator succeed, which will make the whole thing succeed.
    creator_2->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(WebApprovalsManagerTest, LocalWebApprovalDurationHistogramTest) {
  base::HistogramTester histogram_tester;

  std::string host = "www.example.com";
  GURL url("http://" + host);
  base::TimeTicks start_time = base::TimeTicks::Now();
  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;

  // Receive a request rejected by the parent with a total duration of 1 minute.
  // Check that duration metric is recorded.
  base::TimeDelta elapsed_time = base::Minutes(1);
  task_environment().FastForwardBy(elapsed_time);
  web_approvals_manager().OnLocalApprovalRequestCompletedAndroid(
      &supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kRejected);

  histogram_tester.ExpectBucketCount(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 1);
  histogram_tester.ExpectTimeBucketCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(),
      elapsed_time, 1);

  // Receive a request canceled by the parent.
  // Check that no duration metric is recorded for incomplete requests.
  web_approvals_manager().OnLocalApprovalRequestCompletedAndroid(
      &supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kIncomplete);
  histogram_tester.ExpectBucketCount(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kCanceled, 1);
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 1);

  // Receive a request accepted by the parent with a total duration of 5
  // minutes. Check that duration metric is recorded.
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(host));

  base::TimeDelta fast_forward_by = base::Minutes(4);
  elapsed_time =
      elapsed_time + fast_forward_by;  // Elapsed time since the start time.
  task_environment().FastForwardBy(fast_forward_by);
  web_approvals_manager().OnLocalApprovalRequestCompletedAndroid(
      &supervisedUserSettingsServiceMock, url, start_time,
      AndroidLocalWebApprovalFlowOutcome::kApproved);
  histogram_tester.ExpectBucketCount(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kApproved, 1);
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalResultHistogram(), 3);

  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 2);
  histogram_tester.ExpectTimeBucketCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(),
      elapsed_time, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(WebApprovalsManagerTest, LocalWebApprovalApprovedChromeOSTest) {
  base::HistogramTester histogram_tester;
  const GURL url("http://www.example.com");

  testing::NiceMock<MockSupervisedUserSettingsService>
      supervisedUserSettingsServiceMock;
  EXPECT_CALL(supervisedUserSettingsServiceMock,
              RecordLocalWebsiteApproval(url.host()));

  auto result = crosapi::mojom::ParentAccessResult::NewApproved(
      crosapi::mojom::ParentAccessApprovedResult::New(
          "TEST_TOKEN", base::Time::FromDoubleT(123456UL)));

  // Capture approval start time and forward clock by the fake approval
  // duration.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeDelta approval_duration = base::Seconds(35);
  task_environment().FastForwardBy(approval_duration);

  web_approvals_manager().OnLocalApprovalRequestCompletedChromeOS(
      &supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  histogram_tester.ExpectUniqueSample(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kApproved, 1);
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 1);
  histogram_tester.ExpectTimeBucketCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(WebApprovalsManagerTest, LocalWebApprovalDeclinedChromeOSTest) {
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

  web_approvals_manager().OnLocalApprovalRequestCompletedChromeOS(
      &supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  histogram_tester.ExpectUniqueSample(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kDeclined, 1);
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 1);
  histogram_tester.ExpectTimeBucketCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(),
      approval_duration, 1);
}

TEST_F(WebApprovalsManagerTest, LocalWebApprovalCanceledChromeOSTest) {
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

  web_approvals_manager().OnLocalApprovalRequestCompletedChromeOS(
      &supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  // Check that the approval duration was NOT recorded for canceled request.
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 0);
  histogram_tester.ExpectUniqueSample(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kCanceled, 1);
}

TEST_F(WebApprovalsManagerTest, LocalWebApprovalErrorChromeOSTest) {
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

  web_approvals_manager().OnLocalApprovalRequestCompletedChromeOS(
      &supervisedUserSettingsServiceMock, url, start_time, std::move(result));

  // Check that the approval duration was NOT recorded on error.
  histogram_tester.ExpectTotalCount(
      WebApprovalsManager::GetLocalApprovalDurationMillisecondsHistogram(), 0);
  histogram_tester.ExpectUniqueSample(
      WebApprovalsManager::GetLocalApprovalResultHistogram(),
      WebApprovalsManager::LocalApprovalResult::kError, 1);
}
#endif
