// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_context_manager_impl.h"

#include <memory>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "chrome/browser/ash/guest_os/dbus_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_stability_monitor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

MATCHER(IsSuccessResult, "") {
  return arg.has_value() && arg.value()->vm_name() == "test_vm_name";
}

MATCHER(IsFailureResult, "") {
  return !arg.has_value() &&
         arg.error().error() ==
             borealis::BorealisStartupResult::kStartVmFailed &&
         arg.error().description() == "Something went wrong!";
}

class MockTask : public BorealisTask {
 public:
  explicit MockTask(bool success)
      : BorealisTask("MockTask"), success_(success) {}
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
    for (size_t i = 0; i < tasks_; i++) {
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

using StartupCallbackFactory =
    StrictCallbackFactory<void(BorealisContextManager::ContextOrFailure)>;

class BorealisContextManagerTest : public testing::Test,
                                   protected guest_os::FakeVmServicesHelper {
 public:
  BorealisContextManagerTest() = default;
  BorealisContextManagerTest(const BorealisContextManagerTest&) = delete;
  BorealisContextManagerTest& operator=(const BorealisContextManagerTest&) =
      delete;
  ~BorealisContextManagerTest() override = default;

 protected:
  void SetUp() override {
    CreateProfile();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    profile_.reset();
    histogram_tester_.reset();
  }

  void SendVmStartedSignal() {
    auto* concierge_client = ash::FakeConciergeClient::Get();

    vm_tools::concierge::VmStartedSignal signal;
    signal.set_name("test_vm_name");
    signal.set_owner_id(
        ash::ProfileHelper::GetUserIdHashFromProfile(profile_.get()));
    concierge_client->NotifyVmStarted(signal);
  }

  void SendVmStoppedSignal() {
    auto* concierge_client = ash::FakeConciergeClient::Get();

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
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/0, /*success=*/true);
  EXPECT_CALL(callback_expectation, Call(testing::_))
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ContextOrFailure result) {
            EXPECT_TRUE(result.has_value());
            // Even with no tasks, the context will give the VM a name.
            EXPECT_EQ(result.value()->vm_name(), "borealis");
          }));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulTask) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Call(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulGroupOfTasks) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/true);
  EXPECT_CALL(callback_expectation, Call(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulTask) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation, Call(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulGroupOfTasks) {
  StartupCallbackFactory callback_expectation;
  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/false);
  EXPECT_CALL(callback_expectation, Call(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, MultipleSuccessfulStartupsAllCallbacksRan) {
  StartupCallbackFactory callback_expectation_1;
  StartupCallbackFactory callback_expectation_2;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation_1, Call(IsSuccessResult()));
  EXPECT_CALL(callback_expectation_2, Call(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation_1.BindOnce());
  context_manager.StartBorealis(callback_expectation_2.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest,
       MultipleUnsuccessfulStartupsAllCallbacksRan) {
  StartupCallbackFactory callback_expectation_1;
  StartupCallbackFactory callback_expectation_2;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation_1, Call(IsFailureResult()));
  EXPECT_CALL(callback_expectation_2, Call(IsFailureResult()));
  context_manager.StartBorealis(callback_expectation_1.BindOnce());
  context_manager.StartBorealis(callback_expectation_2.BindOnce());
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
    NeverCompletingTask() : BorealisTask("NeverCompletingTask") {}
    void RunInternal(BorealisContext* context) override {}
  };

  base::queue<std::unique_ptr<BorealisTask>> GetTasks() override {
    base::queue<std::unique_ptr<BorealisTask>> queue;
    queue.push(std::make_unique<NeverCompletingTask>());
    return queue;
  }
};

using ShutdownCallbackFactory =
    StrictCallbackFactory<void(BorealisShutdownResult)>;

TEST_F(BorealisContextManagerTest, ShutDownCancelsRequestsAndTerminatesVm) {
  StartupCallbackFactory callback_expectation;
  EXPECT_CALL(callback_expectation, Call(testing::_))
      .WillOnce(
          testing::Invoke([](BorealisContextManager::ContextOrFailure result) {
            EXPECT_FALSE(result.has_value());
            EXPECT_EQ(result.error().error(),
                      BorealisStartupResult::kCancelled);
          }));

  ShutdownCallbackFactory shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler,
              Call(BorealisShutdownResult::kSuccess));

  NeverCompletingContextManager context_manager(profile_.get());
  context_manager.StartBorealis(callback_expectation.BindOnce());
  context_manager.ShutDownBorealis(shutdown_callback_handler.BindOnce());
  task_environment_.RunUntilIdle();

  ash::FakeConciergeClient* fake_concierge_client =
      ash::FakeConciergeClient::Get();
  EXPECT_GE(fake_concierge_client->stop_vm_call_count(), 1);
  histogram_tester_->ExpectTotalCount(kBorealisShutdownNumAttemptsHistogram, 1);
  histogram_tester_->ExpectUniqueSample(kBorealisShutdownResultHistogram,
                                        BorealisShutdownResult::kSuccess, 1);
}

