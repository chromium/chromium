// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

// This class makes the network request to the DBSC registration endpoint to
// get the registration instructions. A new fetcher instance should be created
// per request.
class BoundSessionRegistrationFetcher {
 public:
  virtual ~BoundSessionRegistrationFetcher() = default;

  using RegistrationCompleteCallback = base::OnceCallback<void(
      std::optional<bound_session_credentials::BoundSessionParams>)>;

  // Starts the network request to the DBSC registration endpoint. `callback`
  // is called with the fetch results upon completion. Should be called no more
  // than once per instance.
  virtual void Start(RegistrationCompleteCallback callback) = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_BOUND_SESSION_REGISTRATION_FETCHER_H_
