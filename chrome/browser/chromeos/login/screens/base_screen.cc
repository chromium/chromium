// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/base_screen.h"

#include "base/logging.h"

namespace chromeos {

BaseScreen::BaseScreen(OobeScreenId screen_id) : screen_id_(screen_id) {}

BaseScreen::~BaseScreen() {}

void BaseScreen::OnUserAction(const std::string& action_id) {
  LOG(WARNING) << "Unhandled user action: action_id=" << action_id;
}

void BaseScreen::SetConfiguration(base::Value* configuration) {
  configuration_ = configuration;
}

}  // namespace chromeos
