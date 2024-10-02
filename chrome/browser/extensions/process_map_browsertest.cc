// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_map.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/strings/cstring_view.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ProcessMapBrowserTest : public ExtensionBrowserTest {
 public:
  ProcessMapBrowserTest() = default;
  ProcessMapBrowserTest(const ProcessMapBrowserTest&) = delete;
  ProcessMapBrowserTest& operator=(const ProcessMapBrowserTest&) = delete;
  ~ProcessMapBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns the WebContents of the currently-active tab.
  content::WebContents* GetActiveTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderProcessHost& GetActiveMainFrameProcess() {
    return *GetActiveTab()->GetPrimaryMainFrame()->GetProcess();
  }

  int GetActiveMainFrameProcessID() {
    return GetActiveMainFrameProcess().GetID();
  }

  // Adds a new extension with the given `extension_name` and host permission to
  // the given `host_pattern`.
  const Extension* AddExtensionWithHostPermission(
      std::string_view extension_name,
      std::string_view host_pattern) {
    static constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "version": "0.1",
             "host_permissions": ["%s"]
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(base::StringPrintf(
        kManifestTemplate, extension_name.data(), host_pattern.data()));
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Adds a new extension with the given `extension_name` and a content script
  // that runs on `content_script_pattern`, sending a message when the script
  // injects.
  const Extension* AddExtensionWithContentScript(
      std::string_view extension_name,
      std::string_view content_script_pattern) {
    static constexpr char kManifestTemplate[] =
        R"({
             "name": "%s",
             "manifest_version": 3,
             "version": "0.1",
             "content_scripts": [{
               "matches": ["%s"],
               "js": ["script.js"]
             }]
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(
        base::StringPrintf(kManifestTemplate, extension_name.data(),
                           content_script_pattern.data()));
    extension_dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                             "chrome.test.sendMessage('script injected');");
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  void ExecuteUserScriptInActiveTab(const ExtensionId& extension_id) {
    base::RunLoop run_loop;
    content::WebContents* web_contents = GetActiveTab();
    // TODO(crbug.com/40262660): Add a utility method for user script
    // injection in browser tests.
    ScriptExecutor script_executor(web_contents);
    std::vector<mojom::JSSourcePtr> sources;
    sources.push_back(
        mojom::JSSource::New("document.title = 'injected';", GURL()));
    script_executor.ExecuteScript(
        mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id),
        mojom::CodeInjection::NewJs(mojom::JSInjection::New(
            std::move(sources), mojom::ExecutionWorld::kUserScript,
            /*world_id=*/std::nullopt,
            blink::mojom::WantResultOption::kWantResult,
            blink::mojom::UserActivationOption::kDoNotActivate,
            blink::mojom::PromiseResultOption::kAwait)),
        ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
        ScriptExecutor::DONT_MATCH_ABOUT_BLANK,
        mojom::RunLocation::kDocumentIdle, ScriptExecutor::DEFAULT_PROCESS,
        GURL() /* webview_src */,
        base::IgnoreArgs<std::vector<ScriptExecutor::FrameResult>>(
            run_loop.QuitWhenIdleClosure()));

    run_loop.Run();

    EXPECT_EQ(u"injected", web_contents->GetTitle());
  }

  // Helper function to define the test body for tests that use
  // AddExtensionWithSandboxedWebpage, defined below so it's near the tests that
  // use it.
  void VerifyWhetherSubframesAreIsolated(
      const GURL& frame_url,
      const std::string& content,
      bool expect_subframes_isolated_from_each_other,
      bool expect_sandboxed_subframe_isolated_from_extension_page,
      bool expect_non_sandboxed_subframe_isolated_from_extension_page);

  // Helper function for data: and srcdoc tests regarding resource access from
  // sandboxed frames, defined below so it's near the tests that use it.
  // Expects that `parent_script_template` contains a `%s` which this function
  // will replace with the extension origin. `is_subframe_data_url` should be
  // true if the `parent_script_template` is for a data url frame, so that this
  // function doesn't have to infer that from the template.
  void VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess(
      base::cstring_view parent_script_template,
      const bool is_subframe_data_url,
      const bool expects_api_access);

  // Adds a new extension with a parent frame that in turn loads `url` in two
  // iframes, one of which is sandboxed. If `url` is about:srcdoc, then the
  // srcdoc attribute is set instead using the value contained in `content`.
  const Extension* AddExtensionWithSandboxedWebpage(
      const GURL& url,
      const std::string& content) {
    static constexpr char kManifest[] =
        R"({
             "name": "Sandboxed Page",
             "manifest_version": 3,
             "version": "0.1"
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    std::string page_content;
    if (url.IsAboutSrcdoc()) {
      page_content = base::StringPrintf(
          R"(<html>
             <iframe sandbox srcdoc="%s"></iframe>
             <iframe srcdoc="%s"></iframe>
           </html>)",
          content.c_str(), content.c_str());
    } else {
      page_content = base::StringPrintf(
          R"(<html>
             <iframe sandbox src="%s"></iframe>
             <iframe src="%s"></iframe>
           </html>)",
          url.spec().c_str(), url.spec().c_str());
    }
    extension_dir->WriteFile(FILE_PATH_LITERAL("parent.html"), page_content);
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Create an extension with a page that loads a non-extension page, which in
  // turn contains an about:srcdoc subframe.
  const Extension* AddExtensionWithNonExtensionSubframeWithSrcdocSubframe(
      bool srcdoc_is_sandboxed) {
    static constexpr char kManifest[] =
        R"({
             "name": "Sandboxed Page",
             "manifest_version": 3,
             "version": "0.1"
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);

    GURL non_extension_url = embedded_test_server()->GetURL(
        "example.com", srcdoc_is_sandboxed ? "/iframe_sandboxed_srcdoc.html"
                                           : "/iframe_srcdoc.html");
    const char kPageContentTemplate[] =
        R"(<html>
             <body>
               <iframe src="%s"></iframe>
             </body>
           </html>)";
    extension_dir->WriteFile(
        FILE_PATH_LITERAL("parent.html"),
        base::StringPrintf(kPageContentTemplate,
                           non_extension_url.spec().c_str()));
    // Including a non-web-accessible extension resource for testing access.
    extension_dir->WriteFile(FILE_PATH_LITERAL("data.json"),
                             "{ \"answer\" : 42 }");
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Adds an extension with a page with a sandboxed subframe (that can be
  // manipulated by individual tests), and a simple resource that the subframe
  // might load.
  const Extension* AddExtensionWithResource() {
    static constexpr char kManifest[] =
        R"({
             "name": "Page With Sandboxed Subframe and Resource To Load",
             "manifest_version": 3,
             "version": "0.1"
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    std::string page_content =
        R"(<html>
             <iframe id='test_frame' sandbox="allow-scripts"></iframe>
           </html>)";
    extension_dir->WriteFile(FILE_PATH_LITERAL("parent.html"), page_content);
    std::string resource_js = R"(let foo = "bar";)";
    extension_dir->WriteFile(FILE_PATH_LITERAL("resource.js"), resource_js);
    std::string page_requesting_resource_content =
        R"(<script src="resource.js"></script>)";
    extension_dir->WriteFile(FILE_PATH_LITERAL("page_requesting_resource.html"),
                             page_requesting_resource_content);
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  // Create a pair of nested extensions, where `page.html` from the first
  // extension is nested inside `parent.html` from the second extension.
  std::pair<const Extension*, const Extension*> AddNestedExtensions() {
    const Extension* extension1 = nullptr;
    {
      static constexpr char kManifestTemplate[] =
          R"({
             "name": "Extension1",
             "manifest_version": 3,
             "version": "0.1",
             "web_accessible_resources": [
               {
                 "resources": [ "page.html" ],
                 "matches": [ "%s://*/*" ]
               }
             ]
           })";
      auto extension_dir = std::make_unique<TestExtensionDir>();
      extension_dir->WriteManifest(
          base::StringPrintf(kManifestTemplate, kExtensionScheme));
      extension_dir->WriteFile(FILE_PATH_LITERAL("page.html"),
                               R"(<html>E1</html>)");
      extension1 = LoadExtension(extension_dir->UnpackedPath());
      extension_dirs_.push_back(std::move(extension_dir));
    }
    GURL e1_page_url = extension1->GetResourceURL("page.html");

    const Extension* extension2 = nullptr;
    {
      static constexpr char kManifest[] =
          R"({
             "name": "Extension2",
             "manifest_version": 3,
             "version": "0.1"
           })";
      auto extension_dir = std::make_unique<TestExtensionDir>();
      extension_dir->WriteManifest(kManifest);
      static constexpr char kPageContent[] =
          R"(<html>E2
               <iframe sandbox="allow-scripts" src="%s"></iframe>
             </html>)";
      extension_dir->WriteFile(
          FILE_PATH_LITERAL("parent.html"),
          base::StringPrintf(kPageContent, e1_page_url.spec().c_str()));
      // Create a page that is not listed as a web_accessible_resource.
      extension_dir->WriteFile(FILE_PATH_LITERAL("private_page.html"),
                               R"(<html>E2 Private</html>)");
      extension2 = LoadExtension(extension_dir->UnpackedPath());
      extension_dirs_.push_back(std::move(extension_dir));
    }

    return std::make_pair(extension1, extension2);
  }

  // Adds a new extension with two sandboxed frames, `sandboxed.html` and
  // `sandboxed2.html`, and a parent page, `parent.html` to host it.
  // Having two manifest-sandboxed pages facilitates testing that there is
  // just one sandbox process per extension.
  const Extension* AddExtensionWithSandboxedFrame() {
    static constexpr char kManifest[] =
        R"({
             "name": "Sandboxed Page",
             "manifest_version": 3,
             "version": "0.1",
             "sandbox": {
               "pages": [ "sandboxed.html", "sandboxed2.html" ]
             }
           })";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    extension_dir->WriteFile(FILE_PATH_LITERAL("sandboxed.html"),
                             "<html>Sandboxed</html>");
    extension_dir->WriteFile(FILE_PATH_LITERAL("sandboxed2.html"),
                             "<html>Sandboxed 2</html>");
    extension_dir->WriteFile(FILE_PATH_LITERAL("parent.html"),
                             R"(<html>
             <iframe src="sandboxed.html"></iframe>
             <iframe src="sandboxed2.html"></iframe>
           </html>)");
    const Extension* extension = LoadExtension(extension_dir->UnpackedPath());
    extension_dirs_.push_back(std::move(extension_dir));
    return extension;
  }

  const Extension* AddExtensionWithWebViewAndOpen() {
    static constexpr char kManifest[] =
        R"({
             "name": "Web View",
             "manifest_version": 2,
             "version": "0.1",
             "app": {
               "background": { "scripts": ["background.js"] }
             },
             "webview": {
               "partitions": [{
                 "name": "foo",
                 "accessible_resources": ["accessible.html"]
               }]
             },
             "permissions": ["webview"]
           })";
    static constexpr char kBackgroundJs[] =
        R"(chrome.app.runtime.onLaunched.addListener(() => {
             chrome.app.window.create('embedder.html', {}, function () {});
           });)";
    static constexpr char kEmbedderHtml[] =
        R"(<html>
           <body>
             <webview partition="foo"></webview>
             <script src="embedder.js"></script>
           </body>
           </html>)";
    static constexpr char kEmbedderJs[] =
        R"(onload = () => {
             let webview = document.querySelector('webview');
             webview.addEventListener('loadstop', () => {
               chrome.test.sendMessage('webview loaded');
             });
             webview.addEventListener('loadabort', (e) => {
               console.error('Webview aborted load: ' + e.toString());
             });
             webview.src = 'accessible.html';
           };)";
    auto extension_dir = std::make_unique<TestExtensionDir>();
    extension_dir->WriteManifest(kManifest);
    extension_dir->WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
    extension_dir->WriteFile(FILE_PATH_LITERAL("embedder.html"), kEmbedderHtml);
    extension_dir->WriteFile(FILE_PATH_LITERAL("embedder.js"), kEmbedderJs);
    extension_dir->WriteFile(FILE_PATH_LITERAL("accessible.html"), "hello");

    ExtensionTestMessageListener webview_listener("webview loaded");
    const Extension* extension = LoadAndLaunchApp(extension_dir->UnpackedPath(),
                                                  /*uses_guest_view=*/true);
    extension_dirs_.push_back(std::move(extension_dir));
    EXPECT_TRUE(webview_listener.WaitUntilSatisfied());

    return extension;
  }

  content::WebContents* GetAppWindowContents() {
    AppWindowRegistry* registry = AppWindowRegistry::Get(profile());
    if (registry->app_windows().size() != 1) {
      ADD_FAILURE() << "Incorrect number of app windows: "
                    << registry->app_windows().size();
      return nullptr;
    }

    return (*registry->app_windows().begin())->web_contents();
  }

  content::WebContents* GetWebViewFromEmbedder(content::WebContents* embedder) {
    std::vector<content::WebContents*> inner_web_contents =
        embedder->GetInnerWebContents();
    if (inner_web_contents.size() != 1) {
      ADD_FAILURE() << "Unexpected number of inner web contents: "
                    << inner_web_contents.size();
      return nullptr;
    }

    content::WebContents* inner_contents = inner_web_contents[0];
    if (!WebViewGuest::FromWebContents(inner_contents)) {
      return nullptr;
    }

    return inner_contents;
  }

  // Opens a new tab to the given `domain`.
  void OpenDomain(std::string_view domain) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(domain, "/simple.html")));
  }

  // Opens a new tab to a Web UI page.
  void OpenWebUi() {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  }

  // Opens a new tab to a page in the given `extension`.
  void OpenExtensionPage(const Extension& extension) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension.GetResourceURL("manifest.json")));
  }

  // Opens a new tab to the given `domain` and waits for a content script to
  // inject.
  void OpenDomainAndWaitForContentScript(std::string_view domain) {
    ExtensionTestMessageListener listener("script injected");
    OpenDomain(domain);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Opens a new tab to the page with a sandboxed frame in the given
  // `extension`.
  void OpenExtensionPageWithSandboxedFrame(const Extension& extension) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), extension.GetResourceURL("parent.html")));
  }

  // Determines if a given `frame` is sandboxed. Sandboxed frames don't
  // have access to any special extension APIs, even those that require no
  // specific permissions (like chrome.tabs).
  bool ExtensionFrameIsSandboxed(content::RenderFrameHost* frame) {
    EXPECT_TRUE(frame->GetLastCommittedURL().SchemeIs(kExtensionScheme));

    return FrameHasOriginRestrictedSandboxed(frame) &&
           !FrameHasAccessToExtensionApis(frame);
  }

  bool FrameHasOriginRestrictedSandboxed(content::RenderFrameHost* frame) {
    return frame->IsSandboxed(network::mojom::WebSandboxFlags::kOrigin);
  }

  bool FrameHasAccessToExtensionApis(content::RenderFrameHost* frame) {
    // Verify extension api access by actually running a simple api function.
    static constexpr char api_access_script[] =
        R"(
          (async function hasAccessToExtensionAPIs() {
            try {
              let tabs = await chrome.tabs.query({});
              return tabs && tabs.length && tabs.length != 0;
            } catch(err) {
              return false;
            }
          })();
        )";
    // Note: Calling ExtractBool on EvalJsResult below is expected to be safe as
    // the script above will always return a boolean. But, if called on a
    // sandboxed frame without 'allow-scripts' it will throw a CHECK.
    return content::EvalJs(frame, api_access_script).ExtractBool();
  }

  // Iterates over every context type and checks if it could be hosted given the
  // pairing of `extension` and `process`, expecting it to be allowed if and
  // only if the context type is in `allowed_contexts`. `debug_string` is used
  // in a scoped trace to make test failures more meaningful.
  void RunCanProcessHostContextTypeChecks(
      const Extension* extension,
      const content::RenderProcessHost& process,
      const std::vector<mojom::ContextType>& allowed_contexts,
      std::string_view debug_string) {
    std::vector<mojom::ContextType> all_types = {
        mojom::ContextType::kUnspecified,
        mojom::ContextType::kPrivilegedExtension,
        mojom::ContextType::kUnprivilegedExtension,
        mojom::ContextType::kContentScript,
        mojom::ContextType::kWebPage,
        mojom::ContextType::kPrivilegedWebPage,
        mojom::ContextType::kWebUi,
        mojom::ContextType::kUntrustedWebUi,
        mojom::ContextType::kLockscreenExtension,
        mojom::ContextType::kOffscreenExtension,
        mojom::ContextType::kUserScript,
    };

    for (auto context_type : all_types) {
      SCOPED_TRACE(testing::Message()
                   << "Testing Context Type: " << context_type
                   << ", Extension: "
                   << (extension ? extension->name() : "<no extension>")
                   << ", Debug String: " << debug_string);
      bool expected_to_be_allowed =
          base::Contains(allowed_contexts, context_type);
      EXPECT_EQ(expected_to_be_allowed,
                process_map()->CanProcessHostContextType(extension, process,
                                                         context_type));
    }
  }

  ProcessMap* process_map() { return ProcessMap::Get(profile()); }

 private:
  // Dirs for our test extensions; these have to stay in-scope for the duration
  // of the test.
  std::vector<std::unique_ptr<TestExtensionDir>> extension_dirs_;
};

