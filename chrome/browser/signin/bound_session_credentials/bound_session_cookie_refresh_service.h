// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_param.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

struct BoundSessionDebugInfo;

// BoundSessionCookieRefreshService is responsible for maintaining cookies
// associated with bound sessions. This class does the following:
// - Tracks bound sessions
// - Provides bound session params to renderers
// - Monitors cookie changes and update renderers
// - Preemptively refreshes bound session cookies
class BoundSessionCookieRefreshService
    : public KeyedService,
      public chrome::mojom::BoundSessionRequestThrottledHandler {
 public:
  using RendererBoundSessionThrottlerParamsUpdaterDelegate =
      base::RepeatingClosure;

  class Observer : public base::CheckedObserver {
   public:
    // TODO(b/314280617): Consider passing
    // `bound_session_credentials::BoundSessionParams` instead.
    // - `site` is the top-most origin covered by the terminated session.
    // - `bound_cookie_names` contains names of short lived cookies required for
    //   the user to be authenticated.
    virtual void OnBoundSessionTerminated(
        const GURL& site,
        const base::flat_set<std::string>& bound_cookie_names) = 0;
  };

  BoundSessionCookieRefreshService() = default;

  BoundSessionCookieRefreshService(const BoundSessionCookieRefreshService&) =
      delete;
  BoundSessionCookieRefreshService& operator=(
      const BoundSessionCookieRefreshService&) = delete;

  virtual void Initialize() = 0;

  // Registers a new bound session and starts tracking it immediately. The
  // session persists across browser startups.
  virtual void RegisterNewBoundSession(
      const bound_session_credentials::BoundSessionParams& params) = 0;

  // Terminates a session running on `response_url`'s site if the session
  // termination header is set and the header's session ID matches the current
  // session id. This header is expected to be set on signout.
  virtual void MaybeTerminateSession(
      const GURL& response_url,
      const net::HttpResponseHeaders* headers) = 0;

  // Returns bound session params.
  virtual std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
  GetBoundSessionThrottlerParams() const = 0;

  virtual void CreateRegistrationRequest(
      BoundSessionRegistrationFetcherParam registration_params) = 0;

  virtual base::WeakPtr<BoundSessionCookieRefreshService> GetWeakPtr() = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual std::vector<BoundSessionDebugInfo> GetBoundSessionDebugInfo()
      const = 0;

 private:
  friend class RendererUpdater;
  friend class BoundSessionCookieRefreshServiceImplBrowserTest;

  // `RendererUpdater` class that is responsible for pushing updates to all
  // renderers calls this setter to subscribe for bound session throttler params
  // updates.
  virtual void SetRendererBoundSessionThrottlerParamsUpdaterDelegate(
      RendererBoundSessionThrottlerParamsUpdaterDelegate renderer_updater) = 0;

  // Registers a callback for testing to be notified about session parameters
  // updates.
  // TODO(http://b/303375108): consider exposing an observer interface instead.
  virtual void SetBoundSessionParamsUpdatedCallbackForTesting(
      base::RepeatingClosure updated_callback) = 0;

  // Adds a Receiver to `BoundSessionCookieRefreshService` to receive
  // notification when a request is throttled and requires a fresh cookie.
  virtual void AddBoundSessionRequestThrottledHandlerReceiver(
      mojo::PendingReceiver<chrome::mojom::BoundSessionRequestThrottledHandler>
          receiver) {}
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_COOKIE_REFRESH_SERVICE_H_
