// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_background_page_waiter.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace extensions {

namespace {

GURL CreateBlobURL(content::RenderFrameHost* frame,
                   const std::string& content) {
  std::string blob_url_string;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame,
      "var blob = new Blob(['<html><body>" + content + "</body></html>'],\n"
      "                    {type: 'text/html'});\n"
      "domAutomationController.send(URL.createObjectURL(blob));\n",
      &blob_url_string));
  GURL blob_url(blob_url_string);
  EXPECT_TRUE(blob_url.is_valid());
  EXPECT_TRUE(blob_url.SchemeIsBlob());
  return blob_url;
}

GURL CreateFileSystemURL(content::RenderFrameHost* frame,
                         const std::string& content) {
  std::string filesystem_url_string;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame,
      "var blob = new Blob(['<html><body>" + content + "</body></html>'],\n"
      "                    {type: 'text/html'});\n"
      "window.webkitRequestFileSystem(TEMPORARY, blob.size, fs => {\n"
      "  fs.root.getFile('foo.html', {create: true}, file => {\n"
      "    file.createWriter(writer => {\n"
      "      writer.write(blob);\n"
      "      writer.onwriteend = () => {\n"
      "        domAutomationController.send(file.toURL());\n"
      "      }\n"
      "    });\n"
      "  });\n"
      "});\n",
      &filesystem_url_string));
  GURL filesystem_url(filesystem_url_string);
  EXPECT_TRUE(filesystem_url.is_valid());
  EXPECT_TRUE(filesystem_url.SchemeIsFileSystem());
  return filesystem_url;
}

std::string GetTextContent(content::RenderFrameHost* frame) {
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame, "domAutomationController.send(document.body.innerText)", &result));
  return result;
}

// Helper to send a postMessage from |sender| to |opener| via window.opener,
// wait for a reply, and verify the response.  Defines its own message event
// handlers.
void VerifyPostMessageToOpener(content::RenderFrameHost* sender,
                               content::RenderFrameHost* opener) {
  EXPECT_TRUE(
      ExecuteScript(opener,
                    "window.addEventListener('message', function(event) {\n"
                    "  event.source.postMessage(event.data, '*');\n"
                    "});"));

  EXPECT_TRUE(
      ExecuteScript(sender,
                    "window.addEventListener('message', function(event) {\n"
                    "  window.domAutomationController.send(event.data);\n"
                    "});"));

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      sender, "opener.postMessage('foo', '*');", &result));
  EXPECT_EQ("foo", result);
}

// Takes a snapshot of all frames upon construction. When Wait() is called, a
// MessageLoop is created and Quit when all previously recorded frames are
// either present in the tab, or deleted. If a navigation happens between the
// construction and the Wait() call, then this logic ensures that all obsolete
// RenderFrameHosts have been destructed when Wait() returns.
// See also the comment at ProcessManagerBrowserTest::NavigateToURL.
class NavigationCompletedObserver : public content::WebContentsObserver {
 public:
  explicit NavigationCompletedObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {
    web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [this](content::RenderFrameHost* rfh) {
          if (rfh->IsRenderFrameLive())
            live_original_frames_.insert(rfh);
        });
  }

  NavigationCompletedObserver(const NavigationCompletedObserver&) = delete;
  NavigationCompletedObserver& operator=(const NavigationCompletedObserver&) =
      delete;

  void Wait() {
    if (!AllLiveRenderFrameHostsAreCurrent())
      message_loop_runner_->Run();
  }

  void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
    if (live_original_frames_.erase(rfh) != 0 &&
        message_loop_runner_->loop_running() &&
        AllLiveRenderFrameHostsAreCurrent()) {
      message_loop_runner_->Quit();
    }
  }

 private:
  // Checks whether the RenderFrameHosts that were current when this class was
  // constructed and that are still alive are all current (e.g. not pending
  // deletion). If there is a non-current RenderFrameHost that is still alive,
  // this returns false.
  bool AllLiveRenderFrameHostsAreCurrent() {
    std::set<content::RenderFrameHost*> current_frames;
    web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&current_frames](content::RenderFrameHost* rfh) {
          if (rfh->IsRenderFrameLive())
            current_frames.insert(rfh);
        });

    return base::STLSetDifference<std::set<content::RenderFrameHost*>>(
               live_original_frames_, current_frames)
               .size() == 0;
  }

  std::set<content::RenderFrameHost*> live_original_frames_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Exists as a browser test because ExtensionHosts are hard to create without
// a real browser.
class ProcessManagerBrowserTest : public ExtensionBrowserTest {
 public:
  ProcessManagerBrowserTest() {
    guest_view::GuestViewManager::set_factory_for_testing(&factory_);
    // TODO(https://crbug.com/1110891): Remove this once Extensions are
    // supported with BackForwardCache.
    disabled_feature_list_.InitWithFeatures({}, {features::kBackForwardCache});
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Create an extension with web-accessible frames and an optional background
  // page.
  const Extension* CreateExtension(const std::string& name,
                                   bool has_background_process) {
    std::unique_ptr<TestExtensionDir> dir(new TestExtensionDir());

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1")
        .Set("manifest_version", 2)
        // To allow ExecuteScript* to work.
        .Set("content_security_policy",
             "script-src 'self' 'unsafe-eval'; object-src 'self'")
        .Set("sandbox",
             DictionaryBuilder()
                 .Set("pages",
                      ListBuilder().Append("sandboxed.html").BuildList())
                 .BuildDict())
        .Set("web_accessible_resources",
             ListBuilder().Append("*.html").BuildList());

    if (has_background_process) {
      manifest.Set("background",
                   DictionaryBuilder().Set("page", "bg.html").BuildDict());
      dir->WriteFile(FILE_PATH_LITERAL("bg.html"),
                     "<iframe id='bgframe' src='empty.html'></iframe>");
    }

    dir->WriteFile(FILE_PATH_LITERAL("blank_iframe.html"),
                   "<iframe id='frame0' src='about:blank'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("srcdoc_iframe.html"),
                   "<iframe id='frame0' srcdoc='Hello world'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("two_iframes.html"),
                   "<iframe id='frame1' src='empty.html'></iframe>"
                   "<iframe id='frame2' src='empty.html'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("sandboxed.html"), "Some sandboxed page");

    dir->WriteFile(FILE_PATH_LITERAL("empty.html"), "");

    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->UnpackedPath());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(std::move(dir));
    return extension;
  }

  // ui_test_utils::NavigateToURL sometimes returns too early: It returns as
  // soon as the StopLoading notification has been triggered. This does not
  // imply that RenderFrameDeleted was called, so the test may continue too
  // early and fail when ProcessManager::GetAllFrames() returns too many frames
  // (namely frames that are in the process of being deleted). To work around
  // this problem, we also wait until all previous frames have been deleted.
  void NavigateToURL(const GURL& url) {
    NavigationCompletedObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    // Wait until the last RenderFrameHosts are deleted. This wait doesn't take
    // long.
    observer.Wait();
  }

  content::WebContents* OpenPopup(content::RenderFrameHost* opener,
                                  const GURL& url,
                                  bool expect_success = true) {
    ui_test_utils::TabAddedWaiter waiter(browser());
    EXPECT_TRUE(ExecuteScript(
        opener, "window.popup = window.open('" + url.spec() + "')"));
    waiter.Wait();
    content::WebContents* popup =
        browser()->tab_strip_model()->GetActiveWebContents();
    WaitForLoadStop(popup);
    if (expect_success)
      EXPECT_EQ(url, popup->GetPrimaryMainFrame()->GetLastCommittedURL());
    return popup;
  }

  content::WebContents* OpenPopupNoOpener(content::RenderFrameHost* opener,
                                          const GURL& url) {
    content::WebContentsAddedObserver popup_observer;
    EXPECT_TRUE(ExecuteScript(
        opener, "window.open('" + url.spec() + "', '', 'noopener')"));
    content::WebContents* popup = popup_observer.GetWebContents();
    WaitForLoadStop(popup);
    return popup;
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  std::vector<std::unique_ptr<TestExtensionDir>> temp_dirs_;
  base::test::ScopedFeatureList disabled_feature_list_;
};

class DefaultProfileExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  DefaultProfileExtensionBrowserTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // We want signin profile on ChromeOS, not logged in user profile.
    set_chromeos_user_ = false;
#endif
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
#endif
  }
};

}  // namespace