// Verify that an injected content script can successfully use dynamic imports
// when operating in a sandboxed srcdoc iframe.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       ContentScriptDynamicImportsWorkInSandboxedSrcdocFrames) {
  // Create extension with a content script that relies on dynamic imports.
  static constexpr char kManifest[] = R"(
  {
    "name": "Test Dynamic Import",
    "manifest_version": 2,
    "version": "1.0",
    "web_accessible_resources": [
      "content-import.js"
    ],
    "content_scripts": [{
      "matches": [ "*://*/*" ],
      "all_frames": true,
      "js": [ "content-script.js" ],
      "match_origin_as_fallback": true
    }]
  })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("content-import.js"),
                R"(export function main() {
                     document.body.innerHTML =
                         document.body.innerHTML.replace('sandboxed',
                                                         'SANDBOXED');
                     chrome.test.sendMessage('dynamic import success');
                   })");
  dir.WriteFile(FILE_PATH_LITERAL("content-script.js"), R"(
    const src = chrome.runtime.getURL("content-import.js");
    import(src).then((contentImport) => {
                   contentImport.main();
                 }).catch ((error) => {
                   console.log('Error: import failed: ' + error.message);
                 });
  )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Load a page and give it a sandboxed-srcdoc frame.
  ExtensionTestMessageListener listener_mainframe("dynamic import success");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/simple.html")));
  ASSERT_TRUE(listener_mainframe.WaitUntilSatisfied());

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::TestNavigationObserver observer(web_contents, 1);
  ExtensionTestMessageListener listener_subframe("dynamic import success");
  EXPECT_TRUE(ExecJs(main_frame, R"(
          let frame = document.createElement('iframe');
          frame.sandbox = '';
          frame.srcdoc = '<html><body>sandboxed</body></html>';
          document.body.appendChild(frame);
        )"));
  observer.Wait();

  // Wait for the injected content script to complete.
  ASSERT_TRUE(listener_subframe.WaitUntilSatisfied());

  // Verify that the action performed by the dynamically imported code was
  // successful.
  content::RenderFrameHost* sandboxed_child_frame =
      content::ChildFrameAt(main_frame, 0);
  EXPECT_TRUE(content::EvalJs(sandboxed_child_frame,
                              "document.body.innerHTML == 'SANDBOXED';")
                  .ExtractBool());
}

