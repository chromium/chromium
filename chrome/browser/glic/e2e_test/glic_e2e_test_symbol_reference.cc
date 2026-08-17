// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/e2e_test/glic_e2e_test.h"

namespace glic::test {

namespace {

// Ensure symbols needed by internal e2e tests remain available through
// glic_e2e_test.h.
[[maybe_unused]] void EnsureInternalSymbolsReferenced() {
  // Element identifiers
  (void)kGlicContentsElementId;
  (void)kGlicHostElementId;
  (void)kGlicHandoffButtonElementId;
  (void)kGlicActorTaskState;

  // Getter functions
  (void)GetGlicButtonElementId();
  (void)GetTabStripElementId();
  (void)GetOmniboxElementId();
  (void)GetGlicViewElementId();

  // Features and switches
  (void)&GetGlicLiveModeFeature();
  (void)&GetGlicMultiInstanceFeature();
  (void)GetDisableActorSafetyChecksSwitch();

  // Enums and Prefs
  (void)GlicE2ETest::GlicE2ETestMode::kRealBackend;
  (void)GlicE2ETest::GlicE2ETestMode::kRecord;
  (void)GlicE2ETest::GlicE2ETestMode::kReplay;
  (void)GlicPanelState::kClosed;
  (void)GlicPanelState::kOpen;
  (void)glic::prefs::kGlicTabContextEnabled;
  (void)glic::prefs::kGlicDefaultTabContextEnabled;
  (void)internal::kGlicInstanceCoordinatorState;
  (void)internal::kGlicAppState;

  // Test Utilities from glic_test_util.h
  (void)&glic::GetOnlyGlicInstance;
  (void)&glic::GetInstanceForTab;
  (void)&glic::GetInstanceById;
  (void)&glic::ForceSigninAndGlicCapability;
  (void)&glic::SigninWithPrimaryAccount;
  (void)static_cast<void (*)(Profile*, bool)>(&glic::SetGlicCapability);
  (void)&glic::InvalidateAccount;
  (void)&glic::ReauthAccount;
  (void)&glic::IsSidePanelEnabled;
  (void)&glic::CreateBrowserWindow;
}

}  // namespace

}  // namespace glic::test
