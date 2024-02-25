// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task_controller.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunClosure;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Invoke;
using testing::Property;

namespace file_manager {
namespace io_task {
namespace {

MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

storage::FileSystemURL CreateFileSystemURL(std::string url) {
  return storage::FileSystemURL::CreateForTest(GURL(url));
}

class IOTaskStatusObserver : public IOTaskController::Observer {
 public:
  MOCK_METHOD(void, OnIOTaskStatus, (const ProgressStatus&), (override));
};

class IOTaskControllerTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment browser_task_environment_;

  IOTaskController io_task_controller_;
};

TEST_F(IOTaskControllerTest, SimpleQueueing) {
  IOTaskStatusObserver observer;
  io_task_controller_.AddObserver(&observer);

  std::vector<storage::FileSystemURL> source_urls{
      CreateFileSystemURL("filesystem:chrome-extension://abc/external/foo/src"),
  };
  auto dest = CreateFileSystemURL(
      "filesystem:chrome-extension://abc/external/foo/dest");

  // All progress statuses should return the same |type|, |source_urls| and
  // |destination_folder| as given, so set up a base matcher to check this.
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kCopy),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest));

  // The controller should synchronously send out a progress status when queued.
  EXPECT_CALL(observer, OnIOTaskStatus(
                            AllOf(Field(&ProgressStatus::state, State::kQueued),
                                  base_matcher)));

  // The controller should also synchronously execute the I/O task, which will
  // send out another status.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kInProgress),
                            base_matcher)));

  // Queue the I/O task, which will also synchronously execute it.
  EXPECT_EQ(0, io_task_controller_.wake_lock_counter_for_tests());
  auto task_id = io_task_controller_.Add(
      std::make_unique<DummyIOTask>(source_urls, dest, OperationType::kCopy));
  EXPECT_EQ(1, io_task_controller_.wake_lock_counter_for_tests());
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());
  EXPECT_EQ(io_task_controller_.TaskStatuses()[0].get().task_id, task_id);

  // Wait for the callbacks posted to the main sequence to finish.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                              Field(&ProgressStatus::state, State::kSuccess),
                              base_matcher)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  EXPECT_TRUE(io_task_controller_.TaskStatuses().empty());

  // Cancel() should have no effect once a task is completed.
  io_task_controller_.Cancel(task_id);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, io_task_controller_.wake_lock_counter_for_tests());

  io_task_controller_.RemoveObserver(&observer);
}

TEST_F(IOTaskControllerTest, Cancel) {
  IOTaskStatusObserver observer;
  io_task_controller_.AddObserver(&observer);

  std::vector<storage::FileSystemURL> source_urls{
      CreateFileSystemURL("filesystem:chrome-extension://abc/external/foo/src"),
  };
  auto dest = CreateFileSystemURL(
      "filesystem:chrome-extension://abc/external/foo/dest");

  // All progress statuses should return the same |type|, |source_urls| and
  // |destination_folder| given, so set up a base matcher to check this.
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kMove),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest));

  // The controller should synchronously send out a progress status when queued.
  EXPECT_CALL(observer, OnIOTaskStatus(
                            AllOf(Field(&ProgressStatus::state, State::kQueued),
                                  base_matcher)));

  // The controller should also synchronously execute the I/O task, which will
  // send out another status.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kInProgress),
                            base_matcher)));

  auto task_id = io_task_controller_.Add(
      std::make_unique<DummyIOTask>(source_urls, dest, OperationType::kMove));
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());
  EXPECT_EQ(io_task_controller_.TaskStatuses()[0].get().task_id, task_id);

  // Cancel should synchronously send a progress status.
  EXPECT_CALL(observer,
              OnIOTaskStatus(AllOf(
                  Field(&ProgressStatus::state, State::kCancelled),
                  Field(&ProgressStatus::task_id, task_id), base_matcher)));

  io_task_controller_.Cancel(task_id);
  EXPECT_TRUE(io_task_controller_.TaskStatuses().empty());

  // No more observer notifications should come after Cancel() as the task is
  // deleted.
  base::RunLoop().RunUntilIdle();

  io_task_controller_.RemoveObserver(&observer);
}

