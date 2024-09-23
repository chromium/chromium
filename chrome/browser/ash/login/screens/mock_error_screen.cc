// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_error_screen.h"

namespace ash {

using ::testing::AtLeast;
using ::testing::_;

MockErrorScreen::MockErrorScreen(base::WeakPtr<ErrorScreenView> view)
    : ErrorScreen(std::move(view)) {}

MockErrorScreen::~MockErrorScreen() {}

void MockErrorScreen::FixCaptivePortal() {
  ErrorScreen::FixCaptivePortal();
  MockFixCaptivePortal();
}

void MockErrorScreen::SetUIState(NetworkError::UIState ui_state) {
  ErrorScreen::SetUIState(ui_state);
  MockSetUIState(ui_state);
}

void MockErrorScreen::SetErrorState(NetworkError::ErrorState error_state,
                                    const std::string& network) {
  ErrorScreen::SetErrorState(error_state, network);
  MockSetErrorState(error_state, network);
}

MockErrorScreenView::MockErrorScreenView() = default;

MockErrorScreenView::~MockErrorScreenView() = default;

base::WeakPtr<ErrorScreenView> MockErrorScreenView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
