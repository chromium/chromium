// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/fake_bound_session_cookie_refresh_service.h"

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"
#include "chrome/common/renderer_configuration.mojom-shared.h"

FakeBoundSessionCookieRefreshService::FakeBoundSessionCookieRefreshService() =
    default;

FakeBoundSessionCookieRefreshService::~FakeBoundSessionCookieRefreshService() =
    default;

std::vector<chrome::mojom::BoundSessionThrottlerParamsPtr>
FakeBoundSessionCookieRefreshService::GetBoundSessionThrottlerParams() const {
  return {};
}

void FakeBoundSessionCookieRefreshService::HandleRequestBlockedOnCookie(
    const GURL& untrusted_request_url,
    HandleRequestBlockedOnCookieCallback resume_blocked_request) {
  resume_blocked_request_ = std::move(resume_blocked_request);
}

base::WeakPtr<BoundSessionCookieRefreshService>
FakeBoundSessionCookieRefreshService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeBoundSessionCookieRefreshService::SimulateUnblockRequest(
    chrome::mojom::ResumeBlockedRequestsTrigger resume_trigger) {
  std::move(resume_blocked_request_).Run(resume_trigger);
}

void FakeBoundSessionCookieRefreshService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBoundSessionCookieRefreshService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<BoundSessionDebugInfo>
FakeBoundSessionCookieRefreshService::GetBoundSessionDebugInfo() const {
  return {};
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
