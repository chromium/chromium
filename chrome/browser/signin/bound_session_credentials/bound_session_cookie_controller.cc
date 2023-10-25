// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "url/gurl.h"

BoundSessionCookieController::BoundSessionCookieController(
    const bound_session_credentials::BoundSessionParams& bound_session_params,
    Delegate* delegate)
    : url_(bound_session_params.site()),
      session_id_(bound_session_params.session_id()),
      session_creation_time_(bound_session_credentials::TimestampToTime(
          bound_session_params.creation_time())),
      delegate_(delegate) {
  CHECK(!url_.is_empty());
  CHECK(!bound_session_params.credentials().empty());
  // Note:
  // - Same cookie name with a different scope (Domain, Path) is not
  //   supported. We expect cookie names to be unique.
  // - The scope of the cookie is ignored and is assumed to have the same scope
  //   of the session.
  // - The scope of the session is site based (not origin) and is specified in
  //   `url`.
  // - A cookie path other than '/' isn't supported.
  for (const bound_session_credentials::Credential& credential :
       bound_session_params.credentials()) {
    bound_cookies_info_.insert(
        {credential.cookie_credential().name(), base::Time()});
  }
}

BoundSessionCookieController::~BoundSessionCookieController() = default;

void BoundSessionCookieController::Initialize() {}

base::Time BoundSessionCookieController::min_cookie_expiration_time() {
  CHECK(!bound_cookies_info_.empty());
  return base::ranges::min_element(bound_cookies_info_, {},
                                   [](const auto& bound_cookie_info) {
                                     return bound_cookie_info.second;
                                   })
      ->second;
}

chrome::mojom::BoundSessionThrottlerParamsPtr
BoundSessionCookieController::bound_session_throttler_params() {
  return chrome::mojom::BoundSessionThrottlerParams::New(
      url().host(), url().path(), min_cookie_expiration_time());
}
