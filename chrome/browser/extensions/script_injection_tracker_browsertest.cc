// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/script_injection_tracker.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions/permissions_test_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/version_info/channel.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/commit_message_delayer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/script_executor.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_content_script_load_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

// Asks the |extension_id| to inject |content_script| into |web_contents|.
void ExecuteProgrammaticContentScriptNoWait(content::WebContents* web_contents,
                                            const ExtensionId& extension_id,
                                            const std::string& content_script,
                                            const char* message) {
  // Build a script that executes the original `content_script` and then sends
  // an ack via `domAutomationController.send`.
  const char kAckingScriptTemplate[] = R"(
      %s;
      domAutomationController.send("%s");
  )";
  std::string acking_script = base::StringPrintf(
      kAckingScriptTemplate, content_script.c_str(), message);

  // Build a script to execute in the extension's background page.
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);
  std::string background_script = content::JsReplace(
      "chrome.tabs.executeScript($1, { code: $2 });", tab_id, acking_script);

  // Inject the script and wait for the ack.
  //
  // Note that using ExtensionTestMessageListener / `chrome.test.sendMessage`
  // (instead of DOMMessageQueue / `domAutomationController.send`) would have
  // hung in the ProgrammaticInjectionRacingWithDidCommit testcase.  The root
  // cause is not 100% understood, but it might be because the IPC related to
  // `chrome.test.sendMessage` can't be dispatched while running a nested
  // message loop while handling a DidCommit IPC.
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      web_contents->GetBrowserContext(), extension_id, background_script));
}

// Asks the |extension_id| to inject |content_script| into |web_contents| and
// waits until the script reports that it has finished executing.
void ExecuteProgrammaticContentScript(content::WebContents* web_contents,
                                      const ExtensionId& extension_id,
                                      const std::string& content_script) {
  content::DOMMessageQueue message_queue(web_contents);
  ExecuteProgrammaticContentScriptNoWait(
      web_contents, extension_id, content_script, "Hello from acking script!");
  std::string msg;
  EXPECT_TRUE(message_queue.WaitForMessage(&msg));
  EXPECT_EQ("\"Hello from acking script!\"", msg);
}

// Executes a `script` as a user script associated with the given `extension_id`
// within the primary main frame of `web_contents`, waiting for the injection to
// complete.
void ExecuteUserScript(content::WebContents& web_contents,
                       const ExtensionId& extension_id,
                       const std::string& script) {
  base::RunLoop run_loop;

  ScriptExecutor script_executor(&web_contents);
  std::vector<mojom::JSSourcePtr> sources;
  sources.push_back(mojom::JSSource::New(script, GURL()));
  script_executor.ExecuteScript(
      mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id),
      mojom::CodeInjection::NewJs(mojom::JSInjection::New(
          std::move(sources), mojom::ExecutionWorld::kUserScript,
          /*world_id=*/std::nullopt,
          blink::mojom::WantResultOption::kWantResult,
          blink::mojom::UserActivationOption::kDoNotActivate,
          blink::mojom::PromiseResultOption::kAwait)),
      ScriptExecutor::SPECIFIED_FRAMES, {ExtensionApiFrameIdMap::kTopFrameId},
      ScriptExecutor::DONT_MATCH_ABOUT_BLANK, mojom::RunLocation::kDocumentIdle,
      ScriptExecutor::DEFAULT_PROCESS, GURL() /* webview_src */,
      base::IgnoreArgs<std::vector<ScriptExecutor::FrameResult>>(
          run_loop.QuitWhenIdleClosure()));

  run_loop.Run();
}

// Test suite covering `extensions::ScriptInjectionTracker` from
// //extensions/browser/script_injection_tracker.h.
//
// See also ContentScriptMatchingBrowserTest in
// //extensions/browser/content_script_matching_browsertest.cc.
class ScriptInjectionTrackerBrowserTest : public ExtensionBrowserTest {
 public:
  ScriptInjectionTrackerBrowserTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  // Returns the current active web contents.
  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Navigates to url for given `hostname` and `relative_url`. Returns whether
  // the navigation is in a new process compared to the currently active tab.
  [[nodiscard]] bool NavigateToURLInNewProcess(std::string_view hostname,
                                               std::string_view relative_url) {
    content::WebContents* original_web_contents = GetActiveWebContents();

    // Opening the URL in a new tab should force it into a new process.
    GURL url = embedded_test_server()->GetURL(hostname, relative_url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    content::WebContents* new_web_contents = GetActiveWebContents();
    return original_web_contents->GetPrimaryMainFrame()->GetProcess() !=
           new_web_contents->GetPrimaryMainFrame()->GetProcess();
  }
};

// Helper class for executing a content script right before handling a DidCommit
// IPC.
class ContentScriptExecuterBeforeDidCommit {
 public:
  ContentScriptExecuterBeforeDidCommit(const GURL& postponed_commit_url,
                                       content::WebContents* web_contents,
                                       const ExtensionId& extension_id,
                                       const std::string& content_script)
      : message_queue_(web_contents),
        commit_delayer_(
            web_contents,
            postponed_commit_url,
            base::BindOnce(
                &ContentScriptExecuterBeforeDidCommit::ExecuteContentScript,
                base::Unretained(web_contents),
                extension_id,
                content_script)) {}

  void WaitForMessage() {
    std::string msg;
    EXPECT_TRUE(message_queue_.WaitForMessage(&msg));
    EXPECT_EQ("\"Hello from acking script!\"", msg);
  }

 private:
  static void ExecuteContentScript(content::WebContents* web_contents,
                                   const ExtensionId& extension_id,
                                   const std::string& content_script,
                                   content::RenderFrameHost* ignored) {
    ExecuteProgrammaticContentScriptNoWait(web_contents, extension_id,
                                           content_script,
                                           "Hello from acking script!");
  }

  content::DOMMessageQueue message_queue_;
  content::CommitMessageDelayer commit_delayer_;
};

// Tests tracking of content scripts injected/declared via
// `chrome.scripting.executeScript` API.  See also:
// https://developer.chrome.com/docs/extensions/mv3/content_scripts/#programmatic
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest,
                       ProgrammaticContentScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Programmatic",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an arbitrary, mostly-empty test page.
  GURL page_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Verify that initially no processes show up as having been injected with
  // content scripts.
  content::WebContents* web_contents = GetActiveWebContents();
  content::RenderFrameHost* background_frame =
      ProcessManager::Get(browser()->profile())
          ->GetBackgroundHostForExtension(extension->id())
          ->main_frame_host();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));

  // Programmatically inject a content script.
  const char kContentScript[] = R"(
      document.body.innerText = 'content script has run';
  )";
  ExecuteProgrammaticContentScript(web_contents, extension->id(),
                                   kContentScript);

  // Verify that the right processes show up as having been injected with
  // content scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  // Sanity check: injecting a content script should not count as injecting a
  // user script.
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  // And the extension page should never be considered as a content script
  // target.
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));

  // Navigate to a different same-site document and verify if
  // ScriptInjectionTracker still thinks that content scripts have been
  // injected.
  //
  // DidProcessRunContentScriptFromExtension is expected to return true, because
  // content scripts have been injected into the renderer process in the *past*,
  // even though the *current* set of documents hosted in the renderer process
  // have not run a content script.
  GURL new_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
  EXPECT_EQ("This page has a title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *background_frame->GetProcess(), extension->id()));
}

