// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_wrong_hwid_screen.h"

namespace chromeos {

MockWrongHWIDScreen::MockWrongHWIDScreen(
    WrongHWIDScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : WrongHWIDScreen(view, exit_callback) {}

MockWrongHWIDScreen::~MockWrongHWIDScreen() {}

MockWrongHWIDScreenView::MockWrongHWIDScreenView() = default;

MockWrongHWIDScreenView::~MockWrongHWIDScreenView() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void MockWrongHWIDScreenView::SetDelegate(WrongHWIDScreen* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

}  // namespace chromeos
