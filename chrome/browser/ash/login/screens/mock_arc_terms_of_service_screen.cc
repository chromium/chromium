// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_arc_terms_of_service_screen.h"
#include "base/memory/weak_ptr.h"

namespace ash {

MockArcTermsOfServiceScreen::MockArcTermsOfServiceScreen(
    base::WeakPtr<ArcTermsOfServiceScreenView> view,
    const ScreenExitCallback& exit_callback)
    : ArcTermsOfServiceScreen(std::move(view), exit_callback) {}

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

}  // namespace ash
