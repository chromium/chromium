// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_debug_info.h"

#include <numeric>

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

// static
BoundSessionDebugInfo BoundSessionDebugInfo::Create(
    const BoundSessionCookieController& controller) {
  std::string cookie_names_str =
      base::JoinString(controller.bound_cookie_names(), ", ");

  return BoundSessionDebugInfo(
      controller.session_id(), controller.scope_url().host(),
      controller.scope_url().path(), controller.ShouldPauseThrottlingRequests(),
      controller.min_cookie_expiration_time(), std::move(cookie_names_str),
      controller.refresh_url());
}

BoundSessionDebugInfo::BoundSessionDebugInfo(std::string session_id,
                                             std::string domain,
                                             std::string path,
                                             bool throttling_paused,
                                             base::Time expiration_time,
                                             std::string bound_cookie_names,
                                             GURL refresh_url)
    : session_id(std::move(session_id)),
      domain(std::move(domain)),
      path(std::move(path)),
      throttling_paused(throttling_paused),
      expiration_time(expiration_time),
      bound_cookie_names(std::move(bound_cookie_names)),
      refresh_url(std::move(refresh_url)) {}

BoundSessionDebugInfo::BoundSessionDebugInfo(
    BoundSessionDebugInfo&& other) noexcept = default;

BoundSessionDebugInfo& BoundSessionDebugInfo::operator=(
    BoundSessionDebugInfo&& other) noexcept = default;

BoundSessionDebugInfo::~BoundSessionDebugInfo() = default;
