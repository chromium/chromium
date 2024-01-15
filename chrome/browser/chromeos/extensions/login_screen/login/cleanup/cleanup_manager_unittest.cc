// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/mock_cleanup_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace chromeos {

namespace {

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
    manager_ = std::make_unique<TestCleanupManager>();
  }

  void TearDown() override {
    manager_.reset();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestCleanupManager> manager_;
};

TEST_F(CleanupManagerUnittest, Cleanup) {
  base::HistogramTester histogram_tester;

  auto no_error_callback = [](CleanupHandler::CleanupHandlerCallback callback) {
    std::move(callback).Run(std::nullopt);
  };
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler1 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(no_error_callback)));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(no_error_callback)));

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
  EXPECT_CALL(*mock_cleanup_handler, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(
          ([&callback](CleanupHandler::CleanupHandlerCallback callback_arg) {
            callback = std::move(callback_arg);
          }))));

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
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(_))
      .WillOnce(WithArg<0>(
          Invoke([](CleanupHandler::CleanupHandlerCallback callback) {
            std::move(callback).Run("Error 1");
          })));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(_))
      .WillOnce(WithArg<0>(
          Invoke([](CleanupHandler::CleanupHandlerCallback callback) {
            std::move(callback).Run("Error 2");
          })));

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