// Check that when an extension frame is inadvertently loaded as sandboxed
// because it inherits sandbox flags from its parent, the extension frame can
// still use extension messaging APIs without triggering a renderer kill due
// to sandboxed frame checks in ChildProcessSecurityPolicy.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, SandboxedWebPageEmbedsExtension) {
  GURL sandboxed_url =
      embedded_test_server()->GetURL("a.test", "/csp-sandbox.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), sandboxed_url));
  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* sandboxed_main_frame =
      web_contents->GetPrimaryMainFrame();
  ASSERT_TRUE(sandboxed_main_frame->IsSandboxed(
      network::mojom::WebSandboxFlags::kOrigin));

  // Set up an extension with a web-accessible page that sends a message to a
  // background worker and waits for a response.
  static constexpr char kManifest[] = R"(
      {
        "name": "Foo",
        "version": "1.0",
        "web_accessible_resources": [{
          "resources": ["foo.html"],
          "matches": ["*://*/*"]
        }],
        "manifest_version": 3,
        "background": { "service_worker": "worker.js" }
    })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("foo.html"),
                R"(<script src="foo.js"></script>)");
  dir.WriteFile(FILE_PATH_LITERAL("foo.js"), R"(
    (async function() {
      const response = await chrome.runtime.sendMessage('ping');
      chrome.test.assertEq('pong', response);
      chrome.test.sendMessage('done');
    })();
  )");

  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), R"(
    chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
      if (request == 'ping') {
        sendResponse('pong');
      }
    });
  )");

  const Extension* extension = LoadExtension(dir.UnpackedPath());
  GURL extension_url = extension->GetResourceURL("foo.html");

  // Insert an extension subframe into the sandboxed main frame and ensure that
  // the the sendMessage exchange finishes successfully.
  const char kAddFrameScript[] =
      R"(
        let f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
      )";

  ExtensionTestMessageListener listener("done");
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(ExecJs(sandboxed_main_frame,
                     content::JsReplace(kAddFrameScript, extension_url)));
  observer.Wait();

  // Double-check that the extension frame was sandboxed but maintained access
  // to extension APIs.
  content::RenderFrameHost* sandboxed_extension_frame =
      content::ChildFrameAt(sandboxed_main_frame, 0);
  EXPECT_TRUE(sandboxed_extension_frame->IsSandboxed(
      network::mojom::WebSandboxFlags::kOrigin));
  EXPECT_TRUE(FrameHasAccessToExtensionApis(sandboxed_extension_frame));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
}

