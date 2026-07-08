// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance_process_assignment.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "net/dns/mock_host_resolver.h"
#include "url/origin.h"

namespace extensions {
namespace {

constexpr char kHostA[] = "a.com";
constexpr char kHostB[] = "b.com";
constexpr char kHostC[] = "c.com";
constexpr char kHostD[] = "d.com";
constexpr char kPdfPath[] = "/test.pdf";
constexpr char kTitle1Path[] = "/title1.html";
constexpr char kHelperHtml[] = "helper.html";
constexpr char kCrossOriginPath[] = "/cross_origin.html";
constexpr char kEmbedHostPath[] = "/embed_host.html";
constexpr char kExtensionDir[] = "mime_handler_isolation";

}  // namespace

class MimeHandlerProcessIsolationBrowserTest : public ExtensionApiTest {
 public:
  MimeHandlerProcessIsolationBrowserTest() = default;

  MimeHandlerProcessIsolationBrowserTest(
      const MimeHandlerProcessIsolationBrowserTest&) = delete;
  MimeHandlerProcessIsolationBrowserTest& operator=(
      const MimeHandlerProcessIsolationBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // Serve `test.pdf` from the shared PDF test data directory; serve
    // helper assets from the test extension directory at separate paths
    // so the test extension can request `cross_origin.html` from the test
    // server.
    const base::FilePath chrome_test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    embedded_test_server()->ServeFilesFromDirectory(
        chrome_test_data_dir.AppendASCII("pdf"));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII(kExtensionDir));
    ASSERT_TRUE(StartEmbeddedTestServer());
    extension_ = LoadExtension(test_data_dir_.AppendASCII(kExtensionDir));
    ASSERT_TRUE(extension_);
  }

  void TearDownOnMainThread() override {
    // `extension_` is a non-owning raw_ptr; the Extension is owned by the
    // ExtensionRegistry and torn down by the parent. Reset before the
    // parent's teardown to avoid a dangling raw_ptr.
    extension_ = nullptr;
    ExtensionApiTest::TearDownOnMainThread();
  }

  // Navigates the active tab to `pdf_url` and waits for the MIME handler
  // extension frame to load. Returns the extension frame, or nullptr on
  // failure.
  content::RenderFrameHost* NavigateToPdfAndGetExtensionFrame(
      const GURL& pdf_url) {
    content::WebContents* web_contents = GetActiveWebContents();
    // `NavigateToURL()` already blocks until the load finishes. The hard stop
    // for a failed navigation is the caller's `ASSERT_TRUE()` on the returned
    // frame, which is null when no extension frame committed.
    EXPECT_TRUE(content::NavigateToURL(web_contents, pdf_url));
    return FindExtensionFrame(web_contents);
  }

  // Returns the single chrome-extension:// frame in `web_contents`, or nullptr
  // if there is not exactly one. The PDF helpers in pdf_extension_test_util
  // filter on the built-in PDF extension origin, so they do not apply to the
  // generic handler; this mirrors their `ForEachRenderFrameHost()` walk.
  content::RenderFrameHost* FindExtensionFrame(
      content::WebContents* web_contents) {
    std::vector<content::RenderFrameHost*> frames =
        FindAllExtensionFrames(web_contents);
    return frames.size() == 1 ? frames[0] : nullptr;
  }

  // Returns all chrome-extension:// frames in `web_contents`.
  std::vector<content::RenderFrameHost*> FindAllExtensionFrames(
      content::WebContents* web_contents) {
    std::vector<content::RenderFrameHost*> extension_frames;
    web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
      if (rfh->GetLastCommittedOrigin().scheme() == kExtensionScheme) {
        extension_frames.push_back(rfh);
      }
    });
    return extension_frames;
  }

  // Appends an iframe under `parent_frame` pointing at `iframe_url` and waits
  // for it to load. Returns the child `RenderFrameHost` at `child_index`
  // (0 for the first appended child, 1 for the second, and so on).
  content::RenderFrameHost* AppendChildFrame(
      content::RenderFrameHost* parent_frame,
      const GURL& iframe_url,
      size_t child_index = 0) {
    constexpr char kAppendFrameScript[] = R"(
        new Promise(resolve => {
          const f = document.createElement('iframe');
          f.src = $1;
          f.onload = () => resolve(true);
          document.body.appendChild(f);
        });
        )";
    EXPECT_EQ(true, content::EvalJs(
                        parent_frame,
                        content::JsReplace(kAppendFrameScript, iframe_url)));
    return content::ChildFrameAt(parent_frame, child_index);
  }

  raw_ptr<const Extension> extension_ = nullptr;

 private:
  // `kApiMimeHandler` enables the generic-MIME-handler path that the
  // test extension uses.
  base::test::ScopedFeatureList scoped_feature_list_{
      extensions_features::kApiMimeHandler};
};