// Tests tracking of user scripts through the ScriptExecutor.
// The vast majority of implementation is the same for content script and user
// script tracking, so this is the main spot we explicitly test user script
// specific tracking.
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest,
                       ProgrammaticUserScript) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  // TODO(crbug.com/40262660): There's currently no way for extensions
  // to trigger user script injections, so this extension is really just to
  // have one we force to be associated with the injection. When the userScripts
  // API is fully developed, we should update this to use the developer-facing
  // API.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Programmatic",
        "version": "1.0",
        "manifest_version": 3,
        "host_permissions": ["<all_urls>"]
      } )";
  dir.WriteManifest(kManifestTemplate);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an arbitrary, mostly-empty test page.
  GURL page_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));

  // Verify that initially no processes show up as having been injected with
  // user scripts.
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Programmatically inject a user script.
  static constexpr char kUserScript[] =
      "document.body.innerText = 'user script has run';";
  ExecuteUserScript(*web_contents, extension->id(), kUserScript);

  // Verify that the right processes show up as having been injected with
  // content scripts.
  EXPECT_EQ("user script has run",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  // Sanity check: injecting a user script should not count as injecting a
  // content script.
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a different same-site document and verify if
  // ScriptInjectionTracker still thinks that user scripts have been injected.
  //
  // DidProcessRunUserScriptFromExtension is expected to return true, because
  // user scripts have been injected into the renderer process in the *past*,
  // even though the *current* set of documents hosted in the renderer process
  // have not run a user script.
  GURL new_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
  EXPECT_EQ("This page has a title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests what happens when the ExtensionMsg_ExecuteCode is sent *after* sending
// a Commit IPC to the renderer (i.e. after ReadyToCommit) but *before* a
// corresponding DidCommit IPC has been received by the browser process.  See
// also the "DocumentUserData race w/ Commit IPC" section in the
// document here:
// https://docs.google.com/document/d/1MFprp2ss2r9RNamJ7Jxva1bvRZvec3rzGceDGoJ6vW0/edit#heading=h.n2ppjzx4jpzt
// TODO(crbug.com/40615943): Remove the test after RenderDocument is shipped.
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest,
                       ProgrammaticInjectionRacingWithDidCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The test assumes the RenderFrame stays the same after navigation. Disable
  // back/forward cache to ensure that RenderFrame swap won't happen.
  content::DisableBackForwardCacheForTesting(
      GetActiveWebContents(),
      content::BackForwardCache::TEST_ASSUMES_NO_RENDER_FRAME_CHANGE);
  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - DidCommit race",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an arbitrary, mostly-empty test page.
  GURL page_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), page_url));
  content::WebContents* web_contents = GetActiveWebContents();

  // Programmatically inject a content script between ReadyToCommit and
  // DidCommit events.
  {
    GURL new_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    ContentScriptExecuterBeforeDidCommit content_script_executer(
        new_url, web_contents, extension->id(),
        "document.body.innerText = 'content script has run'");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), new_url));
    content_script_executer.WaitForMessage();
  }

  // Verify that the process shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests tracking of content scripts injected/declared via `content_scripts`
// entry in the extension manifest.  See also:
// https://developer.chrome.com/docs/extensions/mv3/content_scripts/#static-declarative
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest,
                       ContentScriptDeclarationInExtensionManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
          document.body.innerText = 'content script has run';
          chrome.test.sendMessage('Hello from content script!');
      )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that is *not* covered by `content_scripts.matches`
  // manifest entry above.
  GURL ignored_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));
  content::WebContents* first_tab = GetActiveWebContents();

  // Verify that initially no processes show up as having been injected with
  // content scripts.
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    ASSERT_TRUE(NavigateToURLInNewProcess("bar.com", "/title1.html"));
    ASSERT_TRUE(listener.WaitUntilSatisfied());

    // Verify that content script has been injected.
    content::WebContents* second_tab = GetActiveWebContents();
    EXPECT_EQ("content script has run",
              content::EvalJs(second_tab, "document.body.innerText"));

    // Verify that ScriptInjectionTracker detected the injection.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }

  // Verify that the initial tab still is still correctly absent from
  // ScriptInjectionTracker.
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Ensure ScriptInjectionTracker correctly tracks script injections in frames
// which undergo non-network (i.e. no ReadyToCommitNavigation notification)
// navigations after an extension is loaded.  For more details about the
// particular race condition covered by this test please see
// https://docs.google.com/document/d/1Z0-C3Bstva_-NK_bKhcyj4f2kdWjXv8pscuHre7UlSk/edit?usp=sharing
IN_PROC_BROWSER_TEST_F(
    ScriptInjectionTrackerBrowserTest,
    AboutBlankNavigationAfterLoadingExtensionMidwayThroughTest) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry below (the extension is *not* installed at this point yet).
  GURL injected_url =
      embedded_test_server()->GetURL("example.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), injected_url));
  content::WebContents* first_tab = GetActiveWebContents();

  // Create the test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://example.com/*"],
          "js": ["content_script.js"],
          "run_at": "document_end"
        }],
        "background": {"scripts": ["background_script.js"]}
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), "");
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
          document.body.innerText = 'content script has run';
          chrome.test.sendMessage('Hello from content script!');
      )");

  // Load the test extension.  Note that the LoadExtension call below will
  // internally wait for content scripts to be sent to the renderer processes
  // (see ContentScriptLoadWaiter usage in the WaitForExtensionReady method).
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  std::string extension_id = extension->id();

  // Open a new tab with 'about:blank'.  This may be tricky, because 1) the
  // initial empty document commits synchronously, without going through
  // ReadyToCommit step and 2) when this test was being written, the initial
  // 'about:blank' did not send a DidCommit IPC to the Browser process.
  ExtensionTestMessageListener listener("Hello from content script!");
  content::WebContentsAddedObserver popup_observer;
  ExecuteScriptAsync(first_tab, "window.open('about:blank', '_blank')");

  // Verify that the content script has been run.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  content::WebContents* popup = popup_observer.GetWebContents();
  EXPECT_EQ("content script has run",
            content::EvalJs(popup, "document.body.innerText"));

  // Verify that content script didn't run in the opener.  This mostly verifies
  // the test setup/steps.
  EXPECT_NE("content script has run",
            content::EvalJs(first_tab, "document.body.innerText"));

  // Verify that ScriptInjectionTracker correctly says that a content script has
  // been run in the `popup`.  This verifies product code - this is the main
  // verification in this test.
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *popup->GetPrimaryMainFrame()->GetProcess(), extension_id));
}

// Covers detecting content script injection into a 'data:...' URL.
IN_PROC_BROWSER_TEST_F(
    ScriptInjectionTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_DataUrlIframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 3,
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "match_origin_as_fallback": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  content::WebContents* first_tab = nullptr;
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), injected_url));

    // Verify that content script has been injected.
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    first_tab = GetActiveWebContents();
    EXPECT_EQ("content script has run",
              content::EvalJs(first_tab, "document.body.innerText"));

    // Verify that ScriptInjectionTracker detected the injection.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }

  // Add a new subframe with a `data:...` URL.  This will verify that the
  // browser-side ScriptInjectionTracker correctly accounts for the
  // renderer-side support for injecting contents scripts into data: URLs (see
  // r793302).
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    const char kScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.src = 'data:text/html,contents';
        document.body.appendChild(iframe);
    )";
    ExecuteScriptAsync(first_tab, kScript);

    // Verify that content script has been injected.
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    content::RenderFrameHost* main_frame = first_tab->GetPrimaryMainFrame();
    content::RenderFrameHost* child_frame =
        content::ChildFrameAt(main_frame, 0);
    ASSERT_TRUE(child_frame);
    EXPECT_EQ("content script has run",
              content::EvalJs(main_frame, "document.body.innerText"));
    EXPECT_EQ("content script has run",
              content::EvalJs(child_frame, "document.body.innerText"));

    // Verify that ScriptInjectionTracker properly covered the new child frame
    // (and continues to correctly cover the initial frame).
    //
    // The verification below is a bit redundant, because `main_frame` and
    // `child_frame` are currently hosted in the same process, but this kind of
    // verification is important if 1( we ever consider going back to per-frame
    // tracking or 2) we start isolating opaque-origin/sandboxed frames into a
    // separate process (tracked in https://crbug.com/510122).
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *main_frame->GetProcess(), extension->id()));
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *child_frame->GetProcess(), extension->id()));
  }
}

