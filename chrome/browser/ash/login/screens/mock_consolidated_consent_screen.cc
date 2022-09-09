// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_consolidated_consent_screen.h"

#include "base/memory/weak_ptr.h"

namespace ash {

MockConsolidatedConsentScreen::MockConsolidatedConsentScreen(
    base::WeakPtr<ConsolidatedConsentScreenView> view,
    const ScreenExitCallback& exit_callback)
    : ConsolidatedConsentScreen(std::move(view), exit_callback) {}

MockConsolidatedConsentScreen::~MockConsolidatedConsentScreen() = default;

void MockConsolidatedConsentScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockConsolidatedConsentScreenView::MockConsolidatedConsentScreenView() =
    default;

MockConsolidatedConsentScreenView::~MockConsolidatedConsentScreenView() =
    default;

}  // namespace ash
