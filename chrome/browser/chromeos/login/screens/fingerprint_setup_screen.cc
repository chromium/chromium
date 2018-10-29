// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/fingerprint_setup_screen.h"

namespace chromeos {
namespace {

constexpr char kUserActionClose[] = "fingerprint-setup-done";

}  // namespace

FingerprintSetupScreen::FingerprintSetupScreen(
    BaseScreenDelegate* base_screen_delegate,
    FingerprintSetupScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_FINGERPRINT_SETUP),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

FingerprintSetupScreen::~FingerprintSetupScreen() {
  view_->Bind(nullptr);
}

void FingerprintSetupScreen::Show() {
  if (IsPublicSessionOrEphemeralLogin()) {
    Finish(ScreenExitCode::FINGERPRINT_SETUP_FINISHED);
    return;
  }
  view_->Show();
}

void FingerprintSetupScreen::Hide() {
  view_->Hide();
}

void FingerprintSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionClose) {
    Finish(ScreenExitCode::FINGERPRINT_SETUP_FINISHED);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
