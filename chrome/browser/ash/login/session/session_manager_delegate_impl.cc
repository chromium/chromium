// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/session/session_manager_delegate_impl.h"

#include "chrome/browser/lifetime/application_lifetime.h"

namespace ash {

SessionManagerDelegateImpl::SessionManagerDelegateImpl() = default;

SessionManagerDelegateImpl::~SessionManagerDelegateImpl() = default;

// TODO(crbug.com/440044493): Update SessionController::RequestSignOut callers
// to call SessionManager.
void SessionManagerDelegateImpl::RequestSignOut() {
  chrome::AttemptUserExit();
}

}  // namespace ash
