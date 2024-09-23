// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MODEL_UPDATE_MODEL_H_
#define ASH_SYSTEM_MODEL_UPDATE_MODEL_H_

#include "ash/public/cpp/update_types.h"
#include "base/observer_list.h"

namespace ash {

class UpdateObserver {
 public:
  virtual ~UpdateObserver() {}

  virtual void OnUpdateAvailable() = 0;
};

// Model to store system update availability.
class UpdateModel {
 public:
  UpdateModel();

  UpdateModel(const UpdateModel&) = delete;
  UpdateModel& operator=(const UpdateModel&) = delete;

  ~UpdateModel();

  void AddObserver(UpdateObserver* observer);
  void RemoveObserver(UpdateObserver* observer);

  // Stores the state that a software update is available. The state persists
  // until reboot. Based on |severity|, |factory_reset_required| and |rollback|,
  // the observer views can indicate the severity of the update to users by
  // changing the icon, color, and tooltip.
  void SetUpdateAvailable(UpdateSeverity severity,
                          bool factory_reset_required,
                          bool rollback);

  // Stores the state of the notification according to the RelaunchNotification
  // policy. State persists until reboot or another call to this function.
  void SetRelaunchNotificationState(
      const RelaunchNotificationState& relaunch_notification_state);

  // If |available| is true, a software update is available but user's agreement
  // is required as current connection is cellular. If |available| is false, the
  // user's one time permission on update over cellular connection has been
  // granted.
  void SetUpdateOverCellularAvailable(bool available);

  // If `deferred` is true, an update is downloaded but deferred.
  void SetUpdateDeferred(DeferredUpdateState state);

  // Whether a notice about the device reaching end of life should be shown.
  void SetShowEolNotice(bool show);

  // Whether a notice about extended updates support being available for
  // the device should be shown.
  void SetShowExtendedUpdatesNotice(bool show);

  UpdateSeverity GetSeverity() const;

  // Sets |update_required_| back to false.
  void ResetUpdateAvailable();

  bool update_required() const { return update_required_; }
  bool factory_reset_required() const { return factory_reset_required_; }
  bool rollback() const { return rollback_; }
  const RelaunchNotificationState& relaunch_notification_state() const {
    return relaunch_notification_state_;
  }
  bool update_over_cellular_available() const {
    return update_over_cellular_available_;
  }
  DeferredUpdateState update_deferred() const { return update_deferred_; }

  bool show_eol_notice() const { return show_eol_notice_; }
  bool show_extended_updates_notice() const {
    return show_extended_updates_notice_;
  }

 private:
  void NotifyUpdateAvailable();

  bool update_required_ = false;
  UpdateSeverity severity_ = UpdateSeverity::kNone;
  bool factory_reset_required_ = false;
  bool rollback_ = false;
  RelaunchNotificationState relaunch_notification_state_;
  bool update_over_cellular_available_ = false;
  DeferredUpdateState update_deferred_ = DeferredUpdateState::kNone;
  bool show_eol_notice_ = false;
  bool show_extended_updates_notice_ = false;

  base::ObserverList<UpdateObserver>::Unchecked observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MODEL_UPDATE_MODEL_H_
