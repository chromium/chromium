// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/update_model.h"

namespace ash {

UpdateModel::UpdateModel() = default;
UpdateModel::~UpdateModel() = default;

void UpdateModel::AddObserver(UpdateObserver* observer) {
  observers_.AddObserver(observer);
}

void UpdateModel::RemoveObserver(UpdateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void UpdateModel::SetUpdateAvailable(UpdateSeverity severity,
                                     bool factory_reset_required,
                                     bool rollback) {
  update_required_ = true;
  update_deferred_ = DeferredUpdateState::kNone;
  severity_ = severity;
  factory_reset_required_ = factory_reset_required;
  rollback_ = rollback;
  NotifyUpdateAvailable();
}

void UpdateModel::SetRelaunchNotificationState(
    const RelaunchNotificationState& relaunch_notification_state) {
  relaunch_notification_state_ = relaunch_notification_state;
  NotifyUpdateAvailable();
}

void UpdateModel::SetUpdateOverCellularAvailable(bool available) {
  update_over_cellular_available_ = available;
  NotifyUpdateAvailable();
}

void UpdateModel::SetUpdateDeferred(DeferredUpdateState state) {
  update_deferred_ = state;
  NotifyUpdateAvailable();
}

void UpdateModel::SetShowEolNotice(bool show) {
  show_eol_notice_ = show;
}

void UpdateModel::SetShowExtendedUpdatesNotice(bool show) {
  show_extended_updates_notice_ = show;
}

UpdateSeverity UpdateModel::GetSeverity() const {
  // TODO(crbug.com/41438408): adjust severity according the amount of
  // time passing after update is available over cellular connection. Use low
  // severity for update available over cellular connection.
  return update_over_cellular_available_ ? UpdateSeverity::kLow : severity_;
}

void UpdateModel::ResetUpdateAvailable() {
  update_required_ = false;
  update_deferred_ = DeferredUpdateState::kNone;
  NotifyUpdateAvailable();
}

void UpdateModel::NotifyUpdateAvailable() {
  for (auto& observer : observers_)
    observer.OnUpdateAvailable();
}

}  // namespace ash
