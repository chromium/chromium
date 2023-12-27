// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DETACHABLE_BASE_DETACHABLE_BASE_NOTIFICATION_CONTROLLER_H_
#define ASH_DETACHABLE_BASE_DETACHABLE_BASE_NOTIFICATION_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/detachable_base/detachable_base_handler.h"
#include "ash/detachable_base/detachable_base_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace ash {

// Observes DetachableBaseHandler to detect changes to detachable base state,
// and shows relevant notifications as needed:
//   * when the attached base is different from the last one used by the active
//     user, it shows a notification warning the user the base has changed, and
//     that the newly attached base might be malicious (untrusted base might be
//     tracking the user's key strokes).
//   * when the attached base could not be authenticated, it warns the user that
//     the base may not be trusted.
//   * when the detachable base's firmware has to be updated, it shows a
//     notification asking user to reboot in order to update the base. This
//     notification will be shown regardless of the user session state - it will
//     show up on both the lock and the login screen.
class ASH_EXPORT DetachableBaseNotificationController
    : public DetachableBaseObserver,
      public SessionObserver {
 public:
  static const char kBaseChangedNotificationId[];
  static const char kBaseRequiresUpdateNotificationId[];

  explicit DetachableBaseNotificationController(
      DetachableBaseHandler* detachable_base_handler);

  DetachableBaseNotificationController(
      const DetachableBaseNotificationController&) = delete;
  DetachableBaseNotificationController& operator=(
      const DetachableBaseNotificationController&) = delete;

  ~DetachableBaseNotificationController() override;

  // DetachableBaseObserver:
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override;
  void OnDetachableBaseRequiresUpdateChanged(bool requires_update) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  // Called when the session state or detachable base pairing state change.
  // Determines whether the current state requires showing a notification to the
  // user, and show kBaseChangedNotificationId if that is the case.
  void ShowPairingNotificationIfNeeded();

  // Removes kBaseChangedNotificationId notification if it was previously shown
  // within the current session.
  void RemovePairingNotification();

  // Removes kBaseRequiresUpdateNotificationId if it was previously shown.
  void RemoveUpdateRequiredNotification();

  raw_ptr<DetachableBaseHandler> detachable_base_handler_;

  base::ScopedObservation<DetachableBaseHandler, DetachableBaseObserver>
      detachable_base_observation_{this};
  ScopedSessionObserver session_observer_{this};
};

}  // namespace ash

#endif  // ASH_DETACHABLE_BASE_DETACHABLE_BASE_NOTIFICATION_CONTROLLER_H_