// Tests that extension E1 containing a sandboxed webpage A which then contains
// extension E2 in a subframe results in the E2 frame being sandboxed.
IN_PROC_BROWSER_TEST_F(
    ProcessMapBrowserTest,
    ExtensionFrameContainingSandboxedFrameContainingOtherExtensionFrame) {
  GURL a_frame_url =
      embedded_test_server()->GetURL("example.com", "/iframe_blank.html");
  // Create extension 1 (E1).
  static constexpr char kManifestE1[] =
      R"({
             "name": "E1",
             "manifest_version": 3,
             "version": "0.1"
         })";
  TestExtensionDir extension_dir1;
  extension_dir1.WriteManifest(kManifestE1);
  static constexpr char kPageWithSandboxedFrame[] =
      R"(<html>
             <h1>E1</h1>
             <iframe sandbox="allow-scripts" src="%s"></iframe>
          </html>)";
  extension_dir1.WriteFile(
      FILE_PATH_LITERAL("main.html"),
      base::StringPrintf(kPageWithSandboxedFrame, a_frame_url.spec().c_str()));
  const Extension* extension1 = LoadExtension(extension_dir1.UnpackedPath());

  // Create extension 2 (E2).
  static constexpr char kManifestE2[] =
      R"({
             "name": "E2",
             "manifest_version": 3,
             "version": "0.1",
             "web_accessible_resources": [
               {
                 "resources": [ "main.html" ],
                 "matches": [ "*://*/*" ]
               }
             ]
         })";
  TestExtensionDir extension_dir2;
  extension_dir2.WriteManifest(kManifestE2);
  extension_dir2.WriteFile(FILE_PATH_LITERAL("main.html"),
                           "<html><h1>E2</h2></html>");
  const Extension* extension2 = LoadExtension(extension_dir2.UnpackedPath());

  // Load E1.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension1->GetResourceURL("main.html")));
  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_a_frame =
      content::ChildFrameAt(main_frame, 0);

  // Navigate frame in A's subframe to E2.
  GURL e2_main_url = extension2->GetResourceURL("main.html");
  content::TestNavigationObserver observer(web_contents);
  static constexpr char kScriptE2Load[] =
      R"(
        document.getElementById('test').src = $1;
      )";
  EXPECT_TRUE(content::ExecJs(sandboxed_a_frame,
                              content::JsReplace(kScriptE2Load, e2_main_url)));
  observer.Wait();

  // Verify that the E2 is sandboxed.
  content::RenderFrameHost* sandboxed_E2_frame =
      content::ChildFrameAt(sandboxed_a_frame, 0);
  ASSERT_NE(nullptr, sandboxed_E2_frame);
  // The E2 frame has an origin-restricted sandbox flag.
  EXPECT_TRUE(FrameHasOriginRestrictedSandboxed(sandboxed_E2_frame));
  EXPECT_TRUE(sandboxed_E2_frame->GetLastCommittedOrigin().opaque());
  EXPECT_TRUE(content::EvalJs(sandboxed_E2_frame, "window.origin == 'null';")
                  .ExtractBool());
  // The E2 frame has access to extension APIs.
  EXPECT_TRUE(
      process_map()->Contains(sandboxed_E2_frame->GetProcess()->GetID()));
  EXPECT_TRUE(FrameHasAccessToExtensionApis(sandboxed_E2_frame));
  // The E2 frame is sandboxed by virtue of being loaded in an iframe with
  // a sandbox attribute set, but it is not a manifest-sandboxed frame. As such,
  // it gets placed in the main extension process, has access to extension APIs
  // and is not places in a sandboxed SiteInstance.
  EXPECT_FALSE(content::HasSandboxedSiteInstance(sandboxed_E2_frame));

  // Each frame will be in a separate process due to site isolation.
  EXPECT_NE(main_frame->GetProcess(), sandboxed_a_frame->GetProcess());
  EXPECT_NE(main_frame->GetProcess(), sandboxed_E2_frame->GetProcess());
  EXPECT_NE(sandboxed_a_frame->GetProcess(), sandboxed_E2_frame->GetProcess());
}