TEST_F(BorealisContextManagerTest, ShutdownWhenNotRunningCompletesImmediately) {
  ShutdownCallbackFactory shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler,
              Call(BorealisShutdownResult::kSuccess));

  NeverCompletingContextManager context_manager(profile_.get());
  context_manager.ShutDownBorealis(shutdown_callback_handler.BindOnce());
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
  ash::FakeConciergeClient* fake_concierge_client =
      ash::FakeConciergeClient::Get();
  fake_concierge_client->set_stop_vm_response(std::move(response));

  ShutdownCallbackFactory shutdown_callback_handler;
  EXPECT_CALL(shutdown_callback_handler, Call(BorealisShutdownResult::kFailed));
  context_manager.ShutDownBorealis(shutdown_callback_handler.BindOnce());
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
      : BorealisTask("TaskThatDoesSomethingAfterCompletion"),
        something_(std::move(something)) {}

  void RunInternal(BorealisContext* context) override {
    Complete(BorealisStartupResult::kSuccess, "");
    std::move(something_).Run();
  }

  base::OnceClosure something_;
};

TEST_F(BorealisContextManagerTest, TasksCanOutliveCompletion) {
  testing::StrictMock<MockContextManager> context_manager(profile_.get());
  StartupCallbackFactory callback_expectation;
  testing::StrictMock<testing::MockFunction<void()>> something_expectation;

  EXPECT_CALL(context_manager, GetTasks).WillOnce(testing::Invoke([&]() {
    base::queue<std::unique_ptr<BorealisTask>> tasks;
    tasks.push(std::make_unique<TaskThatDoesSomethingAfterCompletion>(
        base::BindOnce(&testing::MockFunction<void()>::Call,
                       base::Unretained(&something_expectation))));
    return tasks;
  }));
  EXPECT_CALL(something_expectation, Call());
  EXPECT_CALL(callback_expectation, Call(testing::_));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedWhenNotRunning) {
  SendVmStoppedSignal();
  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedDuringStartup) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  context_manager.StartBorealis(callback_expectation.BindOnce());

  SendVmStoppedSignal();

  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);
}

TEST_F(BorealisContextManagerTest, ShouldNotLogVmStoppedWhenExpected) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Call(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();

  context_manager.ShutDownBorealis(base::DoNothing());
  SendVmStoppedSignal();

  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 0);

  // Wait for shutdown to complete before destructing |shutdown_callback|.
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, LogVmStoppedWhenUnexpected) {
  StartupCallbackFactory callback_expectation;

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Call(IsSuccessResult()));
  context_manager.StartBorealis(callback_expectation.BindOnce());
  task_environment_.RunUntilIdle();

  SendVmStoppedSignal();
  histogram_tester_->ExpectUniqueSample(kBorealisStabilityHistogram,
                                        guest_os::FailureClasses::VmStopped, 1);
}

TEST_F(BorealisContextManagerTest, VmShutsDownAfterChromeCrashes) {
  ash::FakeConciergeClient* fake_concierge_client =
      ash::FakeConciergeClient::Get();

  // Ensure that GetVmInfo returns success - a VM "still running".
  SendVmStartedSignal();

  ExitTypeService::GetInstanceForProfile(profile_.get())
      ->SetLastSessionExitTypeForTest(ExitType::kCrashed);
  BorealisContextManagerImpl context_manager(profile_.get());
  task_environment_.RunUntilIdle();
  EXPECT_GE(fake_concierge_client->stop_vm_call_count(), 1);
}

}  // namespace
}  // namespace borealis
