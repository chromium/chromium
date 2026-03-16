// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"

#include "chrome/browser/glic/host/context/glic_focused_browser_manager_impl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {

NonInteractiveGlicTest::NonInteractiveGlicTest() {
#if BUILDFLAG(IS_CHROMEOS)
  features_.InitAndEnableFeature(chromeos::features::kFeatureManagementGlic);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

NonInteractiveGlicTest::NonInteractiveGlicTest(
    const base::FieldTrialParams& glic_params,
    const GlicTestEnvironmentConfig& glic_config)
    : test::InteractiveGlicTestMixin<InteractiveBrowserTest>(glic_params,
                                                             glic_config) {
}

NonInteractiveGlicTest::~NonInteractiveGlicTest() = default;

void NonInteractiveGlicTest::SetUpOnMainThread() {
  test::InteractiveGlicTestMixin<InteractiveBrowserTest>::SetUpOnMainThread();

  // Initialize testing mode after browser startup is complete.
  GlicFocusedBrowserManagerImpl::SetTestingModeForTesting(true);

#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  activation_controller_ =
      std::make_unique<views::test::MockActivationController>();
#endif
}

void NonInteractiveGlicTest::TearDownOnMainThread() {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  activation_controller_.reset();
#endif
  test::InteractiveGlicTestMixin<
      InteractiveBrowserTest>::TearDownOnMainThread();
}

}  // namespace glic
