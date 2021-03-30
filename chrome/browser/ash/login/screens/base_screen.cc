// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/base_screen.h"

#include "base/logging.h"

namespace chromeos {

constexpr char BaseScreen::kNotApplicable[];

BaseScreen::BaseScreen(OobeScreenId screen_id,
                       OobeScreenPriority screen_priority)
    : screen_id_(screen_id), screen_priority_(screen_priority) {}

BaseScreen::~BaseScreen() {}

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

void BaseScreen::HandleUserAction(const std::string& action_id) {
  if (is_hidden_) {
    LOG(WARNING) << "User action came when screen is hidden: action_id="
                 << action_id;
    return;
  }
  OnUserAction(action_id);
}

bool BaseScreen::HandleAccelerator(ash::LoginAcceleratorAction action) {
  return false;
}

void BaseScreen::OnUserAction(const std::string& action_id) {
  LOG(WARNING) << "Unhandled user action: action_id=" << action_id;
}

}  // namespace chromeos