// By default, no extension hosts should be present in the profile;
// they should only be present if non-component extensions are loaded
// or if the user takes some action to trigger a component extension.
// TODO(achuith): Expand this testing to include more in-depth
// testing for the signin profile, where we explicitly disallow all
// extension hosts unless it's the off-the-record profile.
IN_PROC_BROWSER_TEST_F(DefaultProfileExtensionBrowserTest, NoExtensionHosts) {
  // Explicitly get the original and off-the-record-profiles, since on CrOS,
  // the signin profile (profile()) is the off-the-record version.
  Profile* original = profile()->GetOriginalProfile();
  Profile* otr = original->GetPrimaryOTRProfile(/*create_if_needed=*/true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(profile(), otr);
  EXPECT_TRUE(ash::ProfileHelper::IsSigninProfile(original));
#endif

  ProcessManager* pm = ProcessManager::Get(original);
  EXPECT_EQ(0u, pm->background_hosts().size());

  pm = ProcessManager::Get(otr);
  EXPECT_EQ(0u, pm->background_hosts().size());
}

// Test that basic extension loading creates the appropriate ExtensionHosts
// and background pages.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       ExtensionHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension);

  EXPECT_TRUE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));
  EXPECT_EQ(-1, pm->GetLazyKeepaliveCount(extension.get()));
  EXPECT_TRUE(pm->GetLazyKeepaliveActivities(extension.get()).empty());

  // Process manager gains a background host.
  EXPECT_EQ(1u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));

  // Unload the extension.
  UnloadExtension(extension->id());

  // Background host disappears.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(-1, pm->GetLazyKeepaliveCount(extension.get()));
  EXPECT_TRUE(pm->GetLazyKeepaliveActivities(extension.get()).empty());
}

// Test that loading an extension with a browser action does not create a
// background page and that clicking on the action creates the appropriate
// ExtensionHost.
// TODO(http://crbug.com/1271329): Times out frequently on Lacros.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_PopupHostCreation DISABLED_PopupHostCreation
#else
#define MAYBE_PopupHostCreation PopupHostCreation
#endif
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, MAYBE_PopupHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // Load an extension with the ability to open a popup but no background
  // page.
  scoped_refptr<const Extension> popup =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("popup"));
  ASSERT_TRUE(popup);

  EXPECT_FALSE(BackgroundInfo::HasBackgroundPage(popup.get()));
  EXPECT_EQ(-1, pm->GetLazyKeepaliveCount(popup.get()));
  EXPECT_TRUE(pm->GetLazyKeepaliveActivities(popup.get()).empty());

  // No background host was added.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));

  // Simulate clicking on the action to open a popup.
  auto test_util = ExtensionActionTestHelper::Create(browser());
  content::CreateAndLoadWebContentsObserver frame_observer;
  // Open popup in the first extension.
  test_util->Press(popup->id());
  frame_observer.Wait();
  ASSERT_TRUE(test_util->HasPopup());

  // We now have a view, but still no background hosts.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(-1, pm->GetLazyKeepaliveCount(popup.get()));
  EXPECT_TRUE(pm->GetLazyKeepaliveActivities(popup.get()).empty());
}

// Content loaded from http://hlogonemlfkgpejgnedahbkiabcdhnnn should not
// interact with an installed extension with that ID. Regression test
// for bug 357382.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HttpHostMatchingExtensionId) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));

  // Set up a test server running at http://[extension-id]
  ASSERT_TRUE(extension.get());
  const std::string& aliased_host = extension->id();
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/test_file_with_body.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr(aliased_host);
  url = url.ReplaceComponents(replace_host);

  // Load a page from the test host in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Sanity check that there's no bleeding between the extension and the tab.
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, tab_web_contents->GetVisibleURL());
  EXPECT_FALSE(pm->GetExtensionForWebContents(tab_web_contents))
      << "Non-extension content must not have an associated extension";
  ASSERT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  content::WebContents* extension_web_contents =
      content::WebContents::FromRenderFrameHost(
          *pm->GetRenderFrameHostsForExtension(extension->id()).begin());
  EXPECT_TRUE(extension_web_contents->GetSiteInstance() !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()) !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, NoBackgroundPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProcessManager* pm = ProcessManager::Get(profile());
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("messaging")
                        .AppendASCII("connect_nobackground"));
  ASSERT_TRUE(extension);

  // The extension has no background page.
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  // Start in a non-extension process, then navigate to an extension process.
  NavigateToURL(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  const GURL extension_url = extension->url().Resolve("manifest.json");
  NavigateToURL(extension_url);
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  NavigateToURL(GURL("about:blank"));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
}

