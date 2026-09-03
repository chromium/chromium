// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_marketing_page_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace glic {

class GlicMarketingPageTabHelperBrowserTest : public GlicBrowserTest {
 public:
  GlicMarketingPageTabHelperBrowserTest() {
    marketing_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }
  void SetUp() override {
    ASSERT_TRUE(marketing_server_.InitializeAndListen());
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicMarketingAutoOpen,
        {{"allowlisted_urls",
          marketing_server_.GetURL("/title1.html").spec()}});
    GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    marketing_server_.StartAcceptingConnections();
    GlicBrowserTest::SetUpOnMainThread();
  }

 protected:
  net::test_server::EmbeddedTestServer marketing_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicMarketingPageTabHelperBrowserTest,
                       HandlesNavigation) {
  GURL allowed_url = marketing_server_.GetURL("/title1.html?utm_source=test");

  EXPECT_TRUE(CreateAndActivateTab(allowed_url));

  // Wait for the instance to be bound/invoked.
  GlicInstanceImpl* instance = GetInstanceImpl();
  EXPECT_TRUE(instance);
  EXPECT_TRUE(WaitForGlicClient(instance).has_value());

  // Checking that Glic UI became visible!
  EXPECT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());
}

IN_PROC_BROWSER_TEST_F(GlicMarketingPageTabHelperBrowserTest,
                       DoesNotHandleNavigationForNonMatchingUrls) {
  GURL not_allowed_url = marketing_server_.GetURL("/title2.html");
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_EQ(0, prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount));

  EXPECT_TRUE(CreateAndActivateTab(not_allowed_url));

  EXPECT_FALSE(GetInstanceImpl());

  // Ensure pref was not incremented
  EXPECT_EQ(0, prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount));
}

IN_PROC_BROWSER_TEST_F(GlicMarketingPageTabHelperBrowserTest,
                       EnforcesMaxCountLimits) {
  GURL allowed_url = marketing_server_.GetURL("/title1.html?utm_source=test");
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_EQ(0, prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount));

  // Navigate once to trigger it
  EXPECT_TRUE(CreateAndActivateTab(allowed_url));

  // Wait for the instance to be bound/invoked.
  GlicInstanceImpl* instance = GetInstanceImpl();
  EXPECT_TRUE(instance);
  EXPECT_TRUE(WaitForGlicClient(instance).has_value());
  EXPECT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());

  // Check pref was updated
  EXPECT_EQ(1, prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount));

  // Close the Glic UI to prepare for next run
  instance->CloseAllEmbedders();

  // Navigate again. By default max_impressions=1, so it should not trigger
  // again.
  EXPECT_TRUE(CreateAndActivateTab(allowed_url));

  // Pref count should remain at 1
  EXPECT_EQ(1, prefs->GetInteger(prefs::kGlicMarketingAutoOpenCount));

  // The helper should not invoke the client a second time.
  EXPECT_EQ(instance, GetInstanceImpl());
}

}  // namespace glic
