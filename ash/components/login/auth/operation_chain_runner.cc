// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/operation_chain_runner.h"

#include <memory>
#include <vector>

#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/containers/stack.h"

namespace ash {
namespace {

void OnOperation(base::stack<AuthOperation> operations,
                 AuthSuccessCallback success_handler,
                 AuthErrorCallback error_handler,
                 std::unique_ptr<UserContext> context,
                 absl::optional<CryptohomeError> error) {
  if (error) {
    std::move(error_handler).Run(std::move(context), error.value());
    return;
  }
  if (operations.empty()) {
    std::move(success_handler).Run(std::move(context));
    return;
  }
  AuthOperation next = std::move(operations.top());
  operations.pop();
  AuthOperationCallback tail =
      base::BindOnce(&OnOperation, std::move(operations),
                     std::move(success_handler), std::move(error_handler));
  std::move(next).Run(std::move(context), std::move(tail));
}
}  // namespace

void RunOperationChain(std::unique_ptr<UserContext> context,
                       std::vector<AuthOperation> operations,
                       AuthSuccessCallback success_handler,
                       AuthErrorCallback error_handler) {
  if (operations.empty()) {
    std::move(success_handler).Run(std::move(context));
    return;
  }
  base::stack<AuthOperation> reversed_ops;
  for (auto rit = operations.rbegin(); rit != operations.rend(); ++rit)
    reversed_ops.push(std::move(*rit));

  AuthOperation first = std::move(reversed_ops.top());
  reversed_ops.pop();
  AuthOperationCallback tail =
      base::BindOnce(&OnOperation, std::move(reversed_ops),
                     std::move(success_handler), std::move(error_handler));
  std::move(first).Run(std::move(context), std::move(tail));
}

}  // namespace ash
