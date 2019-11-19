// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/session/session_types.h"

namespace ash {

bool operator==(const SessionInfo& a, const SessionInfo& b) {
  return a.can_lock_screen == b.can_lock_screen &&
         a.should_lock_screen_automatically ==
             b.should_lock_screen_automatically &&
         a.is_running_in_app_mode == b.is_running_in_app_mode &&
         a.is_demo_session == b.is_demo_session &&
         a.add_user_session_policy == b.add_user_session_policy &&
         a.state == b.state;
}

UserSession::UserSession() = default;
UserSession::UserSession(const UserSession& other) = default;
UserSession::~UserSession() = default;

bool operator==(const UserSession& a, const UserSession& b) {
  return a.session_id == b.session_id && a.user_info == b.user_info &&
         a.custodian_email == b.custodian_email &&
         a.second_custodian_email == b.second_custodian_email &&
         a.should_enable_settings == b.should_enable_settings &&
         a.should_show_notification_tray == b.should_show_notification_tray;
}

}  // namespace ash