// Fixture variant that additionally enables BFCache features so the
// `BFCacheRestorePreservesIsolation` test exercises the cached-restore
// path.
class MimeHandlerProcessIsolationBFCacheBrowserTest
    : public MimeHandlerProcessIsolationBrowserTest {
 public:
  MimeHandlerProcessIsolationBFCacheBrowserTest() {
    bfcache_feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(),
        content::GetDefaultDisabledBackForwardCacheFeaturesForTesting());
  }

 private:
  base::test::ScopedFeatureList bfcache_feature_list_;
};

// Verifies that extension frames serving PDFs from different hosts, loaded in
// two separate tabs, are placed in different renderer processes (cross-site
// process isolation).
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       CrossSiteProcessIsolation) {
  GURL pdf_url_a = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* frame_a =
      NavigateToPdfAndGetExtensionFrame(pdf_url_a);
  ASSERT_TRUE(frame_a);
  content::RenderProcessHost* process_a = frame_a->GetProcess();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  GURL pdf_url_b = embedded_test_server()->GetURL(kHostB, kPdfPath);
  content::RenderFrameHost* frame_b =
      NavigateToPdfAndGetExtensionFrame(pdf_url_b);
  ASSERT_TRUE(frame_b);
  content::RenderProcessHost* process_b = frame_b->GetProcess();

  EXPECT_NE(process_a->GetID(), process_b->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(frame_a));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(frame_b));
}

// Two separate top-level PDF loads from the SAME host are two distinct MIME
// handler instances, so their viewers run in different processes despite the
// shared site:
//
//   tab 1: a.com/test.pdf  ->  viewer (instance N1)
//   tab 2: a.com/test.pdf  ->  viewer (instance N2)      N1 != N2
//
// The per-instance id, not the site, is what separates them.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       SameSiteProcessIsolation) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* frame_a =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(frame_a);
  content::RenderProcessHost* process_a = frame_a->GetProcess();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::RenderFrameHost* frame_b =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(frame_b);
  content::RenderProcessHost* process_b = frame_b->GetProcess();

  EXPECT_NE(process_a->GetID(), process_b->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(frame_a));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(frame_b));
}

// A same-extension-URL iframe under the viewer shares the viewer's process:
//
//   a.com/test.pdf
//     +-- chrome-extension://.../viewer         (instance N)
//           +-- chrome-extension://.../helper    (same extension, inherits N)
//
// The child has the same site (the extension) and the same instance id N, so
// it maps to the viewer's SiteInstance. (Cross-origin children are split by
// site-per-process; a different viewer instance is split by id mismatch -- see
// the tests below.)
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       IframeInheritsInstanceProcess) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  content::RenderFrameHost* iframe =
      AppendChildFrame(viewer_frame, extension_->GetResourceURL(kHelperHtml));
  ASSERT_TRUE(iframe);

  EXPECT_EQ(viewer_frame->GetProcess()->GetID(), iframe->GetProcess()->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe));
}

