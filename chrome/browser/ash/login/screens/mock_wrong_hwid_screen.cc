// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_wrong_hwid_screen.h"

namespace ash {

MockWrongHWIDScreen::MockWrongHWIDScreen(
    base::WeakPtr<WrongHWIDScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : WrongHWIDScreen(std::move(view), exit_callback) {}

MockWrongHWIDScreen::~MockWrongHWIDScreen() = default;

void MockWrongHWIDScreen::ExitScreen() {
  WrongHWIDScreen::OnExit();
}

MockWrongHWIDScreenView::MockWrongHWIDScreenView() = default;

MockWrongHWIDScreenView::~MockWrongHWIDScreenView() = default;

base::WeakPtr<WrongHWIDScreenView> MockWrongHWIDScreenView::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
