// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "url/gurl.h"

namespace unexportable_keys {
class UnexportableKeyService;
}  // namespace unexportable_keys

class SigninClient;
class BoundSessionCookieObserver;
class SessionBindingHelper;
class WaitForNetworkCallbackHelper;

class BoundSessionCookieControllerImpl : public BoundSessionCookieController {
 public:
  BoundSessionCookieControllerImpl(
      unexportable_keys::UnexportableKeyService& key_service,
      SigninClient* client,
      bound_session_credentials::RegistrationParams registration_params,
      const base::flat_set<std::string>& cookie_names,
      Delegate* delegate);

  void Initialize() override;

  void OnRequestBlockedOnCookie(
      base::OnceClosure resume_blocked_request) override;

  ~BoundSessionCookieControllerImpl() override;

  BoundSessionCookieControllerImpl(const BoundSessionCookieControllerImpl&) =
      delete;
  BoundSessionCookieControllerImpl& operator=(
      const BoundSessionCookieControllerImpl&) = delete;

 private:
  friend class BoundSessionCookieControllerImplTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionRefreshCookieFetcher`.
  using RefreshCookieFetcherFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionRefreshCookieFetcher>(
          network::mojom::CookieManager* cookie_manager,
          const GURL& url,
          base::flat_set<std::string> cookie_names)>;

  std::unique_ptr<BoundSessionRefreshCookieFetcher> CreateRefreshCookieFetcher()
      const;
  void CreateBoundCookiesObservers();

  bool AreAllCookiesFresh();
  void MaybeRefreshCookie();
  void SetCookieExpirationTimeAndNotify(const std::string& cookie_name,
                                        base::Time expiration_time);
  void OnCookieRefreshFetched(BoundSessionRefreshCookieFetcher::Result result);
  void MaybeScheduleCookieRotation();
  void ResumeBlockedRequests();

  void set_refresh_cookie_fetcher_factory_for_testing(
      RefreshCookieFetcherFactoryForTesting
          refresh_cookie_fetcher_factory_for_testing) {
    refresh_cookie_fetcher_factory_for_testing_ =
        refresh_cookie_fetcher_factory_for_testing;
  }

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  const raw_ptr<SigninClient> client_;
  std::vector<std::unique_ptr<BoundSessionCookieObserver>>
      bound_cookies_observers_;

  std::unique_ptr<WaitForNetworkCallbackHelper>
      wait_for_network_callback_helper_;
  std::unique_ptr<SessionBindingHelper> session_binding_helper_;
  std::unique_ptr<BoundSessionRefreshCookieFetcher> refresh_cookie_fetcher_;

  std::vector<base::OnceClosure> resume_blocked_requests_;
  // Used to schedule preemptive cookie refresh.
  base::OneShotTimer cookie_refresh_timer_;

  RefreshCookieFetcherFactoryForTesting
      refresh_cookie_fetcher_factory_for_testing_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_CONTROLLER_IMPL_H_