// Two same-origin web-content children of ONE viewer instance:
//
//   a.com/test.pdf
//     +-- chrome-extension://.../viewer       (instance N)
//           +-- child 1: a.com/cross_origin   (web content)
//           +-- child 2: a.com/cross_origin   (web content)
//
// The two children share a single process: same site (a.com) and same instance
// id N map them to the same SiteInstance. Per-instance isolation is not
// per-frame -- same-origin frames must co-locate so they can script each other.
// Both children stay in an isolated MIME handler process, distinct from the
// plain a.com embedder process (`HasUniqueInstanceIsolation` would fail if a
// child had fallen back into the plain a.com embedder process).
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       SameOriginChildrenShareInstanceProcess) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer = NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer);

  GURL child_url = embedded_test_server()->GetURL(kHostA, kCrossOriginPath);
  content::RenderFrameHost* child1 =
      AppendChildFrame(viewer, child_url, /*child_index=*/0);
  ASSERT_TRUE(child1);
  content::RenderFrameHost* child2 =
      AppendChildFrame(viewer, child_url, /*child_index=*/1);
  ASSERT_TRUE(child2);

  EXPECT_EQ(child1->GetProcess()->GetID(), child2->GetProcess()->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(child1));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(child2));
  EXPECT_TRUE(
      content::HaveEmbedderIsolationWithSameUniqueInstance(child1, child2));
  EXPECT_TRUE(
      content::HaveEmbedderIsolationWithSameUniqueInstance(child1, viewer));
}

// Two cross-origin web-content children of ONE viewer instance:
//
//   a.com/test.pdf
//     +-- chrome-extension://.../viewer       (instance N)
//           +-- child 1: c.com/cross_origin   (web content)
//           +-- child 2: d.com/cross_origin   (web content)
//
// Site-per-process splits the children into separate processes (c.com vs
// d.com), but both inherit instance id N, so both stay in isolated MIME handler
// processes and neither can mix with a different instance's frames.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       CrossOriginChildrenSplitWithinInstance) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer = NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer);

  GURL child_c = embedded_test_server()->GetURL(kHostC, kCrossOriginPath);
  GURL child_d = embedded_test_server()->GetURL(kHostD, kCrossOriginPath);
  content::RenderFrameHost* child1 =
      AppendChildFrame(viewer, child_c, /*child_index=*/0);
  ASSERT_TRUE(child1);
  content::RenderFrameHost* child2 =
      AppendChildFrame(viewer, child_d, /*child_index=*/1);
  ASSERT_TRUE(child2);

  EXPECT_NE(child1->GetProcess()->GetID(), child2->GetProcess()->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(child1));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(child2));
}

// Reloading the iframe must keep it in the parent viewer's isolated process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       SubframeReloadKeepsParentIsolationId) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  content::RenderFrameHost* iframe =
      AppendChildFrame(viewer_frame, extension_->GetResourceURL(kHelperHtml));
  ASSERT_TRUE(iframe);
  content::ChildProcessId parent_process_id_before =
      viewer_frame->GetProcess()->GetID();
  content::ChildProcessId iframe_process_id_before =
      iframe->GetProcess()->GetID();
  ASSERT_EQ(parent_process_id_before, iframe_process_id_before);

  // Trigger an iframe reload from the parent's JS context. After the reload,
  // re-acquire the iframe RFH because reload may swap RFHs.
  content::TestNavigationObserver nav_observer(
      content::WebContents::FromRenderFrameHost(viewer_frame));
  ASSERT_TRUE(
      content::ExecJs(viewer_frame,
                      "document.querySelector('iframe').contentWindow.location"
                      ".reload();"));
  nav_observer.Wait();

  content::RenderFrameHost* iframe_after =
      content::ChildFrameAt(viewer_frame, 0);
  ASSERT_TRUE(iframe_after);

  EXPECT_EQ(viewer_frame->GetProcess()->GetID(),
            iframe_after->GetProcess()->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe_after));
}

// Two simultaneous viewers each with one iframe form exactly two distinct
// processes (each viewer pair shares one). Neither pair shares a process
// with the other; neither is the shared extension process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       IframeIsolationAcrossInstances) {
  // Viewer 1 in the active tab.
  GURL pdf_url_a = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer1 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_a);
  ASSERT_TRUE(viewer1);
  content::RenderFrameHost* iframe1 =
      AppendChildFrame(viewer1, extension_->GetResourceURL(kHelperHtml));
  ASSERT_TRUE(iframe1);

  // Viewer 2 in a fresh tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  GURL pdf_url_b = embedded_test_server()->GetURL(kHostB, kPdfPath);
  content::RenderFrameHost* viewer2 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_b);
  ASSERT_TRUE(viewer2);
  content::RenderFrameHost* iframe2 =
      AppendChildFrame(viewer2, extension_->GetResourceURL(kHelperHtml));
  ASSERT_TRUE(iframe2);

  // Pair-internal: each viewer shares its iframe's process.
  EXPECT_EQ(viewer1->GetProcess()->GetID(), iframe1->GetProcess()->GetID());
  EXPECT_EQ(viewer2->GetProcess()->GetID(), iframe2->GetProcess()->GetID());
  // Pair-external: pairs do not share a process.
  EXPECT_NE(viewer1->GetProcess()->GetID(), viewer2->GetProcess()->GetID());
  // Both pair processes are isolated MIME handler processes.
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer1));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer2));
}

