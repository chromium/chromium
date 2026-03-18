// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_federated_credentials_fetcher.h"

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "url/gurl.h"

namespace actor_login {

namespace {
constexpr char kSupportedIdentityProvider[] =
    "https://accounts.google.com/gsi/fedcm.json";
}  // namespace

ActorLoginFederatedCredentialsFetcher::ActorLoginFederatedCredentialsFetcher(
    const url::Origin& request_origin,
    IdentityCredentialSourceCallback get_source_callback,
    ActorLoginPermissionService& permission_service)
    : request_origin_(request_origin),
      get_source_callback_(std::move(get_source_callback)),
      permission_service_(permission_service) {}

ActorLoginFederatedCredentialsFetcher::
    ~ActorLoginFederatedCredentialsFetcher() = default;

void ActorLoginFederatedCredentialsFetcher::Fetch(
    FetchResultCallback callback) {
  callback_ = std::move(callback);

  if (!base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>(),
                       ActorLoginCredentialsFetcher::Status::kSuccess));
    return;
  }

  content::webid::IdentityCredentialSource* source = get_source_callback_.Run();
  if (!source) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>(),
                       ActorLoginCredentialsFetcher::Status::kSuccess));
    return;
  }

  if (metrics_helper_) {
    metrics_helper_->RecordFederatedHangingFedCmRequestExists(
        source->HasPendingRequest());
  }

  std::vector<GURL> supported_idps = {GURL(kSupportedIdentityProvider)};

  auto barrier_callback = base::BarrierCallback<FetchResultVariant>(
      2, base::BindOnce(&ActorLoginFederatedCredentialsFetcher::OnFetchComplete,
                        weak_ptr_factory_.GetWeakPtr()));

  source->GetIdentityCredentialSuggestions(
      supported_idps,
      base::BindOnce(&ActorLoginFederatedCredentialsFetcher::
                         OnGetIdentityCredentialSuggestions,
                     weak_ptr_factory_.GetWeakPtr(), barrier_callback));

  // Request all permissions for the main frame origin.
  std::vector<FederatedOrigins> origins = {{request_origin_, url::Origin()}};
  permission_service_->ListPermissions(
      origins,
      base::BindOnce(
          &ActorLoginFederatedCredentialsFetcher::OnGetPermissionsCompleted,
          weak_ptr_factory_.GetWeakPtr(), barrier_callback));
}

void ActorLoginFederatedCredentialsFetcher::SetMetricsHelper(
    ActorLoginMetricsHelper* metrics_helper) {
  metrics_helper_ = metrics_helper;
}

void ActorLoginFederatedCredentialsFetcher::OnGetIdentityCredentialSuggestions(
    base::RepeatingCallback<void(FetchResultVariant)> barrier_callback,
    const std::optional<
        std::vector<scoped_refptr<content::IdentityRequestAccount>>>&
        accounts) {
  if (!accounts) {
    barrier_callback.Run(std::vector<Credential>());
    return;
  }

  std::vector<Credential> result;
  for (const auto& account : *accounts) {
    Credential credential;
    credential.type = CredentialType::kFederated;
    // At this point, the only IdP(s) we support for actor login use email as an
    // identifier. We'll fallback to the `display_identifier` in anticipation of
    // generalizing to other IdPs.
    credential.username = base::UTF8ToUTF16(
        !account->email.empty() ? account->email : account->display_identifier);
    credential.source_site_or_app =
        base::UTF8ToUTF16(account->identity_provider->idp_for_display);
    credential.request_origin = request_origin_;
    // NOTE: Actor logins are only allowed in secure contexts, so omitting the
    // scheme for display is permissible.
    credential.display_origin = url_formatter::FormatOriginForSecurityDisplay(
        request_origin_, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

    FederationDetail& federation_detail =
        credential.federation_detail.emplace();
    federation_detail.idp_origin = url::Origin::Create(
        account->identity_provider->idp_metadata.config_url);
    federation_detail.account_id = account->id;
    federation_detail.account_picture = account->decoded_picture;
    federation_detail.brand_icon =
        account->identity_provider->idp_metadata.brand_decoded_icon;

    credential.immediatelyAvailableToLogin = true;
    result.push_back(std::move(credential));
  }

  barrier_callback.Run(std::move(result));
}

void ActorLoginFederatedCredentialsFetcher::OnGetPermissionsCompleted(
    base::RepeatingCallback<void(FetchResultVariant)> barrier_callback,
    std::vector<FederatedPermission> permissions) {
  barrier_callback.Run(std::move(permissions));
}

void ActorLoginFederatedCredentialsFetcher::OnFetchComplete(
    std::vector<FetchResultVariant> results) {
  CHECK_EQ(results.size(), 2u);

  std::vector<Credential> credentials;
  std::vector<FederatedPermission> permissions;

  for (auto& result : results) {
    if (std::holds_alternative<std::vector<Credential>>(result)) {
      credentials = std::move(std::get<std::vector<Credential>>(result));
    } else if (std::holds_alternative<std::vector<FederatedPermission>>(
                   result)) {
      permissions =
          std::move(std::get<std::vector<FederatedPermission>>(result));
    }
  }

  for (Credential& credential : credentials) {
    for (const FederatedPermission& permission : permissions) {
      if (credential.federation_detail->account_id ==
              permission.chosen_account_id &&
          credential.request_origin == permission.rp_embedder_origin) {
        credential.has_persistent_permission = true;
        break;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), std::move(credentials),
                     ActorLoginCredentialsFetcher::Status::kSuccess));
}

}  // namespace actor_login
