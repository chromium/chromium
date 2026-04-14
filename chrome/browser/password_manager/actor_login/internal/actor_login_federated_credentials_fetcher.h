// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_

#include <optional>
#include <variant>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/password_manager/actor_login/internal/actor_login_metrics_helper.h"
#include "components/optimization_guide/proto/features/actor_login.pb.h"
#include "components/password_manager/core/browser/actor_login/actor_login_permission_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_quality_logger_interface.h"
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
  using IdentityCredentialSourceCallback =
      base::RepeatingCallback<content::webid::IdentityCredentialSource*()>;

  ActorLoginFederatedCredentialsFetcher(
      const url::Origin& request_origin,
      IdentityCredentialSourceCallback get_source_callback,
      ActorLoginPermissionService& permission_service,
      base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger);
  ~ActorLoginFederatedCredentialsFetcher() override;

  // ActorLoginCredentialsFetcher:
  void Fetch(FetchResultCallback callback) override;

  void SetMetricsHelper(ActorLoginMetricsHelper* metrics_helper);

 private:
  using FetchResultVariant =
      std::variant<std::vector<Credential>, std::vector<FederatedPermission>>;
  void OnGetIdentityCredentialSuggestions(
      base::RepeatingCallback<void(FetchResultVariant)> barrier_callback,
      const std::optional<
          std::vector<scoped_refptr<content::IdentityRequestAccount>>>&
          accounts);
  void OnGetPermissionsCompleted(
      base::RepeatingCallback<void(FetchResultVariant)> barrier_callback,
      base::TimeTicks list_permissions_start_time,
      std::vector<FederatedPermission> permissions);
  void OnFetchComplete(std::vector<FetchResultVariant> results);

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

  // `ProfileKeyedService`, will outlive the fetcher.
  const raw_ref<ActorLoginPermissionService> permission_service_;

  // Owned by `ActorLoginDelegateImpl`.
  raw_ptr<ActorLoginMetricsHelper> metrics_helper_ = nullptr;

  // Owned by `ActorLoginDelegateImpl`.
  base::WeakPtr<ActorLoginQualityLoggerInterface> mqls_logger_;

  // Stores the proto log fields populated during fetch.
  optimization_guide::proto::ActorLoginQuality_FederatedGetCredentialsDetails
      get_credentials_logs_;

  base::WeakPtrFactory<ActorLoginFederatedCredentialsFetcher> weak_ptr_factory_{
      this};
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_FEDERATED_CREDENTIALS_FETCHER_H_