// Covers detecting content script injection into 'about:blank'.
IN_PROC_BROWSER_TEST_F(
    ScriptInjectionTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_AboutBlankPopup) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  content::WebContents* first_tab = nullptr;
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), injected_url));

    // Verify that content script has been injected.
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    first_tab = GetActiveWebContents();
    EXPECT_EQ("content script has run",
              content::EvalJs(first_tab, "document.body.innerText"));

    // Verify that ScriptInjectionTracker properly covered the initial frame.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }

  // Open a new tab with 'about:blank'.  This may be tricky, because the initial
  // 'about:blank' navigation will not go through ReadyToCommit state.
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(first_tab, "window.open('about:blank', '_blank')"));
    content::WebContents* popup = popup_observer.GetWebContents();
    WaitForLoadStop(popup);

    // Verify that content script has been injected.
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("content script has run",
              content::EvalJs(first_tab, "document.body.innerText"));
    EXPECT_EQ("content script has run",
              content::EvalJs(popup, "document.body.innerText"));

    // Verify that ScriptInjectionTracker properly covered the popup (and
    // continues to correctly cover the initial frame).  The verification below
    // is a bit redundant, because `first_tab` and `popup` are hosted in the
    // same process, but this kind of verification is important if we ever
    // consider going back to per-frame tracking.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *popup->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }
}

// Covers detecting content script injection into an initial empty document.
//
// The code below exercises the test steps from "scenario #3" from the "Tracking
// injections in an initial empty document" section of a @chromium.org document
// here:
// https://docs.google.com/document/d/1MFprp2ss2r9RNamJ7Jxva1bvRZvec3rzGceDGoJ6vW0/edit?usp=sharing
IN_PROC_BROWSER_TEST_F(
    ScriptInjectionTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_SubframeWithInitialEmptyDoc) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://bar.com/title1.html"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
      var counter = 0;
      function leaveContentScriptMarker() {
          const kExpectedText = 'content script has run: ';
          if (document.body.innerText.startsWith(kExpectedText))
            return;

          counter += 1;
          document.body.innerText = kExpectedText + counter;
          chrome.test.sendMessage('Hello from content script!');
      }

      // Leave a content script mark *now*.
      leaveContentScriptMarker();

      // Periodically check if the mark needs to be reinserted (with a new value
      // of `counter`).  This helps to demonstrate (in a test step somewhere
      // below) that the content script "survives" a `document.open` operation.
      setInterval(leaveContentScriptMarker, 100);  )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that *is* covered by `content_scripts.matches`
  // manifest entry above.
  content::WebContents* first_tab = nullptr;
  {
    GURL injected_url =
        embedded_test_server()->GetURL("bar.com", "/title1.html");
    ExtensionTestMessageListener listener("Hello from content script!");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), injected_url));

    // Verify that content script has been injected.
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    first_tab = GetActiveWebContents();
    EXPECT_EQ("content script has run: 1",
              content::EvalJs(first_tab, "document.body.innerText"));

    // Verify that ScriptInjectionTracker properly covered the initial frame.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }

  // Add a new subframe with `src=javascript:...` attribute.  This will leave
  // the subframe at the initial empty document (no navigation / no
  // ReadyToCommit), but still end up injecting the content script.
  //
  // (This is "Step 1" from the doc linked in the comment right above
  // IN_PROC_BROWSER_TEST_F.)
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    const char kScript[] = R"(
        let iframe = document.createElement('iframe');
        iframe.name = 'test-child-frame';
        iframe.src = 'javascript:"something"';
        document.body.appendChild(iframe);
    )";
    ExecuteScriptAsync(first_tab, kScript);
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify expected properties of the test scenario - the `child_frame` should
  // have stayed at the initial empty document.
  content::RenderFrameHost* main_frame = first_tab->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(child_frame);
  EXPECT_EQ(main_frame->GetLastCommittedOrigin().Serialize(),
            content::EvalJs(child_frame, "origin"));
  // Renderer-side and browser-side do not exactly agree on the URL of the child
  // frame...
  EXPECT_EQ("about:blank", content::EvalJs(child_frame, "location.href"));
  EXPECT_EQ(GURL(), child_frame->GetLastCommittedURL());

  // Verify that ScriptInjectionTracker properly covered the new child frame
  // (and continues to correctly cover the initial frame).  The verification
  // below is a bit redundant, because `main_frame` and `child_frame` are hosted
  // in the same process, but this kind of verification is important if we ever
  // consider going back to per-frame tracking.
  EXPECT_EQ("content script has run: 1",
            content::EvalJs(main_frame, "document.body.innerText"));
  EXPECT_EQ("content script has run: 1",
            content::EvalJs(child_frame, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *child_frame->GetProcess(), extension->id()));

  // Execute `document.open()` on the initial empty document child frame.  The
  // content script injected previously will survive this (event listeners are
  // reset but the `setInterval` callback keeps executing).
  //
  // This step changes the URL of the `child_frame` (in a same-document
  // navigation) from "about:blank" to a URL that (unlike the parent) is no
  // longer covered by the `matches` patterns from the extension manifest.
  {
    // Inject a new frame to execute `document.open` from.
    //
    // (This is "Step 2" from the doc linked in the comment right above
    // IN_PROC_BROWSER_TEST_F.)
    content::TestNavigationObserver nav_observer(first_tab, 1);
    const char kFrameInsertingScriptTemplate[] = R"(
        var f = document.createElement('iframe');
        f.src = $1;
        document.body.appendChild(f);
    )";
    GURL non_injected_url =
        embedded_test_server()->GetURL("bar.com", "/title2.html");
    ASSERT_TRUE(content::ExecJs(
        main_frame,
        content::JsReplace(kFrameInsertingScriptTemplate, non_injected_url)));
    nav_observer.Wait();
  }
  content::RenderFrameHost* another_frame =
      content::ChildFrameAt(main_frame, 1);
  ASSERT_TRUE(another_frame);
  {
    // Execute `document.open`.
    //
    // (This is "Step 3" from the doc linked in the comment right above
    // IN_PROC_BROWSER_TEST_F.)
    ExtensionTestMessageListener listener("Hello from content script!");
    const char kDocumentWritingScript[] = R"(
        var win = window.open('', 'test-child-frame');
        win.document.open();
        win.document.close();
    )";
    ASSERT_TRUE(content::ExecJs(another_frame, kDocumentWritingScript));

    // Demonstrate that the original content script has survived "resetting" of
    // the document.  (document.open/write/close triggers a same-document
    // navigation - it keeps the document/window/RenderFrame[Host];  OTOH we use
    // setInterval because it is one of few things that survive across such
    // boundary - in particular all event listeners will be reset.)
    ASSERT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("content script has run: 2",
              content::EvalJs(child_frame, "document.body.innerText"));

    // Demonstrate that `document.open` didn't change the URL of the
    // `child_frame`.
    EXPECT_EQ(another_frame->GetLastCommittedURL(),
              content::EvalJs(child_frame, "location.href"));
    EXPECT_EQ(GURL(), child_frame->GetLastCommittedURL());
  }

  // Verify that ScriptInjectionTracker still properly covers both frames.  The
  // verification below is a bit redundant, because `main_frame` and
  // `child_frame` are hosted in the same process, but this kind of verification
  // is important if we ever consider going back to per-frame tracking.
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *child_frame->GetProcess(), extension->id()));
}

