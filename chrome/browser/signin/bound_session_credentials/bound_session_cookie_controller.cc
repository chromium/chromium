// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

BoundSessionCookieController::BoundSessionCookieController(
    bound_session_credentials::RegistrationParams registration_params,
    const base::flat_set<std::string>& cookie_names,
    Delegate* delegate)
    : url_(registration_params.site()), delegate_(delegate) {
  CHECK(!url_.is_empty());
  CHECK(!cookie_names.empty());
  for (const std::string& cookie_name : cookie_names) {
    bound_cookies_info_.insert({cookie_name, base::Time()});
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

chrome::mojom::BoundSessionParamsPtr
BoundSessionCookieController::bound_session_params() {
  return chrome::mojom::BoundSessionParams::New(url().host(), url().path(),
                                                min_cookie_expiration_time());
}
