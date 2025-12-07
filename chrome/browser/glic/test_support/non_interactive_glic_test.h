// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_NON_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_NON_INTERACTIVE_GLIC_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/interactive_glic_test.h"

namespace glic {

// Like InteractiveGlicTest, but expected to be used in a non-interactive
// browser test. Non-interactive browser tests can be run in parallel, and so
// are more efficient to run, but can be flaky if tests are sensitive to focus
// changes. This fixture artificially sets a single browser as focused, so that
// Glic will consider a tab in the browser as focused.
class NonInteractiveGlicTest
    : public test::InteractiveGlicTestMixin<InteractiveBrowserTest> {
 public:
  NonInteractiveGlicTest();
  NonInteractiveGlicTest(const base::FieldTrialParams& glic_params,
                         const GlicTestEnvironmentConfig& glic_config);
  ~NonInteractiveGlicTest() override;

  // Returns this fixture's `BrowserActivator` instance so that tests can
  // customize how browser windows should be activated, if needed.
  BrowserActivator& browser_activator() { return browser_activator_; }

 private:
  base::test::ScopedFeatureList features_;
  BrowserActivator browser_activator_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_NON_INTERACTIVE_GLIC_TEST_H_
