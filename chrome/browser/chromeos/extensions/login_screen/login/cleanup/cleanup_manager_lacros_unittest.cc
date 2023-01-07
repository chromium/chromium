// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/cleanup_manager_lacros.h"

#include <utility>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/mock_cleanup_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::WithArg;

namespace extensions {

class CleanupManagerLacrosUnittest : public testing::Test {
 public:
  CleanupManagerLacrosUnittest() = default;
  ~CleanupManagerLacrosUnittest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder builder;
    builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
    profile_ = builder.Build();

    manager_ = CleanupManagerLacros::GetFactoryInstance()->Get(profile_.get());
  }

  void TearDown() override {
    manager_->ResetCleanupHandlersForTesting();
    testing::Test::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  CleanupManagerLacros* manager_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(CleanupManagerLacrosUnittest, Cleanup) {
  auto no_error_callback =
      [](chromeos::CleanupHandler::CleanupHandlerCallback callback) {
        std::move(callback).Run(absl::nullopt);
      };
  std::unique_ptr<chromeos::MockCleanupHandler> mock_cleanup_handler =
      std::make_unique<chromeos::MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(no_error_callback)));

  std::map<std::string, std::unique_ptr<chromeos::CleanupHandler>>
      cleanup_handlers;
  cleanup_handlers.insert({"Handler", std::move(mock_cleanup_handler)});
  manager_->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  manager_->Cleanup(
      base::BindLambdaForTesting([&](const absl::optional<std::string>& error) {
        EXPECT_EQ(absl::nullopt, error);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

TEST_F(CleanupManagerLacrosUnittest, CleanupError) {
  auto error_callback =
      [](chromeos::CleanupHandler::CleanupHandlerCallback callback) {
        std::move(callback).Run("Error");
      };
  std::unique_ptr<chromeos::MockCleanupHandler> mock_cleanup_handler =
      std::make_unique<chromeos::MockCleanupHandler>();
  EXPECT_CALL(*mock_cleanup_handler, Cleanup(_))
      .WillOnce(WithArg<0>(Invoke(error_callback)));

  std::map<std::string, std::unique_ptr<chromeos::CleanupHandler>>
      cleanup_handlers;
  cleanup_handlers.insert({"Handler", std::move(mock_cleanup_handler)});
  manager_->SetCleanupHandlersForTesting(std::move(cleanup_handlers));

  base::RunLoop run_loop;
  manager_->Cleanup(
      base::BindLambdaForTesting([&](const absl::optional<std::string>& error) {
        EXPECT_EQ("Handler: Error", error);
        run_loop.QuitClosure().Run();
      }));
  run_loop.Run();
}

}  // namespace extensions