// This is a regression test for https://crbug.com/1312125 - it simulates a race
// where an extension is loaded during or before a navigation, resulting in
// ScriptInjectionTracker::DidUpdateContentScriptsInRenderer getting called
// between ReadyToCommit and DidCommit of a navigation from a page where content
// scripts are not injected, to a page where content scripts are injected.
IN_PROC_BROWSER_TEST_F(
    ScriptInjectionTrackerBrowserTest,
    ContentScriptDeclarationInExtensionManifest_ScriptLoadRacesWithDidCommit) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a test page that is *not* covered by `content_scripts.matches`
  // manifest entry used in this test (see `kManifestTemplate` below).
  GURL ignored_url =
      embedded_test_server()->GetURL("foo.test.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));
  content::WebContents* web_contents = GetActiveWebContents();

  // The test uses a long-running `pagehide` handler to postpone DidCommit in a
  // same-process, cross-origin navigation that happens in the next test steps:
  // - "cross-origin" aspect is needed because we need to navigate from a page
  //   not covered by content scripts, into a page covered by content scripts +
  //   because ScriptInjectionTracker ignores the path part of URL patterns
  //   (e.g. calling `MatchesSecurityOrigin()`).
  // - "same-process" aspect is needed because we need a same-process navigation
  //   in order to postpone DidCommit IPC (by having an long-running pagehide
  //   handler).  In a typical desktop setting same-site navigations should be
  //   same-process.
  const char kPagehideHandlerInstallationScript[] = R"(
      window.addEventListener('pagehide', function(event) {
          // BAD CODE - please don't copy&paste.  See below for an explanation
          // why there doesn't seem to a better approach *here* (i.e. see the
          // comment in a section titled "Orchestrate the race condition").
          const sleep_duration = 3000;  // milliseconds
          const start = new Date().getTime();
          do {
            var now = new Date().getTime();
          } while (now < (start + sleep_duration));
      });
  )";
  ASSERT_TRUE(
      content::ExecJs(web_contents, kPagehideHandlerInstallationScript));

  // Prepare a test directory, but don't install an extension just yet.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "match_about_blank": true,
          "matches": ["*://bar.test.com/*"],
          "js": ["content_script.js"]
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
          document.body.innerText = 'content script has run';
          chrome.test.sendMessage('Hello from content script!');
      )");
  base::FilePath unpacked_path = dir.UnpackedPath();

  // *Initiate* navigation to a test page that *is* covered by
  // `content_scripts.matches` manifest entry above and use `navigation_manager`
  // to wait until ReadyToCommit happens,
  GURL injected_url =
      embedded_test_server()->GetURL("bar.test.com", "/title1.html");
  content::TestNavigationManager navigation_manager(web_contents, injected_url);
  bool did_commit_has_happened = false;
  content::CommitMessageDelayer commit_delayer(
      web_contents, injected_url,
      base::BindLambdaForTesting([&](content::RenderFrameHost* frame) {
        // Race step UI.3b (see below).
        did_commit_has_happened = true;
      }));
  ExtensionTestMessageListener listener("Hello from content script!");
  ASSERT_TRUE(
      content::BeginNavigateToURLFromRenderer(web_contents, injected_url));

  // Orchestrate the race condition:
  // *) Race step UI.1: UI thread:
  //      *) UI.1.1: NavigationThrottle pauses the navigation just *before*
  //         ReadyToCommit notifications (when test calls
  //         TestNavigationManager::WaitForResponse).
  //      *) UI.1.2: UI thread: Navigation resumes (when test calls
  //         TestNavigationManager::ResumeNavigation) and
  //         ScriptInjectionTracker::ReadyToCommitNavigation gets called.
  //      *) UI.1.3: UI thread: Loading of the Chrome Extension starts (when
  //         test calls LoadExtension).
  // *) Parallel steps:
  //     *) Race step FILE.2: FILE thread: Extension and its content scripts
  //        continue loading (triggered by step UI.1.3 above; see for example
  //        LoadScriptsOnFileTaskRunner in e/b/extension_user_script_loader.cc).
  //        This is a simplification - loading of content scripts is just *one*
  //        of multiple potential thread hops involved in loading an extension.
  //     *) Race step RENDERER.2: Commit IPC is received and handled:
  //          *) RENDERER.2.1, `pagehide` handler runs
  //          *) RENDERER.2.???, Renderer is notified about newly loaded
  //             extension and its content scripts
  //          *) RENDERER.2.8, `DidCommit` is sent back to the Browser
  //          *) RENDERER.2.9, Content script gets injected (hopefully,
  //             depending on whether step "RENDERER.2.???" happened before)
  // *) Racey steps where ordering matters for the repro, but where the test
  //    doesn't guarantee the ordering between UI.3a and UI.3b:
  //     *) Race step UI.3a: Task posted by FILE.2 gets run on UI thread.
  //        ScriptInjectionTracker::DidUpdateContentScriptsInRenderer get
  //        called.
  //     *) Race step UI.3b: Task posted by IO.2 gets run on UI thread.
  //        DidCommit happens.
  // *) Non-racey step UI.4: UI thread: IPC from the content script is
  //    processed.  The test simulates this by explicitly calling and checking
  //    ScriptInjectionTracker::DidProcessRunContentScriptFromExtension which in
  //    presence of https://crbug.com/1312125 could have incorrectly returned
  //    false.
  //
  // Triggering https://crbug.com/1312125 requires that UI.3a happens before
  // UI.3b - when this happens then ScriptInjectionTracker's
  // DidUpdateContentScriptsInRenderer won't see the newly committed URL and
  // won't realize that content script may be injected into the newly committed
  // document (the fix is to add ScriptInjectionTracker::DidFinishNavigation).
  // Additionally, the repro requires that RENDERER.2.??? happens before the
  // Renderer commits the page.
  //
  // The test doesn't guarantee the ordering of UI.3a and UI.3b, but the desired
  // ordering does happen in practice when running this test (the time from UI.1
  // to UI.3a is around 30 milliseconds which is much shorter than 3000
  // milliseconds used by the `pagehide` handler).  This is already sufficient
  // and helpful for verifying the fix for the product code.  This is not ideal,
  // but making the test more robust seems quite difficult - see the discussion
  // in
  // https://chromium-review.googlesource.com/c/chromium/src/+/3587823/8#message-b4f0abdcc2a6cedf681d33dbe1ddbccc381ad932
  ASSERT_TRUE(navigation_manager.WaitForResponse());          // Step UI.1.1
  navigation_manager.ResumeNavigation();                      // Step UI.1.2
  const Extension* extension = LoadExtension(unpacked_path);  // Step UI.1.3
  ASSERT_TRUE(extension);
  commit_delayer.Wait();                           // Step UI.3b - part1
  ASSERT_TRUE(
      navigation_manager.WaitForNavigationFinished());  // Step UI.3b - part2
  ASSERT_TRUE(listener.WaitUntilSatisfied());      // Step UI.4

  // Verify that content script has been injected.
  EXPECT_EQ("content script has run",
            content::EvalJs(web_contents, "document.body.innerText"));

  // MAIN VERIFICATION: Verify that ScriptInjectionTracker detected the
  // injection.
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests tracking of content scripts injected/declared via
// `chrome.declarativeContent` API. See also:
// https://developer.chrome.com/docs/extensions/reference/declarativeContent/#type-RequestContentScript
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest,
                       ContentScriptViaDeclarativeContentApi) {
#if BUILDFLAG(IS_MAC)
  GTEST_SKIP() << "Very flaky on Mac; https://crbug.com/1311017";
#else
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>", "declarativeContent" ],
        "background": {"scripts": ["background_script.js"]}
      } )";
  const char kBackgroundScript[] = R"(
      var rule = {
        conditions: [
          new chrome.declarativeContent.PageStateMatcher({
            pageUrl: { hostEquals: 'bar.com', schemes: ['http', 'https'] }
          })
        ],
        actions: [ new chrome.declarativeContent.RequestContentScript({
          js: ["content_script.js"]
        }) ]
      };

      chrome.runtime.onInstalled.addListener(function(details) {
          chrome.declarativeContent.onPageChanged.addRules([rule]);
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroundScript);
  const char kContentScript[] = R"(
      function sendResponse() {
          document.body.innerText = 'content script has run';
          chrome.test.sendMessage('Hello from content script!');
      }
      if (document.readyState === 'complete')
          sendResponse();
      else
          window.onload = sendResponse;
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that is *not* covered by the PageStateMatcher used
  // above.
  GURL ignored_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));

  // Verify that initially no frames show up as having been injected with
  // content scripts.
  content::WebContents* first_tab = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a test page that *is* covered by the PageStateMatcher above.
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    ASSERT_TRUE(NavigateToURLInNewProcess("bar.com", "/title1.html"));
    ASSERT_TRUE(listener.WaitUntilSatisfied());

    // Verify that content script has been injected.
    content::WebContents* second_tab = GetActiveWebContents();
    EXPECT_EQ("content script has run",
              content::EvalJs(second_tab, "document.body.innerText"));

    // Verify that ScriptInjectionTracker detected the injection.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  }

  // Verify that still no content script has been run in the `first_tab`.
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
#endif  // BUILDFLAG(IS_MAC)
}

IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerBrowserTest, HistoryPushState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Declarative",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "matches": ["*://bar.com/pushed_url.html"],
          "js": ["content_script.js"],
          "run_at": "document_end"
        }]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
                document.body.innerText = 'content script has run';
                chrome.test.sendMessage('Hello from content script!'); )");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a test page that is *not* covered by the URL patterns above,
  // but that immediately executes `history.pushState` that changes the URL
  // to one that *is* covered by the URL patterns above.
  GURL url =
      embedded_test_server()->GetURL("bar.com", "/History/push_state.html");
  ExtensionTestMessageListener listener("Hello from content script!");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Verify that content script has been injected.
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  content::RenderFrameHost* main_frame = browser()
                                             ->tab_strip_model()
                                             ->GetActiveWebContents()
                                             ->GetPrimaryMainFrame();
  EXPECT_EQ("content script has run",
            content::EvalJs(main_frame, "document.body.innerText"));

  // Verify that ScriptInjectionTracker detected the injection.
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *main_frame->GetProcess(), extension->id()));
}

class DynamicScriptsTrackerBrowserTest
    : public ScriptInjectionTrackerBrowserTest {
 public:
  DynamicScriptsTrackerBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kProcessPerSiteUpToMainFrameThreshold);
  }

 private:
  ScopedCurrentChannel current_channel_{version_info::Channel::UNKNOWN};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests tracking of content scripts dynamically injected/declared via
