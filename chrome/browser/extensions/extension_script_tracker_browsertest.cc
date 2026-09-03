// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"

namespace extensions {

class ExtensionScriptTrackerBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionScriptTrackerBrowserTest()
      : feature_list_({blink::features::kExtensionScriptTagging,
                       blink::features::kExtensionScriptTaggingTestingAPI}),
        prerender_helper_(base::BindRepeating(
            &ExtensionScriptTrackerBrowserTest::GetWebContents,
            base::Unretained(this))) {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExposeInternalsForTesting);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ExtensionScriptTrackerBrowserTest::HandleScriptRequest,
        base::Unretained(this)));
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    SetUpDefaultSearchEngine();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL SetUpDefaultSearchEngine(const std::string& host = "example.com",
                                const std::string& path = "/empty.html") {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(template_url_service);
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetURL(
        embedded_test_server()->GetURL(host, path + "?q={searchTerms}").spec());
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    EXPECT_TRUE(template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
    return GetDefaultSearchResultUrl(host, path);
  }

  GURL GetDefaultSearchResultUrl(const std::string& host = "example.com",
                                 const std::string& path = "/empty.html") {
    return embedded_test_server()->GetURL(host, path + "?q=test");
  }

  GURL RegisterScriptResponse(const std::string& host,
                              const std::string& path,
                              const std::string& content) {
    script_responses_[path] = content;
    return embedded_test_server()->GetURL(host, path);
  }

  // Polls until the given JavaScript expression evaluates to true on `rfh`.
  void WaitForJsCondition(const content::ToRenderFrameHost& rfh,
                          const std::string& js_condition) {
    std::string script = base::StringPrintf(R"(
        new Promise(resolve => {
          const check = () => {
            if (%s) resolve(true);
            else setTimeout(check, 50);
          };
          check();
        });
    )",
                                            js_condition.c_str());
    EXPECT_TRUE(content::ExecJs(rfh.render_frame_host(), script));
  }

  // Returns whether a given extension script relative path is marked.
  bool IsExtensionScriptUrlMarked(const content::ToRenderFrameHost& rfh,
                                  const Extension* extension,
                                  const std::string& relative_path) {
    std::string script_url = extension->GetResourceURL(relative_path).spec();
    return IsScriptUrlMarked(rfh, script_url);
  }

  // Returns whether an arbitrary script URL string is marked.
  bool IsScriptUrlMarked(const content::ToRenderFrameHost& rfh,
                         const std::string& script_url) {
    return content::EvalJs(
               rfh.render_frame_host(),
               content::JsReplace("window.internals.isExtensionScriptUrl($1)",
                                  script_url))
        .ExtractBool();
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleScriptRequest(
      const net::test_server::HttpRequest& request) {
    auto it = script_responses_.find(std::string(request.GetURL().path()));
    if (it == script_responses_.end()) {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/javascript");
    response->set_content(it->second);
    return response;
  }

  // Feature list used to enable extension script tagging and its testing API.
  base::test::ScopedFeatureList feature_list_;

  // Map of URL path to content for mock HTTP script responses.
  std::map<std::string, std::string> script_responses_;

  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ContentScriptInjectionTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["page_script.js"],
      "matches": ["http://example.com/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const script = document.createElement('script');
    script.src = chrome.runtime.getURL('page_script.js');
    document.body.appendChild(script);
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page_script.js"), R"(
    window.contentScriptTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.contentScriptTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "page_script.js"));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ProgrammaticScriptTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Programmatic Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {
      "service_worker": "background.js"
    },
    "permissions": ["scripting", "activeTab"],
    "host_permissions": ["http://example.com/*"],
    "web_accessible_resources": [{
      "resources": ["page_script.js"],
      "matches": ["http://example.com/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === 'complete' && tab.url.includes('example.com')) {
        chrome.scripting.executeScript({
          target: { tabId: tabId },
          func: () => {
            const script = document.createElement('script');
            script.src = chrome.runtime.getURL('page_script.js');
            document.body.appendChild(script);
          }
        });
      }
    });
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page_script.js"), R"(
    window.programmaticScriptTracked =
        window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.programmaticScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "page_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       TransitiveScriptTagging) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Transitive Script Tagging Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  GURL web_script_url =
      embedded_test_server()->GetURL("extension.com", "/web_script.js");

  // Level 1: Extension Content Script
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     base::StringPrintf(R"(
    {
      const script = document.createElement('script');
      script.src = '%s';
      document.body.appendChild(script);
    }
  )",
                                        web_script_url.spec().c_str()));

  // Level 2: Web Script loaded by Content Script
  RegisterScriptResponse("extension.com", "/web_script.js", R"(
    window.transitiveScriptTracked =
        window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.transitiveScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
  EXPECT_TRUE(IsScriptUrlMarked(web_contents, web_script_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ProgrammaticMainWorldInjectionTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Main World Injection Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {
      "service_worker": "background.js"
    },
    "permissions": ["scripting", "activeTab"],
    "host_permissions": ["http://example.com/*"],
    "web_accessible_resources": [{
      "resources": ["main_script.js"],
      "matches": ["http://example.com/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === 'complete' && tab.url.includes('example.com')) {
        chrome.scripting.executeScript({
          target: { tabId: tabId },
          world: 'MAIN',
          files: ['main_script.js']
        });
      }
    });
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("main_script.js"), R"(
    window.mainWorldTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.mainWorldTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "main_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       AsyncTaskBoundaryTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Async Task Boundary Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["page_script.js"],
      "matches": ["http://example.com/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    setTimeout(() => {
      const script = document.createElement('script');
      script.src = chrome.runtime.getURL('page_script.js');
      document.body.appendChild(script);
    }, 10);
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page_script.js"), R"(
    window.asyncScriptRan = true;
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.asyncScriptRan === true");
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "page_script.js"));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       EventListenerTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Event Listener Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    document.addEventListener('click', () => {
      window.eventListenerScriptTracked =
          window.internals.extensionScriptInStack();
    });
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(content::ExecJs(web_contents,
                              "document.dispatchEvent(new Event('click'));"));

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.eventListenerScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       DynamicIframeCreationTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Dynamic Iframe Creation Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  GURL web_script_url =
      RegisterScriptResponse("extension.com", "/iframe_web_script.js", R"(
    window.iframeScriptTracked = window.internals.extensionScriptInStack();
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     base::StringPrintf(R"(
    {
      const iframe = document.createElement('iframe');
      document.body.appendChild(iframe);
      const script = iframe.contentDocument.createElement('script');
      script.src = '%s';
      iframe.contentDocument.body.appendChild(script);
    }
  )",
                                        web_script_url.spec().c_str()));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* iframe_rfh = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating([](content::RenderFrameHost* rfh) {
        return rfh->GetParent() != nullptr;
      }));
  ASSERT_TRUE(iframe_rfh);

  WaitForJsCondition(iframe_rfh,
                     base::StringPrintf("window.iframeScriptTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(IsScriptUrlMarked(iframe_rfh, web_script_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       VanillaPageScriptNotTagged) {
  GURL vanilla_script_url =
      RegisterScriptResponse("example.com", "/vanilla_script.js", R"(
    window.vanillaScriptInStack =
        window.internals.extensionScriptInStack();
  )");

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(R"(
        const script = document.createElement('script');
        script.src = $1;
        document.body.appendChild(script);
      )",
                                                 vanilla_script_url.spec())));

  WaitForJsCondition(web_contents, "window.vanillaScriptInStack === ''");
  EXPECT_FALSE(IsScriptUrlMarked(web_contents, vanilla_script_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       PostExtensionScriptExecutionVanillaScriptNotTagged) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Temporary Scope Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const el = document.createElement('div');
    el.id = 'extension-script-done';
    document.body.appendChild(el);
  )");

  GURL vanilla_script_url =
      RegisterScriptResponse("example.com", "/vanilla_script.js", R"(
    window.subsequentScriptInStack =
        window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     "!!document.getElementById('extension-script-done')");

  EXPECT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(R"(
        const script = document.createElement('script');
        script.src = $1;
        document.body.appendChild(script);
      )",
                                                 vanilla_script_url.spec())));

  WaitForJsCondition(web_contents, "window.subsequentScriptInStack === ''");
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
  EXPECT_FALSE(IsScriptUrlMarked(web_contents, vanilla_script_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       MultiLevelTransitiveScriptTagging) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Multi Level Transitive Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  GURL web_script_1_url =
      embedded_test_server()->GetURL("extension.com", "/web_script_1.js");
  GURL web_script_2_url =
      embedded_test_server()->GetURL("thirdparty.com", "/web_script_2.js");

  // Level 1: Extension Content Script
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"),
                     base::StringPrintf(R"(
    {
      const script = document.createElement('script');
      script.src = '%s';
      document.body.appendChild(script);
    }
  )",
                                        web_script_1_url.spec().c_str()));

  // Level 2: Web Script 1 loaded by Content Script
  RegisterScriptResponse("extension.com", "/web_script_1.js",
                         base::StringPrintf(R"(
        {
          const script = document.createElement('script');
          script.src = '%s';
          document.body.appendChild(script);
        }
      )",
                                            web_script_2_url.spec().c_str()));

  // Level 3: Web Script 2 loaded by Web Script 1
  RegisterScriptResponse("thirdparty.com", "/web_script_2.js", R"(
    window.multiLevelScriptTracked =
        window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.multiLevelScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
  EXPECT_TRUE(IsScriptUrlMarked(web_contents, web_script_1_url.spec()));
  EXPECT_TRUE(IsScriptUrlMarked(web_contents, web_script_2_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       InlineEventHandlerTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Inline Event Handler Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const img = document.createElement('img');
    img.src = 'invalid_image.png';
    img.setAttribute(
        'onerror',
        'window.inlineErrorTracked = ' +
        'window.internals.extensionScriptInStack();');
    document.body.appendChild(img);
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.inlineErrorTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       CrossOriginSubframeInjectionTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Subframe Injection Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://*/*"],
      "js": ["content_script.js"],
      "all_frames": true,
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["page_script.js"],
      "matches": ["http://*/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const script = document.createElement('script');
    script.src = chrome.runtime.getURL('page_script.js');
    document.body.appendChild(script);
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page_script.js"), R"(
    window.subframeScriptTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = SetUpDefaultSearchEngine("a.com", "/iframe_cross_site.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_rfh);

  WaitForJsCondition(child_rfh,
                     base::StringPrintf("window.subframeScriptTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(child_rfh, extension, "page_script.js"));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(child_rfh, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ExtensionInlineTextContentTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Inline textContent Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const script = document.createElement('script');
    script.textContent =
        'window.inlineTextTracked = ' +
        'window.internals.extensionScriptInStack();';
    document.body.appendChild(script);
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.inlineTextTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       EvalInsideExtensionTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Eval Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    eval('window.evalTracked = window.internals.extensionScriptInStack();');
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.evalTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       DocumentWriteScriptTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Document Write Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    window.runDocWrite = () => {
      document.write(
          '<script>' +
          'window.docWriteTracked = ' +
          'window.internals.extensionScriptInStack();' +
          '</script>');
    };
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(content::ExecJs(web_contents, "window.runDocWrite();"));

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.docWriteTracked === '%s'",
                                        extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ContentScriptExecutionWithoutPageScriptTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Content Script Direct Execution Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    window.directContentScriptTracked =
        window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.directContentScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ProgrammaticScriptFuncExecutionTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Programmatic Func Execution Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {
      "service_worker": "background.js"
    },
    "permissions": ["scripting", "activeTab"],
    "host_permissions": ["http://example.com/*"]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === 'complete' && tab.url.includes('example.com')) {
        chrome.scripting.executeScript({
          target: { tabId: tabId },
          world: 'MAIN',
          func: () => {
            window.funcTracked = window.internals.extensionScriptInStack();
          }
        });
      }
    });
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents,
                     base::StringPrintf("window.funcTracked === '%s'",
                                        extension->id().c_str()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       ProgrammaticScriptInjectsWebScriptTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Programmatic Web Script Test",
    "version": "0.1",
    "manifest_version": 3,
    "background": {
      "service_worker": "background.js"
    },
    "permissions": ["scripting", "activeTab"],
    "host_permissions": ["http://example.com/*"]
  })");

  GURL web_script_url =
      RegisterScriptResponse("example.com", "/injected_web_script.js", R"(
    window.injectedWebScriptTracked =
        window.internals.extensionScriptInStack();
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                     base::StringPrintf(R"(
    chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
      if (changeInfo.status === 'complete' && tab.url.includes('example.com')) {
        chrome.scripting.executeScript({
          target: { tabId: tabId },
          world: 'MAIN',
          func: (url) => {
            const script = document.createElement('script');
            script.src = url;
            document.body.appendChild(script);
          },
          args: ['%s']
        });
      }
    });
  )",
                                        web_script_url.spec().c_str()));

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(
      web_contents,
      base::StringPrintf("window.injectedWebScriptTracked === '%s'",
                         extension->id().c_str()));
  EXPECT_TRUE(IsScriptUrlMarked(web_contents, web_script_url.spec()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       SameOriginSrcdocIframeTracked) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Same-Origin Srcdoc Iframe Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const iframe = document.createElement('iframe');
    iframe.srcdoc =
        '<script>' +
        'window.srcdocScriptTracked = ' +
        'window.internals.extensionScriptInStack();' +
        '</script>';
    document.body.appendChild(iframe);
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* iframe_rfh = content::FrameMatchingPredicate(
      web_contents->GetPrimaryPage(),
      base::BindRepeating([](content::RenderFrameHost* rfh) {
        return rfh->GetParent() != nullptr;
      }));
  ASSERT_TRUE(iframe_rfh);

  WaitForJsCondition(iframe_rfh,
                     base::StringPrintf("window.srcdocScriptTracked === '%s'",
                                        extension->id().c_str()));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       TrackingDisabledOnNonDefaultSearchResultPage) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Non-Search Page Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://non-search.com/*"],
      "js": ["content_script.js"],
      "run_at": "document_end"
    }],
    "web_accessible_resources": [{
      "resources": ["page_script.js"],
      "matches": ["http://non-search.com/*"]
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    const script = document.createElement('script');
    script.src = chrome.runtime.getURL('page_script.js');
    document.body.appendChild(script);
  )");

  test_dir.WriteFile(FILE_PATH_LITERAL("page_script.js"), R"(
    window.scriptRan = true;
    window.isExtensionTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  GURL non_search_url =
      embedded_test_server()->GetURL("non-search.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_search_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.scriptRan === true");
  EXPECT_EQ("", content::EvalJs(web_contents, "window.isExtensionTracked")
                    .ExtractString());
  EXPECT_FALSE(
      IsExtensionScriptUrlMarked(web_contents, extension, "page_script.js"));
  EXPECT_FALSE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionScriptTrackerBrowserTest,
    TrackingDisabledAfterNavigationToNonDefaultSearchResultPage) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Navigation Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    window.contentScriptRan = true;
    window.isExtensionTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // 1. Navigate to default search result page: tracking should be enabled.
  GURL search_url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ(extension->id(),
            content::EvalJs(web_contents, "window.isExtensionTracked")
                .ExtractString());
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));

  // 2. Navigate to same-site non-default search result page in the same tab:
  // tracking should now be disabled.
  GURL non_search_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_search_url));

  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ("", content::EvalJs(web_contents, "window.isExtensionTracked")
                    .ExtractString());
  EXPECT_FALSE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionScriptTrackerBrowserTest,
                       TrackingEnabledForPrerenderedDefaultSearchResultPage) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Prerender Default Search Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    window.contentScriptRan = true;
    window.isExtensionTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // 1. Navigate to initial non-default search result page.
  GURL non_search_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), non_search_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ("", content::EvalJs(web_contents, "window.isExtensionTracked")
                    .ExtractString());

  // 2. Prerender default search result page.
  GURL search_url = GetDefaultSearchResultUrl();
  const auto host_id = prerender_helper().AddPrerender(search_url);
  EXPECT_TRUE(prerender_helper().GetPrerenderedMainFrameHost(host_id));

  // 3. Activate the prerendered default search result page.
  prerender_helper().NavigatePrimaryPage(search_url);

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ(extension->id(),
            content::EvalJs(web_contents, "window.isExtensionTracked")
                .ExtractString());
  EXPECT_TRUE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionScriptTrackerBrowserTest,
    TrackingDisabledForPrerenderedNonDefaultSearchResultPage) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
    "name": "Extension Script Tracker Prerender Non-Default Search Test",
    "version": "0.1",
    "manifest_version": 3,
    "content_scripts": [{
      "matches": ["http://example.com/*"],
      "js": ["content_script.js"],
      "world": "MAIN",
      "run_at": "document_end"
    }]
  })");

  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
    window.contentScriptRan = true;
    window.isExtensionTracked = window.internals.extensionScriptInStack();
  )");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // 1. Navigate to initial default search result page: tracking should be
  // enabled.
  GURL search_url = GetDefaultSearchResultUrl();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), search_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ(extension->id(),
            content::EvalJs(web_contents, "window.isExtensionTracked")
                .ExtractString());

  // 2. Prerender a non-default search result page.
  GURL non_search_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  const auto host_id = prerender_helper().AddPrerender(non_search_url);
  EXPECT_TRUE(prerender_helper().GetPrerenderedMainFrameHost(host_id));

  // 3. Activate the prerendered non-default search result page.
  prerender_helper().NavigatePrimaryPage(non_search_url);

  WaitForJsCondition(web_contents, "window.contentScriptRan === true");
  EXPECT_EQ("", content::EvalJs(web_contents, "window.isExtensionTracked")
                    .ExtractString());
  EXPECT_FALSE(
      IsExtensionScriptUrlMarked(web_contents, extension, "content_script.js"));
}
}  // namespace extensions
