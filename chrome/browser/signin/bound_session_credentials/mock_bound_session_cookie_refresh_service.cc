// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/mock_bound_session_cookie_refresh_service.h"

std::unique_ptr<KeyedService> MockBoundSessionCookieRefreshService::Build() {
  return std::make_unique<MockBoundSessionCookieRefreshService>();
}

MockBoundSessionCookieRefreshService::MockBoundSessionCookieRefreshService() =
    default;
MockBoundSessionCookieRefreshService::~MockBoundSessionCookieRefreshService() =
    default;

base::WeakPtr<BoundSessionCookieRefreshService>
MockBoundSessionCookieRefreshService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
