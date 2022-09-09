// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quick_unlock/auth_token.h"

#include "base/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {
namespace quick_unlock {

constexpr base::TimeDelta AuthToken::kTokenExpiration = base::Seconds(5 * 60);

AuthToken::AuthToken(const UserContext& user_context)
    : identifier_(base::UnguessableToken::Create()),
      creation_time_(base::TimeTicks::Now()),
      user_context_(std::make_unique<UserContext>(user_context)) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(&AuthToken::Reset, weak_factory_.GetWeakPtr()),
      kTokenExpiration);
}

AuthToken::~AuthToken() = default;

absl::optional<std::string> AuthToken::Identifier() const {
  if (!user_context_)
    return absl::nullopt;
  return identifier_.ToString();
}

absl::optional<base::UnguessableToken> AuthToken::GetUnguessableToken() const {
  if (!user_context_)
    return absl::nullopt;
  return identifier_;
}

absl::optional<base::TimeDelta> AuthToken::GetAge() const {
  if (!user_context_)
    return absl::nullopt;
  return base::TimeTicks::Now() - creation_time_;
}

void AuthToken::Reset() {
  if (user_context_)
    user_context_->ClearSecrets();
  user_context_.reset();
}

}  // namespace quick_unlock
}  // namespace ash
