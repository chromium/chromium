// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace policy {
class ClearWindowNameForNewBrowsingContextGroupTestP
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  ClearWindowNameForNewBrowsingContextGroupTestP() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kClearCrossSiteCrossBrowsingContextGroupWindowName);
  }
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    policy::PolicyMap policies;
    policies.Set(policy::key::kClearWindowNameForNewBrowsingContextGroup,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(IsPolicyEnabled()),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  bool IsPolicyEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verify that cross-site main frame navigation that swaps BrowsingInstances
// clears window.name when the corresponding policy is enabled.
IN_PROC_BROWSER_TEST_P(ClearWindowNameForNewBrowsingContextGroupTestP,
                       ClearWindowNameCrossSite) {
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title1.html"));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  // Navigate to a.com/title1.html.
  EXPECT_TRUE(content::NavigateToURL(web_contents, url_a));

  // Set window.name.
  EXPECT_TRUE(content::ExecJs(web_contents, "window.name='foo'"));
  content::RenderFrameHost* frame_a = web_contents->GetPrimaryMainFrame();
  scoped_refptr<content::SiteInstance> site_instance_a =
      frame_a->GetSiteInstance();
  EXPECT_EQ("foo", frame_a->GetFrameName());

  // Navigate to b.com/title1.html. The navigation is cross-site, top-level and
  // swaps BrowsingInstances, thus should clear window.name.
  EXPECT_TRUE(NavigateToURL(web_contents, url_b));
  content::RenderFrameHost* frame_b = web_contents->GetPrimaryMainFrame();
  // Check that a.com/title1.html and b.com/title1.html are in different
  // BrowsingInstances.
  scoped_refptr<content::SiteInstance> site_instance_b =
      frame_b->GetSiteInstance();
  EXPECT_FALSE(site_instance_a->IsRelatedSiteInstance(site_instance_b.get()));
  // window.name should be cleared if the policy is enabled.
  if (IsPolicyEnabled()) {
    EXPECT_EQ("", frame_b->GetFrameName());
  } else {
    EXPECT_EQ("foo", frame_b->GetFrameName());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ClearWindowNameForNewBrowsingContextGroupTestP,
                         testing::Bool());

}  // namespace policy
