// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"

namespace glic {

NonInteractiveGlicTest::NonInteractiveGlicTest() = default;

NonInteractiveGlicTest::NonInteractiveGlicTest(
    const base::FieldTrialParams& glic_params,
    const GlicTestEnvironmentConfig& glic_config)
    : test::InteractiveGlicTestT<InteractiveBrowserTest>(glic_params,
                                                         glic_config) {}
}
