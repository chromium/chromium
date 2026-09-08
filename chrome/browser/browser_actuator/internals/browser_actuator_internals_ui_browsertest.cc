// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_actuator/internals/browser_actuator_internals_ui.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_actuator/internals/browser_actuator_internals.mojom.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browser_actuator/public/features.h"
#include "components/prefs/pref_service.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace browser_actuator {
namespace {

class BrowserActuatorInternalsUIBrowserTest : public InProcessBrowserTest {
 public:
  BrowserActuatorInternalsUIBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kBrowserActuator, kBrowserActuatorInternals},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserActuatorInternalsUIBrowserTest, PageRenders) {
  GURL url(content::GetWebUIURL(kChromeUIBrowserActuatorInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(web_contents->IsCrashed());
  EXPECT_TRUE(web_contents->GetWebUI());

  // Verify the custom LitElement component is stamped into the DOM.
  EXPECT_TRUE(content::EvalJs(
                  web_contents,
                  "!!document.querySelector('browser-actuator-internals-app')")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(BrowserActuatorInternalsUIBrowserTest,
                       BindsFactoryAndCreatesUI) {
  GURL url(content::GetWebUIURL(kChromeUIBrowserActuatorInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(web_contents->GetWebUI());

  auto* controller = web_contents->GetWebUI()
                         ->GetController()
                         ->GetAs<BrowserActuatorInternalsUI>();
  ASSERT_TRUE(controller);

  mojo::Remote<
      browser_actuator_internals::mojom::BrowserActuatorInternalsUIFactory>
      factory;
  controller->BindInterface(factory.BindNewPipeAndPassReceiver());

  mojo::Remote<browser_actuator_internals::mojom::BrowserActuatorInternalsUI>
      ui;
  mojo::PendingRemote<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
      page;
  mojo::PendingReceiver<
      browser_actuator_internals::mojom::BrowserActuatorInternalsPage>
      page_receiver = page.InitWithNewPipeAndPassReceiver();

  factory->CreateUI(std::move(page), ui.BindNewPipeAndPassReceiver());
  factory.FlushForTesting();
  EXPECT_TRUE(ui.is_bound());
  EXPECT_TRUE(ui.is_connected());
}

class BrowserActuatorInternalsUIBrowserTestDisabled
    : public InProcessBrowserTest {
 public:
  BrowserActuatorInternalsUIBrowserTestDisabled() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{kBrowserActuator, kBrowserActuatorInternals});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowserActuatorInternalsUIBrowserTestDisabled,
                       PageDisabledWhenFeatureDisabled) {
  GURL url(content::GetWebUIURL(kChromeUIBrowserActuatorInternalsHost));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(web_contents);
  EXPECT_FALSE(web_contents->GetWebUI());
  EXPECT_EQ(
      content::PAGE_TYPE_ERROR,
      web_contents->GetController().GetLastCommittedEntry()->GetPageType());
}

}  // namespace
}  // namespace browser_actuator