// Tests that web pages are not considered privileged extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebPages) {
  // For fun, make sure an extension with access to the given web page is
  // loaded (just to validate we're not doing anything related to
  // extension permissions in our calculations).
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in web page processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebPages) {
  // For fun, make sure an extension with access to the given web page is
  // loaded (just to validate we're not doing anything related to
  // extension permissions in our calculations).
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");
  content::RenderProcessHost& web_page_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(extension, web_page_process,
                                     {mojom::ContextType::kContentScript},
                                     "web page with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, web_page_process,
      {mojom::ContextType::kWebPage, mojom::ContextType::kUntrustedWebUi},
      "web page without extension passed");
}

// Tests that web ui pages are not considered privileged extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebUiPages) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenWebUi();

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of processes that can be hosted in web ui processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebUiPages) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenWebUi();
  content::RenderProcessHost& webui_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(extension, webui_process,
                                     {mojom::ContextType::kContentScript},
                                     "webui page with extension passed");
  RunCanProcessHostContextTypeChecks(nullptr, webui_process,
                                     {mojom::ContextType::kWebUi},
                                     "webui page without extension passed");
}

// Tests that normal extension pages are considered privileged extension
// processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_ExtensionPages) {
  // Load up two extensions, each with the same permissions.
  const Extension* extension1 =
      AddExtensionWithHostPermission("test1", "*://example.com/*");
  const Extension* extension2 =
      AddExtensionWithHostPermission("test2", "*://example.com/*");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  // Navigate to a page within the first extension. It should be a privileged
  // page for that extension, but not the other.
  OpenExtensionPage(*extension1);
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension1, GetActiveMainFrameProcessID()));
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension2, GetActiveMainFrameProcessID()));

  // Inversion: Navigate to the page of the second extension. It should be a
  // privileged page in the second, but not the first.
  OpenExtensionPage(*extension2);
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension1, GetActiveMainFrameProcessID()));
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension2, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in regular extension processes.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_ExtensionPages) {
  // Load up two extensions, each with the same permissions.
  const Extension* extension1 =
      AddExtensionWithHostPermission("test1", "*://example.com/*");
  const Extension* extension2 =
      AddExtensionWithHostPermission("test2", "*://example.com/*");
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  // Navigate to a page within the first extension. It should be a privileged
  // page for that extension, but not the other.
  OpenExtensionPage(*extension1);

  content::RenderProcessHost& extension1_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(extension1, extension1_process,
                                     {mojom::ContextType::kContentScript,
                                      mojom::ContextType::kPrivilegedExtension,
                                      mojom::ContextType::kOffscreenExtension},
                                     "extension1 page with extension1 passed");
  RunCanProcessHostContextTypeChecks(extension2, extension1_process,
                                     {mojom::ContextType::kContentScript},
                                     "extension1 page with extension2 passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, extension1_process, {},
      "extension1 page without extension passed");

  // Inversion: Navigate to the page of the second extension. It should be a
  // privileged page in the second, but not the first.
  OpenExtensionPage(*extension2);

  content::RenderProcessHost& extension2_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(extension2, extension2_process,
                                     {mojom::ContextType::kContentScript,
                                      mojom::ContextType::kPrivilegedExtension,
                                      mojom::ContextType::kOffscreenExtension},
                                     "extension2 page with extension2 passed");
  RunCanProcessHostContextTypeChecks(extension1, extension2_process,
                                     {mojom::ContextType::kContentScript},
                                     "extension2 page with extension1 passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, extension2_process, {},
      "extension2 page without extension passed");
}

// Tests that a web page with injected content scripts is not considered a
// privileged extension process.
IN_PROC_BROWSER_TEST_F(
    ProcessMapBrowserTest,
    IsPrivilegedExtensionProcess_WebPagesWithContentScripts) {
  const Extension* extension =
      AddExtensionWithContentScript("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  // Navigate to a web page and wait for the content script to inject.
  OpenDomainAndWaitForContentScript("example.com");

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

// Tests the type of contexts that can be hosted in a web page process that has
// had a content script injected in it.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_WebPagesWithContentScripts) {
  const Extension* extension =
      AddExtensionWithContentScript("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  // Navigate to a web page and wait for the content script to inject.
  OpenDomainAndWaitForContentScript("example.com");
  content::RenderProcessHost& page_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(extension, page_process,
                                     {mojom::ContextType::kContentScript},
                                     "web page with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, page_process,
      {mojom::ContextType::kWebPage, mojom::ContextType::kUntrustedWebUi},
      "web page without extension passed");
}

// The following defines a common test body used by the
// Sandboxed*Are*Isolated tests that follow. `frame_url` defines the page to
// be loaded, and may be an regular (http/s) page, a data url, or an
// about:srcdoc url. If it's about:srcdoc, the iframe srcdoc attribute will be
// used, and set to the value of `content`. `expect_isolated_from_each_other`
// indicates whether the subframes are expected to be isolated from each other,
// and if the sandboxed frame should have a sandboxed SiteInstance.
// `expect_sandboxed_subframe_isolated_from_extension_page` indicates we
// expect the sandboxed frame to be isolated from the extension mainframe,
// and `expect_non_sandboxed_subframe_isolated_from_extension_page` indicates
// that we expect the non-sandboxed subframe to be process isolated from the
// extension mainframe. This function is defined here to keep it close to the
// tests that use it, for easier reference.
void ProcessMapBrowserTest::VerifyWhetherSubframesAreIsolated(
    const GURL& frame_url,
    const std::string& content,
    bool expect_subframes_isolated_from_each_other,
    bool expect_sandboxed_subframe_isolated_from_extension_page,
    bool expect_non_sandboxed_subframe_isolated_from_extension_page) {
  const Extension* extension =
      AddExtensionWithSandboxedWebpage(frame_url, content);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("parent.html")));

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_child_frame =
      content::ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* non_sandboxed_child_frame =
      content::ChildFrameAt(main_frame, 1);

  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));

  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int sandboxed_frame_process_id = sandboxed_child_frame->GetProcess()->GetID();
  int non_sandboxed_frame_process_id =
      non_sandboxed_child_frame->GetProcess()->GetID();

  if (expect_subframes_isolated_from_each_other) {
    EXPECT_NE(sandboxed_frame_process_id, non_sandboxed_frame_process_id);
    EXPECT_TRUE(content::HasSandboxedSiteInstance(sandboxed_child_frame));
  } else {
    EXPECT_EQ(sandboxed_frame_process_id, non_sandboxed_frame_process_id);
    EXPECT_FALSE(content::HasSandboxedSiteInstance(sandboxed_child_frame));
  }
  if (expect_sandboxed_subframe_isolated_from_extension_page) {
    EXPECT_NE(main_frame_process_id, sandboxed_frame_process_id);
  } else {
    EXPECT_EQ(main_frame_process_id, sandboxed_frame_process_id);
  }
  if (expect_non_sandboxed_subframe_isolated_from_extension_page) {
    EXPECT_NE(main_frame_process_id, non_sandboxed_frame_process_id);
  } else {
    EXPECT_EQ(main_frame_process_id, non_sandboxed_frame_process_id);
  }
  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_FALSE(content::HasSandboxedSiteInstance(non_sandboxed_child_frame));
}