// `chrome.scripting` API.
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest,
                       ContentScriptViaScriptingApi) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - ScriptingAPI",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting" ],
        "host_permissions": ["*://*/*"],
        "background": { "service_worker": "worker.js" }
      } )";
  const char kWorkerScript[] = R"(
      var scripts = [{
        id: 'script1',
        matches: ['*://a.com/*'],
        js: ['content_script.js'],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(function(details) {
        chrome.scripting.registerContentScripts(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
      window.onload = function() {
          chrome.test.assertEq('complete', document.readyState);
          document.body.innerText = 'content script has run';
          chrome.test.notifyPass();
      }
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Navigate to a test page that is *not* covered by the dynamic content script
  // used above.
  GURL ignored_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));

  // Verify that initially no frames show up as having been injected with
  // content scripts.
  content::WebContents* first_tab = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a test page that *is* covered by the dynamic content script
  // above.
  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURLInNewProcess("a.com", "/title1.html"));
  ASSERT_TRUE(catcher.GetNextResult());
  content::WebContents* second_tab = GetActiveWebContents();

  // Verify that the new tab shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests tracking of content scripts dynamically injected/declared via
// `chrome.scripting` API only when extension requests host permissions.
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest,
                       ContentScriptViaScriptingApi_HostPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install an extension with a content script that wants to inject in all
  // sites but extension only requests 'requested.com' host permissions.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptingAPI - host permissions",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting" ],
        "host_permissions": ["*://requested.com/*"],
        "background": { "service_worker": "worker.js" }
      } )";
  const char kWorkerScript[] = R"(
      var scripts = [{
        id: 'script1',
        matches: ['<all_urls>'],
        js: ['content_script.js'],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(function(details) {
        chrome.scripting.registerContentScripts(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
      document.body.innerText = 'content script has run';
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Navigate to a test page that is not in the extension's host permissions.
  GURL ignored_url =
      embedded_test_server()->GetURL("non-requested.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));

  // Verify that initially no frames show up as having been injected with
  // content scripts.
  content::WebContents* first_tab = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a page that is in the extension's host permission and is in the
  // content script 'matches'.
  ASSERT_TRUE(NavigateToURLInNewProcess("requested.com", "/title1.html"));
  content::WebContents* second_tab = GetActiveWebContents();

  // Verify that the new tab shows up as having been injected with content
  // scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Regression test for https://crbug.com/1439642.
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest,
                       ContentScriptViaScriptingApiWhileIdle) {
  // The test orchestrates the following sequence of events.
  //
  // Step 1: `DidFinishNavigation` for a.com/controllable_request.html
  //         - At this point DOMContentLoaded will not happen yet because
  //           we use `ControllableHttpResponse`.
  //         - At this point `ScriptInjectionTracker::DidFinishNavigation`
  //           will be called (and we want that to happen before step 2, because
  //           we want to prevent `ScriptInjectionTracker` from relying on
  //           `DidFinishNavigation` to learn about newly registered content
  //           scripts)
  //
  // Step 2: `chrome.scripting.registerContentScripts`
  //         - registering content script injection for a.com
  //         - when the script gets loaded (step 2b)
  //           `ScriptInjectionTracker::DidUpdateContentScriptsInRenderer` will
  //           be called (but as described in https://crbug.com/1439642 there
  //           may be trouble with seeing the newly registered scripts)
  //
  // Step 3: DOMContentLoaded
  //         - Triggered by `controllable_request.Done()`
  //         - This enables injecting the content script (at `document_end`)
  //
  // Step 4: Content script gets injected
  //
  // Step 5: Verification if `ScriptInjectionTracker` understands that the
  // content script has been injected.

  // Set up ControllableHttpResponse to control the timing of the navigation
  // (and therefore to control the timing of the "DOMContentLoaded" event
  // and therefore the timing of content script injection).
  std::string navigation_relative_path = "/controllable_request.html";
  net::test_server::ControllableHttpResponse navigation_response(
      embedded_test_server(), navigation_relative_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - ScriptingAPI",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting" ],
        "host_permissions": ["*://*/*"]
      } )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("page.html"), "<p>Extension page</p>");
  const char kContentScript[] = R"(
      // TODO(crbug.com/40943155): Remove `console.log` after confirming
      // that the test is no longer flaky
      console.log('CONTENT SCRIPT: running...');

      // `document_end` waits for `DOMContentLoaded`.  `document.body` should
      // therefore be already available.
      chrome.test.assertTrue(!!document.body);

      document.body.innerText = 'content script has run';
      chrome.test.notifyPass();

      // TODO(crbug.com/40943155): Remove `console.log` after confirming
      // that the test is no longer flaky
      console.log('CONTENT SCRIPT: running... DONE.');
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an extension page (so that later we can call
  // `chrome.scripting.registerContentScripts`).
  content::RenderFrameHost* extension_frame = ui_test_utils::NavigateToURL(
      browser(), extension->GetResourceURL("/page.html"));
  ASSERT_TRUE(extension_frame);

  // Step 1: Navigate to a test page that *will later* be covered by the dynamic
  // content script.  Wait for DidFinishNavigation, but do *not* wait for
  // `onload` event.
  {
    GURL main_url =
        embedded_test_server()->GetURL("a.com", navigation_relative_path);
    content::TestNavigationObserver nav_observer(main_url);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    navigation_response.WaitForRequest();
    navigation_response.Send(net::HTTP_OK, "text/html",
                             "<p>First paragraph</p>");
    nav_observer.WaitForNavigationFinished();
  }
  content::WebContents* second_tab = GetActiveWebContents();

  // Verify that initially the process doesn't show up as having been injected
  // with content scripts.  We can't inspect `document.body.innerText` because
  // "DOMContentLoaded" didn't happen yet (i.e. maybe none of HTML has been
  // parsed yet).
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  {
    UserScriptManager* user_script_manager =
        ExtensionSystem::Get(second_tab->GetBrowserContext())
            ->user_script_manager();
    ExtensionUserScriptLoader* user_script_loader =
        user_script_manager->GetUserScriptLoaderForExtension(extension->id());
    ContentScriptLoadWaiter content_script_load_waiter(user_script_loader);

    // Step 2: Register a dynamic content script.
    {
      const char kRegistrationScript[] = R"(
          chrome.scripting.registerContentScripts([{
            id: 'script1',
            matches: ['*://a.com/*'],
            js: ['content_script.js'],
            runAt: 'document_idle'
          }]);
      )";
      ASSERT_TRUE(content::ExecJs(extension_frame, kRegistrationScript));
    }

    // Step 2b: Wait until the dynamic content script loads (in the same message
    // loop iteration the ScriptInjectionTracker's
    // DidUpdateContentScriptsInRenderer will run.
    ResultCatcher catcher;
    content_script_load_waiter.Wait();

    // At this point ScriptInjectionTracker should already be aware about the
    // content script.
    EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
        *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

    // Step 3: Finish sending the page contents over the network.  This will
    // unblock `DOMContentLoaded` event and will allow injecting the script at
    // `document_end` time.
    {
      navigation_response.Send(net::HTTP_OK, "text/html",
                               "<p>Second paragraph</p>");
      navigation_response.Done();

      // Step 4: Wait until content script gets injected.
      ASSERT_TRUE(catcher.GetNextResult());
    }
  }

  // Step 5: Verify again that the second tab shows up as having been injected
  // with content scripts.
  EXPECT_EQ("content script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests that ScriptInjectionTracker monitors extension permission changes and
// updates the renderer data accordingly.
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest,
                       UpdateHostPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Step 1: Install extension with <all_urls> optional host permissions and
  // dynamic content script with a.com matches.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptingAPI - Update host permissions",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting" ],
        "optional_host_permissions": ["<all_urls>"],
        "background": { "service_worker": "worker.js" }
      } )";
  const char kWorkerScript[] = R"(
      var scripts = [{
        id: 'script1',
        matches: ['*://a.com/*'],
        js: ['content_script.js'],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(function(details) {
        chrome.scripting.registerContentScripts(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
      document.body.title = 'Content script has run';
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Step 2: Navigate to a.com. Verify that the process doesn't show up
  // as having been injected with content scripts.
  GURL optional_url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), optional_url));

  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 3: Grant optional permissions.
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionsParser::GetOptionalPermissions(extension));

  // Step 4: Navigate to a.com in the same renderer. Verify process
  // shows up as having been injected with content script and content script is
  // injected.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), optional_url));

  EXPECT_EQ("Content script has run",
            content::EvalJs(web_contents, "document.body.title"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests that ScriptInjectionTracker monitors extension permission changes
// between commit and load, and updates the renderer data accordingly.
// TODO(crbug.com/41495179): Flaky test.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_UpdateHostPermissions_RaceCondition \
  DISABLED_UpdateHostPermissions_RaceCondition
#else
#define MAYBE_UpdateHostPermissions_RaceCondition \
  UpdateHostPermissions_RaceCondition
#endif
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest,
                       MAYBE_UpdateHostPermissions_RaceCondition) {
  // Step 0: Set up ControllableHttpResponse to control the timing of the
  // navigation (and therefore to control the timing of the "DOMContentLoaded"
  // event and therefore the timing of content script injection).
  std::string navigation_relative_path = "/controllable_request.html";
  net::test_server::ControllableHttpResponse navigation_response(
      embedded_test_server(), navigation_relative_path);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Step 1: Install extension with <all_urls> optional host permissions and
  // dynamic content script with a.com matches.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "ScriptingAPI - Update host permissions, race condition",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting" ],
        "optional_host_permissions": ["<all_urls>"],
        "background": { "service_worker": "worker.js" }
      } )";
  const char kWorkerScript[] = R"(
      var scripts = [{
        id: 'script1',
        matches: ['*://a.com/*'],
        js: ['content_script.js'],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(function(details) {
        chrome.scripting.registerContentScripts(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
      document.body.title = 'Content script has run';
      chrome.test.notifyPass();
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Step 2: Start navigation to a.com and verify tracker doesn't run
  // content script.

  // Navigate to a test page that *will later* be covered by the dynamic
  // content script.  Wait for DidFinishNavigation, but do *not* wait for
  // `onload` event.
  {
    GURL main_url =
        embedded_test_server()->GetURL("a.com", navigation_relative_path);
    content::TestNavigationObserver nav_observer(main_url);
    nav_observer.StartWatchingNewWebContents();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
    navigation_response.WaitForRequest();
    navigation_response.Send(net::HTTP_OK, "text/html",
                             "<p>First paragraph</p>");
    nav_observer.WaitForNavigationFinished();
  }

  // Verify that initially the process doesn't show up as having been injected
  // with content scripts.  We can't inspect `document.body.innerText` because
  // "DOMContentLoaded" didn't happen yet (i.e. maybe none of HTML has been
  // parsed yet).
  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 3: Grant optional permissions and verify tracker runs the content
  // script.
  permissions_test_util::GrantOptionalPermissionsAndWaitForCompletion(
      profile(), *extension,
      PermissionsParser::GetOptionalPermissions(extension));

  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 4: Finish navigation and verify content script is injected.
  // Finish sending the page contents over the network.  This will unblock
  // `DOMContentLoaded` event and will allow injecting the script at
  // `document_end` time.
  {
    ResultCatcher catcher;
    navigation_response.Send(net::HTTP_OK, "text/html",
                             "<p>Second paragraph</p>");
    navigation_response.Done();
    ASSERT_TRUE(catcher.GetNextResult());
  }

  EXPECT_EQ("Content script has run",
            content::EvalJs(web_contents, "document.body.title"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests that ScriptInjectionTracker updates the renderer data when activeTab is
// granted.
IN_PROC_BROWSER_TEST_F(DynamicScriptsTrackerBrowserTest, ActiveTabGranted) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Step 1: Install extension with <all_urls> optional host permissions and
  // dynamic content script with a.com matches.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "activeTab extension",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": [ "scripting", "activeTab" ],
        "background": { "service_worker": "worker.js" }
      } )";
  const char kWorkerScript[] = R"(
      var scripts = [{
        id: 'script1',
        matches: ['*://a.com/*'],
        js: ['content_script.js'],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(function(details) {
        chrome.scripting.registerContentScripts(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteManifest(kManifestTemplate);
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerScript);
  const char kContentScript[] = R"(
      document.body.title = 'Content script has run';
  )";
  dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Step 2: Navigate to a.com. Verify that the process doesn't show up as
  // having been injected with content scripts.
  GURL url = embedded_test_server()->GetURL("a.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* web_contents = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 3: Grant activeTab and verify tracker runs the content script.
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  tab_helper->active_tab_permission_granter()->GrantIfRequested(extension);
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 4: Navigate to a.com in the same renderer. Verify process shows up as
  // having been injected with content script, and content script is injected.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_EQ("Content script has run",
            content::EvalJs(web_contents, "document.body.title"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Step 5: Navigate to a.com on a new renderer. Verify process doesn't show up
  // as having been injected with content scripts, since tab permission was
  // granted only to the active tab.
  ASSERT_TRUE(NavigateToURLInNewProcess("a.com", "/title1.html"));
  web_contents = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *web_contents->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

class UserScriptTrackerBrowserTest : public ScriptInjectionTrackerBrowserTest {
 public:
  UserScriptTrackerBrowserTest() = default;
  void SetUpOnMainThread() override {
    ScriptInjectionTrackerBrowserTest::SetUpOnMainThread();
    // The userScripts API is only available to users in developer mode.
    util::SetDeveloperModeForProfile(profile(), true);
  }
};

// Tests tracking of user scripts dynamically injected/declared via
// `chrome.userScripts` API.
IN_PROC_BROWSER_TEST_F(UserScriptTrackerBrowserTest,
                       UserScriptViaUserScriptsApi) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension with a user script.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "register user script",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": ["userScripts"],
        "host_permissions": ["<all_urls>"],
        "background": {"service_worker": "worker.js"}
      })";
  dir.WriteManifest(kManifestTemplate);

  const char kServiceWorker[] = R"(
      var scripts = [{
        id: 'us1',
        matches: ['*://requested.com/*'],
        js: [{ file: "user_script.js"}],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(async function(details) {
        await chrome.userScripts.register(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kServiceWorker);

  const char kUserScript[] = R"(
      window.onload = function() {
          chrome.test.assertEq('complete', document.readyState);
          document.body.innerText = 'user script has run';
          chrome.test.notifyPass();
      }
  )";
  dir.WriteFile(FILE_PATH_LITERAL("user_script.js"), kUserScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Navigate to a page that is not in the user script 'matches'.
  GURL ignored_url =
      embedded_test_server()->GetURL("other.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));

  // Verify that no frames show up as having been injected with user scripts.
  content::WebContents* first_tab = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a page that is in the user script 'matches'.
  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURLInNewProcess("requested.com", "/title1.html"));
  ASSERT_TRUE(catcher.GetNextResult());
  content::WebContents* second_tab = GetActiveWebContents();

  // Verify that the new tab shows up as having been injected with user scripts.
  EXPECT_EQ("user script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Confidence check: injecting a user script should not count as injecting a
  // content script.
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

// Tests tracking of user scripts dynamically injected/declared via
// `chrome.userScripts` API only when extension requests host permissions.
IN_PROC_BROWSER_TEST_F(UserScriptTrackerBrowserTest,
                       UserScriptViaUserScriptsApi_HostPermissions) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install an extension with a user script that wants to inject in all
  // sites but extension only requests 'requested.com' host permissions.
  TestExtensionDir dir;
  const char kManifestTemplate[] = R"(
      {
        "name": "UserScriptAPI - host permissions",
        "version": "1.0",
        "manifest_version": 3,
        "permissions": ["userScripts"],
        "host_permissions": ["*://requested.com/*"],
        "background": {"service_worker": "worker.js"}
      })";
  dir.WriteManifest(kManifestTemplate);

  const char kServiceWorker[] = R"(
      var scripts = [{
        id: 'us1',
        matches: ['<all_urls>'],
        js: [{ file: "user_script.js"}],
        runAt: 'document_end'
      }];

      chrome.runtime.onInstalled.addListener(async function(details) {
        await chrome.userScripts.register(scripts, () => {
          chrome.test.sendMessage('SCRIPT_LOADED');
        });
      }); )";
  dir.WriteFile(FILE_PATH_LITERAL("worker.js"), kServiceWorker);

  const char kUserScript[] = R"(
      document.body.innerText = 'user script has run';
      chrome.test.sendMessage('SCRIPT_INJECTED');
  )";
  dir.WriteFile(FILE_PATH_LITERAL("user_script.js"), kUserScript);

  ExtensionTestMessageListener script_loaded_listener("SCRIPT_LOADED");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(script_loaded_listener.WaitUntilSatisfied());

  // Navigate to a page that is not in the extension's host permissions.
  GURL ignored_url =
      embedded_test_server()->GetURL("non-requested.com", "/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ignored_url));

  // Verify that no frames show up as having been injected with user scripts.
  content::WebContents* first_tab = GetActiveWebContents();
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Navigate to a page that is in the extension's host permission and is in the
  // user script 'matches'.
  ExtensionTestMessageListener listener("SCRIPT_INJECTED");
  ASSERT_TRUE(NavigateToURLInNewProcess("requested.com", "/title1.html"));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  content::WebContents* second_tab = GetActiveWebContents();

  // Verify that the new tab shows up as having been injected with user scripts.
  EXPECT_EQ("user script has run",
            content::EvalJs(second_tab, "document.body.innerText"));
  EXPECT_EQ("This page has no title.",
            content::EvalJs(first_tab, "document.body.innerText"));
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunUserScriptFromExtension(
      *first_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));

  // Confidence check: injecting a user script should not count as injecting a
  // content script.
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *second_tab->GetPrimaryMainFrame()->GetProcess(), extension->id()));
}

class ScriptInjectionTrackerAppBrowserTest : public PlatformAppBrowserTest {
 public:
  ScriptInjectionTrackerAppBrowserTest() = default;

  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Tests that ScriptInjectionTracker detects content scripts injected via
// <webview> (aka GuestView) APIs. This test covers a basic injection scenario.
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerAppBrowserTest,
                       WebViewContentScript) {
  // Install an unrelated test extension (for testing that
  // ScriptInjectionTracker doesn't think that *all* extensions are injecting
  // scripts into a webView).
  TestExtensionDir unrelated_dir;
  const char kUnrelatedManifest[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - Unrelated",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": [ "tabs", "<all_urls>" ],
        "content_scripts": [{
          "all_frames": true,
          "matches": ["*://bar.com/*"],
          "js": ["content_script.js"],
          "run_at": "document_start"
        }]
      } )";
  unrelated_dir.WriteManifest(kUnrelatedManifest);
  unrelated_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), R"(
      chrome.test.sendMessage('Hello from extension content script!'); )");
  const Extension* unrelated_extension =
      LoadExtension(unrelated_dir.UnpackedPath());
  ASSERT_TRUE(unrelated_extension);

  // Load the test app.
  TestExtensionDir dir;
  const char kManifest[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - App",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": ["*://*/*", "webview"],
        "app": {
          "background": {
            "scripts": ["background_script.js"]
          }
        }
      } )";
  dir.WriteManifest(kManifest);
  const char kBackgroundScript[] = R"(
      chrome.app.runtime.onLaunched.addListener(function() {
        chrome.app.window.create('page.html', {}, function () {});
      });
  )";
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroundScript);
  const char kPage[] = R"(
      <div id="webview-tag-container"></div>
  )";
  dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPage);

  // Launch the test app and grab its WebContents.
  const Extension* app = LoadAndLaunchApp(dir.UnpackedPath());
  ASSERT_TRUE(app);
  content::WebContents* app_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(app_contents));

  // Navigate the <webview> tag and grab the `guest_process`.
  content::WebContents* guest_contents = nullptr;
  {
    const char kWebViewInjectionScriptTemplate[] = R"(
        document.querySelector('#webview-tag-container').innerHTML =
            '<webview style="width: 100px; height: 100px;"></webview>';
        var webview = document.querySelector('webview');
        webview.src = $1;
    )";
    GURL guest_url1(embedded_test_server()->GetURL("foo.com", "/title1.html"));

    content::WebContentsAddedObserver guest_contents_observer;
    ASSERT_TRUE(ExecJs(
        app_contents,
        content::JsReplace(kWebViewInjectionScriptTemplate, guest_url1)));
    guest_contents = guest_contents_observer.GetWebContents();
  }

  // Verify that ScriptInjectionTracker correctly shows that no content scripts
  // got injected just yet.
  content::RenderProcessHost* guest_process =
      guest_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, app->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, unrelated_extension->id()));

  // Declare content scripts + trigger their injection with another navigation.
  //
  // TODO(lukasza): Ideally the URL pattern would be more restrictive for the
  // content script `matches` below (to enable testing whether the target of
  // navigation URL actually matched the pattern from the `addContentScripts`
  // call).
  {
    const char kContentScriptDeclarationScriptTemplate[] = R"(
        var webview = document.querySelector('webview');
        webview.addContentScripts([{
            name: 'rule',
            matches: ['*://*/*'],
            js: { code: $1 },
            run_at: 'document_start'}]);
        webview.src = $2;
    )";
    const char kContentScript[] = R"(
        chrome.test.sendMessage("Hello from webView content script!");
    )";
    GURL guest_url2(embedded_test_server()->GetURL("bar.com", "/title2.html"));

    ExtensionTestMessageListener app_script_listener(
        "Hello from webView content script!");
    ExtensionTestMessageListener unrelated_extension_script_listener(
        "Hello from extension content script!");
    content::TestNavigationObserver nav_observer(guest_contents);
    content::ExecuteScriptAsync(
        app_contents,
        content::JsReplace(kContentScriptDeclarationScriptTemplate,
                           kContentScript, guest_url2));

    // Wait for the navigation to complete and verify via `listener` that the
    // expected content script has run.
    nav_observer.Wait();
    ASSERT_TRUE(app_script_listener.WaitUntilSatisfied());
    EXPECT_FALSE(unrelated_extension_script_listener.was_satisfied());
  }

  // Verify that ScriptInjectionTracker detected the content script injection
  // from `app` in the bar.com guest process (but not from
  // `unrelated_extension`).
  guest_process = guest_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, app->id()));
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, unrelated_extension->id()));
}

