// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_params.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class SigninClient;

namespace user_prefs {
class PrefRegistrySyncable;
}

// If the feature is on, `BoundSessionCookieRefreshServiceImpl` uses only
// explicitly registered sessions instead of relying on the primary account
// state.
BASE_DECLARE_FEATURE(kBoundSessionExplicitRegistration);

class BoundSessionCookieRefreshServiceImpl
    : public BoundSessionCookieRefreshService,
      public BoundSessionCookieController::Delegate {
 public:
  explicit BoundSessionCookieRefreshServiceImpl(
      SigninClient* client,
      signin::IdentityManager* identity_manager);

  ~BoundSessionCookieRefreshServiceImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // BoundSessionCookieRefreshService:
  void Initialize() override;
  // Can be called iff the kBoundSessionExplicitRegistration feature is enabled.
  void RegisterNewBoundSession(
      const bound_session_credentials::RegistrationParams& params) override;
  bool IsBoundSession() const override;
  chrome::mojom::BoundSessionParamsPtr GetBoundSessionParams() const override;
  void AddBoundSessionRequestThrottledListenerReceiver(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledListener>
          receiver) override;

  // chrome::mojom::BoundSessionRequestThrottledListener:
  void OnRequestBlockedOnCookie(
      OnRequestBlockedOnCookieCallback resume_blocked_request) override;

  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;

 private:
  class BoundSessionStateTracker;
  friend class BoundSessionCookieRefreshServiceImplTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionCookieController`.
  using BoundSessionCookieControllerFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionCookieController>(
          const GURL& url,
          const std::string& cookie_name,
          Delegate* delegate)>;

  // BoundSessionCookieRefreshService:
  void SetRendererBoundSessionParamsUpdaterDelegate(
      RendererBoundSessionParamsUpdaterDelegate renderer_updater) override;

  void set_controller_factory_for_testing(
      const BoundSessionCookieControllerFactoryForTesting&
          controller_factory_for_testing) {
    controller_factory_for_testing_ = controller_factory_for_testing;
  }

  // BoundSessionCookieController::Delegate
  void OnCookieExpirationDateChanged() override;
  void TerminateSession() override;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(const GURL& url,
                                     const std::string& cookie_name);
  void StartManagingBoundSessionCookie();
  void StopManagingBoundSessionCookie();
  void OnBoundSessionUpdated();

  void UpdateAllRenderers();

  const raw_ptr<SigninClient> client_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  BoundSessionCookieControllerFactoryForTesting controller_factory_for_testing_;
  RendererBoundSessionParamsUpdaterDelegate renderer_updater_;

  std::unique_ptr<BoundSessionStateTracker> bound_session_tracker_;
  std::unique_ptr<BoundSessionCookieController> cookie_controller_;

  mojo::ReceiverSet<chrome::mojom::BoundSessionRequestThrottledListener>
      renderer_request_throttled_listener_;

  // TODO(b/273920956): Remove when the registration flow is implemented and we
  // no longer rely on chrome signin status. Note: This is not stored on disk.
  // On next startup, the session will still be bound. This is fine as the
  // feature is still WIP.
  bool force_terminate_bound_session_ = false;

  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_IMPL_H_
