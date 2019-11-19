// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "chromeos/constants/chromeos_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using content::BrowserThread;

namespace policy {

namespace {

// If true, fake policy tokens will be sent instead of making network requests.
bool use_fake_tokens_for_testing_ = false;

// Max retry count for token fetching requests.
const int kMaxRequestAttemptCount = 5;

// OAuth token request retry delay in milliseconds.
const int kRequestRestartDelay = 3000;

class PolicyOAuth2TokenFetcherImpl : public PolicyOAuth2TokenFetcher,
                                     public GaiaAuthConsumer,
                                     public OAuth2AccessTokenConsumer {
 public:
  PolicyOAuth2TokenFetcherImpl();
  ~PolicyOAuth2TokenFetcherImpl() override;

 private:
  // PolicyOAuth2TokenFetcher overrides.
  void StartWithAuthCode(
      const std::string& auth_code,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) override;
  void StartWithRefreshToken(
      const std::string& oauth2_refresh_token,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) override;

  // Returns true if we have previously attempted to fetch tokens with this
  // class and failed.
  bool Failed() const override { return failed_; }

  const std::string& OAuth2RefreshToken() const override {
    return oauth2_refresh_token_;
  }
  const std::string& OAuth2AccessToken() const override {
    return oauth2_access_token_;
  }

  // GaiaAuthConsumer overrides.
  void OnClientOAuthSuccess(
      const GaiaAuthConsumer::ClientOAuthResult& oauth_tokens) override;
  void OnClientOAuthFailure(const GoogleServiceAuthError& error) override;

  // OAuth2AccessTokenConsumer overrides.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const GoogleServiceAuthError& error) override;

  // Starts fetching OAuth2 refresh token.
  void StartFetchingRefreshToken();

  // Starts fetching OAuth2 access token for the device management service.
  void StartFetchingAccessToken();

  // Decides how to proceed on GAIA |error|. If the error looks temporary,
  // retries |task| until max retry count is reached.
  // If retry count runs out, or error condition is unrecoverable, it calls
  // Delegate::OnOAuth2TokenFetchFailed().
  void RetryOnError(const GoogleServiceAuthError& error,
                    const base::Closure& task);

  // Passes |token| and |error| to the |callback_|.
  void ForwardPolicyToken(const std::string& token,
                          const GoogleServiceAuthError& error);

  // Auth code which is used to retreive a refresh token.
  std::string auth_code_;

  scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory_;
  std::unique_ptr<GaiaAuthFetcher> refresh_token_fetcher_;
  std::unique_ptr<OAuth2AccessTokenFetcher> access_token_fetcher_;

  // OAuth2 refresh token. Could come either from the outside or through
  // refresh token fetching flow within this class.
  std::string oauth2_refresh_token_;

  // OAuth2 access token.
  std::string oauth2_access_token_;

  // The retry counter. Increment this only when failure happened.
  int retry_count_ = 0;

  // True if we have already failed to fetch the policy.
  bool failed_ = false;

  // The callback to invoke when done.
  TokenCallback callback_;

  base::WeakPtrFactory<PolicyOAuth2TokenFetcherImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PolicyOAuth2TokenFetcherImpl);
};

PolicyOAuth2TokenFetcherImpl::PolicyOAuth2TokenFetcherImpl() {}

PolicyOAuth2TokenFetcherImpl::~PolicyOAuth2TokenFetcherImpl() {}

void PolicyOAuth2TokenFetcherImpl::StartWithAuthCode(
    const std::string& auth_code,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
    const TokenCallback& callback) {
  DCHECK(!refresh_token_fetcher_ && !access_token_fetcher_);

  auth_code_ = auth_code;
  system_url_loader_factory_ = system_url_loader_factory;
  callback_ = callback;
  StartFetchingRefreshToken();
}

void PolicyOAuth2TokenFetcherImpl::StartWithRefreshToken(
    const std::string& oauth2_refresh_token,
    scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
    const TokenCallback& callback) {
  DCHECK(!refresh_token_fetcher_ && !access_token_fetcher_);

  oauth2_refresh_token_ = oauth2_refresh_token;
  system_url_loader_factory_ = system_url_loader_factory;
  callback_ = callback;
  StartFetchingAccessToken();
}

void PolicyOAuth2TokenFetcherImpl::StartFetchingRefreshToken() {
  // Don't fetch tokens for test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    failed_ = true;
    ForwardPolicyToken(
        std::string(),
        GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
    return;
  }

  DCHECK(!auth_code_.empty());
  refresh_token_fetcher_.reset(new GaiaAuthFetcher(
      this, gaia::GaiaSource::kChrome, system_url_loader_factory_));
  refresh_token_fetcher_->StartAuthCodeForOAuth2TokenExchange(auth_code_);
}

void PolicyOAuth2TokenFetcherImpl::StartFetchingAccessToken() {
  std::vector<std::string> scopes;
  scopes.push_back(GaiaConstants::kDeviceManagementServiceOAuth);
  scopes.push_back(GaiaConstants::kOAuthWrapBridgeUserInfoScope);
  access_token_fetcher_.reset(new OAuth2AccessTokenFetcherImpl(
      this, system_url_loader_factory_, oauth2_refresh_token_));
  access_token_fetcher_->Start(
      GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
      scopes);
}

void PolicyOAuth2TokenFetcherImpl::OnClientOAuthSuccess(
    const GaiaAuthConsumer::ClientOAuthResult& oauth2_tokens) {
  VLOG(1) << "OAuth2 tokens for policy fetching succeeded.";
  oauth2_refresh_token_ = oauth2_tokens.refresh_token;
  retry_count_ = 0;
  StartFetchingAccessToken();
}

void PolicyOAuth2TokenFetcherImpl::OnClientOAuthFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "OAuth2 tokens fetch for policy fetch failed! (error = "
          << error.state() << ")";
  RetryOnError(
      error,
      base::Bind(&PolicyOAuth2TokenFetcherImpl::StartFetchingRefreshToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void PolicyOAuth2TokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  VLOG(1) << "OAuth2 access token (device management) fetching succeeded.";
  oauth2_access_token_ = token_response.access_token;
  ForwardPolicyToken(token_response.access_token,
                     GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void PolicyOAuth2TokenFetcherImpl::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "OAuth2 access token (device management) fetching failed!";
  RetryOnError(
      error, base::Bind(&PolicyOAuth2TokenFetcherImpl::StartFetchingAccessToken,
                        weak_ptr_factory_.GetWeakPtr()));
}

void PolicyOAuth2TokenFetcherImpl::RetryOnError(
    const GoogleServiceAuthError& error,
    const base::Closure& task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error.IsTransientError() && retry_count_ < kMaxRequestAttemptCount) {
    retry_count_++;
    base::PostDelayedTask(
        FROM_HERE, {BrowserThread::UI}, task,
        base::TimeDelta::FromMilliseconds(kRequestRestartDelay));
    return;
  }
  LOG(ERROR) << "Unrecoverable error or retry count max reached: "
             << error.state();
  failed_ = true;
  // Invoking the |callback_| signals to the owner of this object that it has
  // completed, and the owner may delete this object on the callback method.
  // So don't rely on |this| still being valid after ForwardPolicyToken()
  // returns i.e. don't write to |failed_| or other fields.
  ForwardPolicyToken(std::string(), error);
}

void PolicyOAuth2TokenFetcherImpl::ForwardPolicyToken(
    const std::string& token,
    const GoogleServiceAuthError& error) {
  if (!callback_.is_null())
    callback_.Run(token, error);
}

// Fake token fetcher that immediately returns tokens without making network
// requests.
class PolicyOAuth2TokenFetcherFake : public PolicyOAuth2TokenFetcher {
 public:
  PolicyOAuth2TokenFetcherFake() {}
  ~PolicyOAuth2TokenFetcherFake() override {}

 private:
  void StartWithAuthCode(
      const std::string& auth_code,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) override {
    ForwardPolicyToken(callback);
  }

  void StartWithRefreshToken(
      const std::string& oauth2_refresh_token,
      scoped_refptr<network::SharedURLLoaderFactory> system_url_loader_factory,
      const TokenCallback& callback) override {
    ForwardPolicyToken(callback);
  }

  bool Failed() const override { return false; }
  const std::string& OAuth2RefreshToken() const override {
    return refresh_token_;
  }
  const std::string& OAuth2AccessToken() const override {
    return access_token_;
  }

 private:
  void ForwardPolicyToken(const TokenCallback& callback) {
    if (!callback.is_null())
      callback.Run(access_token_, GoogleServiceAuthError::AuthErrorNone());
  }

  const std::string refresh_token_ = "fake_refresh_token";
  const std::string access_token_ = "fake_access_token";

  DISALLOW_COPY_AND_ASSIGN(PolicyOAuth2TokenFetcherFake);
};

}  // namespace

// static
void PolicyOAuth2TokenFetcher::UseFakeTokensForTesting() {
  use_fake_tokens_for_testing_ = true;
}

// static
std::unique_ptr<PolicyOAuth2TokenFetcher>
PolicyOAuth2TokenFetcher::CreateInstance() {
  if (use_fake_tokens_for_testing_)
    return std::make_unique<PolicyOAuth2TokenFetcherFake>();
  return std::make_unique<PolicyOAuth2TokenFetcherImpl>();
}

PolicyOAuth2TokenFetcher::PolicyOAuth2TokenFetcher() {}

PolicyOAuth2TokenFetcher::~PolicyOAuth2TokenFetcher() {}

}  // namespace policy
