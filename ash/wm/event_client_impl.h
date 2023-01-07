// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_EVENT_CLIENT_IMPL_H_
#define ASH_WM_EVENT_CLIENT_IMPL_H_

#include "ash/ash_export.h"
#include "ui/aura/client/event_client.h"

namespace ash {

class ScopedSkipUserSessionBlockedCheck;

class ASH_EXPORT EventClientImpl : public aura::client::EventClient {
 public:
  EventClientImpl();

  EventClientImpl(const EventClientImpl&) = delete;
  EventClientImpl& operator=(const EventClientImpl&) = delete;

  ~EventClientImpl() override;

 private:
  friend class ScopedSkipUserSessionBlockedCheck;

  // Overridden from aura::client::EventClient:
  bool GetCanProcessEventsWithinSubtree(
      const aura::Window* window) const override;
  ui::EventTarget* GetToplevelEventTarget() override;

  // This should only be used by ScopedSkipUserSessionBlockedCheck for blocks of
  // code which we want the checking of the user session to be blocked (ie.
  // building mru list when entering tablet mode in lock screen).
  void set_skip_user_session_blocked_check(
      bool skip_user_session_blocked_check) {
    skip_user_session_blocked_check_ = skip_user_session_blocked_check;
  }

  // Flag indicating whether the user sesion checks in
  // GetCanProcessEventsWithinSubtree should be skipped.
  bool skip_user_session_blocked_check_ = false;
};

}  // namespace ash

#endif  // ASH_WM_EVENT_CLIENT_IMPL_H_
