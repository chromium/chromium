// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_eula_screen.h"

namespace ash {

using ::testing::AtLeast;
using ::testing::_;

MockEulaScreen::MockEulaScreen(base::WeakPtr<EulaView> view,
                               const ScreenExitCallback& exit_callback)
    : EulaScreen(std::move(view), exit_callback) {}

MockEulaScreen::~MockEulaScreen() {}

void MockEulaScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockEulaView::MockEulaView() = default;

MockEulaView::~MockEulaView() = default;

}  // namespace ash
