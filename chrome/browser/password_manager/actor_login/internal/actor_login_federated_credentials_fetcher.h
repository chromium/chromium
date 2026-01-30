// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_credentials_fetcher.h"
#include "content/public/browser/webid/identity_request_account.h"
#include "url/origin.h"

namespace content::webid {
class IdentityCredentialSource;
}  // namespace content::webid

namespace actor_login {

class ActorLoginFederatedCredentialsFetcher
    : public ActorLoginCredentialsFetcher {
 public:
  // Status specific to federated credentials fetching.
  class FederatedFetcherStatus : public ActorLoginCredentialsFetcher::Status {
   public:
    FederatedFetcherStatus() = default;

    std::optional<ActorLoginError> GetGlobalError() const override;
  };

  using IdentityCredentialSourceCallback =
      base::RepeatingCallback<content::webid::IdentityCredentialSource*()>;

  ActorLoginFederatedCredentialsFetcher(
      const url::Origin& request_origin,
      IdentityCredentialSourceCallback get_source_callback);
  ~ActorLoginFederatedCredentialsFetcher() override;

  // ActorLoginCredentialsFetcher:
  void Fetch(FetchResultCallback callback) override;

 private:
  void OnGetIdentityCredentialSuggestions(
      const std::optional<
          std::vector<scoped_refptr<content::IdentityRequestAccount>>>&
          accounts);

  FetchResultCallback callback_;
  url::Origin request_origin_;
  // Callback to get the IdentityCredentialSource for the current page. This is
  // a callback instead of a direct pointer, because the
  // `IdentityCredentialSource` (`DocumentUserData`) is bound to a document on
  // the page and not to the `WebContents` like the `ActorLoginDelegateImpl`.
  // It's possible that the document is destroyed while the fetcher is alive, so
  // we need to get a fresh source whenever the fetch is triggered.
  // TODO(crbug.com/480004512): Check the origin of the source before using it.
  IdentityCredentialSourceCallback get_source_callback_;

  base::WeakPtrFactory<ActorLoginFederatedCredentialsFetcher> weak_ptr_factory_{
      this};
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_
