// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/auth_token.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {
namespace quick_unlock {

constexpr base::TimeDelta AuthToken::kTokenExpiration = base::Seconds(5 * 60);

AuthToken::AuthToken(const UserContext& user_context)
    : identifier_(base::UnguessableToken::Create()),
      creation_time_(base::TimeTicks::Now()),
      user_context_(std::make_unique<UserContext>(user_context)) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AuthToken::Reset, weak_factory_.GetWeakPtr()),
      kTokenExpiration);
}

AuthToken::~AuthToken() = default;

std::optional<std::string> AuthToken::Identifier() const {
  if (!user_context_)
    return std::nullopt;
  return identifier_.ToString();
}

std::optional<base::UnguessableToken> AuthToken::GetUnguessableToken() const {
  if (!user_context_)
    return std::nullopt;
  return identifier_;
}

std::optional<base::TimeDelta> AuthToken::GetAge() const {
  if (!user_context_)
    return std::nullopt;
  return base::TimeTicks::Now() - creation_time_;
}

void AuthToken::ReplaceUserContext(std::unique_ptr<UserContext> user_context) {
  if (!user_context_) {
    // This happens theoretically in the following rare situation:
    // 1. An auth editing operation is triggered, e.g. the user changes their
    // pin.
    // 2. After the operation is triggered, the auth token is reset, e.g.
    //    because its lifetime timeout is exceeded. This clears the user
    //    context stored in the auth token.
    // 3. Nevertheless, the auth operation triggered in 1 succeeds, and we
    //    reload the auth factors configuration.
    // 4. We try to set the user context with the updated user context.
    //
    // Usually we make the user reauthenticate a bit before the token is
    // invalidated, so this should only happen if some clocks are off, or if an
    // auth editing operation takes very long.
    LOG(WARNING) << "Replacement user context is ignored because auth token "
                    "has been reset";
    return;
  }

  Reset();
  user_context_ = std::move(user_context);
}

void AuthToken::Reset() {
  if (user_context_)
    user_context_->ClearSecrets();
  user_context_.reset();
}

}  // namespace quick_unlock
}  // namespace ash