// Tests whether frames are correctly classified. Non-extension frames should
// never appear in the list. Top-level extension frames should always appear.
// Child extension frames should only appear if it is hosted in an extension
// process (i.e. if the top-level frame is an extension page, or if OOP frames
// are enabled for extensions).
// Disabled due to flake: https://crbug.com/693287.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_FrameClassification) {
  const Extension* extension1 = CreateExtension("Extension 1", false);
  const Extension* extension2 = CreateExtension("Extension 2", true);
  embedded_test_server()->ServeFilesFromDirectory(extension1->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kExt1TwoFramesUrl(extension1->url().Resolve("two_iframes.html"));
  const GURL kExt1EmptyUrl(extension1->url().Resolve("empty.html"));
  const GURL kExt2TwoFramesUrl(extension2->url().Resolve("two_iframes.html"));
  const GURL kExt2EmptyUrl(extension2->url().Resolve("empty.html"));

  ProcessManager* pm = ProcessManager::Get(profile());

  // 1 background page + 1 frame in background page from Extension 2.
  ExtensionBackgroundPageWaiter(profile(), *extension2).WaitForBackgroundOpen();
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  ExecuteScriptInBackgroundPageNoWait(extension2->id(),
                                      "setTimeout(window.close, 0)");
  ExtensionBackgroundPageWaiter(profile(), *extension2)
      .WaitForBackgroundClosed();
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  EXPECT_EQ(0u, pm->GetAllFrames().size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Tests extension frames in non-extension page.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", kExt2EmptyUrl));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  EXPECT_EQ(2u, pm->GetAllFrames().size());

  // Tests non-extension page in extension frame.
  NavigateToURL(kExt1TwoFramesUrl);
  // 1 top-level + 2 child frames from Extension 1.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1",
                                           embedded_test_server()
                                           ->GetURL("/empty.html")));
  // 1 top-level + 1 child frame from Extension 1.
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetAllFrames().size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  // 1 top-level + 2 child frames from Extension 1.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());

  // Load a frame from another extension.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt2EmptyUrl));
  // 1 top-level + 1 child frame from Extension 1,
  // 1 child frame from Extension 2.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Destroy all existing frames by navigating to another extension.
  NavigateToURL(extension2->url().Resolve("empty.html"));
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Test about:blank and about:srcdoc child frames.
  NavigateToURL(extension2->url().Resolve("srcdoc_iframe.html"));
  // 1 top-level frame + 1 child frame from Extension 2.
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(extension2->url().Resolve("blank_iframe.html"));
  // 1 top-level frame + 1 child frame from Extension 2.
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Sandboxed frames are not viewed as extension frames.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0",
                                           extension2->url()
                                           .Resolve("sandboxed.html")));
  // 1 top-level frame from Extension 2.
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(extension2->url().Resolve("sandboxed.html"));
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Test nested frames (same extension).
  NavigateToURL(kExt2TwoFramesUrl);
  // 1 top-level + 2 child frames from Extension 2.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt2TwoFramesUrl));
  // 1 top-level + 2 child frames from Extension 1,
  // 2 child frames in frame1 from Extension 2.
  EXPECT_EQ(5u, pm->GetAllFrames().size());
  EXPECT_EQ(5u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // The extension frame from the other extension should not be classified as an
  // extension (unless out-of-process frames are enabled).
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  // 1 top-level + 1 child frames from Extension 2,
  // 1 child frame from Extension 1.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", kExt1TwoFramesUrl));
  // 1 top-level + 1 child frames from Extension 2,
  // 1 child frame + 2 child frames in frame2 from Extension 1.
  EXPECT_EQ(5u, pm->GetAllFrames().size());
  EXPECT_EQ(4u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Crash tab where the top-level frame is an extension frame.
  content::CrashTab(tab);
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Now load an extension page and a non-extension page...
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), kExt1EmptyUrl, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  // ... load an extension frame in the non-extension process
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());

  // ... and take down the tab. The extension process is not part of the tab,
  // so it should be kept alive (minus the frames that died).
  content::CrashTab(tab);
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
}

// Verify correct keepalive count behavior on network request events.
// Regression test for http://crbug.com/535716.
// Disabled on Linux for flakiness: http://crbug.com/1030435.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_KeepaliveOnNetworkRequest DISABLED_KeepaliveOnNetworkRequest
#else
#define MAYBE_KeepaliveOnNetworkRequest KeepaliveOnNetworkRequest
#endif
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       MAYBE_KeepaliveOnNetworkRequest) {
  // Load an extension with a lazy background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("lazy_background_page")
                        .AppendASCII("broadcast_event"));
  ASSERT_TRUE(extension.get());

  ProcessManager* pm = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      pm->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());

  // Keepalive count at this point is unpredictable as there may be an
  // outstanding event dispatch. We use the current keepalive count as a
  // reliable baseline for future expectations.
  EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));
  int baseline_keepalive = pm->GetLazyKeepaliveCount(extension.get());
  size_t baseline_activities_count =
      pm->GetLazyKeepaliveActivities(extension.get()).size();
  EXPECT_LE(0, baseline_keepalive);
  EXPECT_LE(0u, baseline_activities_count);

  // Simulate some network events. This test assumes no other network requests
  // are pending, i.e., that there are no conflicts with the fake request IDs
  // we're using. This should be a safe assumption because LoadExtension should
  // wait for loads to complete, and we don't run the message loop otherwise.
  content::RenderFrameHost* frame_host = *frames.begin();
  constexpr int kRequestId = 1;
  const auto activity =
      std::make_pair(Activity::NETWORK, base::NumberToString(kRequestId));

  pm->NetworkRequestStarted(frame_host, kRequestId);
  EXPECT_EQ(baseline_keepalive + 1, pm->GetLazyKeepaliveCount(extension.get()));
  EXPECT_EQ(1u,
            pm->GetLazyKeepaliveActivities(extension.get()).count(activity));
  pm->NetworkRequestDone(frame_host, kRequestId);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));
  EXPECT_EQ(0u,
            pm->GetLazyKeepaliveActivities(extension.get()).count(activity));

  // Simulate only a request completion for this ID and ensure it doesn't result
  // in keepalive decrement.
  pm->NetworkRequestDone(frame_host, 2);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));
  EXPECT_EQ(baseline_activities_count,
            pm->GetLazyKeepaliveActivities(extension.get()).size());
}

IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, ExtensionProcessReuse) {
  const size_t kNumExtensions = 3;
  content::RenderProcessHost::SetMaxRendererProcessCount(kNumExtensions - 1);
  ProcessManager* pm = ProcessManager::Get(profile());

  std::set<int> processes;
  std::set<const Extension*> installed_extensions;

  // Create 3 extensions, which is more than the process limit.
  for (int i = 1; i <= static_cast<int>(kNumExtensions); ++i) {
    const Extension* extension =
        CreateExtension(base::StringPrintf("Extension %d", i), true);
    installed_extensions.insert(extension);
    ExtensionHost* extension_host =
        pm->GetBackgroundHostForExtension(extension->id());

    EXPECT_EQ(extension->url(),
              extension_host->host_contents()->GetSiteInstance()->GetSiteURL());

    processes.insert(extension_host->render_process_host()->GetID());
  }

  EXPECT_EQ(kNumExtensions, installed_extensions.size());

  EXPECT_EQ(kNumExtensions, processes.size()) << "Extension process reuse is "
                                                 "expected to be disabled.";

  // Interact with each extension background page by setting and reading back
  // the cookie. This would fail for one of the two extensions in a shared
  // process, if that process is locked to a single origin. This is a regression
  // test for http://crbug.com/600441.
  for (const Extension* extension : installed_extensions) {
    ExtensionHost* host =
        ProcessManager::Get(profile())->GetBackgroundHostForExtension(
            extension->id());
    ASSERT_TRUE(host);
    content::DOMMessageQueue queue(host->host_contents());

    ExecuteScriptInBackgroundPageNoWait(
        extension->id(),
        "document.cookie = 'extension_cookie';"
        "window.domAutomationController.send(document.cookie);");
    std::string message;
    ASSERT_TRUE(queue.WaitForMessage(&message));
    EXPECT_EQ(message, "\"extension_cookie\"");
  }
}

