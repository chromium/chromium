// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_params.pb.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SigninClient;
class PrefService;

namespace unexportable_keys {
class UnexportableKeyService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class BoundSessionCookieRefreshServiceImpl
    : public BoundSessionCookieRefreshService,
      public BoundSessionCookieController::Delegate {
 public:
  explicit BoundSessionCookieRefreshServiceImpl(
      unexportable_keys::UnexportableKeyService& key_service,
      PrefService* pref_service,
      SigninClient* client);

  ~BoundSessionCookieRefreshServiceImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // BoundSessionCookieRefreshService:
  void Initialize() override;
  // Can be called iff the kBoundSessionExplicitRegistration feature is enabled.
  void RegisterNewBoundSession(
      const bound_session_credentials::RegistrationParams& params) override;
  void MaybeTerminateSession(const net::HttpResponseHeaders* headers) override;
  bool IsBoundSession() const override;
  chrome::mojom::BoundSessionParamsPtr GetBoundSessionParams() const override;
  void AddBoundSessionRequestThrottledListenerReceiver(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledListener>
          receiver) override;

  // chrome::mojom::BoundSessionRequestThrottledListener:
  void OnRequestBlockedOnCookie(
      OnRequestBlockedOnCookieCallback resume_blocked_request) override;

  void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) override;

  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;

 private:
  class BoundSessionStateTracker;
  friend class BoundSessionCookieRefreshServiceImplTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionCookieController`.
  using BoundSessionCookieControllerFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionCookieController>(
          bound_session_credentials::RegistrationParams registration_params,
          const base::flat_set<std::string>& cookie_names,
          Delegate* delegate)>;

  // BoundSessionCookieRefreshService:
  void SetRendererBoundSessionParamsUpdaterDelegate(
      RendererBoundSessionParamsUpdaterDelegate renderer_updater) override;

  void set_controller_factory_for_testing(
      const BoundSessionCookieControllerFactoryForTesting&
          controller_factory_for_testing) {
    controller_factory_for_testing_ = controller_factory_for_testing;
  }

  void OnRegistrationRequestComplete(
      absl::optional<bound_session_credentials::RegistrationParams>
          registration_params);
  bool IsValidRegistrationParams(
      const bound_session_credentials::RegistrationParams& registration_params);
  bool PersistRegistrationParams(
      const bound_session_credentials::RegistrationParams& registration_params);
  absl::optional<bound_session_credentials::RegistrationParams>
  GetRegistrationParams();

  // BoundSessionCookieController::Delegate
  void OnBoundSessionParamsChanged() override;
  void TerminateSession() override;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(
      bound_session_credentials::RegistrationParams registration_params,
      const base::flat_set<std::string>& cookie_names);
  void InitializeBoundSession();
  void ResetBoundSession();
  void OnBoundSessionUpdated();

  void UpdateAllRenderers();

  const raw_ref<unexportable_keys::UnexportableKeyService> key_service_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<SigninClient> client_;
  BoundSessionCookieControllerFactoryForTesting controller_factory_for_testing_;
  RendererBoundSessionParamsUpdaterDelegate renderer_updater_;

  std::unique_ptr<BoundSessionCookieController> cookie_controller_;

  mojo::ReceiverSet<chrome::mojom::BoundSessionRequestThrottledListener>
      renderer_request_throttled_listener_;

  // There is only one active session registration at a time.
  std::unique_ptr<BoundSessionRegistrationFetcher> active_registration_request_;

  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
