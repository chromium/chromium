// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/dialog_manager.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "components/enterprise/idle/idle_pref_names.h"
#include "components/enterprise/idle/metrics.h"
#include "components/prefs/pref_service.h"

namespace enterprise_idle {

namespace {

constexpr base::TimeDelta kTestDialogTimeout = base::Seconds(5);
constexpr base::TimeDelta kDialogTimeout = base::Seconds(30);

IdleDialog::ActionSet GetActionSet(PrefService* prefs) {
  std::vector<ActionType> actions;
  std::ranges::transform(prefs->GetList(prefs::kIdleTimeoutActions),
                         std::back_inserter(actions),
                         [](const base::Value& action) {
                           return static_cast<ActionType>(action.GetInt());
                         });
  return ActionsToActionSet(base::flat_set<ActionType>(std::move(actions)));
}

}  // namespace

DialogManager::DialogManager() = default;
DialogManager::~DialogManager() = default;

// static
DialogManager* DialogManager::GetInstance() {
  static base::NoDestructor<DialogManager> instance;
  return instance.get();
}

base::CallbackListSubscription DialogManager::MaybeShowDialog(
    Profile* profile,
    base::TimeDelta threshold,
    const base::flat_set<ActionType>& action_types,
    FinishedCallback on_finished) {
  if (dialog_) {
    // The dialog is already visible, re-use it.
    return callbacks_.Add(std::move(on_finished));
  }

  BrowserWindowInterface* const active_bwi =
      GetLastActiveBrowserWindowInterfaceWithAnyProfile();
  if (!active_bwi || !active_bwi->IsActive() ||
      active_bwi->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    // User is in another app, or in a window that shouldn't show the dialog
    // (e.g. DevTools). Skip the dialog, and run actions immediately.
    std::move(on_finished).Run(/*expired=*/true);
    return base::CallbackListSubscription();
  }

  // Create a new dialog, modal to `bwi`.
  base::TimeDelta timeout = base::CommandLine::ForCurrentProcess()->HasSwitch(
                                switches::kSimulateIdleTimeout)
                                ? kTestDialogTimeout
                                : kDialogTimeout;
  dialog_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&DialogManager::OnDialogExpired, base::Unretained(this)));
  dialog_ = IdleDialog::Show(
      active_bwi, timeout, threshold, GetActionSet(profile->GetPrefs()),
      base::BindOnce(&DialogManager::OnDialogDismissedByUser,
                     base::Unretained(this)));
  metrics::RecordIdleTimeoutDialogEvent(
      metrics::IdleTimeoutDialogEvent::kDialogShown);
  return callbacks_.Add(std::move(on_finished));
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

  metrics::RecordIdleTimeoutDialogEvent(
      metrics::IdleTimeoutDialogEvent::kDialogDismissedByUser);
  callbacks_.Notify(/*expired=*/false);
}

void DialogManager::OnDialogExpired() {
  if (dialog_) {
    dialog_->Close();
  }
  dialog_.reset();
  dialog_timer_.Stop();

  metrics::RecordIdleTimeoutDialogEvent(
      metrics::IdleTimeoutDialogEvent::kDialogExpired);
  callbacks_.Notify(/*expired=*/true);
}

}  // namespace enterprise_idle
