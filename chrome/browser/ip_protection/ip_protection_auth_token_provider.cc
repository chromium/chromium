// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ip_protection/blind_sign_http_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth.h"

IpProtectionAuthTokenProvider::IpProtectionAuthTokenProvider(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      blind_sign_http_impl_(
          std::make_unique<BlindSignHttpImpl>(url_loader_factory_.get())),
      blind_sign_auth_(
          std::make_unique<quiche::BlindSignAuth>(blind_sign_http_impl_.get())),
      bsa_(blind_sign_auth_.get()) {
  CHECK(identity_manager);
}

IpProtectionAuthTokenProvider::~IpProtectionAuthTokenProvider() = default;

void IpProtectionAuthTokenProvider::TryGetAuthTokens(
    uint32_t batch_size,
    TryGetAuthTokensCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (try_get_auth_token_callback_) {
    mojo::ReportBadMessage(
        "Concurrent calls to TryGetAuthTokens are not allowed");
    return;
  }
  // The `batch_size` is cast to an `int` for use by BlindSignAuth, so check for
  // overflow here.
  if (batch_size == 0 || batch_size > INT_MAX) {
    mojo::ReportBadMessage("Invalid batch_size");
    return;
  }
  try_get_auth_token_callback_ = std::move(callback);
  batch_size_ = batch_size;
  RequestOAuthToken();
}

void IpProtectionAuthTokenProvider::RequestOAuthToken() {
  if (!identity_manager_ ||
      !identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    TryGetAuthTokensComplete(
        absl::nullopt, IpProtectionTryGetAuthTokensResult::kFailedNoAccount);
    return;
  }

  // If the user is not eligible, do not even try to fetch tokens. If unknown,
  // fall back to trying anyway.
  const CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  CHECK(!account_id.empty());
  const AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (!account_info.IsEmpty() &&
      account_info.capabilities.can_use_chrome_ip_protection() ==
          signin::Tribool::kFalse) {
    TryGetAuthTokensComplete(
        absl::nullopt, IpProtectionTryGetAuthTokensResult::kFailedNotEligible);
    return;
  }

  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kIpProtectionAuthScope);

  // Waits for the account to have a refresh token before making the request.
  auto mode =
      signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable;

  // Create the OAuth token fetcher and call `OnRequestOAuthTokenCompleted()`
  // when complete. base::Unretained() is safe since `this` owns
  // `access_token_fetcher_`
  start_time_ = base::TimeTicks::Now();
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          /*consumer_name=*/"IpProtectionService", identity_manager_, scopes,
          base::BindOnce(
              &IpProtectionAuthTokenProvider::OnRequestOAuthTokenCompleted,
              base::Unretained(this)),
          mode, signin::ConsentLevel::kSignin);
}

void IpProtectionAuthTokenProvider::OnRequestOAuthTokenCompleted(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  // If we fail to get an OAuth token don't attempt to fetch from Phosphor as
  // the request is guaranteed to fail.
  if (error.state() != GoogleServiceAuthError::NONE) {
    TryGetAuthTokensComplete(
        absl::nullopt, IpProtectionTryGetAuthTokensResult::kFailedOAuthToken);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  base::UmaHistogramTimes("NetworkService.IpProtection.OAuthTokenFetchTime",
                          current_time - start_time_);
  FetchBlindSignedToken(access_token_info);
}

void IpProtectionAuthTokenProvider::FetchBlindSignedToken(
    signin::AccessTokenInfo access_token_info) {
  DCHECK(bsa_);
  start_time_ = base::TimeTicks::Now();
  bsa_->GetTokens(
      access_token_info.token, batch_size_,
      [this](absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
        OnFetchBlindSignedTokenCompleted(tokens);
      });
}

void IpProtectionAuthTokenProvider::OnFetchBlindSignedTokenCompleted(
    absl::StatusOr<absl::Span<quiche::BlindSignToken>> tokens) {
  if (!tokens.ok()) {
    // Apply the canonical mapping from abseil status to HTTP status.
    IpProtectionTryGetAuthTokensResult result;
    switch (tokens.status().code()) {
      case absl::StatusCode::kInvalidArgument:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA400;
        break;
      case absl::StatusCode::kUnauthenticated:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA401;
        break;
      case absl::StatusCode::kPermissionDenied:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSA403;
        break;
      default:
        result = IpProtectionTryGetAuthTokensResult::kFailedBSAOther;
        break;
    }
    TryGetAuthTokensComplete(absl::nullopt, result);
    return;
  }

  if (tokens.value().size() == 0) {
    TryGetAuthTokensComplete(
        absl::nullopt, IpProtectionTryGetAuthTokensResult::kFailedBSAOther);
    return;
  }

  const base::TimeTicks current_time = base::TimeTicks::Now();
  base::UmaHistogramTimes("NetworkService.IpProtection.TokenBatchRequestTime",
                          current_time - start_time_);

  std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens;
  std::transform(tokens->begin(), tokens->end(), std::back_inserter(bsa_tokens),
                 [](quiche::BlindSignToken bsa_token) {
                   base::Time expiration = base::Time::FromTimeT(
                       absl::ToTimeT(bsa_token.expiration));
                   return network::mojom::BlindSignedAuthToken::New(
                       bsa_token.token, expiration);
                 });

  TryGetAuthTokensComplete(absl::make_optional(std::move(bsa_tokens)),
                           IpProtectionTryGetAuthTokensResult::kSuccess);
}

void IpProtectionAuthTokenProvider::TryGetAuthTokensComplete(
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
        bsa_tokens,
    IpProtectionTryGetAuthTokensResult result) {
  base::UmaHistogramEnumeration(
      "NetworkService.IpProtection.TryGetAuthTokensResult", result);
  std::move(try_get_auth_token_callback_).Run(std::move(bsa_tokens));
}

void IpProtectionAuthTokenProvider::Shutdown() {
  is_shutting_down_ = true;
  identity_manager_ = nullptr;
  receiver_.reset();
}

/*static*/
IpProtectionAuthTokenProvider* IpProtectionAuthTokenProvider::Get(
    Profile* profile) {
  return IpProtectionAuthTokenProviderFactory::GetForProfile(profile);
}

void IpProtectionAuthTokenProvider::SetReceiver(
    mojo::PendingReceiver<network::mojom::IpProtectionAuthTokenGetter>
        pending_receiver) {
  if (is_shutting_down_) {
    return;
  }
  if (receiver_.is_bound()) {
    // TODO(awillia): I'm not sure if this case is possible since a receiver
    // should only be added when a NetworkContext is created, but maybe this can
    // occur if the network service crashes and is restarted? If this can't
    // happen, just replace this if statement with a CHECK.
    receiver_.reset();
    // Reset any pending callbacks as well since this class only expects to have
    // only one pending call to `TryGetAuthTokens()` at any given time.
    try_get_auth_token_callback_.Reset();
  }
  receiver_.Bind(std::move(pending_receiver));
}
