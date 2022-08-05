// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_BASE_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_BASE_SCREEN_H_

#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ash/login/wizard_context.h"
#include "components/login/base_screen_handler_utils.h"

namespace ash {

// Base class for the all OOBE/login/before-session screens.
// Screens are identified by ID, screen and it's JS counterpart must have same
// id.
// Most of the screens will be re-created for each appearance with Initialize()
// method called just once.
class BaseScreen {
 public:
  // String which represents not applicable exit code. This exit code refers to
  // skipping the screen due to specific unmet condition.
  constexpr static const char kNotApplicable[] = "NotApplicable";

  BaseScreen(OobeScreenId screen_id, OobeScreenPriority screen_priority);

  BaseScreen(const BaseScreen&) = delete;
  BaseScreen& operator=(const BaseScreen&) = delete;

  virtual ~BaseScreen();

  // Makes wizard screen visible.
  void Show(WizardContext* context);

  // Makes wizard screen invisible.
  void Hide();

  // Returns whether the screen should be skipped i. e. should be exited due to
  // specific unmet conditions. Returns true if skips the screen.
  [[nodiscard]] virtual bool MaybeSkip(WizardContext& context);

  // Forwards user action if screen is shown.
  void HandleUserAction(const base::Value::List& args);
  // DEPRECATED: Use HandleUserAction.
  void HandleUserActionDeprecated(const std::string& action);

  // Returns `true` if `action` was handled by the screen.
  virtual bool HandleAccelerator(LoginAcceleratorAction action);

  // Returns the identifier of the screen.
  OobeScreenId screen_id() const { return screen_id_; }

  // Returns the priority of the screen.
  OobeScreenPriority screen_priority() const { return screen_priority_; }

  bool is_hidden() { return is_hidden_; }

 protected:
  virtual void ShowImpl() = 0;
  virtual void HideImpl() = 0;

  // Called when user action event with happened. Notification about this event
  // comes from the JS counterpart. Not called if the screen is hidden
  virtual void OnUserAction(const base::Value::List& args);
  // DEPRECATED: Use OnUserAction.
  virtual void OnUserActionDeprecated(const std::string& action_id);

  WizardContext* context() const { return wizard_context_; }

 private:
  bool is_hidden_ = true;

  // Wizard context itself is owned by WizardController and is accessible
  // to screen only between OnShow / OnHide calls.
  WizardContext* wizard_context_ = nullptr;

  const OobeScreenId screen_id_;

  const OobeScreenPriority screen_priority_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::BaseScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::BaseScreen;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::BaseScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_BASE_SCREEN_H_
