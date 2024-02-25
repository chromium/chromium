// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_session_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_observer.h"
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
  void OnClientConnecting() override;
  void OnClientConnected() override;
  void OnClientDisconnected() override;

 private:
  class Notification {
   public:
    void Show();
    void Hide();
    void SetAsHidden();

   private:
    // Unfortunately the `LocalHost` has no API to query if the notification
    // screen is showing, so this must manually be tracked to prevent
    // dismissing other OOBE screens when trying to hide the notification.
    bool is_showing_ = false;
  };

  void OnRemoteAdminWasPresentPrefChanged();

  base::RepeatingCallback<bool()> is_current_session_curtained_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      observation_{this};
  BooleanPrefMember remote_admin_was_present_;
  Notification notification_;

  base::WeakPtrFactory<RemoteActivityNotificationController> weak_ptr_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_REMOTE_ACTIVITY_NOTIFICATION_CONTROLLER_H_