// Test that navigations to blob: and filesystem: URLs with extension origins
// are disallowed when initiated from non-extension processes.  See
// https://crbug.com/645028 and https://crbug.com/644426.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       NestedURLNavigationsToExtensionBlocked) {
  // Disabling web security is necessary to test the browser enforcement;
  // without it, the loads in this test would be blocked by
  // SecurityOrigin::canDisplay() as invalid local resource loads.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kWebKitWebSecurityEnabled, false);

  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page with two web iframes.  There should be no
  // extension frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate first subframe to an extension URL. This will go into a new
  // extension process.
  const GURL extension_empty_resource(extension->url().Resolve("empty.html"));
  EXPECT_TRUE(
      content::NavigateIframeToURL(tab, "frame1", extension_empty_resource));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_frame = ChildFrameAt(main_frame, 0);

  // Ideally, this would be a GURL, but it's easier to compose the rest of the
  // URLs if this is a std::string. Meh.
  const std::string extension_base_url =
      base::StrCat({"chrome-extension://", extension->id()});
  const GURL extension_blob_url =
      GURL(base::StrCat({"blob:", extension_base_url, "/some-guid"}));
  const GURL extension_file_system_url =
      GURL(base::StrCat({"filesystem:", extension_base_url, "/some-path"}));
  const GURL extension_url =
      GURL(base::StrCat({extension_base_url, "/some-path"}));

  // Validate that permissions have been granted for the extension scheme
  // to the process of the extension iframe.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_TRUE(policy->CanRequestURL(extension_frame->GetProcess()->GetID(),
                                    extension_blob_url));
  EXPECT_TRUE(policy->CanRequestURL(main_frame->GetProcess()->GetID(),
                                    extension_blob_url));
  EXPECT_TRUE(policy->CanRequestURL(extension_frame->GetProcess()->GetID(),
                                    extension_file_system_url));
  EXPECT_TRUE(policy->CanRequestURL(main_frame->GetProcess()->GetID(),
                                    extension_file_system_url));
  EXPECT_TRUE(policy->CanRequestURL(extension_frame->GetProcess()->GetID(),
                                    extension_url));
  EXPECT_TRUE(
      policy->CanRequestURL(main_frame->GetProcess()->GetID(), extension_url));

  EXPECT_TRUE(policy->CanCommitURL(extension_frame->GetProcess()->GetID(),
                                   extension_blob_url));
  EXPECT_FALSE(policy->CanCommitURL(main_frame->GetProcess()->GetID(),
                                    extension_blob_url));
  EXPECT_TRUE(policy->CanCommitURL(extension_frame->GetProcess()->GetID(),
                                   extension_file_system_url));
  EXPECT_FALSE(policy->CanCommitURL(main_frame->GetProcess()->GetID(),
                                    extension_file_system_url));
  EXPECT_TRUE(policy->CanCommitURL(extension_frame->GetProcess()->GetID(),
                                   extension_url));
  EXPECT_FALSE(
      policy->CanCommitURL(main_frame->GetProcess()->GetID(), extension_url));

  // Open a new about:blank popup from main frame.  This should stay in the web
  // process.
  content::WebContents* popup =
      OpenPopup(main_frame, GURL(url::kAboutBlankURL));
  EXPECT_NE(popup, tab);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  // Create valid blob and filesystem URLs in the extension's origin.
  url::Origin extension_origin(extension_frame->GetLastCommittedOrigin());
  GURL blob_url(CreateBlobURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(blob_url));
  GURL filesystem_url(CreateFileSystemURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(filesystem_url));

  // Navigate the popup to each nested URL with extension origin.
  GURL nested_urls[] = {blob_url, filesystem_url};
  // TODO(https://crbug.com/1332598): Remove filesystem: test branch entirely
  // when filesystem: navigation is removed for good.
  size_t nested_url_count =
      base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation)
          ? 2
          : 1;
  for (size_t i = 0; i < nested_url_count; i++) {
    EXPECT_TRUE(ExecuteScript(
        popup, "location.href = '" + nested_urls[i].spec() + "';"));

    // If a navigation was started, wait for it to finish.  This can't just use
    // a TestNavigationObserver, since after https://crbug.com/811558 blob: and
    // filesystem: navigations have different failure modes: blob URLs will be
    // blocked on the browser side, and filesystem URLs on the renderer side,
    // without notifying the browser.  Since these navigations are scheduled in
    // Blink, run a dummy script on the renderer to ensure that the navigation,
    // if started, has made it to the browser process before we call
    // WaitForLoadStop().
    EXPECT_TRUE(ExecuteScript(popup, "true"));
    EXPECT_TRUE(content::WaitForLoadStop(popup));

    // This is a top-level navigation that should be blocked since it
    // originates from a non-extension process.  Ensure that the error page
    // doesn't commit an extension URL or origin.
    EXPECT_NE(nested_urls[i], popup->GetLastCommittedURL());
    EXPECT_FALSE(extension_origin.IsSameOriginWith(
        popup->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
    EXPECT_NE("foo", GetTextContent(popup->GetPrimaryMainFrame()));

    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(1u, pm->GetAllFrames().size());
  }

  // Close the popup.  It won't be needed anymore, and bringing the original
  // page back into foreground makes the remainder of this test a bit faster.
  popup->Close();

  // Navigate second subframe to each nested URL from the main frame (i.e.,
  // from non-extension process).  These should be canceled.
  for (size_t i = 0; i < nested_url_count; i++) {
    EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", nested_urls[i]));
    content::RenderFrameHost* second_frame = ChildFrameAt(main_frame, 1);

    EXPECT_NE(nested_urls[i], second_frame->GetLastCommittedURL());
    EXPECT_FALSE(extension_origin.IsSameOriginWith(
        second_frame->GetLastCommittedOrigin()));
    EXPECT_NE("foo", GetTextContent(second_frame));
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(1u, pm->GetAllFrames().size());

    EXPECT_TRUE(
        content::NavigateIframeToURL(tab, "frame2", GURL(url::kAboutBlankURL)));
  }
}

// Check that browser-side restrictions on extension blob URLs allow
// navigations that will result in downloads.  See https://crbug.com/714373.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       BlobURLDownloadsToExtensionAllowed) {
  // Disabling web security is necessary to test the browser enforcement;
  // without it, the loads in this test would be blocked by
  // SecurityOrigin::CanDisplay() as invalid local resource loads.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kWebKitWebSecurityEnabled, false);

  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page an iframe.  There should be no extension
  // frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/blank_iframe.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate iframe to an extension URL.
  const GURL extension_url(extension->url().Resolve("empty.html"));
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0", extension_url));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_frame = ChildFrameAt(main_frame, 0);

  // Create a valid blob URL in the extension's origin.
  url::Origin extension_origin(extension_frame->GetLastCommittedOrigin());
  GURL blob_url(CreateBlobURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(blob_url));

  // Check that extension blob URLs still can be downloaded via an HTML anchor
  // tag with the download attribute (i.e., <a download>) (which starts out as
  // a top-level navigation).
  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(tab);
  permission_request_manager->set_auto_response_for_test(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  content::DownloadTestObserverTerminal observer(
      profile()->GetDownloadManager(), 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  std::string script = base::StringPrintf(
      R"(var anchor = document.createElement('a');
         anchor.href = '%s';
         anchor.download = '';
         anchor.click();)",
      blob_url.spec().c_str());
  EXPECT_TRUE(ExecuteScript(tab, script));
  observer.WaitForFinished();
  EXPECT_EQ(1u,
            observer.NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  // This is a top-level navigation that should have resulted in a download.
  // Ensure that the tab stayed at its original location.
  EXPECT_NE(blob_url, tab->GetLastCommittedURL());
  EXPECT_FALSE(
      extension_origin.IsSameOriginWith(main_frame->GetLastCommittedOrigin()));
  EXPECT_NE("foo", GetTextContent(main_frame));

  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
}

// Test that navigations to blob: and filesystem: URLs with extension origins
// are disallowed in subframes when initiated from non-extension processes, even
// when the main frame lies about its origin.  See https://crbug.com/836858.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       NestedURLNavigationsToExtensionBlockedInSubframe) {
  // Disabling web security is necessary to test the browser enforcement;
  // without it, the loads in this test would be blocked by
  // SecurityOrigin::canDisplay() as invalid local resource loads.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kWebKitWebSecurityEnabled, false);

  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page with two web iframes.  There should be no
  // extension frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate first subframe to an extension URL. This will go into a new
  // extension process.
  const GURL extension_url(extension->url().Resolve("empty.html"));
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", extension_url));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_frame = ChildFrameAt(main_frame, 0);

  // Create valid blob and filesystem URLs in the extension's origin.
  url::Origin extension_origin(extension_frame->GetLastCommittedOrigin());
  GURL blob_url(CreateBlobURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(blob_url));
  GURL filesystem_url(CreateFileSystemURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(filesystem_url));

  // Suppose that the main frame's origin incorrectly claims it is an extension,
  // even though it is not in an extension process. This used to bypass the
  // checks in ExtensionNavigationThrottle.
  OverrideLastCommittedOrigin(main_frame, extension_origin);

  // Navigate second subframe to each nested URL from the main frame (i.e.,
  // from non-extension process).  These should be canceled.
  GURL nested_urls[] = {blob_url, filesystem_url};
  // TODO(https://crbug.com/1332598): Remove filesystem: test branch entirely
  // when filesystem: navigation is removed for good.
  size_t nested_url_count =
      base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation)
          ? 2
          : 1;
  for (size_t i = 0; i < nested_url_count; i++) {
    EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", nested_urls[i]));
    content::RenderFrameHost* second_frame = ChildFrameAt(main_frame, 1);

    EXPECT_NE(nested_urls[i], second_frame->GetLastCommittedURL());
    EXPECT_FALSE(extension_origin.IsSameOriginWith(
        second_frame->GetLastCommittedOrigin()));
    EXPECT_NE("foo", GetTextContent(second_frame));
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(1u, pm->GetAllFrames().size());

    EXPECT_TRUE(
        content::NavigateIframeToURL(tab, "frame2", GURL(url::kAboutBlankURL)));
  }
}

