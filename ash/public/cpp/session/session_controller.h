// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
#define ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/session/session_types.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

class AccountId;

namespace ash {

class SessionObserver;
class SessionControllerClient;
class SessionActivationObserver;

// Interface to manage user sessions in ash.
class ASH_PUBLIC_EXPORT SessionController {
 public:
  // Get the instance of session controller.
  static SessionController* Get();

  // Sets the client interface.
  virtual void SetClient(SessionControllerClient* client) = 0;

  // Sets the ash session info.
  virtual void SetSessionInfo(const SessionInfo& info) = 0;

  // Updates a user session. This is called when a user session is added or
  // its meta data (e.g. name, avatar) is changed. There is no method to remove
  // a user session because ash/chrome does not support that. All users are
  // logged out at the same time.
  virtual void UpdateUserSession(const UserSession& user_session) = 0;

  // Sets the order of user sessions. The order is keyed by the session id.
  // Currently, session manager set a LRU order with the first one being the
  // active user session.
  virtual void SetUserSessionOrder(
      const std::vector<uint32_t>& user_session_ids) = 0;

  // Prepares ash for lock screen. Currently this ensures the current active
  // window could not be used to mimic the lock screen. Lock screen is created
  // after this call returns.
  using PrepareForLockCallback = base::OnceClosure;
  virtual void PrepareForLock(PrepareForLockCallback callback) = 0;

  // Runs the pre-lock animation to start locking ash. When the call returns,
  // |locked| == true means that the ash post-lock animation is finished and ash
  // is fully locked. Otherwise |locked| is false, which means something is
  // wrong for the lock and ash is not locked. When the call returns with a true
  // |locked|, screen locker runs the post lock jobs such as a11y announcement
  // etc. Invoked by the screen locker during initialization.
  using StartLockCallback = base::OnceCallback<void(bool locked)>;
  virtual void StartLock(StartLockCallback callback) = 0;

  // Notifies ash that chrome lock animations are finished. This is the last
  // event for locking. SessionController forwards it to PowerEventObserver.
  virtual void NotifyChromeLockAnimationsComplete() = 0;

  // Runs the pre-unlock animation. Invoked by the screen locker before
  // dismissing. When the mojo call returns, screen locker takes that as a
  // signal of finished unlock animation and dismisses itself.
  // The boolean parameter will be `true` if the unlock animation was aborted,
  // resulting in the lock screen being reshown. It will be false otherwise and
  // the unlock will proceed as normal.
  using RunUnlockAnimationCallback = base::OnceCallback<void(bool)>;
  virtual void RunUnlockAnimation(RunUnlockAnimationCallback callback) = 0;

  // Notifies that chrome is terminating.
  virtual void NotifyChromeTerminating() = 0;

  // Adds a countdown timer to the system tray menu and creates or updates a
  // notification saying the session length is limited (e.g. a public session in
  // a library). Setting |length_limit| to zero removes the notification.
  virtual void SetSessionLengthLimit(base::TimeDelta length_limit,
                                     base::Time start_time) = 0;

  // Returns whether it's ok to switch the active multiprofile user. May affect
  // or be affected by system state such as window overview mode and screen
  // casting.
  using CanSwitchActiveUserCallback = base::OnceCallback<void(bool can_switch)>;
  virtual void CanSwitchActiveUser(CanSwitchActiveUserCallback callback) = 0;

  // Shows a dialog to explain the implications of signing in multiple users.
  // If |on_accept| is false, |permanently_accept| is ignored.
  using ShowMultiprofilesIntroDialogCallback =
      base::OnceCallback<void(bool on_accept, bool permanently_accept)>;
  virtual void ShowMultiprofilesIntroDialog(
      ShowMultiprofilesIntroDialogCallback callback) = 0;

  // Shows a dialog to confirm that the user wants to teleport a window to
  // another desktop. If |on_accept| is false, |permanently_accept| is ignored.
  using ShowTeleportWarningDialogCallback =
      base::OnceCallback<void(bool on_accept, bool permanently_accept)>;
  virtual void ShowTeleportWarningDialog(
      ShowTeleportWarningDialogCallback callback) = 0;

  // Shows a dialog that explains that the given user is no longer allowed in
  // the session due to a policy change, and aborts the session.
  virtual void ShowMultiprofilesSessionAbortedDialog(
      const std::string& user_email) = 0;

  // Adds/removes session activation observer. The observer is called when
  // session with |account_id| is becoming active or inactive. The observer is
  // immediately called for upon registration with the current status.
  virtual void AddSessionActivationObserverForAccountId(
      const AccountId& account_id,
      SessionActivationObserver* observer) = 0;
  virtual void RemoveSessionActivationObserverForAccountId(
      const AccountId& account_id,
      SessionActivationObserver* observer) = 0;

  // Adds/remove session observer.
  virtual void AddObserver(SessionObserver* observer) = 0;
  virtual void RemoveObserver(SessionObserver* observer) = 0;

  // Returns true if the screen is currently locked.
  virtual bool IsScreenLocked() const = 0;

  // Return the number of users that have previously logged in on the device.
  // Returns nullopt in the event where we cannot query the number of existing
  // users, for instance, when `UserManager` is uninitialized.
  virtual std::optional<int> GetExistingUsersCount() const = 0;

  // Notifies the first user session has finished post login works.
  virtual void NotifyFirstSessionReady() = 0;

  // Notifies the user specified by `account_id` is going to be removed soon.
  virtual void NotifyUserToBeRemoved(const AccountId& account_id) = 0;

 protected:
  SessionController();
  virtual ~SessionController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SESSION_SESSION_CONTROLLER_H_
