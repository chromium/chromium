// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/discover_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/discover_screen_view.h"

namespace chromeos {

namespace {
const char kFinished[] = "finished";
}

DiscoverScreen::DiscoverScreen(BaseScreenDelegate* base_screen_delegate,
                               DiscoverScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_DISCOVER),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

DiscoverScreen::~DiscoverScreen() {
  view_->Bind(nullptr);
}

void DiscoverScreen::Show() {
  if (IsPublicSessionOrEphemeralLogin()) {
    Finish(ScreenExitCode::DISCOVER_FINISHED);
    return;
  }
  view_->Show();
  is_shown_ = true;
}

void DiscoverScreen::Hide() {
  view_->Hide();
  is_shown_ = false;
}

void DiscoverScreen::OnUserAction(const std::string& action_id) {
  // Only honor finish if discover is currently being shown.
  if (action_id == kFinished && is_shown_) {
    Finish(ScreenExitCode::DISCOVER_FINISHED);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
