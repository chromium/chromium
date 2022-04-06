// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/operation_chain_runner.h"

#include <memory>
#include "ash/components/login/auth/auth_callbacks.h"
#include "ash/components/login/auth/user_context.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {}  // namespace

TEST(OperationChainRunnerTest, TestEmptyList) {
  RunOperationChain(
      std::make_unique<UserContext>(), {/* no operations */},
      /* success callback */
      base::BindLambdaForTesting(
          [](std::unique_ptr<UserContext> context) { EXPECT_TRUE(context); }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    CryptohomeError error) { FAIL(); }));
}

TEST(OperationChainRunnerTest, TestSingleSuccessfulOperation) {
  std::vector<AuthOperation> operations;
  operations.push_back(base::BindLambdaForTesting(
      [](std::unique_ptr<UserContext> context, AuthOperationCallback callback) {
        context->SetAuthSessionId("session");
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context) {
        EXPECT_TRUE(context);
        EXPECT_EQ(context->GetAuthSessionId(), "session");
        chain_finished = true;
      }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    CryptohomeError error) { FAIL(); }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestSingleFailedOperation) {
  std::vector<AuthOperation> operations;
  operations.push_back(base::BindLambdaForTesting(
      [](std::unique_ptr<UserContext> context, AuthOperationCallback callback) {
        context->SetAuthSessionId("session");
        std::move(callback).Run(
            std::move(context),
            CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context) { FAIL(); }),
      /* failure callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context, CryptohomeError error) {
            EXPECT_TRUE(context);
            EXPECT_EQ(context->GetAuthSessionId(), "session");
            chain_finished = true;
          }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestSuccesfulSequenceOrdering) {
  std::vector<AuthOperation> operations;
  int order = 0;
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 0);
        order++;
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 1);
        order++;
        context->SetAuthSessionId("session");
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        EXPECT_EQ(order, 2);
        order++;
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context) {
        EXPECT_TRUE(context);
        EXPECT_EQ(context->GetAuthSessionId(), "session");
        chain_finished = true;
      }),
      /* failure callback */
      base::BindLambdaForTesting([](std::unique_ptr<UserContext> context,
                                    CryptohomeError error) { FAIL(); }));
  EXPECT_TRUE(chain_finished);
}

TEST(OperationChainRunnerTest, TestFailedMiddleOperation) {
  std::vector<AuthOperation> operations;
  bool called_first = false;
  bool called_last = false;

  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        called_first = true;
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        std::move(callback).Run(
            std::move(context),
            CryptohomeError{user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND});
      }));
  operations.push_back(
      base::BindLambdaForTesting([&](std::unique_ptr<UserContext> context,
                                     AuthOperationCallback callback) {
        called_last = true;
        std::move(callback).Run(std::move(context), absl::nullopt);
      }));

  bool chain_finished = false;
  RunOperationChain(
      std::make_unique<UserContext>(), std::move(operations),
      /* success callback */
      base::BindLambdaForTesting(
          [](std::unique_ptr<UserContext> context) { FAIL(); }),
      /* failure callback */
      base::BindLambdaForTesting(
          [&](std::unique_ptr<UserContext> context, CryptohomeError error) {
            chain_finished = true;
            EXPECT_TRUE(context);
          }));
  EXPECT_TRUE(chain_finished);
  EXPECT_TRUE(called_first);
  EXPECT_FALSE(called_last);
}

}  // namespace ash
