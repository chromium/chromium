// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/dialog_manager.h"

#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace enterprise_idle {

namespace {

constexpr base::TimeDelta kDialogTimeout = base::Seconds(30);

IdleDialog::ActionSet ActionsToActionSet(
    const base::flat_set<ActionType>& action_types) {
  IdleDialog::ActionSet action_set = {.close = false, .clear = false};
  for (ActionType action_type : action_types) {
    switch (action_type) {
      case ActionType::kCloseBrowsers:
        action_set.close = true;
        break;

      case ActionType::kShowProfilePicker:
        break;

      case ActionType::kClearBrowsingHistory:
      case ActionType::kClearDownloadHistory:
      case ActionType::kClearCookiesAndOtherSiteData:
      case ActionType::kClearCachedImagesAndFiles:
      case ActionType::kClearPasswordSignin:
      case ActionType::kClearAutofill:
      case ActionType::kClearSiteSettings:
      case ActionType::kClearHostedAppData:
      case ActionType::kReloadPages:
        action_set.clear = true;
        break;
    }
  }
  return action_set;
}

}  // namespace

DialogManager::DialogManager() = default;
DialogManager::~DialogManager() = default;

// static
DialogManager* DialogManager::GetInstance() {
  static base::NoDestructor<DialogManager> instance;
  return instance.get();
}

base::CallbackListSubscription DialogManager::ShowDialog(
    base::TimeDelta threshold,
    const base::flat_set<ActionType>& action_types,
    FinishedCallback on_finished) {
  // Passed the guards: we're really going to show the dialog and close
  // browsers.
  base::CallbackListSubscription subscription =
      callbacks_.Add(std::move(on_finished));

  if (dialog_) {
    // The dialog is already visible, re-use it.
    return subscription;
  }

  dialog_ = IdleDialog::Show(
      kDialogTimeout, threshold, ActionsToActionSet(action_types),
      base::BindOnce(&DialogManager::OnDialogDismissedByUser,
                     base::Unretained(this)));
  dialog_timer_.Start(
      FROM_HERE, kDialogTimeout,
      base::BindOnce(&DialogManager::OnDialogExpired, base::Unretained(this)));
  return subscription;
}

void DialogManager::DismissDialogForTesting() {
  CHECK_IS_TEST();
  OnDialogDismissedByUser();
}

bool DialogManager::IsDialogOpenForTesting() const {
  return bool(dialog_);
}

void DialogManager::OnDialogDismissedByUser() {
  if (dialog_) {
    dialog_->Close();
  }
  dialog_.reset();
  dialog_timer_.Stop();

  callbacks_.Notify(/*expired=*/false);
}

void DialogManager::OnDialogExpired() {
  if (dialog_) {
    dialog_->Close();
  }
  dialog_.reset();
  dialog_timer_.Stop();

  callbacks_.Notify(/*expired=*/true);
}

}  // namespace enterprise_idle
