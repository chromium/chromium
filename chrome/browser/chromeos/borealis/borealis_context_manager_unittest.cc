// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager_impl.h"

#include <memory>

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

void CallbackForTesting(bool expected_success,
                        bool* callback_ran,
                        const BorealisContext& context) {
  EXPECT_EQ(context.borealis_running(), expected_success);
  *callback_ran = true;
}

class BorealisContextManagerTest : public testing::Test {
 public:
  BorealisContextManagerTest() = default;
  BorealisContextManagerTest(const BorealisContextManagerTest&) = delete;
  BorealisContextManagerTest& operator=(const BorealisContextManagerTest&) =
      delete;
  ~BorealisContextManagerTest() override = default;

 protected:
  void SetUp() override {
    auto mock_user_manager =
        std::make_unique<testing::NiceMock<chromeos::MockUserManager>>();
    mock_user_manager->AddUser(
        AccountId::FromUserEmailGaiaId(profile_.GetProfileUserName(), "id"));
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(mock_user_manager));
    chromeos::DBusThreadManager::Initialize();
    context_manager_ = BorealisContextManagerFactory::GetForProfile(&profile_);
    callback_ran_ = false;
  }

  void TearDown() override { chromeos::DBusThreadManager::Shutdown(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  BorealisContextManager* context_manager_;
  bool callback_ran_ = false;
};

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulTask) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  base::OnceCallback<void(const BorealisContext&)> callback = base::BindOnce(
      CallbackForTesting, /*expected_success=*/true, &callback_ran_);

  context_manager_->StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran_);
}

TEST_F(BorealisContextManagerTest, StartupSucceedsForSuccessfulGroupOfTasks) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  base::OnceCallback<void(const BorealisContext&)> callback = base::BindOnce(
      CallbackForTesting, /*expected_success=*/true, &callback_ran_);

  context_manager_->StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran_);
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulTask) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/false));
  base::OnceCallback<void(const BorealisContext&)> callback = base::BindOnce(
      CallbackForTesting, /*expected_success=*/false, &callback_ran_);

  context_manager_->StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran_);
}

TEST_F(BorealisContextManagerTest, StartupFailsForUnsuccessfulGroupOfTasks) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/false));
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  base::OnceCallback<void(const BorealisContext&)> callback = base::BindOnce(
      CallbackForTesting, /*expected_success=*/false, &callback_ran_);

  context_manager_->StartBorealis(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran_);
}

TEST_F(BorealisContextManagerTest, MultipleSuccessfulStartupsAllCallbacksRan) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/true));
  bool callback_one_ran = false;
  bool callback_two_ran = false;
  base::OnceCallback<void(const BorealisContext&)> callback_one =
      base::BindOnce(CallbackForTesting, /*expected_success=*/true,
                     &callback_one_ran);
  base::OnceCallback<void(const BorealisContext&)> callback_two =
      base::BindOnce(CallbackForTesting, /*expected_success=*/true,
                     &callback_two_ran);

  context_manager_->StartBorealis(std::move(callback_one));
  context_manager_->StartBorealis(std::move(callback_two));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_one_ran);
  EXPECT_TRUE(callback_two_ran);
}

TEST_F(BorealisContextManagerTest,
       MultipleUnsuccessfulStartupsAllCallbacksRan) {
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/false));
  context_manager_->AddTaskForTesting(
      std::make_unique<MockTask>(/*success=*/false));
  bool callback_one_ran = false;
  bool callback_two_ran = false;
  base::OnceCallback<void(const BorealisContext&)> callback_one =
      base::BindOnce(CallbackForTesting, /*expected_success=*/false,
                     &callback_one_ran);
  base::OnceCallback<void(const BorealisContext&)> callback_two =
      base::BindOnce(CallbackForTesting, /*expected_success=*/false,
                     &callback_two_ran);

  context_manager_->StartBorealis(std::move(callback_one));
  context_manager_->StartBorealis(std::move(callback_two));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_one_ran);
  EXPECT_TRUE(callback_two_ran);
}
}  // namespace
}  // namespace borealis