// Two cross-origin iframes that load the same URL but belong to two
// different MIME handler viewer instances must run in different processes,
// and each must run in an isolated MIME handler process.
//
// Site-per-process splits the cross-origin iframe from its viewer parent;
// the per-instance isolation id then keeps the two cross-origin iframes
// of the two viewer instances apart from each other.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       CrossOriginIframeIsolationAcrossInstances) {
  // Viewer 1 in the active tab.
  GURL pdf_url_a = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer1 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_a);
  ASSERT_TRUE(viewer1);
  GURL cross_origin_url =
      embedded_test_server()->GetURL(kHostC, kCrossOriginPath);
  content::RenderFrameHost* iframe1 =
      AppendChildFrame(viewer1, cross_origin_url);
  ASSERT_TRUE(iframe1);

  // Viewer 2 in a fresh tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  GURL pdf_url_b = embedded_test_server()->GetURL(kHostB, kPdfPath);
  content::RenderFrameHost* viewer2 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_b);
  ASSERT_TRUE(viewer2);
  content::RenderFrameHost* iframe2 =
      AppendChildFrame(viewer2, cross_origin_url);
  ASSERT_TRUE(iframe2);

  // Cross-origin iframes inherit isolation from their viewer parents, so
  // each must run in an isolated MIME handler process (not the shared
  // extension process, not a generic web-content process).
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe1));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe2));

  // Per-instance discrimination: cross-origin iframes from different
  // viewers must not share a process.
  EXPECT_NE(iframe1->GetProcess()->GetID(), iframe2->GetProcess()->GetID());

  // Cross-instance contamination guards: an iframe of one viewer must not
  // land in the other viewer's process.
  EXPECT_NE(iframe1->GetProcess()->GetID(), viewer2->GetProcess()->GetID());
  EXPECT_NE(iframe2->GetProcess()->GetID(), viewer1->GetProcess()->GetID());
}

// A MIME handler viewer's cross-origin child is web content, so unlike the
// viewer it is eligible for the warm spare process. Taking the spare must not
// collapse per-instance isolation: once instance 1's child has taken the
// spare and locked it to instance 1's id, instance 2's same-origin child must
// not be placed in that now-locked process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       CrossOriginChildUsesSpareButStaysIsolated) {
  auto& spare_manager = content::SpareRenderProcessHostManager::Get();
  GURL cross_origin_url =
      embedded_test_server()->GetURL(kHostC, kCrossOriginPath);

  // Viewer 1 in the active tab. Clear any auto-warmed spare, warm a fresh one
  // and wait until it is ready, so the cross-origin child's process selection
  // can take it.
  GURL pdf_url_a = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer1 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_a);
  ASSERT_TRUE(viewer1);
  spare_manager.CleanupSparesForTesting();
  content::SpareRenderProcessHostStartedObserver spare_observer;
  spare_manager.WarmupSpare(browser()->GetProfile());
  spare_observer.WaitForSpareRenderProcessStarted();
  content::RenderFrameHost* iframe1 =
      AppendChildFrame(viewer1, cross_origin_url);
  ASSERT_TRUE(iframe1);
  // The child took the spare, which is then locked to instance 1's id.
  EXPECT_EQ(content::SiteInstanceProcessAssignment::USED_SPARE_PROCESS,
            iframe1->GetSiteInstance()->GetLastProcessAssignmentOutcome());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe1));

  // Viewer 2 in a fresh tab, with no spare of its own (the former spare is now
  // locked to instance 1). Instance 2's child must not reuse that locked
  // process -- `IsSuitableHost()` rejects it on the per-instance id -- so it
  // lands in its own isolated process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  GURL pdf_url_b = embedded_test_server()->GetURL(kHostB, kPdfPath);
  content::RenderFrameHost* viewer2 =
      NavigateToPdfAndGetExtensionFrame(pdf_url_b);
  ASSERT_TRUE(viewer2);
  spare_manager.CleanupSparesForTesting();
  content::RenderFrameHost* iframe2 =
      AppendChildFrame(viewer2, cross_origin_url);
  ASSERT_TRUE(iframe2);
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(iframe2));
  EXPECT_NE(iframe1->GetProcess()->GetID(), iframe2->GetProcess()->GetID());
}

