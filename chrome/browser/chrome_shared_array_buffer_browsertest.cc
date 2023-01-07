// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace policy {

// This is a Chrome test since we need to access the Profile and Preferences.
class ChromeSharedArrayBufferBrowserTest : public PolicyTest {
 public:
  using WebFeature = blink::mojom::WebFeature;

  ChromeSharedArrayBufferBrowserTest() {
    feature_list_.InitWithFeatures(
        // Enabled:
        {},
        // Disabled:
        {
            features::kSharedArrayBuffer,
            features::kSharedArrayBufferOnDesktop,
        });
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetPolicyAndRestartBrowser() {
    // The preference is false by default.
    EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kSharedArrayBufferUnrestrictedAccessAllowed));

    PolicyMap policies;
    policies.Set(key::kSharedArrayBufferUnrestrictedAccessAllowed,
                 POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                 base::Value(true), nullptr);
    UpdateProviderPolicy(policies);

    // Now the preference should be true.
    EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
        prefs::kSharedArrayBufferUnrestrictedAccessAllowed));

    // The old browser has already created the ContentBrowserClient which reads
    // the preference, so it can't create renderers with SABs enabled by policy.
    // Create a new browser that will pick up the preference and enable SABs for
    // new renderer processes.
    Browser* new_browser = CreateBrowser(browser()->profile());
    CloseBrowserSynchronously(browser());
    SelectFirstBrowser();
    ASSERT_EQ(browser(), new_browser);

    // Navigate the new browser to 'localhost', so the tests will get new
    // renderer processes when they navigate to xxx.com origins.
    GURL local_host = embedded_test_server()->GetURL("/empty.html");
    EXPECT_TRUE(NavigateToURL(web_contents(), local_host));
  }

 private:
  void SetUpOnMainThread() final {
    PolicyTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    ASSERT_FALSE(base::FeatureList::IsEnabled(features::kSharedArrayBuffer));
    ASSERT_FALSE(
        base::FeatureList::IsEnabled(features::kSharedArrayBufferOnDesktop));
  }

  void SetUpCommandLine(base::CommandLine* command_line) final {
    PolicyTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeSharedArrayBufferBrowserTest,
                       PolicyEnablesSabConstructor) {
  SetPolicyAndRestartBrowser();

  GURL url = embedded_test_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), url));
  EXPECT_EQ(true, content::EvalJs(web_contents(),
                                  "'SharedArrayBuffer' in globalThis"));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedArrayBufferBrowserTest,
                       NoPolicyNoSabConstructor) {
  GURL url = embedded_test_server()->GetURL("a.com", "/empty.html");
  EXPECT_TRUE(NavigateToURL(web_contents(), url));
  EXPECT_EQ(false, content::EvalJs(web_contents(),
                                   "'SharedArrayBuffer' in globalThis"));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedArrayBufferBrowserTest,
                       PolicyEnablesSharing) {
  SetPolicyAndRestartBrowser();

  GURL main_url = embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL sub_url = embedded_test_server()->GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();

  EXPECT_TRUE(content::ExecJs(main_document, content::JsReplace(R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });

    g_iframe = document.createElement('iframe');
    g_iframe.src = $1;
    document.body.appendChild(g_iframe);
  )",
                                                                sub_url)));
  WaitForLoadStop(web_contents());
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(false, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  EXPECT_TRUE(ExecJs(sub_document, R"(
    let sab = new SharedArrayBuffer(1234);
    parent.postMessage(sab, "*");
  )"));

  EXPECT_EQ(1234, EvalJs(main_document, "g_sab_size"));
}

IN_PROC_BROWSER_TEST_F(ChromeSharedArrayBufferBrowserTest, NoPolicyNoSharing) {
  GURL main_url = embedded_test_server()->GetURL("a.com", "/empty.html");
  GURL sub_url = embedded_test_server()->GetURL("a.com", "/empty.html");

  EXPECT_TRUE(content::NavigateToURL(web_contents(), main_url));
  content::RenderFrameHost* main_document =
      web_contents()->GetPrimaryMainFrame();

  EXPECT_TRUE(content::ExecJs(web_contents(), content::JsReplace(R"(
    g_sab_size = new Promise(resolve => {
      addEventListener("message", event => resolve(event.data.byteLength));
    });

    g_iframe = document.createElement('iframe');
    g_iframe.src = $1;
    document.body.appendChild(g_iframe);
  )",
                                                                 sub_url)));
  WaitForLoadStop(web_contents());
  content::RenderFrameHost* sub_document = ChildFrameAt(main_document, 0);

  EXPECT_EQ(false, EvalJs(main_document, "self.crossOriginIsolated"));
  EXPECT_EQ(false, EvalJs(sub_document, "self.crossOriginIsolated"));

  auto postSharedArrayBuffer = EvalJs(main_document, R"(
    // Create a WebAssembly Memory to bypass the SAB constructor restriction.
    const sab =
        new WebAssembly.Memory({ shared:true, initial:1, maximum:1 }).buffer;
    g_iframe.contentWindow.postMessage(sab,"*");
  )");
  EXPECT_THAT(
      postSharedArrayBuffer.error,
      testing::HasSubstr("Failed to execute 'postMessage' on 'Window': "));
}

}  // namespace policy
