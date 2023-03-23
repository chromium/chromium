// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include <memory>
#include "base/functional/callback_forward.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

using signin::IdentityManager;

class SigninClient;

// This service is responsible for the following:
// - Tracking bound session
// - It owns `cookie_controller_` that fully manages a bound session cookie
// - Monitors cookie changes and update the renderers
// - Provides bound session params to renderers
// This class is still work in progress.
//
//                                         BoundSessionCookieFetcher
//                                                     ^
//                                                     | 1
//                                                     |
// BoundSessionCookieRefreshService------> BoundSessionCookieController
//                                                     | 1
//                                                     |
//                                                     V
//                                         BoundSessionCookieObserver
class BoundSessionCookieRefreshService
    : public KeyedService,
      public BoundSessionCookieController::Delegate {
 public:
  explicit BoundSessionCookieRefreshService(SigninClient* client,
                                            IdentityManager* identity_manager);
  ~BoundSessionCookieRefreshService() override;

  void Initialize();

  // Returns true if session is bound.
  bool IsBoundSession() const;

  // Called when a network request requires a fresh SIDTS cookie. This function
  // is intended to be called by network requests throttlers.
  // The callback will be called once the cookie is fresh or the session is
  // terminated. Note: The callback might be called synchronously if the
  // previous conditions apply.
  void OnRequestBlockedOnCookie(base::OnceClosure resume_blocked_request);

 private:
  class BoundSessionStateTracker;
  friend class BoundSessionCookieRefreshServiceTest;

  // Used by tests to provide their own implementation of the
  // `BoundSessionCookieController`.
  using BoundSessionCookieControllerFactoryForTesting =
      base::RepeatingCallback<std::unique_ptr<BoundSessionCookieController>(
          const GURL& url,
          const std::string& cookie_name,
          Delegate* delegate)>;

  void set_controller_factory_for_testing(
      const BoundSessionCookieControllerFactoryForTesting&
          controller_factory_for_testing) {
    controller_factory_for_testing_ = controller_factory_for_testing;
  }

  // BoundSessionCookieController::Delegate
  void OnCookieExpirationDateChanged() override;

  std::unique_ptr<BoundSessionCookieController>
  CreateBoundSessionCookieController(const GURL& url,
                                     const std::string& cookie_name);
  void StartManagingBoundSessionCookie();
  void StopManagingBoundSessionCookie();
  void OnBoundSessionUpdated();

  void UpdateAllRenderers();

  const raw_ptr<SigninClient> client_;
  const raw_ptr<IdentityManager> identity_manager_;
  BoundSessionCookieControllerFactoryForTesting controller_factory_for_testing_;

  std::unique_ptr<BoundSessionStateTracker> bound_session_tracker_;
  std::unique_ptr<BoundSessionCookieController> cookie_controller_;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