// Tests that web pages loaded in sandboxed iframes inside an extension are
// isolated from the extension and from non-sandboxed iframes of the same web
// origin, if IsolateSandboxedIframes is enabled. There are three variations,
// one for a web url, one for a data: url, and one for about:srcdoc.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedNonExtensionWebPagesAreIsolated) {
  GURL frame_url =
      embedded_test_server()->GetURL("example.com", "/simple.html");
  bool expect_subframes_isolated_from_each_other =
      content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  // The subframes should be cross-process to each other, and the sandboxed
  // frame should be in a sandboxed SiteInstance. Web-based content inside an
  // extension is always cross-process to the extension frame that contains it.
  VerifyWhetherSubframesAreIsolated(
      frame_url, /*content=*/std::string(),
      expect_subframes_isolated_from_each_other,
      /*expect_sandboxed_subframe_isolated_from_extension_page=*/true,
      /*expect_non_sandboxed_subframe_isolated_from_extension_page=*/true);
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedDataFramesAreMaybeIsolated) {
  GURL frame_url("data:text/html, foo");
  // Srcdoc/data-url content inside a sandboxed frame in an extension is
  // same-process to the extension frame that contains it, unless
  // IsolateSandboxedIframes is enabled, in which case it is cross-process.
  bool expect_subframes_isolated_from_each_other =
      content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  bool expect_sandboxed_subframe_isolated_from_extension_page =
      content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  VerifyWhetherSubframesAreIsolated(
      frame_url, /*content=*/std::string(),
      expect_subframes_isolated_from_each_other,
      expect_sandboxed_subframe_isolated_from_extension_page,
      /*expect_non_sandboxed_subframe_isolated_from_extension_page=*/false);
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedSrcdocFramesAreMaybeIsolated) {
  GURL frame_url("about:srcdoc");
  // Srcdoc/data-url content inside a sandboxed frame in an extension is
  // same-process to the extension frame that contains it, unless
  // IsolateSandboxedIframes is enabled, in which case it is cross-process.
  bool expect_subframes_isolated_from_each_other =
      content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  bool expect_sandboxed_subframe_isolated_from_extension_page =
      content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled();
  VerifyWhetherSubframesAreIsolated(
      frame_url, /*content=*/std::string("foo"),
      expect_subframes_isolated_from_each_other,
      expect_sandboxed_subframe_isolated_from_extension_page,
      /*expect_non_sandboxed_subframe_isolated_from_extension_page=*/false);
}

// Function implementation defined here to be close to the tests that use it.
void ProcessMapBrowserTest::
    VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess(
        base::cstring_view parent_script_template,
        const bool is_subframe_data_url,
        const bool expects_api_access) {
  const Extension* extension = AddExtensionWithResource();
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("parent.html")));

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  // Use JS to add content to the child frame.
  const std::string parent_script = base::StringPrintfNonConstexpr(
      parent_script_template.data(),
      extension->origin().GetURL().spec().c_str());
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(content::ExecJs(main_frame, parent_script));
  observer.Wait();

  content::RenderFrameHost* sandboxed_child_frame =
      content::ChildFrameAt(main_frame, 0);
  int sandboxed_frame_process_id = sandboxed_child_frame->GetProcess()->GetID();
  // Sandboxed extension frames should still have access to other extension
  // resources. Verify the extension script (resource.js) was properly loaded
  // by looking for foo variable.
  EXPECT_EQ("bar",
            content::EvalJs(sandboxed_child_frame, "foo;").ExtractString());
  // Sandboxed data and about:srcdoc frames, as well as manifest-sandboxed
  // extension pages, do not expect API access. As such, they are placed in
  // a non-privileged process. Extension pages that are sandboxed, but not
  // listed as sandboxed in the manifest, do get API access and are placed in an
  // privileged extension process.
  EXPECT_EQ(expects_api_access, process_map()->IsPrivilegedExtensionProcess(
                                    *extension, sandboxed_frame_process_id));

  // Verify expected api access.
  EXPECT_EQ(expects_api_access,
            FrameHasAccessToExtensionApis(sandboxed_child_frame));
}

// Tests that a data: url in a sandboxed frame in an extension still has access
// to resources.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedDataUrlStillHasAccessToExtensionResources) {
  // The %s in the string below will be filled in with the extension's origin by
  // VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess.
  std::string parent_script_template =
      R"(let test_frame = document.getElementById('test_frame');
      test_frame.src =
      'data:text/html, <script src="%sresource.js"></script>';)";
  VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess(
      parent_script_template, /*is_subframe_data_url=*/true,
      /*expects_api_access=*/false);
}

// Tests that a srcdoc in a sandboxed frame in an extension still has access to
// resources.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedSrcdocStillHasAccessToExtensionResources) {
  // The %s in the string below will be filled in with the extension's origin by
  // VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess.
  std::string parent_script_template =
      R"(let test_frame = document.getElementById('test_frame');
      test_frame.srcdoc = '<script src="%sresource.js"></script>';)";
  VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess(
      parent_script_template, /*is_subframe_data_url=*/false,
      /*expects_api_access=*/false);
}

// Tests that an extension page in a sandboxed frame in an extension still has
// access to resources.
IN_PROC_BROWSER_TEST_F(
    ProcessMapBrowserTest,
    SandboxedExtensionPageStillHasAccessToExtensionResources) {
  std::string parent_script_template =
      R"(let test_frame = document.getElementById('test_frame');
      test_frame.src = 'page_requesting_resource.html';)";
  VerifySandboxedSubframeHasResourceAccessButMaybeApiAccess(
      parent_script_template, /*is_subframe_data_url=*/false,
      /*expects_api_access=*/true);
}

