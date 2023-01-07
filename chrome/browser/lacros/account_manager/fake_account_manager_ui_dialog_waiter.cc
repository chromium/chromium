// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/fake_account_manager_ui_dialog_waiter.h"

FakeAccountManagerUIDialogWaiter::FakeAccountManagerUIDialogWaiter(
    FakeAccountManagerUI* account_manager_ui,
    Event event)
    : event_(event) {
  scoped_observation_.Observe(account_manager_ui);
}

FakeAccountManagerUIDialogWaiter::~FakeAccountManagerUIDialogWaiter() = default;

void FakeAccountManagerUIDialogWaiter::Wait() {
  run_loop_.Run();
}

void FakeAccountManagerUIDialogWaiter::OnEventReceived(Event event) {
  if (event != event_)
    return;
  run_loop_.Quit();
}

void FakeAccountManagerUIDialogWaiter::OnAddAccountDialogShown() {
  OnEventReceived(Event::kAddAccount);
}

void FakeAccountManagerUIDialogWaiter::OnReauthAccountDialogShown() {
  OnEventReceived(Event::kReauth);
}

void FakeAccountManagerUIDialogWaiter::OnManageAccountsSettingsShown() {
  OnEventReceived(Event::kSettings);
}
