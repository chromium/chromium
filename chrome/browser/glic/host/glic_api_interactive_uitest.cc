// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/interactive_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file runs the respective JS tests from
// chrome/test/data/webui/glic/browser_tests/glic_api_interactive_uitest.ts.

// Note: Prefer to use a browsertest, like
// chrome/browser/glic/host/glic_api_browsertest.cc, unless there's a reason an
// interactive UI test is helpful. Browsertests can be run more quickly.

namespace glic {
namespace {

class GlicApiUiTest : public InteractiveGlicApiTest {
 public:
  GlicApiUiTest() : InteractiveGlicApiTest("./glic_api_interactive_uitest.js") {
    features_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {},
        /*disabled_features=*/
        {
            features::kGlicWarming,
        });
  }

 protected:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicApiUiTest, testAllTestsAreRegistered) {
  AssertAllTestsRegistered({
      "GlicApiUiTest",
  });
}

IN_PROC_BROWSER_TEST_F(GlicApiUiTest, testOpenGlic) {
  RunTestSequence(
      OpenGlicWindow(GlicWindowMode::kDetached, GlicInstrumentMode::kNone));
  ExecuteJsTest();
}

}  // namespace
}  // namespace glic
