// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/actor_login/internal/actor_login_federated_credentials_fetcher.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "url/gurl.h"

namespace actor_login {

namespace {
constexpr char kSupportedIdentityProvider[] =
    "https://accounts.google.com/gsi/fedcm.json";
}  // namespace

std::optional<ActorLoginError>
ActorLoginFederatedCredentialsFetcher::FederatedFetcherStatus::GetGlobalError()
    const {
  return std::nullopt;
}

ActorLoginFederatedCredentialsFetcher::ActorLoginFederatedCredentialsFetcher(
    const url::Origin& request_origin,
    IdentityCredentialSourceCallback get_source_callback)
    : request_origin_(request_origin),
      get_source_callback_(std::move(get_source_callback)) {}

ActorLoginFederatedCredentialsFetcher::
    ~ActorLoginFederatedCredentialsFetcher() = default;

void ActorLoginFederatedCredentialsFetcher::Fetch(
    FetchResultCallback callback) {
  callback_ = std::move(callback);

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kActorLoginFederatedLoginSupport)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>(),
                       std::make_unique<FederatedFetcherStatus>()));
    return;
  }

  content::webid::IdentityCredentialSource* source = get_source_callback_.Run();
  if (!source) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>(),
                       std::make_unique<FederatedFetcherStatus>()));
    return;
  }

  std::vector<GURL> supported_idps = {GURL(kSupportedIdentityProvider)};
  source->GetIdentityCredentialSuggestions(
      supported_idps, base::BindOnce(&ActorLoginFederatedCredentialsFetcher::
                                         OnGetIdentityCredentialSuggestions,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void ActorLoginFederatedCredentialsFetcher::OnGetIdentityCredentialSuggestions(
    const std::optional<
        std::vector<scoped_refptr<content::IdentityRequestAccount>>>&
        accounts) {
  if (!accounts) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::vector<Credential>(),
                       std::make_unique<FederatedFetcherStatus>()));
    return;
  }
  std::vector<Credential> result;
  for (const auto& account : *accounts) {
    // TODO(crbug.com/479069886): Go over all displayed fields and check their
    // use in credential picker.
    Credential credential;
    credential.type = CredentialType::kFederated;
    // TODO(crbug.com/479069886): properly format the username
    credential.username =
        base::UTF8ToUTF16(account->display_name + " " +
                          account->identity_provider->idp_for_display);
    credential.source_site_or_app =
        base::UTF8ToUTF16(account->identity_provider->idp_for_display);
    credential.request_origin = request_origin_;
    // NOTE: Actor logins are only allowed in secure contexts, so omitting the
    // scheme for display is permissible.
    credential.display_origin = url_formatter::FormatOriginForSecurityDisplay(
        request_origin_, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);

    credential.federation_detail = FederationDetail{
        .idp_origin = url::Origin::Create(
            account->identity_provider->idp_metadata.config_url),
        .account_id = account->id};
    credential.immediatelyAvailableToLogin = true;
    result.push_back(std::move(credential));
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), std::move(result),
                                std::make_unique<FederatedFetcherStatus>()));
}

}  // namespace actor_login
