// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_actor_tracker.h"

#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace enterprise_connectors {

class ExtensionActorTrackerBrowserTest : public InProcessBrowserTest {
 public:
  ExtensionActorTrackerBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kEnterpriseExtensionActorAttribution);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("dm_token"));
    test::SetOnSecurityEventReporting(browser()->GetProfile()->GetPrefs(),
                                      true);
  }

 protected:
  // Records the extension actor when a navigation starts.
  class ActorAtNavigationStartObserver : public content::WebContentsObserver {
   public:
    explicit ActorAtNavigationStartObserver(content::WebContents* web_contents)
        : content::WebContentsObserver(web_contents) {}

    void DidStartNavigation(content::NavigationHandle* handle) override {
      actor_ = ExtensionActorTracker::GetActiveActor(web_contents());
    }

    const std::optional<std::string>& actor() const { return actor_; }

   private:
    std::optional<std::string> actor_;
  };

  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The URL lookup reads the actor when the navigation starts, so tabs.update
// should set it before navigating.
IN_PROC_BROWSER_TEST_F(ExtensionActorTrackerBrowserTest,
                       TabsUpdateSetsActorBeforeNavigation) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  ActorAtNavigationStartObserver observer(web_contents());

  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("Test").Build();
  auto function = base::MakeRefCounted<extensions::TabsUpdateFunction>();
  function->set_extension(extension.get());
  ASSERT_TRUE(extensions::api_test_utils::RunFunction(
      function.get(),
      base::StringPrintf(
          R"([%d, {"url": "%s"}])",
          extensions::ExtensionTabUtil::GetTabId(web_contents()),
          embedded_test_server()->GetURL("/title2.html").spec().c_str()),
      browser()->GetProfile(),
      extensions::api_test_utils::FunctionMode::kNone));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));

  EXPECT_EQ(observer.actor(), extension->id());
}

}  // namespace enterprise_connectors