// Test that navigations to blob: and filesystem: URLs with extension origins
// are allowed when initiated from extension processes.  See
// https://crbug.com/645028 and https://crbug.com/644426.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       NestedURLNavigationsToExtensionAllowed) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to an extension URL with a blank subframe.
  const GURL extension_url(extension->url().Resolve("blank_iframe.html"));
  NavigateToURL(extension_url);
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // Create blob and filesystem URLs in the extension's origin.
  url::Origin extension_origin(main_frame->GetLastCommittedOrigin());
  GURL blob_url(CreateBlobURL(main_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(blob_url));
  GURL filesystem_url(CreateFileSystemURL(main_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(filesystem_url));

  // From the main frame, navigate its subframe to each nested URL.  This
  // should be allowed and should stay in the extension process.
  GURL nested_urls[] = {blob_url, filesystem_url};
  // TODO(https://crbug.com/1332598): Remove filesystem: test branch entirely
  // when filesystem: navigation is removed for good.
  size_t nested_url_count =
      base::FeatureList::IsEnabled(blink::features::kFileSystemUrlNavigation)
          ? 2
          : 1;
  for (size_t i = 0; i < nested_url_count; i++) {
    EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0", nested_urls[i]));
    content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);
    EXPECT_EQ(nested_urls[i], child->GetLastCommittedURL());
    EXPECT_EQ(extension_origin, child->GetLastCommittedOrigin());
    EXPECT_EQ("foo", GetTextContent(child));
    EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(2u, pm->GetAllFrames().size());
  }

  // From the main frame, create a blank popup and navigate it to the nested
  // blob URL. This should also be allowed, since the navigation originated from
  // an extension process.
  {
    content::WebContents* popup =
        OpenPopup(main_frame, GURL(url::kAboutBlankURL));
    EXPECT_NE(popup, tab);

    content::TestNavigationObserver observer(popup);
    EXPECT_TRUE(ExecuteScript(
        popup, "location.href = '" + nested_urls[0].spec() + "';"));
    observer.Wait();

    EXPECT_EQ(nested_urls[0], popup->GetLastCommittedURL());
    EXPECT_EQ(extension_origin,
              popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
    EXPECT_EQ("foo", GetTextContent(popup->GetPrimaryMainFrame()));

    EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(3u, pm->GetAllFrames().size());
  }

  // Same as above, but renderers cannot navigate top frame to filesystem URLs.
  // So this will result in a console message.
  {
    content::WebContents* popup =
        OpenPopup(main_frame, GURL(url::kAboutBlankURL));
    EXPECT_NE(popup, tab);

    content::WebContentsConsoleObserver console_observer(popup);
    console_observer.SetPattern("Not allowed to navigate to*");
    EXPECT_TRUE(ExecuteScript(
        popup, "location.href = '" + nested_urls[1].spec() + "';"));
    ASSERT_TRUE(console_observer.Wait());

    // about:blank URLs can be modified by their opener. In that case their
    // effective origin changes to that of the opener, but the page URL remains
    // about:blank. Here the popup is being modified by the extension page,
    // so it's origin will change to the extension URL.
    EXPECT_EQ(GURL(url::kAboutBlankURL), popup->GetLastCommittedURL());
    EXPECT_EQ(extension_origin,
              popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
    EXPECT_EQ(std::string(), GetTextContent(popup->GetPrimaryMainFrame()));

    EXPECT_EQ(4u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(4u, pm->GetAllFrames().size());
  }
}

// Test that navigations to blob: and filesystem: URLs with extension origins
// are disallowed in an unprivileged, non-guest web process when the extension
// origin corresponds to a Chrome app with the "webview" permission.  See
// https://crbug.com/656752.  These requests should still be allowed inside
// actual <webview> guest processes created by a Chrome app; this is checked in
// WebViewTest.Shim_TestBlobURL.
// TODO(alexmos): Enable this test once checks are implemented in the
// extensions NavigationThrottle. See https://crbug.com/919194.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_NestedURLNavigationsToAppBlocked) {
  // Disabling web security is necessary to test the browser enforcement;
  // without it, the loads in this test would be blocked by
  // SecurityOrigin::canDisplay() as invalid local resource loads.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kWebKitWebSecurityEnabled, false);

  // Load a simple app that has the "webview" permission.  The app will also
  // open a <webview> when it's loaded.
  ASSERT_TRUE(embedded_test_server()->Start());
  base::FilePath dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &dir);
  dir = dir.AppendASCII("extensions")
            .AppendASCII("platform_apps")
            .AppendASCII("web_view")
            .AppendASCII("simple");
  const Extension* app = LoadAndLaunchApp(dir);
  EXPECT_TRUE(app->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kWebView));

  auto app_windows = AppWindowRegistry::Get(browser()->profile())
                         ->GetAppWindowsForApp(app->id());
  EXPECT_EQ(1u, app_windows.size());
  content::WebContents* app_tab = (*app_windows.begin())->web_contents();
  content::RenderFrameHost* app_rfh = app_tab->GetPrimaryMainFrame();
  url::Origin app_origin(app_rfh->GetLastCommittedOrigin());
  EXPECT_EQ(url::Origin::Create(app->url()), app_rfh->GetLastCommittedOrigin());

  // Wait for the app's guest WebContents to load.
  guest_view::TestGuestViewManager* guest_manager =
      static_cast<guest_view::TestGuestViewManager*>(
          guest_view::TestGuestViewManager::FromBrowserContext(
              browser()->profile()));
  auto* guest_view = guest_manager->WaitForSingleGuestViewCreated();
  guest_manager->WaitUntilAttached(guest_view);
  auto* guest_rfh = guest_manager->GetLastGuestRenderFrameHostCreated();

  // There should be two extension frames in ProcessManager: the app's main
  // page and the background page.
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(app->id()).size());

  // Create valid blob and filesystem URLs in the app's origin.
  GURL blob_url(CreateBlobURL(app_rfh, "foo"));
  EXPECT_EQ(app_origin, url::Origin::Create(blob_url));
  GURL filesystem_url(CreateFileSystemURL(app_rfh, "foo"));
  EXPECT_EQ(app_origin, url::Origin::Create(filesystem_url));

  // Create a new tab, unrelated to the app, and navigate it to a web URL.
  chrome::NewTab(browser());
  content::WebContents* web_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL web_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), web_url));
  EXPECT_NE(web_tab, app_tab);
  EXPECT_NE(web_tab->GetPrimaryMainFrame()->GetProcess(),
            app_rfh->GetProcess());

  // The web process shouldn't have permission to request URLs in the app's
  // origin, but the guest process should.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  EXPECT_FALSE(policy->CanRequestURL(
      web_tab->GetPrimaryMainFrame()->GetProcess()->GetID(),
      app_origin.GetURL()));
  EXPECT_TRUE(policy->CanRequestURL(guest_rfh->GetProcess()->GetID(),
                                    app_origin.GetURL()));

  // Try navigating the web tab to each nested URL with the app's origin.  This
  // should be blocked.
  GURL nested_urls[] = {blob_url, filesystem_url};
  for (size_t i = 0; i < std::size(nested_urls); i++) {
    content::TestNavigationObserver observer(web_tab);
    EXPECT_TRUE(ExecuteScript(
        web_tab, "location.href = '" + nested_urls[i].spec() + "';"));
    observer.Wait();
    EXPECT_NE(nested_urls[i], web_tab->GetLastCommittedURL());
    EXPECT_FALSE(app_origin.IsSameOriginWith(
        web_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
    EXPECT_NE("foo", GetTextContent(web_tab->GetPrimaryMainFrame()));
    EXPECT_NE(web_tab->GetPrimaryMainFrame()->GetProcess(),
              app_rfh->GetProcess());

    EXPECT_EQ(2u, pm->GetAllFrames().size());
    EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(app->id()).size());
  }
}

