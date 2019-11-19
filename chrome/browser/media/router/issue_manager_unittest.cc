// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::SaveArg;

namespace media_router {
namespace {

IssueInfo CreateTestIssue(IssueInfo::Severity severity) {
  IssueInfo issue("title", IssueInfo::Action::DISMISS, severity);
  issue.message = "message";
  issue.help_page_id = 12345;
  return issue;
}

}  // namespace

class IssueManagerTest : public ::testing::Test {
 protected:
  IssueManagerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    manager_.set_task_runner_for_test(
        task_environment_.GetMainThreadTaskRunner());
  }
  ~IssueManagerTest() override {}

  content::BrowserTaskEnvironment task_environment_;
  IssueManager manager_;
};

TEST_F(IssueManagerTest, AddAndClearIssue) {
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::WARNING);

  // Add initial issue.
  manager_.AddIssue(issue_info1);

  Issue issue1((IssueInfo()));
  MockIssuesObserver observer(&manager_);
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue1));
  observer.Init();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));
  EXPECT_EQ(issue_info1, issue1.info());
  Issue::Id issue1_id = issue1.id();
  EXPECT_FALSE(issue1.info().is_blocking);

  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::FATAL);
  EXPECT_TRUE(issue_info2.is_blocking);

  // Blocking issue takes precedence.
  Issue issue2((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue2));
  manager_.AddIssue(issue_info2);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));
  EXPECT_EQ(issue_info2, issue2.info());

  // Clear |issue2|. Observer will be notified with |issue1| again as it is now
  // the top issue.
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue1));
  manager_.ClearIssue(issue2.id());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));
  EXPECT_EQ(issue1_id, issue1.id());
  EXPECT_EQ(issue_info1, issue1.info());

  // All issues cleared. Observer will be notified with |nullptr| that there are
  // no more issues.
  EXPECT_CALL(observer, OnIssuesCleared());
  manager_.ClearIssue(issue1.id());
}

TEST_F(IssueManagerTest, AddSameIssueInfoHasNoEffect) {
  IssueInfo issue_info = CreateTestIssue(IssueInfo::Severity::WARNING);

  MockIssuesObserver observer(&manager_);
  observer.Init();

  Issue issue((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue));
  manager_.AddIssue(issue_info);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));
  EXPECT_EQ(issue_info, issue.info());

  // Adding the same IssueInfo has no effect.
  manager_.AddIssue(issue_info);

  EXPECT_CALL(observer, OnIssuesCleared());
  manager_.ClearIssue(issue.id());
}

TEST_F(IssueManagerTest, NonBlockingIssuesGetAutoDismissed) {
  MockIssuesObserver observer(&manager_);
  observer.Init();

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::NOTIFICATION);
  manager_.AddIssue(issue_info1);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_TRUE(task_environment_.MainThreadIsIdle());
  task_environment_.FastForwardBy(timeout);

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::WARNING);
  manager_.AddIssue(issue_info2);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  timeout = IssueManager::GetAutoDismissTimeout(issue_info2);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  task_environment_.FastForwardBy(timeout);
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(IssueManagerTest, IssueAutoDismissNoopsIfAlreadyCleared) {
  MockIssuesObserver observer(&manager_);
  observer.Init();

  Issue issue1((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).Times(1).WillOnce(SaveArg<0>(&issue1));
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::NOTIFICATION);
  manager_.AddIssue(issue_info1);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  EXPECT_GT(task_environment_.GetPendingMainThreadTaskCount(), 0u);
  manager_.ClearIssue(issue1.id());

  EXPECT_CALL(observer, OnIssuesCleared()).Times(0);
  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(IssueManagerTest, BlockingIssuesDoNotGetAutoDismissed) {
  MockIssuesObserver observer(&manager_);
  observer.Init();

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::WARNING);
  issue_info1.is_blocking = true;
  manager_.AddIssue(issue_info1);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(0);

  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_TRUE(timeout.is_zero());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);

  // FATAL issues are always blocking.
  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::FATAL);
  manager_.AddIssue(issue_info2);

  timeout = IssueManager::GetAutoDismissTimeout(issue_info2);
  EXPECT_TRUE(timeout.is_zero());
  EXPECT_EQ(task_environment_.GetPendingMainThreadTaskCount(), 0u);
}

TEST_F(IssueManagerTest, ClearNonBlockingIssues) {
  MockIssuesObserver observer(&manager_);
  observer.Init();

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  manager_.AddIssue(CreateTestIssue(IssueInfo::Severity::NOTIFICATION));
  manager_.AddIssue(CreateTestIssue(IssueInfo::Severity::WARNING));

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  manager_.ClearNonBlockingIssues();
}

TEST_F(IssueManagerTest, ClearNonBlockingIssuesDoesNotClearBlockingIssue) {
  MockIssuesObserver observer(&manager_);
  observer.Init();

  // Add a blocking issue and a couple of non-blocking issues.
  Issue blocking_issue((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&blocking_issue));
  manager_.AddIssue(CreateTestIssue(IssueInfo::Severity::FATAL));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer));

  manager_.AddIssue(CreateTestIssue(IssueInfo::Severity::NOTIFICATION));
  manager_.AddIssue(CreateTestIssue(IssueInfo::Severity::WARNING));

  EXPECT_CALL(observer, OnIssuesCleared()).Times(0);

  // The blocking issue remains.
  manager_.ClearNonBlockingIssues();

  Issue same_blocking_issue((IssueInfo()));
  MockIssuesObserver observer2(&manager_);
  EXPECT_CALL(observer2, OnIssue(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&same_blocking_issue));
  observer2.Init();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&observer2));
  EXPECT_EQ(blocking_issue.id(), same_blocking_issue.id());
}

}  // namespace media_router
