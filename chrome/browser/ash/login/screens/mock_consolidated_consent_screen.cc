// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/mock_consolidated_consent_screen.h"

#include "base/check.h"
#include "base/memory/weak_ptr.h"

namespace ash {

MockConsolidatedConsentScreen::MockConsolidatedConsentScreen(
    const ApplicationLocaleStorage* application_locale_storage,
    ::metrics::MetricsService* metrics_service,
    base::WeakPtr<ConsolidatedConsentScreenView> view,
    const ScreenExitCallback& exit_callback)
    : ConsolidatedConsentScreen(application_locale_storage,
                                metrics_service,
                                std::move(view),
                                exit_callback) {
  CHECK(metrics_service);
}

MockConsolidatedConsentScreen::~MockConsolidatedConsentScreen() = default;

void MockConsolidatedConsentScreen::ExitScreen(Result result) {
  exit_callback()->Run(result);
}

MockConsolidatedConsentScreenView::MockConsolidatedConsentScreenView() =
    default;

MockConsolidatedConsentScreenView::~MockConsolidatedConsentScreenView() =
    default;

}  // namespace ash
