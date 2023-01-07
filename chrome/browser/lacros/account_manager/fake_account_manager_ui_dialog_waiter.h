// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_UI_DIALOG_WAITER_H_
#define CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_UI_DIALOG_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/account_manager_core/chromeos/fake_account_manager_ui.h"

// Test helper class to wait until the `FakeAccountManagerUI` is requested to
// show a dialog.
class FakeAccountManagerUIDialogWaiter : public FakeAccountManagerUI::Observer {
 public:
  // The dialog event waited by this class. Each event corresponds to one of the
  // `FakeAccountManagerUI::Observer` methods.
  enum class Event { kAddAccount, kReauth, kSettings };

  FakeAccountManagerUIDialogWaiter(FakeAccountManagerUI* account_manager_ui,
                                   Event event);
  ~FakeAccountManagerUIDialogWaiter() override;

  // Waits until the event specified in the constructor is triggered. This stops
  // at the first event, and the `FakeAccountManagerUIDialogWaiter` cannot be
  // reused.
  void Wait();

  // FakeAccountManagerUI::Observer:
  void OnAddAccountDialogShown() override;
  void OnReauthAccountDialogShown() override;
  void OnManageAccountsSettingsShown() override;

 private:
  void OnEventReceived(Event event);

  base::RunLoop run_loop_;
  Event event_;
  base::ScopedObservation<FakeAccountManagerUI, FakeAccountManagerUI::Observer>
      scoped_observation_{this};
};

#endif  // CHROME_BROWSER_LACROS_ACCOUNT_MANAGER_FAKE_ACCOUNT_MANAGER_UI_DIALOG_WAITER_H_
