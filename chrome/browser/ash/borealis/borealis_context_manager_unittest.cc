// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context_manager_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

MATCHER(IsSuccessResult, "") {
  return arg && arg.Value()->vm_name() == "test_vm_name";
}

MATCHER(IsFailureResult, "") {
  return !arg &&
         arg.Error().error() ==
             borealis::BorealisStartupResult::kStartVmFailed &&
         arg.Error().description() == "Something went wrong!";
}

class MockTask : public BorealisTask {
 public:
  explicit MockTask(bool success) : success_(success) {}
  void RunInternal(BorealisContext* context) override {
    if (success_) {
      context->set_vm_name("test_vm_name");
      Complete(borealis::BorealisStartupResult::kSuccess, "");
    } else {
      // Just use a random error.
      Complete(borealis::BorealisStartupResult::kStartVmFailed,
               "Something went wrong!");
    }
  }
  bool success_ = true;
};

class BorealisContextManagerImplForTesting : public BorealisContextManagerImpl {
 public:
  BorealisContextManagerImplForTesting(Profile* profile,
                                       uint tasks,
                                       bool success)
      : BorealisContextManagerImpl(profile), tasks_(tasks), success_(success) {}

 private:
  base::queue<std::unique_ptr<BorealisTask>> GetTasks() override {
    base::queue<std::unique_ptr<BorealisTask>> task_queue;
    for (int i = 0; i < tasks_; i++) {
      if (!success_ && tasks_ > 1 && i == 0) {
        // If we are testing the case for multiple tasks, and at least one of
        // them fails, we want the first task to succeed.
        task_queue.push(std::make_unique<MockTask>(/*success=*/true));
      } else {
        task_queue.push(std::make_unique<MockTask>(/*success=*/success_));
      }
    }
    return (task_queue);
  }

  uint tasks_ = 0;
  bool success_ = true;
};

class ResultCallbackHandler {
 public:
  BorealisContextManager::ResultCallback GetCallback() {
    return base::BindOnce(&ResultCallbackHandler::Callback,
                          base::Unretained(this));
  }
  MOCK_METHOD(void, Callback, (BorealisContextManager::ContextOrFailure), ());
};

class BorealisContextManagerTest : public testing::Test {
 public:
  BorealisContextManagerTest() = default;
  BorealisContextManagerTest(const BorealisContextManagerTest&) = delete;
  BorealisContextManagerTest& operator=(const BorealisContextManagerTest&) =
      delete;
  ~BorealisContextManagerTest() override = default;

 protected:
  void SetUp() override {
    CreateProfile();
    chromeos::DBusThreadManager::Initialize();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    chromeos::DBusThreadManager::Shutdown();
    profile_.reset();
    histogram_tester_.reset();
  }

  void SendVmStoppedSignal() {
    auto* concierge_client = static_cast<chromeos::FakeConciergeClient*>(
        chromeos::DBusThreadManager::Get()->GetConciergeClient());

    vm_tools::concierge::VmStoppedSignal signal;
    signal.set_name("test_vm_name");
    signal.set_owner_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
    concierge_client->NotifyVmStopped(signal);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  void CreateProfile() {
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("defaultprofile");
    profile_ = profile_builder.Build();
  }
};

TEST_F(BorealisContextManagerTest, GetTasksReturnsCorrectTaskList) {
  BorealisContextManagerImpl context_manager(profile_.get());
  base::queue<std::unique_ptr<BorealisTask>> tasks = context_manager.GetTasks();
  EXPECT_FALSE(tasks.empty());
}

TEST_F(BorealisContextManagerTest, NoTasksImpliesSuccess) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/0, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(testing::_))
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ContextOrFailure result) {
            EXPECT_TRUE(result);
            // Even with no tasks, the context will give the VM a name.
            EXPECT_EQ(result.Value()->vm_name(), "borealis");
          }));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulTask) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulGroupOfTasks) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulTask) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation, Callback(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulGroupOfTasks) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;
  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/false);
  EXPECT_CALL(callback_expectation, Callback(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, MultipleSuccessfulStartupsAllCallbacksRan) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation_1;
  testing::StrictMock<ResultCallbackHandler> callback_expectation_2;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation_1, Callback(IsSuccessResult()));
  EXPECT_CALL(callback_expectation_2, Callback(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation_1.GetCallback());
  context_manager.StartBorealis(callback_expectation_2.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest,
       MultipleUnsuccessfulStartupsAllCallbacksRan) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation_1;
  testing::StrictMock<ResultCallbackHandler> callback_expectation_2;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation_1, Callback(IsFailureResult()));
  EXPECT_CALL(callback_expectation_2, Callback(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation_1.GetCallback());
  context_manager.StartBorealis(callback_expectation_2.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsMetricsRecorded) {
  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  context_manager.StartBorealis(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectTotalCount(kBorealisStartupNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(kBorealisStartupResultHistogram,
                                        BorealisStartupResult::kSuccess, 1);
  histogram_tester_->ExpectTotalCount(kBorealisStartupOverallTimeHistogram, 1);
}

TEST_F(BorealisContextManagerTest, StartupFailsMetricsRecorded) {
  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  context_manager.StartBorealis(base::DoNothing());
  task_environment_.RunUntilIdle();

  histogram_tester_->ExpectTotalCount(kBorealisStartupNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(kBorealisStartupResultHistogram,
                                        BorealisStartupResult::kStartVmFailed,
                                        1);
  histogram_tester_->ExpectTotalCount(kBorealisStartupOverallTimeHistogram, 0);
}

class NeverCompletingContextManager : public BorealisContextManagerImpl {
 public:
  explicit NeverCompletingContextManager(Profile* profile)
      : BorealisContextManagerImpl(profile) {}

 private:
  class NeverCompletingTask : public BorealisTask {
   public:
    void RunInternal(BorealisContext* context) override {}
  };

  base::queue<std::unique_ptr<BorealisTask>> GetTasks() override {
    base::queue<std::unique_ptr<BorealisTask>> queue;
    queue.push(std::make_unique<NeverCompletingTask>());
    return queue;
  }
};

class ShutdownCallbackHandler {
 public:
  base::OnceCallback<void(BorealisShutdownResult)> GetCallback() {
    return base::BindOnce(&ShutdownCallbackHandler::Callback,
                          base::Unretained(this));
  }
  MOCK_METHOD(void, Callback, (BorealisShutdownResult), ());
};

TEST_F(BorealisContextManagerTest, ShutDownCancelsRequestsAndTerminatesVm) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;
  EXPECT_CALL(callback_expectation, Callback(testing::_))
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ContextOrFailure result) {
            EXPECT_FALSE(result);
            EXPECT_EQ(result.Error().error(),
                      BorealisStartupResult::kCancelled);
          }));

  ShutdownCallbackHandler shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler,
              Callback(BorealisShutdownResult::kSuccess));

  NeverCompletingContextManager context_manager(profile_.get());
  context_manager.StartBorealis(callback_expectation.GetCallback());
  context_manager.ShutDownBorealis(shutdown_callback_handler.GetCallback());
  task_environment_.RunUntilIdle();

  chromeos::FakeConciergeClient* fake_concierge_client =
      static_cast<chromeos::FakeConciergeClient*>(
          chromeos::DBusThreadManager::Get()->GetConciergeClient());
  EXPECT_TRUE(fake_concierge_client->stop_vm_called());
  histogram_tester_->ExpectTotalCount(kBorealisShutdownNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(kBorealisShutdownResultHistogram,
                                        BorealisShutdownResult::kSuccess, 1);
}

