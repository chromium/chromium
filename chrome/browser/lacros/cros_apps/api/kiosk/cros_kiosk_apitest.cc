// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"
#include "chrome/browser/chromeos/cros_apps/api/test/cros_apps_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lacros {

class CrosKioskApiTest : public CrosAppsApiTest {
 public:
  CrosKioskApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::features::kBlinkExtensionKiosk);
  }

  void SetUpOnMainThread() override {
    CrosAppsApiTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    CrosAppsApiMutableRegistry::GetInstance(browser()->profile())
        .AddOrReplaceForTesting(std::move(
            CrosAppsApiInfo(
                blink::mojom::RuntimeFeature::kBlinkExtensionChromeOSKiosk,
                &blink::RuntimeFeatureStateContext::
                    SetBlinkExtensionChromeOSKioskEnabled)
                .AddAllowlistedOrigins({embedded_test_server()->GetOrigin()})));

    ASSERT_TRUE(
        NavigateToURL(browser()->tab_strip_model()->GetActiveWebContents(),
                      embedded_test_server()->GetURL("/empty.html")));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrosKioskApiTest, ChromeosKioskApiExists) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true,
            content::EvalJs(web_contents,
                            "typeof window.chromeos.kiosk !== 'undefined';"));
}

}  // namespace lacros
