// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/ambient/ambient_constants.h"
#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"

namespace ash {

// A class to manage the access token for ambient mode. Request will be async
// and will be returned as soon as the token is refreshed. If the token has
// already been refreshed, request call will be returned immediately.
class ASH_EXPORT AmbientAccessTokenController {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(const std::string& gaia_id,
                              const std::string& access_token)>;

  AmbientAccessTokenController();
  AmbientAccessTokenController(const AmbientAccessTokenController&) = delete;
  AmbientAccessTokenController& operator=(const AmbientAccessTokenController&) =
      delete;
  ~AmbientAccessTokenController();

  // The caller will pass in a preference |may_refresh_token_on_lock| whether
  // to refresh token on lock screen when it expires. In current implementation,
  // the AmbientController will request token once the screen is locked. This is
  // allowed (We could make the request before screen is locked, then the logic
  // in AmbientAccessTokenController could be simpler, i.e. just check if the
  // lock screen is on or not). Future requests on lock screen can not refresh
  // token if it expires.
  void RequestAccessToken(AccessTokenCallback callback,
                          bool may_refresh_token_on_lock = false);

  base::WeakPtr<AmbientAccessTokenController> AsWeakPtr();

 private:
  friend class AmbientAshTestBase;

  void RefreshAccessToken();
  void AccessTokenRefreshed(const std::string& gaia_id,
                            const std::string& access_token,
                            const base::Time& expiration_time);
  void RetryRefreshAccessToken();
  void NotifyAccessTokenRefreshed();
  void RunCallback(AccessTokenCallback callback);

  void SetTokenUsageBufferForTesting(base::TimeDelta time);

  base::TimeDelta GetTimeUntilReleaseForTesting();

  std::string gaia_id_;
  std::string access_token_;

  // The expiration time of the |access_token_|.
  base::Time expiration_time_;

  // True if has already sent access token request and waiting for result.
  bool has_pending_request_ = false;

  // The buffer time to use the access token.
  base::TimeDelta token_usage_time_buffer_ = kTokenUsageTimeBuffer;

  base::OneShotTimer token_refresh_timer_;

  net::BackoffEntry refresh_token_retry_backoff_;

  std::vector<AccessTokenCallback> callbacks_;

  base::WeakPtrFactory<AmbientAccessTokenController> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_ACCESS_TOKEN_CONTROLLER_H_