TEST_F(IOTaskControllerTest, PauseResume) {
  IOTaskStatusObserver observer;
  io_task_controller_.AddObserver(&observer);

  std::vector<storage::FileSystemURL> source_urls{
      CreateFileSystemURL("filesystem:chrome-extension://abc/external/foo/src"),
  };
  auto dest = CreateFileSystemURL(
      "filesystem:chrome-extension://abc/external/foo/dest");

  // All progress statuses should return the same |type|, |source_urls| and
  // |destination_folder| given, so set up a base matcher to check this.
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kMove),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest));

  // The controller should synchronously send out a progress status when queued.
  EXPECT_CALL(observer, OnIOTaskStatus(
                            AllOf(Field(&ProgressStatus::state, State::kQueued),
                                  base_matcher)));

  // The controller should also synchronously execute the I/O task, which will
  // send out another status.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kInProgress),
                            base_matcher)));

  auto task_id = io_task_controller_.Add(
      std::make_unique<DummyIOTask>(source_urls, dest, OperationType::kMove));
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());
  EXPECT_EQ(io_task_controller_.TaskStatuses()[0].get().task_id, task_id);

  // Pause should synchronously send a progress status.
  PauseParams pause_params;
  pause_params.policy_params = PolicyPauseParams(policy::Policy::kDlp);
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kPaused),
                            Field(&ProgressStatus::task_id, task_id),
                            Field(&ProgressStatus::pause_params, pause_params),
                            base_matcher)));
  io_task_controller_.Pause(task_id, pause_params);
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());

  // ProgressPausedTasks should synchronously send another paused status update.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kPaused),
                            Field(&ProgressStatus::task_id, task_id),
                            Field(&ProgressStatus::pause_params, pause_params),
                            base_matcher)));
  io_task_controller_.ProgressPausedTasks();

  // Resume should synchronously send a progress status.
  EXPECT_CALL(observer,
              OnIOTaskStatus(AllOf(
                  Field(&ProgressStatus::state, State::kInProgress),
                  Field(&ProgressStatus::task_id, task_id), base_matcher)));
  io_task_controller_.Resume(task_id, ResumeParams());
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());

  // Wait for the task to finish successfully.
  EXPECT_CALL(observer,
              OnIOTaskStatus(AllOf(
                  Field(&ProgressStatus::state, State::kSuccess),
                  Field(&ProgressStatus::task_id, task_id), base_matcher)));
  base::RunLoop().RunUntilIdle();

  // ProgressPausedTasks shouldn't send any more updates after task completes.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kPaused),
                            Field(&ProgressStatus::task_id, task_id),
                            Field(&ProgressStatus::pause_params, pause_params),
                            base_matcher)))
      .Times(0);
  io_task_controller_.ProgressPausedTasks();

  io_task_controller_.RemoveObserver(&observer);
}

TEST_F(IOTaskControllerTest, CompleteWithError) {
  IOTaskStatusObserver observer;
  io_task_controller_.AddObserver(&observer);

  std::vector<storage::FileSystemURL> source_urls{
      CreateFileSystemURL("filesystem:chrome-extension://abc/external/foo/src"),
  };
  auto dest = CreateFileSystemURL(
      "filesystem:chrome-extension://abc/external/foo/dest");

  // All progress statuses should return the same |type|, |source_urls| and
  // |destination_folder| given, so set up a base matcher to check this.
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kMove),
            Field(&ProgressStatus::sources, EntryStatusUrls(source_urls)),
            Property(&ProgressStatus::GetDestinationFolder, dest));

  // The controller should synchronously send out a progress status when queued.
  EXPECT_CALL(observer, OnIOTaskStatus(
                            AllOf(Field(&ProgressStatus::state, State::kQueued),
                                  base_matcher)));

  // The controller should also synchronously execute the I/O task, which will
  // send out another status.
  EXPECT_CALL(observer, OnIOTaskStatus(AllOf(
                            Field(&ProgressStatus::state, State::kInProgress),
                            base_matcher)));

  auto task_id = io_task_controller_.Add(std::make_unique<DummyIOTask>(
      source_urls, dest, OperationType::kMove,
      /*show_notification=*/true, /*progress_succeeds=*/false));
  EXPECT_FALSE(io_task_controller_.TaskStatuses().empty());
  EXPECT_EQ(io_task_controller_.TaskStatuses()[0].get().task_id, task_id);

  // CompleteWithError should synchronously send a progress status.
  EXPECT_CALL(observer,
              OnIOTaskStatus(AllOf(Field(&ProgressStatus::state, State::kError),
                                   Field(&ProgressStatus::task_id, task_id),
                                   Field(&ProgressStatus::policy_error,
                                         PolicyError(PolicyErrorType::kDlp,
                                                     /*blocked_files=*/2)),
                                   base_matcher)));
  io_task_controller_.CompleteWithError(
      task_id, PolicyError(PolicyErrorType::kDlp, /*blocked_files=*/2));

  EXPECT_TRUE(io_task_controller_.TaskStatuses().empty());

  // No more observer notifications should come after CompleteWithError as the
  // task is deleted.
  base::RunLoop().RunUntilIdle();

  io_task_controller_.RemoveObserver(&observer);
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
