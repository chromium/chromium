// Copyright 2018 The Chromium Authors. All rights reserved.
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
                                     bool rollback,
                                     UpdateType update_type) {
  update_required_ = true;
  severity_ = severity;
  factory_reset_required_ = factory_reset_required;
  rollback_ = rollback;
  update_type_ = update_type;
  NotifyUpdateAvailable();
}

void UpdateModel::SetUpdateNotificationState(
    NotificationStyle style,
    const base::string16& notification_title,
    const base::string16& notification_body) {
  DCHECK_EQ(update_type_, UpdateType::kSystem);
  notification_style_ = style;
  notification_title_ = notification_title;
  notification_body_ = notification_body;
  NotifyUpdateAvailable();
}

void UpdateModel::SetUpdateOverCellularAvailable(bool available) {
  update_over_cellular_available_ = available;
  NotifyUpdateAvailable();
}

UpdateSeverity UpdateModel::GetSeverity() const {
  // TODO(https://crbug.com/927010): adjust severity according the amount of
  // time passing after update is available over cellular connection. Use low
  // severity for update available over cellular connection.
  return update_over_cellular_available_ ? UpdateSeverity::kLow : severity_;
}

void UpdateModel::NotifyUpdateAvailable() {
  for (auto& observer : observers_)
    observer.OnUpdateAvailable();
}

}  // namespace ash
