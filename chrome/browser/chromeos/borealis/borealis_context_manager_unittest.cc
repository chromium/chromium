// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <memory>

#include "base/containers/queue.h"
#include "chrome/browser/chromeos/borealis/borealis_context_manager_factory.h"
#include "chrome/browser/chromeos/borealis/borealis_task.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {
namespace {

class MockTask : public BorealisTask {
 public:
  explicit MockTask(bool success) : success_(success) {}
  void Run(BorealisContext* context,
           base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(/*should_continue=*/success_);
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

void CallbackForTesting(base::OnceCallback<void(bool)> callback_expectation,
                        const BorealisContext& context) {
  std::move(callback_expectation).Run(context.borealis_running());
}

class CallbackForTestingExpectation {
 public:
  base::OnceCallback<void(bool)> GetCallback() {
    return base::BindOnce(&CallbackForTestingExpectation::Callback,
                          base::Unretained(this));
  }

  MOCK_METHOD(void, Callback, (bool), ());
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
  }

  void TearDown() override {
    chromeos::DBusThreadManager::Shutdown();
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

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

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulTask) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/true));
  context_manager.StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulGroupOfTasks) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/true));
  context_manager.StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulTask) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/false));
  context_manager.StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulGroupOfTasks) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/3, /*success=*/false);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/false));
  context_manager.StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest, MultipleSuccessfulStartupsAllCallbacksRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback_one =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());
  base::OnceCallback<void(const BorealisContext&)> callback_two =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/true);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/true))
      .Times(2);
  context_manager.StartBorealis(std::move(callback_one));
  context_manager.StartBorealis(std::move(callback_two));
  task_environment_.RunUntilIdle();
}

TEST_F(BorealisContextManagerTest,
       MultipleUnsuccessfulStartupsAllCallbacksRan) {
  testing::StrictMock<CallbackForTestingExpectation> callback_expectation;
  base::OnceCallback<void(const BorealisContext&)> callback_one =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());
  base::OnceCallback<void(const BorealisContext&)> callback_two =
      base::BindOnce(CallbackForTesting, callback_expectation.GetCallback());

  BorealisContextManagerImplForTesting context_manager(
      profile_.get(), /*tasks=*/1, /*success=*/false);
  EXPECT_CALL(callback_expectation, Callback(/*expected_success=*/false))
      .Times(2);
  context_manager.StartBorealis(std::move(callback_one));
  context_manager.StartBorealis(std::move(callback_two));
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
