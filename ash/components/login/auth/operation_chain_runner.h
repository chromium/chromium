// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_OPERATION_CHAIN_RUNNER_H_
#define ASH_COMPONENTS_LOGIN_AUTH_OPERATION_CHAIN_RUNNER_H_

#include <memory>
#include <vector>

#include "ash/components/login/auth/auth_callbacks.h"
#include "base/callback.h"
#include "base/component_export.h"

namespace ash {

class UserContext;

// This method allows to run a chain of `AuthOperationCallback`'s without
// creating intermediate methods.
void COMPONENT_EXPORT(ASH_LOGIN_AUTH)
    RunOperationChain(std::unique_ptr<UserContext> context,
                      std::vector<AuthOperation> callbacks,
                      AuthSuccessCallback success_handler,
                      AuthErrorCallback error_handler);

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_OPERATION_CHAIN_RUNNER_H_