// Tests that an extension inside a sandboxed subframe of another extension
// still has privileges. It will be process isolated regardless of the sandbox
// attribute since extensions are isolated from one another.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       SandboxedSubframeExtensionHasPrivilege) {
  std::pair<const Extension*, const Extension*> nested_extensions =
      AddNestedExtensions();
  const Extension* extension1 = nested_extensions.first;
  const Extension* extension2 = nested_extensions.second;
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(extension2);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension2->GetResourceURL("parent.html")));

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_child_frame =
      content::ChildFrameAt(main_frame, 0);

  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int sandboxed_frame_process_id = sandboxed_child_frame->GetProcess()->GetID();

  // Since we normally process-isolate E1 from E2, placing E1 in a sandboxed
  // iframe will make no difference.
  EXPECT_NE(main_frame_process_id, sandboxed_frame_process_id);
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension2, main_frame_process_id));
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension1, sandboxed_frame_process_id));
  // From an extensions point of view, applying 'sandbox' to the child iframe
  // in the manifest prevents it from having access to extension APIs, and
  // also places it in a non-privileged process if IsolateSandboxedFrames is
  // enabled.
  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_FALSE(ExtensionFrameIsSandboxed(sandboxed_child_frame));

  // Attempt to have `extension1` (in `sandboxed_child_frame`) load a
  // non-web-accessible resource from `extension2`. This should fail. The fact
  // that the child is sandboxed doesn't matter.
  GURL e2_private_page_url = extension2->GetResourceURL("private_page.html");
  const char kJsScript[] =
      R"(
        frm = document.createElement('iframe');
        frm.src = $1;
        document.body.appendChild(frm);
      )";
  content::TestNavigationObserver observer(GetActiveTab(), 1);
  EXPECT_TRUE(ExecJs(sandboxed_child_frame,
                     content::JsReplace(kJsScript, e2_private_page_url)));
  observer.Wait();

  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, observer.last_net_error_code());
  content::RenderFrameHost* grand_child_frame =
      content::ChildFrameAt(sandboxed_child_frame, 0);
  EXPECT_NE(nullptr, grand_child_frame);
  EXPECT_EQ(e2_private_page_url, grand_child_frame->GetLastCommittedURL());
}

// At present, the default mode is IsolatedSandboxedIframes mode (which isolates
// manifest-sandboxed extension pages in a different process that is not
// privileged). If there are multiple manifest-sandboxed extension pages,
// they will share a SiteInstance and non-privileged process. This test verifies
// that all manifest-sandboxed frames load into the same (non-privileged)
// process.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_SandboxedExtensionFrame) {
  const Extension* extension = AddExtensionWithSandboxedFrame();
  ASSERT_TRUE(extension);

  OpenExtensionPageWithSandboxedFrame(*extension);

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_frame =
      content::ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* other_sandboxed_frame =
      content::ChildFrameAt(main_frame, 1);

  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_TRUE(ExtensionFrameIsSandboxed(sandboxed_frame));
  EXPECT_TRUE(ExtensionFrameIsSandboxed(other_sandboxed_frame));

  int main_frame_process_id = main_frame->GetProcess()->GetID();
  int sandboxed_frame_process_id = sandboxed_frame->GetProcess()->GetID();
  int other_sandboxed_frame_process_id =
      other_sandboxed_frame->GetProcess()->GetID();

  // The two manifest-sandboxed frames will be in the same process, regardless
  // of whether IsolateSandboxedIframes is enabled or not.
  EXPECT_EQ(other_sandboxed_frame_process_id, sandboxed_frame_process_id);
  if (content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
    EXPECT_NE(main_frame_process_id, sandboxed_frame_process_id);
    EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
        *extension, sandboxed_frame_process_id));
  } else {
    EXPECT_EQ(main_frame_process_id, sandboxed_frame_process_id);
    EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
        *extension, sandboxed_frame_process_id));
  }

  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, main_frame_process_id));
}

// Test class to run tests both with and without sandboxing.
class ProcessMapAboutSrcdocBrowserTest
    : public ProcessMapBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ProcessMapAboutSrcdocBrowserTest() = default;
};

// This test verifies that an about:srcdoc frame with a non-extension parent
// cannot inherit an extension precursor origin that allows it to incorrectly
// access extension resources. The srcdoc frame should also not inherit the
// base URI of the extension.
IN_PROC_BROWSER_TEST_P(ProcessMapAboutSrcdocBrowserTest,
                       ExtensionCannotNavigateAboutSrcdocGrandchild) {
  bool srcdoc_is_sandboxed = GetParam();
  const Extension* extension =
      AddExtensionWithNonExtensionSubframeWithSrcdocSubframe(
          srcdoc_is_sandboxed);
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("parent.html")));

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* extension_frame =
      web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* non_extension_frame =
      content::ChildFrameAt(extension_frame, 0);
  content::RenderFrameHost* srcdoc_frame =
      content::ChildFrameAt(non_extension_frame, 0);

  // Verify that srcdoc frame has baseURI from it's parent.
  std::string extension_base_uri =
      EvalJs(extension_frame, "document.baseURI").ExtractString();
  std::string non_extension_base_uri =
      EvalJs(non_extension_frame, "document.baseURI").ExtractString();
  std::string srcdoc_base_uri =
      EvalJs(srcdoc_frame, "document.baseURI").ExtractString();
  EXPECT_EQ(non_extension_base_uri, srcdoc_base_uri);

  // Attempt to have `extension_frame` navigate the srcdoc frame to
  // about:srcdoc.
  content::TestNavigationObserver observer(web_contents, 1);
  EXPECT_TRUE(
      ExecJs(extension_frame, "frames[0][0].location.href = 'about:srcdoc';"));
  observer.Wait();

  // Verify that the srcdoc frame doesn't have the access to the extension's
  // origin, or any privileges.
  srcdoc_frame = content::ChildFrameAt(non_extension_frame, 0);
  std::string new_srcdoc_base_uri =
      EvalJs(srcdoc_frame, "document.baseURI").ExtractString();
  // The srcdoc gets a baseURI for an error page, but at least it's not the
  // extension's baseURI.
  EXPECT_NE(extension_base_uri, new_srcdoc_base_uri);
  EXPECT_FALSE(content::EvalJs(srcdoc_frame, "!!chrome && !!chrome.tabs;")
                   .ExtractBool());

  EXPECT_FALSE(process_map()->Contains(srcdoc_frame->GetProcess()->GetID()));

  // Make sure the resulting srcdoc frame cannot fetch() extension resources.
  // The only way `success` in the JS below can become true is if the fetch()
  // fails and the error is 'Failed to fetch'. If the fetch() succeeds and
  // a response is received, the test fails.
  const char jsTemplate[] = R"(
      (async () => {
         success = await fetch($1, { mode: 'no-cors'})
                             .then(response => { return false; })
                             .catch(err => {
                                 return (err instanceof TypeError) &&
                                        (err.message == 'Failed to fetch');
                              });
         return success;
      })();
  )";
  GURL json_resource_url = extension->GetResourceURL("data.json");
  EXPECT_TRUE(
      EvalJs(srcdoc_frame, content::JsReplace(jsTemplate, json_resource_url))
          .ExtractBool());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProcessMapAboutSrcdocBrowserTest,
    testing::Values(true, false),
    [](const testing::TestParamInfo<bool>& info) {
      bool srcdoc_is_sandboxed = info.param;
      std::string label = base::StringPrintf(
          "kBlockCrossOriginInitiatedAboutSrcdocNavigation_%s",
          srcdoc_is_sandboxed ? "Sandboxed" : "NotSandboxed");
      return label;
    });