TEST_F(BorealisContextManagerTest, ShutdownWhenNotRunningCompletesImmediately) {
  ShutdownCallbackHandler shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler,
              Callback(BorealisShutdownResult::kSuccess));

  NeverCompletingContextManager context_manager(profile_.get());
  context_manager.ShutDownBorealis(shutdown_callback_handler.GetCallback());
}

TEST_F(BorealisContextManagerTest, FailureToShutdownReportsError) {
  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/0, /*success=*/true);
  context_manager.StartBorealis(base::DoNothing());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(context_manager.IsRunning());

  vm_tools::concierge::StopVmResponse response;
  response.set_success(false);
  response.set_failure_reason("expected failure");
  chromeos::FakeConciergeClient* fake_concierge_client =
      static_cast<chromeos::FakeConciergeClient*>(
          chromeos::DBusThreadManager::Get()->GetConciergeClient());
  fake_concierge_client->set_stop_vm_response(std::move(response));

  ShutdownCallbackHandler shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler,
              Callback(BorealisShutdownResult::kFailed));
  context_manager.ShutDownBorealis(shutdown_callback_handler.GetCallback());
  task_environment_.RunUntilIdle();
}

class MockContextManager : public BorealisContextManagerImpl {
 public:
  explicit MockContextManager(Profile* profile)
      : BorealisContextManagerImpl(profile) {}

  ~MockContextManager() override = default;

  MOCK_METHOD(base::queue<std::unique_ptr<BorealisTask>>, GetTasks, (), ());
};

class TaskThatDoesSomethingAfterCompletion : public BorealisTask {
 public:
  explicit TaskThatDoesSomethingAfterCompletion(base::OnceClosure something)
      : something_(std::move(something)) {}

  void RunInternal(BorealisContext* context) override {
    Complete(BorealisStartupResult::kSuccess, "");
    std::move(something_).Run();
  }

  base::OnceClosure something_;
};

TEST_F(BorealisContextManagerTest, TasksCanOutliveCompletion) {
  testing::StrictMock<MockContextManager> context_manager(profile_.get());
  testing::StrictMock<ResultCallbackHandler> callback_expectation;
  testing::StrictMock<testing::MockFunction<void()>> something_expectation;

  EXPECT_CALL(context_manager, GetTasks).WillOnce(testing::Invoke([&]() {
    base::queue<std::unique_ptr<BorealisTask>> tasks;
    tasks.push(std::make_unique<TaskThatDoesSomethingAfterCompletion>(
        base::BindOnce(&testing::MockFunction<void()>::Call,
                       base::Unretained(&something_expectation))));
    return tasks;
  }));
  EXPECT_CALL(something_expectation, Call());
  EXPECT_CALL(callback_expectation, Callback(testing::_));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedWhenNotRunning) {
  SendVmStoppedSignal();
  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedDuringStartup) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  context_manager.StartBorealis(callback_expectation.GetCallback());

  SendVmStoppedSignal();

  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedWhenExpected) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;
  // No need to verify the shutdown callback for this test case.
  ShutdownCallbackHandler shutdown_callback;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();

  context_manager.ShutDownBorealis(shutdown_callback.GetCallback());
  SendVmStoppedSignal();

  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);

  // Wait for shutdown to complete before destructing |shutdown_callback|.
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, LogVmStoppedWhenUnexpected) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.GetCallback());
  task_environment_.RunUntilIdle();

  SendVmStoppedSignal();
  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 1);
}

TEST_F(BorealisContextManagerTest, VmShutsDownAfterChromeCrashes) {
  chromeos::FakeConciergeClient* fake_concierge_client =
      static_cast<chromeos::FakeConciergeClient*>(
          chromeos::DBusThreadManager::Get()->GetConciergeClient());
  profile_->set_last_session_exited_cleanly(false);
  BorealisContextManagerImpl context_manager(profile_.get());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(fake_concierge_client->stop_vm_called());
}

}  // namespace
}  // namespace borealis
