// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/ttc_interactive_browser_test_base.h"

#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/features.h"
#include "chrome/browser/ttc/ttc_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace ttc {

TtcInteractiveBrowserTestBase::TtcInteractiveBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

TtcInteractiveBrowserTestBase::~TtcInteractiveBrowserTestBase() = default;

Profile* TtcInteractiveBrowserTestBase::profile() {
  return browser()->GetProfile();
}

TtcKeyedService& TtcInteractiveBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

TtcInteractiveBrowserTestBase::StepBuilder
TtcInteractiveBrowserTestBase::CheckHasSession(bool expected_has_session) {
  return CheckResult(
      [this]() { return ttc_service().session_controller() != nullptr; },
      expected_has_session);
}

}  // namespace ttc
