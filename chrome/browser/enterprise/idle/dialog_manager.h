// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_DIALOG_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_DIALOG_MANAGER_H_

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/idle/action.h"
#include "chrome/browser/ui/idle_dialog.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace enterprise_idle {

// A singleton that manages IdleDialog's state. Shows the dialog, manages its
// expiration, and runs a callback when it closes.
//
// The dialog is anchored to the currently-active browser window. If there is no
// active browser window (e.g. user is in another app), the timer is completely
// skipped and other actions resolve immediately.
class DialogManager {
 public:
  using FinishedCallback = base::OnceCallback<void(bool expired)>;

  static DialogManager* GetInstance();

  // Show a 30s dialog--or if it's already visible, re-use the existing one.
  // Run `on_finished` after 30s, or if the user dismisses the dialog.
  //
  // If the user is in another app, skip the dialog and call
  // `on_finished(true)`.
  base::CallbackListSubscription MaybeShowDialog(
      Profile* profile,
      base::TimeDelta threshold,
      const base::flat_set<ActionType>& action_types,
      FinishedCallback on_finished);

  void DismissDialogForTesting();
  bool IsDialogOpenForTesting() const;

 private:
  friend class base::NoDestructor<DialogManager>;

  DialogManager();
  ~DialogManager();

  // Runs after a 30s timeout.
  void OnDialogExpired();

  // Runs when the user hits Escape, or clicks the "Continue using Chrome"
  // button in the dialog.
  void OnDialogDismissedByUser();

  // Pending `on_finished` callbacks.
  base::OnceCallbackList<void(bool expired)> callbacks_;

  base::WeakPtr<views::Widget> dialog_;

  // Timer for `dialog_`. Runs OnDialogExpired().
  base::OneShotTimer dialog_timer_;
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_DIALOG_MANAGER_H_
