// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "url/gurl.h"

BoundSessionCookieController::BoundSessionCookieController(
    const bound_session_credentials::BoundSessionParams& bound_session_params,
    Delegate* delegate)
    : scope_url_(bound_session_credentials::GetBoundSessionScope(
          bound_session_params)),
      session_id_(bound_session_params.session_id()),
      session_creation_time_(bound_session_credentials::TimestampToTime(
          bound_session_params.creation_time())),
      refresh_url_(bound_session_params.refresh_url()),
      site_(bound_session_params.site()),
      delegate_(delegate) {
  CHECK(!scope_url_.is_empty());
  CHECK(!site_.is_empty());
  CHECK(!bound_session_params.credentials().empty());
  // Note:
  // - Same cookie name with a different scope (Domain, Path) is not
  //   supported. We expect cookie names to be unique.
  // - The scope of the cookie is ignored and is assumed to have the same scope
  //   of the session.
  for (const bound_session_credentials::Credential& credential :
       bound_session_params.credentials()) {
    bound_cookies_info_.insert(
        {credential.cookie_credential().name(), base::Time()});
  }
}

BoundSessionCookieController::~BoundSessionCookieController() = default;

void BoundSessionCookieController::Initialize() {}

base::Time BoundSessionCookieController::min_cookie_expiration_time() const {
  CHECK(!bound_cookies_info_.empty());
  return base::ranges::min_element(bound_cookies_info_, {},
                                   [](const auto& bound_cookie_info) {
                                     return bound_cookie_info.second;
                                   })
      ->second;
}

chrome::mojom::BoundSessionThrottlerParamsPtr
BoundSessionCookieController::bound_session_throttler_params() const {
  if (ShouldPauseThrottlingRequests()) {
    return nullptr;
  }

  return chrome::mojom::BoundSessionThrottlerParams::New(
      scope_url().host(), scope_url().path(), min_cookie_expiration_time());
}

base::flat_set<std::string> BoundSessionCookieController::bound_cookie_names()
    const {
  return base::MakeFlatSet<std::string>(
      bound_cookies_info_, {},
      [](const auto& bound_cookie_info) { return bound_cookie_info.first; });
}

BoundSessionKey BoundSessionCookieController::GetBoundSessionKey() const {
  return {.site = site(), .session_id = session_id()};
}