// Navigating away from the viewer and then back must not fall back to the
// shared extension process. The back-navigated viewer's process must be
// classified as an isolated MIME handler process (either the same one as
// before or a fresh one is acceptable).
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       BackForwardPreservesIsolation) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  // Navigate away to a non-extension URL.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL away_url = embedded_test_server()->GetURL(kHostA, kTitle1Path);
  ASSERT_TRUE(content::NavigateToURL(web_contents, away_url));

  // Go back.
  content::TestNavigationObserver back_observer(web_contents);
  web_contents->GetController().GoBack();
  back_observer.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* viewer_after = FindExtensionFrame(web_contents);
  ASSERT_TRUE(viewer_after);
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer_after));
}

// Reloading the viewer must keep the bit set on the resulting process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       ReloadPreservesIsolation) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  content::WebContents* web_contents = GetActiveWebContents();
  content::TestNavigationObserver reload_observer(web_contents);
  web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/false);
  reload_observer.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* viewer_after = FindExtensionFrame(web_contents);
  ASSERT_TRUE(viewer_after);
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer_after));
}

// Navigating away from a viewer and going back must produce a viewer in
// an isolated MIME handler process. Either outcome is acceptable: BFCache
// restores the original RFH (and therefore the original isolated process),
// or BFCache misses and the fresh navigation runs through
// `MimeHandlerStreamManager` again, which restamps the flag and produces
// a fresh isolated process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBFCacheBrowserTest,
                       BFCacheRestorePreservesIsolation) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  // Navigate away. With BFCache enabled in the fixture, the prior page's RFH
  // may be cached.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL away_url = embedded_test_server()->GetURL(kHostB, kTitle1Path);
  ASSERT_TRUE(content::NavigateToURL(web_contents, away_url));

  // Go back; either BFCache restores the original page or the navigation
  // re-runs through the stream manager.
  content::TestNavigationObserver back_observer(web_contents);
  web_contents->GetController().GoBack();
  back_observer.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* viewer_after = FindExtensionFrame(web_contents);
  ASSERT_TRUE(viewer_after);
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer_after));
}

// Closing the viewer's tab and restoring it via TabRestoreService must land
// the restored navigation in an isolated MIME handler process. The
// restoration path re-fires a fresh navigation through the network stack;
// MimeHandlerStreamManager runs again and re-stamps the bit.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       SessionRestorePreservesIsolation) {
  // Open the viewer in a new tab so the original tab can stay open while we
  // close the viewer tab. (Closing the only tab in a browser closes the
  // browser too, which is more disruptive than this test needs.)
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);

  Profile* profile = browser()->profile();
  sessions::TabRestoreService* trs =
      TabRestoreServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(trs);
  TabRestoreServiceLoadWaiter(trs).Wait();

  // Close the viewer tab. TabRestoreService records an entry synchronously
  // during the close.
  chrome::CloseTab(browser());
  ASSERT_FALSE(trs->entries().empty());

  // Restore the most recent entry.
  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  chrome::RestoreTab(browser());
  content::WebContents* restored_wc = tab_waiter.Wait();
  ASSERT_TRUE(restored_wc);
  ASSERT_TRUE(content::WaitForLoadStop(restored_wc));

  content::RenderFrameHost* viewer_after = FindExtensionFrame(restored_wc);
  ASSERT_TRUE(viewer_after);
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer_after));
}

