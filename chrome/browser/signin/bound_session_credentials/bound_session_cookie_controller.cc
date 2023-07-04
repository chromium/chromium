// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

BoundSessionCookieController::BoundSessionCookieController(
    const GURL& url,
    const std::vector<std::string>& cookie_names,
    Delegate* delegate)
    : url_(url), delegate_(delegate) {
  CHECK(!url.is_empty());
  CHECK(!cookie_names.empty());
  for (const std::string& cookie_name : cookie_names) {
    bound_cookies_info_.insert({cookie_name, base::Time()});
  }
}

BoundSessionCookieController::~BoundSessionCookieController() = default;

void BoundSessionCookieController::Initialize() {}

const std::string& BoundSessionCookieController::cookie_name() const {
  CHECK(!bound_cookies_info_.empty());
  return bound_cookies_info_.begin()->first;
}
base::Time BoundSessionCookieController::cookie_expiration_time() {
  CHECK(!bound_cookies_info_.empty());
  return bound_cookies_info_.begin()->second;
}