// Test that a web frame can't navigate a proxy for an extension frame to a
// blob/filesystem extension URL.  See https://crbug.com/656752.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       NestedURLNavigationsViaProxyBlocked) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to an empty web page.  There should be no extension
  // frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/empty.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // Have the web page navigate the popup to each nested URL with extension
  // origin via the window reference it obtained earlier from window.open.
  const GURL extension_url(extension->url().Resolve("empty.html"));
  for (auto create_function : {&CreateBlobURL, &CreateFileSystemURL}) {
    // Setup the test by navigating popup to an extension page. This is allowed
    // because it's web accessible.
    content::WebContents* popup = OpenPopup(main_frame, extension_url);

    // This frame should now be in an extension process.
    EXPECT_EQ(1u, pm->GetAllFrames().size());
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

    // Create a valid blob or filesystem URL in the extension's origin.
    GURL nested_url = (*create_function)(popup->GetPrimaryMainFrame(), "foo");

    // Navigate via the proxy to |nested_url|. This should be blocked by
    // FilterURL.
    EXPECT_TRUE(ExecuteScript(
        tab, "window.popup.location.href = '" + nested_url.spec() + "';"));
    EXPECT_TRUE(WaitForLoadStop(popup));

    // Because the navigation was blocked, the URL doesn't change.
    EXPECT_NE(nested_url, popup->GetLastCommittedURL());
    EXPECT_EQ(extension_url, popup->GetLastCommittedURL().spec());
    EXPECT_NE("foo", GetTextContent(popup->GetPrimaryMainFrame()));
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(1u, pm->GetAllFrames().size());
    popup->Close();
    EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(0u, pm->GetAllFrames().size());
  }
}

// TODO(crbug.com/909570): This test is flaky everywhere.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_NestedURLNavigationsViaNoOpenerPopupBlocked) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to an empty web page.  There should be no extension
  // frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/empty.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // Open a new about:blank popup from main frame.  This should stay in the web
  // process.
  content::WebContents* popup =
      OpenPopup(main_frame, GURL(url::kAboutBlankURL));
  EXPECT_NE(popup, tab);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());

  // Navigate popup to an extension page.
  const GURL extension_url(extension->url().Resolve("empty.html"));
  content::TestNavigationObserver observer(popup);
  EXPECT_TRUE(
      ExecuteScript(popup, "location.href = '" + extension_url.spec() + "';"));
  observer.Wait();
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  content::RenderFrameHost* extension_frame = popup->GetPrimaryMainFrame();

  // Create valid blob and filesystem URLs in the extension's origin.
  url::Origin extension_origin(extension_frame->GetLastCommittedOrigin());
  GURL blob_url(CreateBlobURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(blob_url));
  GURL filesystem_url(CreateFileSystemURL(extension_frame, "foo"));
  EXPECT_EQ(extension_origin, url::Origin::Create(filesystem_url));

  // Attempt opening the nested urls using window.open(url, '', 'noopener').
  // This should not be allowed.
  GURL nested_urls[] = {blob_url, filesystem_url};
  for (size_t i = 0; i < std::size(nested_urls); i++) {
    content::WebContents* new_popup =
        OpenPopupNoOpener(tab->GetPrimaryMainFrame(), nested_urls[i]);

    // This is a top-level navigation to a local resource, that should be
    // blocked by FilterURL, since it originates from a non-extension process.
    EXPECT_NE(nested_urls[i], new_popup->GetLastCommittedURL());
    EXPECT_EQ("about:blank", new_popup->GetLastCommittedURL().spec());
    EXPECT_NE("foo", GetTextContent(new_popup->GetPrimaryMainFrame()));

    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
    EXPECT_EQ(1u, pm->GetAllFrames().size());

    new_popup->Close();
  }
}

IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       ServerRedirectToNonWebAccessibleResource) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to an empty web page.  There should be no extension
  // frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/empty.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // For this extension, only "*.html" resources are listed as web accessible;
  // "manifest.json" doesn't match that pattern, so it shouldn't be possible for
  // a webpage to initiate such a navigation.
  const GURL inaccessible_extension_resource(
      extension->url().Resolve("manifest.json"));
  // This is an HTTP request that redirects to a non-webaccessible resource.
  const GURL redirect_to_inaccessible(embedded_test_server()->GetURL(
      "/server-redirect?" + inaccessible_extension_resource.spec()));
  content::WebContents* sneaky_popup =
      OpenPopup(main_frame, redirect_to_inaccessible, false);
  EXPECT_EQ(inaccessible_extension_resource,
            sneaky_popup->GetLastCommittedURL());
  EXPECT_EQ(
      content::PAGE_TYPE_ERROR,
      sneaky_popup->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());

  // Adding "noopener" to the navigation shouldn't make it work either.
  content::WebContents* sneaky_noopener_popup =
      OpenPopupNoOpener(main_frame, redirect_to_inaccessible);
  EXPECT_EQ(inaccessible_extension_resource,
            sneaky_noopener_popup->GetLastCommittedURL());
  EXPECT_EQ(content::PAGE_TYPE_ERROR, sneaky_noopener_popup->GetController()
                                          .GetLastCommittedEntry()
                                          ->GetPageType());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
}

IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       CrossExtensionEmbeddingOfWebAccessibleResources) {
  // Create a simple extension without a background page.
  const Extension* extension1 = CreateExtension("Extension 1", false);
  const Extension* extension2 = CreateExtension("Extension 2", false);
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the "extension 1" page with two iframes.
  auto url = extension1->url().Resolve("two_iframes.html");
  NavigateToURL(url);
  auto initiator_origin = absl::optional<url::Origin>(url::Origin::Create(url));

  ProcessManager* pm = ProcessManager::Get(profile());
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // Navigate the first iframe to a webaccessible resource of extension 2. This
  // should work.
  GURL extension2_empty = extension2->url().Resolve("/empty.html");
  EXPECT_TRUE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension2, extension2_empty.path(), initiator_origin));
  {
    content::RenderFrameDeletedObserver frame_deleted_observer(
        ChildFrameAt(main_frame, 0));
    EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", extension2_empty));
    EXPECT_EQ(extension2_empty,
              ChildFrameAt(main_frame, 0)->GetLastCommittedURL());
    frame_deleted_observer.WaitUntilDeleted();
    EXPECT_EQ(3u, pm->GetAllFrames().size());
    EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  }

  // Manifest.json is not a webaccessible resource. extension1 should not be
  // able to navigate to extension2's manifest.json.
  GURL extension2_manifest = extension2->url().Resolve("/manifest.json");
  EXPECT_FALSE(WebAccessibleResourcesInfo::IsResourceWebAccessible(
      extension2, extension2_manifest.path(), initiator_origin));
  {
    content::TestNavigationObserver nav_observer(tab, 1);
    EXPECT_TRUE(ExecuteScript(
        tab, base::StringPrintf("frames[0].location.href = '%s';",
                                extension2_manifest.spec().c_str())));
    nav_observer.Wait();
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_EQ(extension2_manifest,
              ChildFrameAt(main_frame, 0)->GetLastCommittedURL());
    EXPECT_EQ(2u, pm->GetAllFrames().size());
    EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
    EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  }

  // extension1 should not be able to navigate its second iframe to
  // extension2's manifest by bouncing off an HTTP redirect.
  const GURL sneaky_extension2_manifest(embedded_test_server()->GetURL(
      "/server-redirect?" + extension2_manifest.spec()));
  {
    content::TestNavigationObserver nav_observer(tab, 1);
    EXPECT_TRUE(ExecuteScript(
        tab, base::StringPrintf("frames[1].location.href = '%s';",
                                sneaky_extension2_manifest.spec().c_str())));
    nav_observer.Wait();
    EXPECT_FALSE(nav_observer.last_navigation_succeeded())
        << "The initial navigation should be allowed, but not the server "
           "redirect to extension2's manifest";
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, nav_observer.last_net_error_code());
    EXPECT_EQ(extension2_manifest, nav_observer.last_navigation_url());
    EXPECT_EQ(extension2_manifest,
              ChildFrameAt(main_frame, 1)->GetLastCommittedURL());
    EXPECT_EQ(1u, pm->GetAllFrames().size());
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
    EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  }

  // extension1 can embed a webaccessible resource of extension2 by means of
  // an HTTP redirect.
  {
    content::RenderFrameDeletedObserver frame_deleted_observer(
        ChildFrameAt(main_frame, 1));
    const GURL extension2_accessible_redirect(embedded_test_server()->GetURL(
        "/server-redirect?" + extension2_empty.spec()));
    EXPECT_TRUE(ExecuteScript(
        tab,
        base::StringPrintf("frames[1].location.href = '%s';",
                           extension2_accessible_redirect.spec().c_str())));
    EXPECT_TRUE(WaitForLoadStop(tab));
    frame_deleted_observer.WaitUntilDeleted();
    EXPECT_EQ(extension2_empty,
              ChildFrameAt(main_frame, 1)->GetLastCommittedURL())
        << "The URL of frames[1] should have changed";
    EXPECT_EQ(2u, pm->GetAllFrames().size());
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
    EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  }
}

// Verify that a web popup created via window.open from an extension page can
// communicate with the extension page via window.opener.  See
// https://crbug.com/590068.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       WebPopupFromExtensionMainFrameHasValidOpener) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to an extension page.
  NavigateToURL(extension->GetResourceURL("empty.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();

  // Open a new web popup from the extension tab.  The popup should go into a
  // new process.
  GURL popup_url(embedded_test_server()->GetURL("/empty.html"));
  content::WebContents* popup = OpenPopup(main_frame, popup_url);
  EXPECT_NE(popup, tab);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_NE(popup->GetPrimaryMainFrame()->GetProcess(),
            main_frame->GetProcess());

  // Ensure the popup's window.opener is defined.
  bool is_opener_defined = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup, "window.domAutomationController.send(!!window.opener)",
      &is_opener_defined));
  EXPECT_TRUE(is_opener_defined);

  // Verify that postMessage to window.opener works.
  VerifyPostMessageToOpener(popup->GetPrimaryMainFrame(), main_frame);
}

