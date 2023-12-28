// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_SESSION_LIMIT_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_SESSION_SESSION_LIMIT_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/model/session_length_limit_model.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class ASH_EXPORT SessionLimitNotificationController
    : public SessionLengthLimitModel::Observer {
 public:
  SessionLimitNotificationController();

  SessionLimitNotificationController(
      const SessionLimitNotificationController&) = delete;
  SessionLimitNotificationController& operator=(
      const SessionLimitNotificationController&) = delete;

  ~SessionLimitNotificationController() override;

  // SessionLengthLimitModel::Observer:
  void OnSessionLengthLimitUpdated() override;

 private:
  friend class SessionLimitNotificationControllerTest;

  void UpdateNotification();

  std::u16string ComposeNotificationTitle() const;

  static const char kNotificationId[];

  // Unowned.
  const raw_ptr<SessionLengthLimitModel> model_;

  // LimitState of the last time OnSessionLengthLimitUpdate() is called.
  SessionLengthLimitModel::LimitState last_limit_state_ =
      SessionLengthLimitModel::LIMIT_NONE;

  bool has_notification_been_shown_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_SESSION_LIMIT_NOTIFICATION_CONTROLLER_H_
