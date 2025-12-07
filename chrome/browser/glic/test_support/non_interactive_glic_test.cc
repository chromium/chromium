// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"

#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace glic {

NonInteractiveGlicTest::NonInteractiveGlicTest() {
#if BUILDFLAG(IS_CHROMEOS)
  features_.InitAndEnableFeature(chromeos::features::kFeatureManagementGlic);
#endif  // BUILDFLAG(IS_CHROMEOS)

  GlicFocusedBrowserManager::SetTestingModeForTesting(true);
}

NonInteractiveGlicTest::NonInteractiveGlicTest(
    const base::FieldTrialParams& glic_params,
    const GlicTestEnvironmentConfig& glic_config)
    : test::InteractiveGlicTestMixin<InteractiveBrowserTest>(glic_params,
                                                             glic_config) {
  GlicFocusedBrowserManager::SetTestingModeForTesting(true);
}

NonInteractiveGlicTest::~NonInteractiveGlicTest() = default;

}  // namespace glic
