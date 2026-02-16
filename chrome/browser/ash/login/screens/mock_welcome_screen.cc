// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_welcome_screen.h"
#include "base/memory/weak_ptr.h"

namespace ash {

MockWelcomeScreen::MockWelcomeScreen(
    PrefService* local_state,
    ApplicationLocaleStorage* application_locale_storage,
    base::WeakPtr<WelcomeView> view,
    const WelcomeScreen::ScreenExitCallback& exit_callback)
    : WelcomeScreen(local_state,
                    application_locale_storage,
                    std::move(view),
                    exit_callback) {}

void MockWelcomeScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockWelcomeScreen::~MockWelcomeScreen() = default;

MockWelcomeView::MockWelcomeView() = default;

MockWelcomeView::~MockWelcomeView() = default;

}  // namespace ash
