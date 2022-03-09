// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/calendar/calendar_controller.h"

#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"

namespace ash {

CalendarController::CalendarController() {
  SessionController::Get()->AddObserver(this);
}

CalendarController::~CalendarController() {
  SessionController::Get()->RemoveObserver(this);
}

void CalendarController::RegisterClientForUser(const AccountId& account_id,
                                               CalendarClient* client) {
  clients_by_account_id_[account_id] = client;
}

CalendarClient* CalendarController::GetClient() {
  auto client_by_id = clients_by_account_id_.find(active_user_account_id_);
  if (client_by_id == clients_by_account_id_.end())
    return nullptr;
  return client_by_id->second;
}

void CalendarController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  active_user_account_id_ = account_id;
}

void CalendarController::SetActiveUserAccountIdForTesting(
    const AccountId& account_id) {
  active_user_account_id_ = account_id;
}

}  // namespace ash
