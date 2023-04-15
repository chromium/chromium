// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_LOGIN_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_LOGIN_NOTIFICATION_H_

#include "base/scoped_observation.h"
#include "chromeos/ash/components/login/login_state/login_state.h"

namespace ash {

// Notification that is shown on user login if multi capture is enabled.
class MultiCaptureLoginNotification : public LoginState::Observer {
 public:
  MultiCaptureLoginNotification();
  MultiCaptureLoginNotification(const MultiCaptureLoginNotification&) = delete;
  MultiCaptureLoginNotification& operator=(
      const MultiCaptureLoginNotification&) = delete;
  ~MultiCaptureLoginNotification() override;

  // LoginState::Observer:
  void LoggedInStateChanged() override;

 private:
  base::ScopedObservation<LoginState, LoginState::Observer>
      login_state_observation_{this};
};

void SetIsMultiCaptureAllowedCallbackForTesting(bool is_multi_capture_allowed);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_MULTI_CAPTURE_LOGIN_NOTIFICATION_H_
