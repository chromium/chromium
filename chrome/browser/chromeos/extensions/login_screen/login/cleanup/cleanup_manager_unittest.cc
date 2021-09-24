// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/mock_cleanup_handler.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace chromeos {

class CleanupManagerUnittest : public testing::Test {
 public:
  CleanupManagerUnittest() = default;
  ~CleanupManagerUnittest() override = default;

  void TearDown() override {
    CleanupManager::Get()->ResetCleanupHandlersForTesting();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(CleanupManagerUnittest, Cleanup) {
  auto no_error_callback = [](CleanupHandler::CleanupHandlerCallback callback) {
    std::move(callback).Run(absl::nullopt);
  };
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler1 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(no_error_callback)));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(no_error_callback)));

  std::vector<std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.emplace_back(std::move(mock_cleanup_handler1));
  cleanup_handlers.emplace_back(std::move(mock_cleanup_handler2));
  CleanupManager* manager = CleanupManager::Get();
  manager->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  manager->Cleanup(
      base::BindLambdaForTesting([&](absl::optional<std::string> error) {
        EXPECT_EQ(absl::nullopt, error);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
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

  std::vector<std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.emplace_back(std::move(mock_cleanup_handler));
  CleanupManager* manager = CleanupManager::Get();
  manager->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(2, run_loop.QuitClosure());

  manager->Cleanup(
      base::BindLambdaForTesting([&](absl::optional<std::string> error) {
        EXPECT_EQ(absl::nullopt, error);
        barrier_closure.Run();
      }));

  manager->Cleanup(
      base::BindLambdaForTesting([&](absl::optional<std::string> error) {
        EXPECT_EQ("Cleanup is already in progress", *error);
        barrier_closure.Run();
      }));

  std::move(callback).Run(absl::nullopt);
  run_loop.Run();
}

TEST_F(CleanupManagerUnittest, CleanupErrors) {
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler1 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler1, Cleanup(_))
      .WillOnce(WithArg<0>(
          Invoke([](CleanupHandler::CleanupHandlerCallback callback) {
            std::move(callback).Run("Handler 1 error");
          })));
  std::unique_ptr<MockCleanupHandler> mock_cleanup_handler2 =
      std::make_unique<MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler2, Cleanup(_))
      .WillOnce(WithArg<0>(
          Invoke([](CleanupHandler::CleanupHandlerCallback callback) {
            std::move(callback).Run("Handler 2 error");
          })));

  std::vector<std::unique_ptr<CleanupHandler>> cleanup_handlers;
  cleanup_handlers.emplace_back(std::move(mock_cleanup_handler1));
  cleanup_handlers.emplace_back(std::move(mock_cleanup_handler2));
  CleanupManager* manager = CleanupManager::Get();
  manager->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  manager->Cleanup(
      base::BindLambdaForTesting([&](absl::optional<std::string> error) {
        EXPECT_EQ("Handler 1 error\nHandler 2 error", *error);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

}  // namespace chromeos
