// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_arc_terms_of_service_screen.h"

namespace chromeos {

MockArcTermsOfServiceScreen::MockArcTermsOfServiceScreen(
    ArcTermsOfServiceScreenView* view,
    const ScreenExitCallback& exit_callback)
    : ArcTermsOfServiceScreen(view, exit_callback) {}

MockArcTermsOfServiceScreen::~MockArcTermsOfServiceScreen() = default;

void MockArcTermsOfServiceScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockArcTermsOfServiceScreenView::MockArcTermsOfServiceScreenView() = default;

MockArcTermsOfServiceScreenView::~MockArcTermsOfServiceScreenView() {
  if (observer_)
    observer_->OnViewDestroyed(this);
}

void MockArcTermsOfServiceScreenView::AddObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_ = observer;
  MockAddObserver(observer);
}

void MockArcTermsOfServiceScreenView::RemoveObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  if (observer_ == observer)
    observer_ = nullptr;
  MockRemoveObserver(observer);
}

}  // namespace chromeos
