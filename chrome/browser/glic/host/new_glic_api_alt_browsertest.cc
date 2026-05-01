// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/glic_features.mojom-features.h"
#include "chrome/browser/glic/test_support/glic_browser_interactive_migration_test.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace glic {
namespace {

struct TestParams {
  bool enable_scroll_to_pdf = false;
  bool trust_first_onboarding_arm1 = false;
  bool trust_first_onboarding_arm2 = false;
  bool auto_open_pdf = false;
};

class WithTestParams : public testing::WithParamInterface<TestParams> {
 public:
  WithTestParams() {}

  static std::string PrintTestVariant(
      const ::testing::TestParamInfo<TestParams>& info) {
    return "Default";
  }
};

using GlicApiBrowserAltTest =
    GlicApiBrowserTestMixin<GlicBrowserInteractiveMigrationTest>;

class NewGlicApiAltTest : public GlicApiBrowserAltTest, public WithTestParams {
 public:
  NewGlicApiAltTest() : GlicApiBrowserAltTest("./new_glic_api_browsertest.js") {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlic, {}}, {mojom::features::kGlicMultiTab, {}}}, {});
  }

  void SetUpOnMainThread() override {
    GlicApiBrowserAltTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(
        GetTabListInterface()->GetActiveTab()->GetContents(),
        GetTestUrl("page.html")));
  }

 private:
  base::test::ScopedFeatureList features_;
};

// !!!!!!!!!!!!  WARNING  !!!!!!!!!!!!!
// TODO(b/508621027): DO NOT ADD MORE TESTS HERE!
// This test is here only to compare flake rates with new_glic_api_browsertest.
// !!!!!!!!!!!!  WARNING  !!!!!!!!!!!!!

IN_PROC_BROWSER_TEST_P(NewGlicApiAltTest, testDoNothing) {
  ASSERT_EQ(GetTabListInterface()->GetTabCount(), 1);
  ASSERT_EQ(GetTabListInterface()->GetTab(0)->GetContents()->GetURL(),
            GetTestUrl("page.html"));
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

// !!!!!!!!!!!!  WARNING  !!!!!!!!!!!!!
// TODO(b/508621027): DO NOT ADD MORE TESTS HERE!
// This test is here only to compare flake rates with new_glic_api_browsertest.
// !!!!!!!!!!!!  WARNING  !!!!!!!!!!!!!

auto DefaultTestParamSet() {
  return testing::Values(TestParams{});
}

INSTANTIATE_TEST_SUITE_P(,
                         NewGlicApiAltTest,
                         DefaultTestParamSet(),
                         &WithTestParams::PrintTestVariant);

}  // namespace
}  // namespace glic
