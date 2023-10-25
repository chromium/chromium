// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/remote_commands/crd_session_observer.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace policy {

// When a private CRD session is finished, ChromeOS shows a notification to
// inform the local user that a remote admin was present. This class handles
// both remembering when this notification must be shown (even after a reboot)
// and actually showing it.
class RemoteActivityNotificationController
    : public session_manager::SessionManagerObserver,
      public CrdSessionObserver {
 public:
  RemoteActivityNotificationController(
      PrefService& local_state,
      base::RepeatingCallback<bool()> is_current_session_curtained);
  ~RemoteActivityNotificationController() override;

  // `session_manager::SessionManagerObserver` implementation:
  void OnLoginOrLockScreenVisible() override;

  // `CrdSessionObserver` implementation:
  void OnClientConnected() override;

  void ClickNotificationButtonForTesting();

 private:
  class WidgetController;

  void OnNotificationCloseButtonClick();

  void Init();

  void ShowNotification();
  void HideNotification();

  raw_ref<PrefService> local_state_;
  std::unique_ptr<WidgetController> widget_controller_;
  base::RepeatingCallback<bool()> is_current_session_curtained_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
