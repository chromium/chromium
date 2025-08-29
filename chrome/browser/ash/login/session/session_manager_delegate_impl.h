// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_MANAGER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_MANAGER_DELEGATE_IMPL_H_

#include "components/session_manager/core/session_manager_delegate.h"

namespace ash {

// This is the concrete implementation of our delegate interface.
class SessionManagerDelegateImpl
    : public session_manager::SessionManagerDelegate {
 public:
  SessionManagerDelegateImpl();
  SessionManagerDelegateImpl(const SessionManagerDelegateImpl&) = delete;
  SessionManagerDelegateImpl& operator=(const SessionManagerDelegateImpl&) =
      delete;

  ~SessionManagerDelegateImpl() override;

  // session_manager::SessionManagerDelegate override:
  void RequestSignOut() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SESSION_SESSION_MANAGER_DELEGATE_IMPL_H_
