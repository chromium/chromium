// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/gemini_enterprise/gemini_enterprise.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace glic {
namespace {

class GlicGeicApiBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicApiBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kGlicGeminiEnterpriseSettingsEnabled);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kGlicGeminiEnterpriseSettingsOverride,
        "{\"project_id\": \"test-project\", \"app_id\": \"test-app\", "
        "\"location\": \"test-location\"}");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicSignInTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicGeicConsumerBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicConsumerBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {}
};

IN_PROC_BROWSER_TEST_F(GlicGeicConsumerBrowserTest,
                       NonEnterpriseInstanceRefusesGeminiEnterpriseHandler) {
  ASSERT_OK(OpenGlicForActiveTab());
  GlicInstanceImpl* instance = GetOnlyGlicInstance();
  ASSERT_TRUE(instance);

  mojo::Remote<mojom::GeminiEnterpriseHandler> remote;
  mojo::PendingReceiver<mojom::GeminiEnterpriseHandler> receiver =
      remote.BindNewPipeAndPassReceiver();
  base::RunLoop run_loop;
  remote.set_disconnect_handler(run_loop.QuitClosure());

  instance->CreateGeminiEnterpriseHandler(std::move(receiver));

  run_loop.Run();
  EXPECT_FALSE(remote.is_connected());
}

}  // namespace
}  // namespace glic
