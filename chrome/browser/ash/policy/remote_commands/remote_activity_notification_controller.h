// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/views/widget/widget.h"

namespace policy {

class RemoteActivityNotificationController
    : public session_manager::SessionManagerObserver {
 public:
  RemoteActivityNotificationController(
      PrefService& local_state,
      base::RepeatingCallback<bool()> is_current_session_curtained);
  ~RemoteActivityNotificationController() override;

  // `session_manager::SessionManagerObserver` implementation.
  void OnLoginOrLockScreenVisible() override;

  void OnCurtainSessionStarted();

  void ClickNotificationButtonForTesting();

 private:
  void OnNotificationCloseButtonClick();

  void Init();

  void ShowNotification();

  raw_ref<PrefService> local_state_;
  std::unique_ptr<views::Widget> widget_;
  base::RepeatingCallback<bool()> is_current_session_curtained_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