// A renderer-initiated top-level navigation away from the viewer through a
// server redirect must NOT inherit the isolated MIME handler bit. The
// destination's process must not be the viewer's isolated process.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       RedirectChainDoesNotInheritFlag) {
  GURL pdf_url = embedded_test_server()->GetURL(kHostA, kPdfPath);
  content::RenderFrameHost* viewer_frame =
      NavigateToPdfAndGetExtensionFrame(pdf_url);
  ASSERT_TRUE(viewer_frame);
  content::ChildProcessId viewer_process_id =
      viewer_frame->GetProcess()->GetID();
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewer_frame));

  // Trigger a renderer-initiated top-level navigation through a cross-site
  // server redirect to a non-extension URL.
  content::WebContents* web_contents = GetActiveWebContents();
  GURL destination = embedded_test_server()->GetURL(kHostA, kTitle1Path);
  GURL redirect_url = embedded_test_server()->GetURL(
      kHostB, "/server-redirect?" + destination.spec());
  content::TestNavigationObserver nav_observer(web_contents);
  ASSERT_TRUE(content::ExecJs(
      web_contents->GetPrimaryMainFrame(),
      content::JsReplace("window.location.href = $1;", redirect_url.spec())));
  nav_observer.Wait();
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  content::RenderFrameHost* dest_main_frame =
      web_contents->GetPrimaryMainFrame();
  ASSERT_EQ(destination, dest_main_frame->GetLastCommittedURL());
  EXPECT_FALSE(content::HasUniqueInstanceIsolation(dest_main_frame));
  EXPECT_NE(viewer_process_id, dest_main_frame->GetProcess()->GetID());
}

// Embedded-load entry path: a top-level page on `kHostA` embeds the PDF in
// an iframe. With `can_embed: true` in the manifest, the MIME handler is
// selected for the embedded load too. The inner viewer must still get a
// per-instance isolated process distinct from the embedder.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       EmbeddedLoadProducesIsolatedViewer) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL host_url = embedded_test_server()->GetURL(kHostA, kEmbedHostPath);
  ASSERT_TRUE(content::NavigateToURL(web_contents, host_url));

  // The host page injects an iframe pointing at the PDF; wait for the
  // extension frame to appear inside the tree.
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  content::RenderFrameHost* extension_frame = FindExtensionFrame(web_contents);
  ASSERT_TRUE(extension_frame);

  // Embedded entry must still produce a per-instance isolated process,
  // distinct from the embedder main frame's process.
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(extension_frame));
  EXPECT_NE(web_contents->GetPrimaryMainFrame()->GetProcess()->GetID(),
            extension_frame->GetProcess()->GetID());
}

// Two PDF iframes in ONE top-level page (a.com/test.pdf + b.com/test.pdf)
// resolve to two distinct MIME handler viewer instances. The two extension
// viewer frames share the same extension site in one frame tree, so they would
// normally co-locate; they end up in different processes only because their
// per-instance isolation ids differ. This is the most direct test of
// unique-instance isolation, free of cross-tab process-reuse confounds.
IN_PROC_BROWSER_TEST_F(MimeHandlerProcessIsolationBrowserTest,
                       TwoEmbeddedViewersInOneTreeAreIsolated) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL host_url = embedded_test_server()->GetURL(kHostA, kTitle1Path);
  ASSERT_TRUE(content::NavigateToURL(web_contents, host_url));

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  GURL pdf_url_a = embedded_test_server()->GetURL(kHostA, kPdfPath);
  GURL pdf_url_b = embedded_test_server()->GetURL(kHostB, kPdfPath);

  // Inject two PDF iframes and wait for the full frame tree (including the
  // nested extension viewer frames) to finish loading.
  AppendChildFrame(main_frame, pdf_url_a, /*child_index=*/0);
  AppendChildFrame(main_frame, pdf_url_b, /*child_index=*/1);
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));

  // The MIME handler architecture creates an extension viewer iframe nested
  // inside each PDF embedder frame. Collect all extension frames; there must
  // be exactly two (one per embedded PDF).
  std::vector<content::RenderFrameHost*> viewers =
      FindAllExtensionFrames(web_contents);
  ASSERT_EQ(2u, viewers.size());

  // Same extension site in one frame tree → normally co-locates.
  // Unique-instance isolation separates them: different per-instance ids map
  // to different SiteInstances and therefore different processes.
  EXPECT_NE(viewers[0]->GetProcess()->GetID(),
            viewers[1]->GetProcess()->GetID());
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewers[0]));
  EXPECT_TRUE(content::HasUniqueInstanceIsolation(viewers[1]));
  EXPECT_FALSE(content::HaveEmbedderIsolationWithSameUniqueInstance(
      viewers[0], viewers[1]));
}

}  // namespace extensions
