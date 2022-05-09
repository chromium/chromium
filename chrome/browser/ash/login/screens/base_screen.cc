// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/base_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"

namespace ash {

constexpr char BaseScreen::kNotApplicable[];

BaseScreen::BaseScreen(OobeScreenId screen_id,
                       OobeScreenPriority screen_priority)
    : screen_id_(screen_id), screen_priority_(screen_priority) {}

BaseScreen::~BaseScreen() = default;

void BaseScreen::Show(WizardContext* context) {
  wizard_context_ = context;
  ShowImpl();
  is_hidden_ = false;
}

void BaseScreen::Hide() {
  HideImpl();
  is_hidden_ = true;
  wizard_context_ = nullptr;
}

bool BaseScreen::MaybeSkip(WizardContext* context) {
  return false;
}

void BaseScreen::HandleUserActionDeprecated(const std::string& action) {
  base::Value::List args;
  args.Append(action);
  HandleUserAction(args);
}

void BaseScreen::HandleUserAction(const base::Value::List& args) {
  CHECK(!args.empty());
  if (is_hidden_) {
    LOG(WARNING) << "User action came when screen is hidden: action_id="
                 << args[0].GetString();
    const bool debugger_enabled =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowOobeDevOverlay);

    // When debugger is enabled actions might come while screen is considered
    // hidden. In that case let the action pass through.
    if (!debugger_enabled)
      return;
  }
  OnUserAction(args);
}

bool BaseScreen::HandleAccelerator(LoginAcceleratorAction action) {
  return false;
}

void BaseScreen::OnUserActionDeprecated(const std::string& action_id) {
  NOTREACHED() << "Unhandled user action: action_id=" << action_id;
}

void BaseScreen::OnUserAction(const base::Value::List& args) {
  CHECK_EQ(args.size(), 1);
  OnUserActionDeprecated(args[0].GetString());
}

}  // namespace ash