// Tests that ScriptInjectionTracker detects content scripts injected via
// <webview> (aka GuestView) APIs.  This test covers a scenario where the
// `addContentScripts` API is called in the middle of the test - after
// a matching guest content has already loaded (no content scripts there)
// but before a matching about:blank guest navigation happens (need to detect
// content scripts there).
IN_PROC_BROWSER_TEST_F(ScriptInjectionTrackerAppBrowserTest,
                       WebViewContentScriptForLateAboutBlank) {
  // Load the test app.
  TestExtensionDir dir;
  const char kManifest[] = R"(
      {
        "name": "ScriptInjectionTrackerBrowserTest - App",
        "version": "1.0",
        "manifest_version": 2,
        "permissions": ["*://*/*", "webview"],
        "app": {
          "background": {
            "scripts": ["background_script.js"]
          }
        }
      } )";
  dir.WriteManifest(kManifest);
  const char kBackgroundScript[] = R"(
      chrome.app.runtime.onLaunched.addListener(function() {
        chrome.app.window.create('page.html', {}, function () {});
      });
  )";
  dir.WriteFile(FILE_PATH_LITERAL("background_script.js"), kBackgroundScript);
  const char kPage[] = R"(
      <div id="webview-tag-container"></div>
  )";
  dir.WriteFile(FILE_PATH_LITERAL("page.html"), kPage);

  // Launch the test app and grab its WebContents.
  const Extension* app = LoadAndLaunchApp(dir.UnpackedPath());
  ASSERT_TRUE(app);
  content::WebContents* app_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(app_contents));

  // Navigate the <webview> tag and grab the `guest_process`.
  content::WebContents* guest_contents = nullptr;
  {
    const char kWebViewInjectionScriptTemplate[] = R"(
        document.querySelector('#webview-tag-container').innerHTML =
            '<webview style="width: 100px; height: 100px;"></webview>';
        var webview = document.querySelector('webview');
        webview.src = $1;
    )";
    GURL guest_url1(embedded_test_server()->GetURL("foo.com", "/title1.html"));

    content::WebContentsAddedObserver guest_contents_observer;
    ASSERT_TRUE(ExecJs(
        app_contents,
        content::JsReplace(kWebViewInjectionScriptTemplate, guest_url1)));
    guest_contents = guest_contents_observer.GetWebContents();

    // Wait until the "document_end" timepoint is reached.  (Since this is done
    // before the `addContentScripts` call below, it means that no content
    // scripts will get injected into the initial document.)
    EXPECT_TRUE(WaitForLoadStop(guest_contents));
  }

  // Verify that ScriptInjectionTracker correctly shows that no content scripts
  // got injected just yet.
  content::RenderProcessHost* guest_process =
      guest_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_FALSE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, app->id()));

  // Declare content scripts and wait until they have been loaded (and
  // communicated to the renderer process).
  {
    const char kContentScriptDeclarationScriptTemplate[] = R"(
        var webview = document.querySelector('webview');
        webview.addContentScripts([{
            name: 'rule',
            all_frames: true,
            match_about_blank: true,
            matches: ['*://foo.com/*'],
            js: { code: $1 },
            run_at: 'document_end'}]);
    )";
    const char kContentScript[] = R"(
        chrome.test.sendMessage("Hello from content script!");
    )";
    std::string script = content::JsReplace(
        kContentScriptDeclarationScriptTemplate, kContentScript);

    UserScriptManager* user_script_manager =
        ExtensionSystem::Get(guest_process->GetBrowserContext())
            ->user_script_manager();
    ExtensionUserScriptLoader* user_script_loader =
        user_script_manager->GetUserScriptLoaderForExtension(app->id());
    ContentScriptLoadWaiter content_script_load_waiter(user_script_loader);

    content::ExecuteScriptAsync(app_contents, script);
    content_script_load_waiter.Wait();
  }

  // Create an about:blank subframe where content script should get injected
  // into.
  {
    ExtensionTestMessageListener listener("Hello from content script!");
    content::TestNavigationObserver nav_observer(guest_contents);
    const char kAboutBlankScript[] = R"(
        var f = document.createElement('iframe');
        f.src = 'about:blank';
        document.body.appendChild(f);
    )";
    content::ExecuteScriptAsync(guest_contents, kAboutBlankScript);

    // Wait for the navigation to complete and verify via `listener` that the
    // content script has run.
    nav_observer.Wait();
    ASSERT_TRUE(listener.WaitUntilSatisfied());
  }

  // Verify that ScriptInjectionTracker detected the content script injection.
  EXPECT_TRUE(ScriptInjectionTracker::DidProcessRunContentScriptFromExtension(
      *guest_process, app->id()));
}

}  // namespace extensions
