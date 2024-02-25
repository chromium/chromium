// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_access_token_controller.h"

#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr int kMaxRetries = 3;

constexpr net::BackoffEntry::Policy kRetryBackoffPolicy = {
    0,          // Number of initial errors to ignore.
    1000,       // Initial delay in ms.
    2.0,        // Factor by which the waiting time will be multiplied.
    0.2,        // Fuzzing percentage.
    60 * 1000,  // Maximum delay in ms.
    -1,         // Never discard the entry.
    true,       // Use initial delay.
};

}  // namespace

AmbientAccessTokenController::AmbientAccessTokenController()
    : refresh_token_retry_backoff_(&kRetryBackoffPolicy) {}

AmbientAccessTokenController::~AmbientAccessTokenController() = default;

void AmbientAccessTokenController::RequestAccessToken(
    AccessTokenCallback callback,
    bool may_refresh_token_on_lock) {
  // |token_refresh_timer_| may become stale during sleeping.
  if (token_refresh_timer_.IsRunning())
    token_refresh_timer_.AbandonAndStop();

  if (!access_token_.empty()) {
    DCHECK(!has_pending_request_);

    // Return the token if there is enough time to use the access token when
    // requested.
    if (expiration_time_ - base::Time::Now() > token_usage_time_buffer_) {
      RunCallback(std::move(callback));
      return;
    }

    access_token_ = std::string();
    expiration_time_ = base::Time::Now();
  }

  if (!may_refresh_token_on_lock && LockScreen::HasInstance()) {
    RunCallback(std::move(callback));
    return;
  }

  callbacks_.emplace_back(std::move(callback));

  if (has_pending_request_)
    return;

  RefreshAccessToken();
}

base::WeakPtr<AmbientAccessTokenController>
AmbientAccessTokenController::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AmbientAccessTokenController::RefreshAccessToken() {
  DCHECK(!token_refresh_timer_.IsRunning());

  has_pending_request_ = true;
  AmbientClient::Get()->RequestAccessToken(
      base::BindOnce(&AmbientAccessTokenController::AccessTokenRefreshed,
                     weak_factory_.GetWeakPtr()));
}

void AmbientAccessTokenController::AccessTokenRefreshed(
    const std::string& gaia_id,
    const std::string& access_token,
    const base::Time& expiration_time) {
  has_pending_request_ = false;

  if (gaia_id.empty() || access_token.empty()) {
    refresh_token_retry_backoff_.InformOfRequest(/*succeeded=*/false);
    if (refresh_token_retry_backoff_.failure_count() <= kMaxRetries) {
      LOG(WARNING) << "Unable to refresh access token. Retrying..";
      RetryRefreshAccessToken();
    } else {
      LOG(ERROR) << "Unable to refresh access token. All retries attempted.";
      NotifyAccessTokenRefreshed();
    }

    return;
  }

  DVLOG(1) << "Access token fetched.";
  DCHECK(gaia_id_.empty() || gaia_id_ == gaia_id);
  refresh_token_retry_backoff_.Reset();
  gaia_id_ = gaia_id;
  access_token_ = access_token;
  expiration_time_ = expiration_time;
  NotifyAccessTokenRefreshed();
}

void AmbientAccessTokenController::RetryRefreshAccessToken() {
  base::TimeDelta backoff_delay =
      refresh_token_retry_backoff_.GetTimeUntilRelease();
  token_refresh_timer_.Start(
      FROM_HERE, backoff_delay,
      base::BindOnce(&AmbientAccessTokenController::RefreshAccessToken,
                     base::Unretained(this)));
}

void AmbientAccessTokenController::NotifyAccessTokenRefreshed() {
  for (auto& callback : callbacks_)
    RunCallback(std::move(callback));

  callbacks_.clear();
}

void AmbientAccessTokenController::RunCallback(AccessTokenCallback callback) {
  std::move(callback).Run(gaia_id_, access_token_);
}

void AmbientAccessTokenController::SetTokenUsageBufferForTesting(
    base::TimeDelta time) {
  token_usage_time_buffer_ = time;
}

base::TimeDelta AmbientAccessTokenController::GetTimeUntilReleaseForTesting() {
  return refresh_token_retry_backoff_.GetTimeUntilRelease();
}

}  // namespace ash
