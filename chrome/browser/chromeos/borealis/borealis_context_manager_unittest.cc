// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager_factory.h"
#include "chrome/browser/chromeos/borealis/borealis_metrics.h"
#include "chrome/browser/chromeos/borealis/borealis_task.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

MATCHER(IsSuccessResult, "") {
  return arg.Ok() && arg.Success().vm_name() == "test_vm_name";
}

MATCHER(IsFailureResult, "") {
  return !arg.Ok() &&
         arg.Failure() == borealis::BorealisStartupResult::kStartVmFailed &&
         arg.FailureReason() == "Something went wrong!";
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
  MOCK_METHOD(void, Callback, (BorealisContextManager::Result), ());
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
      .WillOnce(testing::Invoke([](BorealisContextManager::Result result) {
        EXPECT_TRUE(result.Ok());
        // Even with no tasks, the context will give the VM a name.
        EXPECT_EQ(result.Success().vm_name(), "borealis");
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

TEST_F(BorealisContextManagerTest, ShutDownCancelsRequestsAndTerminatesVm) {
  testing::StrictMock<ResultCallbackHandler> callback_expectation;
  EXPECT_CALL(callback_expectation, Callback(testing::_))
      .WillOnce(testing::Invoke([](BorealisContextManager::Result result) {
        EXPECT_FALSE(result.Ok());
        EXPECT_EQ(result.Failure(), BorealisStartupResult::kCancelled);
      }));

  NeverCompletingContextManager context_manager(profile_.get());
  context_manager.StartBorealis(callback_expectation.GetCallback());
  context_manager.ShutDownBorealis();
  task_environment_.RunUntilIdle();

  chromeos::FakeConciergeClient* fake_concierge_client =
      static_cast<chromeos::FakeConciergeClient*>(
          chromeos::DBusThreadManager::Get()->GetConciergeClient());
  EXPECT_TRUE(fake_concierge_client->stop_vm_called());
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

class BorealisContextManagerFactoryTest : public testing::Test {
 public:
  BorealisContextManagerFactoryTest() = default;
  BorealisContextManagerFactoryTest(const BorealisContextManagerFactoryTest&) =
      delete;
  BorealisContextManagerFactoryTest& operator=(
      const BorealisContextManagerFactoryTest&) = delete;
  ~BorealisContextManagerFactoryTest() override = default;

 protected:
  void TearDown() override { chromeos::DBusThreadManager::Shutdown(); }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BorealisContextManagerFactoryTest, ReturnsContextManagerForMainProfile) {
  TestingProfile profile;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager;
  auto mock_user_manager =
      std::make_unique<testing::NiceMock<chromeos::MockUserManager>>();
  mock_user_manager->AddUser(
      AccountId::FromUserEmailGaiaId(profile.GetProfileUserName(), "id"));
  scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::move(mock_user_manager));
  chromeos::DBusThreadManager::Initialize();

  BorealisContextManager* context_manager =
      BorealisContextManagerFactory::GetForProfile(&profile);
  EXPECT_TRUE(context_manager);
}

TEST_F(BorealisContextManagerFactoryTest,
       ReturnsNullpointerForSecondaryProfile) {
  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName("defaultprofile");
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  chromeos::DBusThreadManager::Initialize();

  BorealisContextManager* context_manager =
      BorealisContextManagerFactory::GetForProfile(profile.get());
  EXPECT_FALSE(context_manager);
}

}  // namespace
}  // namespace borealis
