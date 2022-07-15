// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_
#define ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_

#include <memory>

#include "ash/components/login/auth/public/cryptohome_error.h"
#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class UserContext;

using AuthOperationCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>,
                            absl::optional<CryptohomeError>)>;
using AuthOperation = base::OnceCallback<void(std::unique_ptr<UserContext>,
                                              AuthOperationCallback)>;
using AuthErrorCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>, CryptohomeError)>;
using AuthSuccessCallback =
    base::OnceCallback<void(std::unique_ptr<UserContext>)>;
using NoContextOperationCallback =
    base::OnceCallback<void(absl::optional<CryptohomeError>)>;

}  // namespace ash

#endif  // ASH_COMPONENTS_LOGIN_AUTH_PUBLIC_AUTH_CALLBACKS_H_
