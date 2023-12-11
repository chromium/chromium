// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_cookie_refresh_service.h"

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"

FakeBoundSessionCookieRefreshService::FakeBoundSessionCookieRefreshService() =
    default;

FakeBoundSessionCookieRefreshService::~FakeBoundSessionCookieRefreshService() =
    default;

chrome::mojom::BoundSessionThrottlerParamsPtr
FakeBoundSessionCookieRefreshService::GetBoundSessionThrottlerParams() const {
  return chrome::mojom::BoundSessionThrottlerParams::New();
}

void FakeBoundSessionCookieRefreshService::HandleRequestBlockedOnCookie(
    HandleRequestBlockedOnCookieCallback resume_blocked_request) {
  resume_blocked_request_ = std::move(resume_blocked_request);
}

base::WeakPtr<BoundSessionCookieRefreshService>
FakeBoundSessionCookieRefreshService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeBoundSessionCookieRefreshService::SimulateUnblockRequest() {
  std::move(resume_blocked_request_).Run();
}

void FakeBoundSessionCookieRefreshService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBoundSessionCookieRefreshService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeBoundSessionCookieRefreshService::IsRequestBlocked() {
  return !resume_blocked_request_.is_null();
}

void FakeBoundSessionCookieRefreshService::SimulateOnBoundSessionTerminated(
    const GURL& site,
    const base::flat_set<std::string>& bound_cookie_names) {
  for (BoundSessionCookieRefreshService::Observer& observer : observers_) {
    observer.OnBoundSessionTerminated(site, bound_cookie_names);
  }
}