// Tests the type of contexts that can be hosted in extension processes with
// a sandboxed process frame.
IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       CanHostContextType_SandboxedExtensionFrame) {
  const Extension* extension = AddExtensionWithSandboxedFrame();
  ASSERT_TRUE(extension);

  OpenExtensionPageWithSandboxedFrame(*extension);

  content::WebContents* web_contents = GetActiveTab();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* sandboxed_frame =
      content::ChildFrameAt(main_frame, 0);

  EXPECT_FALSE(ExtensionFrameIsSandboxed(main_frame));
  EXPECT_TRUE(ExtensionFrameIsSandboxed(sandboxed_frame));

  content::RenderProcessHost& main_frame_process = *main_frame->GetProcess();
  content::RenderProcessHost& sandboxed_frame_process =
      *sandboxed_frame->GetProcess();

  if (content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
    EXPECT_NE(main_frame_process.GetID(), sandboxed_frame_process.GetID());
  } else {
    EXPECT_EQ(main_frame_process.GetID(), sandboxed_frame_process.GetID());
  }

  RunCanProcessHostContextTypeChecks(
      extension, main_frame_process,
      {mojom::ContextType::kContentScript,
       mojom::ContextType::kPrivilegedExtension,
       mojom::ContextType::kOffscreenExtension},
      "main frame process with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, main_frame_process, {},
      "main frame process without extension passed");

  if (content::SiteIsolationPolicy::AreIsolatedSandboxedIframesEnabled()) {
    RunCanProcessHostContextTypeChecks(
        extension, sandboxed_frame_process,
        {mojom::ContextType::kContentScript},
        "sandboxed frame process with extension passed");
    RunCanProcessHostContextTypeChecks(
        nullptr, sandboxed_frame_process,
        {mojom::ContextType::kWebPage, mojom::ContextType::kUntrustedWebUi},
        "sandboxed frame process without extension passed");
  } else {
    RunCanProcessHostContextTypeChecks(
        extension, sandboxed_frame_process,
        {mojom::ContextType::kContentScript,
         mojom::ContextType::kPrivilegedExtension,
         mojom::ContextType::kOffscreenExtension},
        "sandboxed frame process with extension passed");
    RunCanProcessHostContextTypeChecks(
        nullptr, sandboxed_frame_process, {},
        "sandboxed frame process without extension passed");
  }
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_WebViews) {
  const Extension* extension = AddExtensionWithWebViewAndOpen();
  ASSERT_TRUE(extension);

  content::WebContents* embedder = GetAppWindowContents();
  ASSERT_TRUE(embedder);

  content::WebContents* webview = GetWebViewFromEmbedder(embedder);
  ASSERT_TRUE(webview);

  // The embedder (the app window) should be a privileged extension process,
  // but the webview should not.
  EXPECT_TRUE(process_map()->IsPrivilegedExtensionProcess(
      *extension, embedder->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, webview->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_WebViews) {
  const Extension* extension = AddExtensionWithWebViewAndOpen();
  ASSERT_TRUE(extension);

  content::WebContents* embedder = GetAppWindowContents();
  ASSERT_TRUE(embedder);

  content::WebContents* webview = GetWebViewFromEmbedder(embedder);
  ASSERT_TRUE(webview);

  // The embedder (the app window) can host any kind of extension context
  // except an unprivileged extension context (which is only available to
  // webviews).
  RunCanProcessHostContextTypeChecks(
      extension, *embedder->GetPrimaryMainFrame()->GetProcess(),
      {mojom::ContextType::kContentScript,
       mojom::ContextType::kPrivilegedExtension,
       mojom::ContextType::kOffscreenExtension},
      "embedder process");

  // The webview can only host content scripts, user scripts, and
  // unprivileged extension contexts (accessible resources).
  RunCanProcessHostContextTypeChecks(
      extension, *webview->GetPrimaryMainFrame()->GetProcess(),
      {mojom::ContextType::kContentScript,
       mojom::ContextType::kUnprivilegedExtension},
      "webview process with extension passed");

  // If the extension isn't associated with the call, the webview could only
  // possibly contain web pages and untrusted web ui.
  RunCanProcessHostContextTypeChecks(
      nullptr, *webview->GetPrimaryMainFrame()->GetProcess(),
      {mojom::ContextType::kWebPage, mojom::ContextType::kUntrustedWebUi},
      "webview process without extension passed");
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest,
                       IsPrivilegedExtensionProcess_UserScripts) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");
  ExecuteUserScriptInActiveTab(extension->id());

  EXPECT_FALSE(process_map()->IsPrivilegedExtensionProcess(
      *extension, GetActiveMainFrameProcessID()));
}

IN_PROC_BROWSER_TEST_F(ProcessMapBrowserTest, CanHostContextType_UserScripts) {
  const Extension* extension =
      AddExtensionWithHostPermission("test", "*://example.com/*");
  ASSERT_TRUE(extension);

  OpenDomain("example.com");
  ExecuteUserScriptInActiveTab(extension->id());

  content::RenderProcessHost& web_page_process = GetActiveMainFrameProcess();

  RunCanProcessHostContextTypeChecks(
      extension, web_page_process,
      {mojom::ContextType::kContentScript, mojom::ContextType::kUserScript},
      "page with injected user script with extension passed");
  RunCanProcessHostContextTypeChecks(
      nullptr, web_page_process,
      {mojom::ContextType::kWebPage, mojom::ContextType::kUntrustedWebUi},
      "page with injected user script without extension passed");
}

}  // namespace extensions