// Verify that a web popup created via window.open from an extension subframe
// can communicate with the extension page via window.opener.  Similar to the
// test above, but for subframes.  See https://crbug.com/590068.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       WebPopupFromExtensionSubframeHasValidOpener) {
  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page with a blank iframe.  There should be no
  // extension frames yet.
  NavigateToURL(embedded_test_server()->GetURL("/blank_iframe.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate first subframe to an extension URL.
  const GURL extension_url(extension->GetResourceURL("empty.html"));
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0", extension_url));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_frame = ChildFrameAt(main_frame, 0);

  // Open a new web popup from extension frame.  The popup should go into main
  // frame's web process.
  GURL popup_url(embedded_test_server()->GetURL("/empty.html"));
  content::WebContents* popup = OpenPopup(extension_frame, popup_url);
  EXPECT_NE(popup, tab);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_NE(popup->GetPrimaryMainFrame()->GetProcess(),
            extension_frame->GetProcess());
  EXPECT_EQ(popup->GetPrimaryMainFrame()->GetProcess(),
            main_frame->GetProcess());

  // Ensure the popup's window.opener is defined.
  bool is_opener_defined = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup, "window.domAutomationController.send(!!window.opener)",
      &is_opener_defined));
  EXPECT_TRUE(is_opener_defined);

  // Verify that postMessage to window.opener works.
  VerifyPostMessageToOpener(popup->GetPrimaryMainFrame(), extension_frame);
}

// Test that when a web site has an extension iframe, navigating that iframe to
// a different web site without --site-per-process will place it in the parent
// frame's process.  See https://crbug.com/711006.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       ExtensionFrameNavigatesToParentSiteInstance) {
  // This test matters only *without* --site-per-process.
  if (content::AreAllSitesIsolatedForTesting())
    return;

  // Create a simple extension without a background page.
  const Extension* extension = CreateExtension("Extension", false);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page with a blank iframe.  There should be no
  // extension frames yet.
  NavigateToURL(embedded_test_server()->GetURL("a.com", "/blank_iframe.html"));
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate subframe to an extension URL.  This should go into a new
  // extension process.
  const GURL extension_url(extension->url().Resolve("empty.html"));
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0", extension_url));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  content::RenderFrameHost* main_frame = tab->GetPrimaryMainFrame();
  {
    content::RenderFrameHost* subframe = ChildFrameAt(main_frame, 0);
    EXPECT_NE(subframe->GetProcess(), main_frame->GetProcess());
    EXPECT_NE(subframe->GetSiteInstance(), main_frame->GetSiteInstance());
  }

  // Navigate subframe to b.com.  This should be brought back to the parent
  // frame's (a.com) process.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/empty.html"));
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0", b_url));
  {
    content::RenderFrameHost* subframe = ChildFrameAt(main_frame, 0);
    EXPECT_EQ(subframe->GetProcess(), main_frame->GetProcess());
    if (content::AreStrictSiteInstancesEnabled()) {
      EXPECT_NE(subframe->GetSiteInstance(), main_frame->GetSiteInstance());
    } else {
      EXPECT_EQ(subframe->GetSiteInstance(), main_frame->GetSiteInstance());
    }
  }
}

// Verify that web iframes on extension frames do not attempt to aggressively
// reuse existing processes for the same site.  This helps prevent a
// misbehaving web iframe on an extension from slowing down other processes.
// See https://crbug.com/899418.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       WebSubframeOnExtensionDoesNotReuseExistingProcess) {
  // This test matters only *with* --site-per-process.  It depends on process
  // reuse logic that subframes use to look for existing processes, but that
  // logic is only turned on for sites that require a dedicated process.
  if (!content::AreAllSitesIsolatedForTesting())
    return;

  // Create a simple extension with a background page that has an empty iframe.
  const Extension* extension = CreateExtension("Extension", true);
  embedded_test_server()->ServeFilesFromDirectory(extension->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate main tab to a web page on foo.com.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(foo_url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(foo_url, tab->GetLastCommittedURL());

  // So far, there should be two extension frames: one for the background page,
  // one for the empty subframe on it.
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  // Navigate the subframe on the extension background page to foo.com, and
  // wait for the old subframe to go away.
  ExtensionHost* background_host =
      pm->GetBackgroundHostForExtension(extension->id());
  content::RenderFrameHost* background_rfh =
      background_host->host_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* extension_subframe =
      ChildFrameAt(background_rfh, 0);
  content::RenderFrameDeletedObserver deleted_observer(extension_subframe);
  EXPECT_TRUE(
      content::ExecJs(extension_subframe,
                      content::JsReplace("window.location = $1;", foo_url)));
  deleted_observer.WaitUntilDeleted();

  // There should now only be one extension frame for the background page.  The
  // subframe should've swapped processes and should now be a web frame.
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  content::RenderFrameHost* subframe = ChildFrameAt(background_rfh, 0);
  EXPECT_EQ(foo_url, subframe->GetLastCommittedURL());

  // Verify that the subframe did *not* reuse the existing foo.com process.
  EXPECT_NE(tab->GetPrimaryMainFrame()->GetProcess(), subframe->GetProcess());
}

// Test to verify that loading a resource other than an icon file is
// disallowed for hosted apps, while icons are allowed.
// See https://crbug.com/717626.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HostedAppFilesAccess) {
  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("hosted_app"));
  ASSERT_TRUE(extension);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigating to the manifest should be blocked with an error page.
  {
    content::TestNavigationObserver observer(tab);
    NavigateToURL(extension->GetResourceURL("/manifest.json"));
    EXPECT_FALSE(observer.last_navigation_succeeded());
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_ERROR);
  }

  // Navigation to the icon file should succeed.
  {
    content::TestNavigationObserver observer(tab);
    NavigateToURL(extension->GetResourceURL("/icon.png"));
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(tab->GetController().GetLastCommittedEntry()->GetPageType(),
              content::PAGE_TYPE_NORMAL);
  }
}

// Tests that we correctly account for vanilla web URLs that may be in the
// same SiteInstance as a hosted app, and display alerts correctly.
// https://crbug.com/746517.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HostedAppAlerts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("hosted_app"));
  ASSERT_TRUE(extension);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL hosted_app_url(embedded_test_server()->GetURL(
      "localhost", "/extensions/hosted_app/main.html"));
  {
    content::TestNavigationObserver observer(tab);
    NavigateToURL(hosted_app_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
  EXPECT_EQ(hosted_app_url, tab->GetLastCommittedURL());
  ProcessManager* pm = ProcessManager::Get(profile());
  EXPECT_EQ(extension, pm->GetExtensionForWebContents(tab));
  javascript_dialogs::AppModalDialogManager* js_dialog_manager =
      javascript_dialogs::AppModalDialogManager::GetInstance();
  std::u16string hosted_app_title = u"hosted_app";
  EXPECT_EQ(hosted_app_title,
            js_dialog_manager->GetTitle(
                tab, tab->GetPrimaryMainFrame()->GetLastCommittedOrigin()));

  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::ExecuteScript(
      tab, base::StringPrintf("window.open('%s');", web_url.spec().c_str())));
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_tab, tab);
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));
  EXPECT_EQ(web_url, new_tab->GetLastCommittedURL());
  EXPECT_EQ(nullptr, pm->GetExtensionForWebContents(new_tab));
  EXPECT_NE(
      hosted_app_title,
      js_dialog_manager->GetTitle(
          new_tab, new_tab->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
}

}  // namespace extensions
