// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_controller.h"

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

BoundSessionCookieController::BoundSessionCookieController(
    const GURL& url,
    const std::string& cookie_name,
    Delegate* delegate)
    : url_(url), cookie_name_(cookie_name), delegate_(delegate) {}

BoundSessionCookieController::~BoundSessionCookieController() = default;

void BoundSessionCookieController::Initialize() {}
