// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "content/public/test/browser_test.h"

namespace glic {
namespace {

class GlicGeicApiBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicApiBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {
    feature_list_.InitAndEnableFeature(features::kGeic);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(geic::kGeicGuestURLSwitch,
                                    GetGuestURL().spec());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicSignInTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicAuthTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

}  // namespace
}  // namespace glic
