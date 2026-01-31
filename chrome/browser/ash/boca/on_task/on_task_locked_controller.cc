// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_locked_controller.h"

#include "base/callback_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace ash::boca {

DEFINE_USER_DATA(OnTaskLockedController);

OnTaskLockedController::OnTaskLockedController(
    BrowserWindowInterface* browser_window_interface)
    : scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {}

OnTaskLockedController::~OnTaskLockedController() = default;

// static
OnTaskLockedController* OnTaskLockedController::From(
    BrowserWindowInterface* browser_window_interface) {
  return Get(browser_window_interface->GetUnownedUserDataHost());
}

base::CallbackListSubscription
OnTaskLockedController::AddLockedForOnTaskUpdatedCallback(
    LockedForOnTaskUpdatedCallbackList::CallbackType callback) {
  return locked_for_on_task_updated_callbacks_.Add(callback);
}

bool OnTaskLockedController::is_locked_for_on_task() const {
  return is_locked_for_on_task_;
}

void OnTaskLockedController::set_locked_for_on_task(bool locked) {
  is_locked_for_on_task_ = locked;
  locked_for_on_task_updated_callbacks_.Notify(locked);
}

}  // namespace ash::boca
