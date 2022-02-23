// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_consolidated_consent_screen.h"

namespace ash {

MockConsolidatedConsentScreen::MockConsolidatedConsentScreen(
    ConsolidatedConsentScreenView* view,
    const ScreenExitCallback& exit_callback)
    : ConsolidatedConsentScreen(view, exit_callback) {}

MockConsolidatedConsentScreen::~MockConsolidatedConsentScreen() {}

void MockConsolidatedConsentScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockConsolidatedConsentScreenView::MockConsolidatedConsentScreenView() {}

MockConsolidatedConsentScreenView::~MockConsolidatedConsentScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockConsolidatedConsentScreenView::Bind(
    ConsolidatedConsentScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockConsolidatedConsentScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace ash
