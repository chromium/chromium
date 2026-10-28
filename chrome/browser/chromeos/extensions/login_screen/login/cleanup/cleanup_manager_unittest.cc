// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/mock_cleanup_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::WithArg;

namespace chromeos {

namespace {

constexpr AccountId::Literal kTestAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test-user@example.com",
                                            GaiaId::Literal("1234567890"));
constexpr char kHandler1Name[] = "Handler1";
constexpr char kHandler2Name[] = "Handler2";

class TestCleanupManager : public CleanupManager {
 public:
  TestCleanupManager() = default;
  ~TestCleanupManager() override = default;

  void InitializeCleanupHandlers() override {}
};

}  // namespace

class CleanupManagerUnittest : public testing::Test {
 public:
  CleanupManagerUnittest() = default;
  ~CleanupManagerUnittest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kTestAccountId));
    user_session_test_environment_->LogIn(kTestAccountId);
    manager_ = std::make_unique<TestCleanupManager>();
  }

  void TearDown() override {
    manager_.reset();
    user_session_test_environment_.reset();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::unique_ptr<TestCleanupManager> manager_;
};

TEST_F(CleanupManagerUnittest, Cleanup) {
  base::HistogramTester histogram_tester;

  auto no_error_callback = [](CleanupHandler::CleanupHandlerCallback callback) {
    std::move(callback).Run(std::nullopt);
  };
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler1 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(AccountId(kTestAccountId), _))
      .WillOnce(WithArg<1>(no_error_callback));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(AccountId(kTestAccountId), _))
      .WillOnce(WithArg<1>(no_error_callback));

  std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.insert({kHandler1Name, std::move(mock_cleanup_handler1)});
  cleanup_handlers.insert({kHandler2Name, std::move(mock_cleanup_handler2)});
  manager_->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::test::TestFuture<std::optional<std::string>> future;
  manager_->Cleanup(future.GetCallback<const std::optional<std::string>&>());
  EXPECT_EQ(std::nullopt, future.Get());

  histogram_tester.ExpectBucketCount(
      "Enterprise.LoginApiCleanup.Handler1.Success", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.LoginApiCleanup.Handler1.Timing", 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LoginApiCleanup.Handler2.Success", 1, 1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.LoginApiCleanup.Handler2.Timing", 1);
}

TEST_F(CleanupManagerUnittest, CleanupInProgress) {
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler =
      std::make_unique<MockCleanupHandler>();
  CleanupHandler::CleanupHandlerCallback callback;
  EXPECT_CALL(*mock_cleanup_handler, Cleanup(AccountId(kTestAccountId), _))
      .WillOnce(WithArg<1>(
          ([&callback](CleanupHandler::CleanupHandlerCallback callback_arg) {
            callback = std::move(callback_arg);
          })));

  std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.insert({kHandler1Name, std::move(mock_cleanup_handler)});
  manager_->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  manager_->Cleanup(
      base::BindLambdaForTesting([&](const std::optional<std::string>& error) {
        EXPECT_EQ(std::nullopt, error);
        barrier_closure.Run();
      }));

  manager_->Cleanup(
      base::BindLambdaForTesting([&](const std::optional<std::string>& error) {
        EXPECT_EQ("Cleanup is already in progress", *error);
        barrier_closure.Run();
      }));

  std::move(callback).Run(std::nullopt);
  run_loop.Run();
}

TEST_F(CleanupManagerUnittest, CleanupErrors) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler1 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(AccountId(kTestAccountId), _))
      .WillOnce(WithArg<1>([](CleanupHandler::CleanupHandlerCallback callback) {
        std::move(callback).Run("Error 1");
      }));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(AccountId(kTestAccountId), _))
      .WillOnce(WithArg<1>([](CleanupHandler::CleanupHandlerCallback callback) {
        std::move(callback).Run("Error 2");
      }));

  std::map<std::string, std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.insert({kHandler1Name, std::move(mock_cleanup_handler1)});
  cleanup_handlers.insert({kHandler2Name, std::move(mock_cleanup_handler2)});
  manager_->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::test::TestFuture<std::optional<std::string>> future;
  manager_->Cleanup(future.GetCallback<const std::optional<std::string>&>());
  EXPECT_EQ("Handler1: Error 1\nHandler2: Error 2", future.Get());

  histogram_tester.ExpectBucketCount(
      "Enterprise.LoginApiCleanup.Handler1.Success", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Enterprise.LoginApiCleanup.Handler2.Success", 0, 1);
}

}  // namespace chromeos
