// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/common/bound_session_request_throttled_handler.h"
#include "chrome/common/renderer_configuration.mojom-shared.h"

class FakeBoundSessionCookieRefreshService
    : public BoundSessionCookieRefreshService {
 public:
  FakeBoundSessionCookieRefreshService();

  ~FakeBoundSessionCookieRefreshService() override;

  void SimulateUnblockRequest(
      chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger);
  bool IsRequestBlocked();
  void SimulateOnBoundSessionTerminated(
      const GURL& site,
      const base::flat_set<std::string>& bound_cookie_names);

  // BoundSessionCookieRefreshService:
  void Initialize() override {}
  void RegisterNewBoundSession(
      const bound_session_credentials::BoundSessionParams& params) override {}
  void MaybeTerminateSession(const GURL& response_url,
                             const net::HttpResponseHeaders* headers) override {
  }
  std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
  GetBoundSessionThrottlerParams() const override;
  void HandleRequestBlockedOnCookie(
      const GURL& untrusted_request_url,
      HandleRequestBlockedOnCookieCallback resume_blocked_request) override;
  void SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
      RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater)
      override {}
  void SetBoundSessionParamsUpdatedCallbackForTesting(
      base::RepeatingClosure updated_callback) override {}
  void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) override {}
  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<BoundSessionDebugInfo> GetBoundSessionDebugInfo() const override;

 private:
  HandleRequestBlockedOnCookieCallback resume_blocked_request_;
  base::ObserverList<BoundSessionCookieRefreshService::Observer> observers_;
  base::WeakPtrFactory<BoundSessionCookieRefreshService> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_FAKE_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
