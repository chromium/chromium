// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IN_SESSION_AUTH_TOKEN_PROVIDER_H_
#define ASH_PUBLIC_CPP_IN_SESSION_AUTH_TOKEN_PROVIDER_H_

#include "ash/components/login/auth/user_context.h"
#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"

namespace ash {

class ASH_PUBLIC_EXPORT InSessionAuthTokenProvider {
 public:
  using OnAuthTokenGenerated =
      base::OnceCallback<void(const base::UnguessableToken&, base::TimeDelta)>;

  virtual ~InSessionAuthTokenProvider() = default;

  // Constructs an unguessable token for a given `UserContext`
  virtual void ExchangeForToken(std::unique_ptr<UserContext> user_context,
                                OnAuthTokenGenerated callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IN_SESSION_AUTH_TOKEN_PROVIDER_H_
