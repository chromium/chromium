// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_CONTROLLER_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_CONTROLLER_H_

#include "base/callback_list.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ash::boca {
class OnTaskLockedController {
 public:
  DECLARE_USER_DATA(OnTaskLockedController);

  static OnTaskLockedController* From(
      BrowserWindowInterface* browser_window_interface);

  using LockedForOnTaskUpdatedCallbackList =
      base::RepeatingCallbackList<void(bool)>;

  explicit OnTaskLockedController(
      BrowserWindowInterface* browser_window_interface);
  ~OnTaskLockedController();

  OnTaskLockedController(const OnTaskLockedController&) = delete;
  OnTaskLockedController& operator=(const OnTaskLockedController&) = delete;

  base::CallbackListSubscription AddLockedForOnTaskUpdatedCallback(
      LockedForOnTaskUpdatedCallbackList::CallbackType callback);

  bool is_locked_for_on_task() const;
  void set_locked_for_on_task(bool locked);

 private:
  bool is_locked_for_on_task_ = false;
  LockedForOnTaskUpdatedCallbackList locked_for_on_task_updated_callbacks_;
  ui::ScopedUnownedUserData<OnTaskLockedController> scoped_unowned_user_data_;
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_LOCKED_CONTROLLER_H_
