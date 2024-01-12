// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_CONTEXT_H_
#define CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_CONTEXT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/scope_set.h"

class Profile;

namespace arc {

class ArcAuthContext : public signin::IdentityManager::Observer {
 public:
  // Creates an |ArcAuthContext| for the given |account_id|. This |account_id|
  // must be the |account_id| used by the OAuth Token Service chain.
  // Note: |account_id| can be the Device Account or a Secondary Account stored
  // in Chrome OS Account Manager.
  ArcAuthContext(Profile* profile, const CoreAccountId& account_id);

  ArcAuthContext(const ArcAuthContext&) = delete;
  ArcAuthContext& operator=(const ArcAuthContext&) = delete;

  ~ArcAuthContext() override;

  // Prepares the context. Calling while an inflight operation exists will
  // cancel the inflight operation.
  // On completion, |true| is passed to the callback. On error, |false|
  // is passed.
  using PrepareCallback = base::OnceCallback<void(bool success)>;
  void Prepare(PrepareCallback callback);

  // Creates and starts a request to fetch an access token for the given
  // |scopes|. The caller owns the returned request. |callback| will be
  // called with results if the returned request is not deleted.
  std::unique_ptr<signin::AccessTokenFetcher> CreateAccessTokenFetcher(
      const std::string& consumer_name,
      const signin::ScopeSet& scopes,
      signin::AccessTokenFetcher::TokenCallback callback);

  void RemoveAccessTokenFromCache(const signin::ScopeSet& scopes,
                                  const std::string& access_token);

  // signin::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnRefreshTokensLoaded() override;

 private:
  void OnRefreshTokenTimeout();

  const CoreAccountId account_id_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  PrepareCallback callback_;
  bool context_prepared_ = false;

  base::OneShotTimer refresh_token_timeout_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_AUTH_ARC_AUTH_CONTEXT_H_
