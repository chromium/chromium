// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_MOCK_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_MOCK_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBoundSessionCookieRefreshService
    : public BoundSessionCookieRefreshService {
 public:
  static std::unique_ptr<KeyedService> Build();

  MockBoundSessionCookieRefreshService();
  ~MockBoundSessionCookieRefreshService() override;

  MOCK_METHOD(void,
              MaybeTerminateSession,
              (const GURL& response_url,
               const net::HttpResponseHeaders* headers),
              (override));
  MOCK_METHOD(void,
              CreateRegistrationRequest,
              (BoundSessionRegistrationFetcherParam registration_params),
              (override));

  MOCK_METHOD(void, Initialize, (), (override));
  MOCK_METHOD(void,
              RegisterNewBoundSession,
              (const bound_session_credentials::BoundSessionParams& params),
              (override));
  MOCK_METHOD(void,
              StopCookieRotation,
              (const BoundSessionKey& key),
              (override));
  MOCK_METHOD(std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>,
              GetBoundSessionThrottlerParams,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      SetRendererBoundSessionThrottlerParamsUpdaterDelegate,
      (RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater),
      (override));
  MOCK_METHOD(void,
              SetBoundSessionParamsUpdatedCallbackForTesting,
              (base::RepeatingClosure updated_callback),
              (override));
  MOCK_METHOD(void,
              HandleRequestBlockedOnCookie,
              (const GURL&,
               HandleRequestBlockedOnCookieCallback resume_blocked_request),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD((std::vector<BoundSessionDebugInfo>),
              GetBoundSessionDebugInfo,
              (),
              (const, override));

  base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() override;

 private:
  base::WeakPtrFactory<MockBoundSessionCookieRefreshService> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_MOCK_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
