// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end adversarial suite for PrivilegedWebContents. Each invariant that
// keeps privileged content contained is exercised here against a live
// PrivilegedWebContents, so the security review has a single artifact to point
// at. The per-CL browsertests remain the focused unit of each mechanism; this
// file is the integration backstop that a regression in any single layer would
// trip.
//
// Coverage in this file:
//   - Process isolation: the privileged main frame and its subframes run in
//     privileged processes, distinct from an ordinary tab on the same origin
//     (renderer-initiated navigations and subframes included).
//   - Origin lock: the main frame cannot leave the navigation allowlist, via a
//     renderer-initiated navigation or a server open-redirect.
//   - Capability bridge gating: only the fully-qualified main frame binds the
//     bridge; a subframe or a navigation-only origin is terminated.
//
// Invariants with heavier or separately-staged fixtures keep their dedicated
// suites, which this file deliberately does not duplicate:
//   - New-window denial: PrivilegedWebContentsBrowserTest.WindowOpenReturnsNull
//     in privileged_web_contents_browsertest.cc.
//   - Service-worker stack: ServiceWorkerBrowserTest.PrivilegedWebContents{
//     CannotRegisterServiceWorker, CannotBeClaimed, SkipsServiceWorker,
//     HiddenFromMatchAll} in
//     content/browser/service_worker/service_worker_browsertest.cc, and
//     WorkerTest.PrivilegedWebContentsCannotUseSharedWorker in
//     content/browser/worker_host/worker_browsertest.cc.
//   - No prerender / no bfcache activation:
//     PrivilegedWebContentsTest.DisablesPrerendering (unittest) and
//     PrivilegedWebContentsBrowserTest.DisablesBackForwardCache in
//     privileged_web_contents_browsertest.cc.
//   - Extension invisibility:
//     ContentScriptApiTest.NoInjectionIntoPrivilegedWebContents,
//     DeclarativeNetRequestBrowserTest.PrivilegedWebContentsExemptFromDNR, and
//     PwcDebuggerApiTest.CannotAttachToPrivilegedWebContents.

#include <memory>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc.mojom.h"
#include "chrome/browser/pwc/pwc_api_binder.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace pwc {
namespace {

class PwcSecurityBrowserTest : public InProcessBrowserTest {
 public:
  PwcSecurityBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    // Standard test pages (/title1.html and friends) and default handlers,
    // including /server-redirect?<target>.
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  // A PrivilegedWebContents whose capability allowlist is {`capability`} and
  // whose navigation allowlist is {`capability`, `navigation_only`}.
  std::unique_ptr<PrivilegedWebContents> MakePrivileged(
      const GURL& capability,
      const GURL& navigation_only) {
    return PrivilegedWebContents::Create(
        PrivilegedComponent::kTestComponent, browser()->GetProfile(),
        std::make_unique<FixedPwcPolicyDelegate>(
            std::vector<url::Origin>{url::Origin::Create(capability),
                                     url::Origin::Create(navigation_only)},
            std::vector<url::Origin>{url::Origin::Create(capability)}));
  }

  // Attaches a subframe at `url` to the main frame and returns it.
  content::RenderFrameHost* AddSubframe(content::WebContents* web_contents,
                                        const GURL& url) {
    EXPECT_TRUE(content::ExecJs(web_contents, content::JsReplace(R"(
      new Promise((resolve) => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.onload = () => resolve(true);
        document.body.appendChild(f);
      })
    )",
                                                                 url)));
    return content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The privileged main frame runs in a privileged process, and an ordinary tab
// navigated to the very same origin does not -- the process model is keyed on
// the privileged context, not the URL.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest, MainFrameProcessIsPrivileged) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  EXPECT_TRUE(web_contents->IsPrivileged());
  content::RenderProcessHost* privileged_process =
      web_contents->GetPrimaryMainFrame()->GetProcess();
  EXPECT_TRUE(privileged_process->IsPrivileged());

  // An ordinary tab on the same origin is not privileged and cannot share the
  // privileged process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), capability));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(tab->IsPrivileged());
  content::RenderProcessHost* tab_process =
      tab->GetPrimaryMainFrame()->GetProcess();
  EXPECT_FALSE(tab_process->IsPrivileged());
  EXPECT_NE(privileged_process, tab_process);
}

// A cross-origin subframe of the privileged main frame also runs in a
// privileged process (all frames of a PrivilegedWebContents inherit the
// privileged process model), still distinct from an ordinary tab's process.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest, SubframeProcessIsPrivileged) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  const GURL subframe_url = https_server_.GetURL("c.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  // Navigate an ordinary tab to the subframe's origin first, so that when the
  // PWC subframe picks its process below there is an existing same-origin
  // ordinary process it must *not* reuse.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), subframe_url));
  content::RenderProcessHost* tab_process = browser()
                                                ->tab_strip_model()
                                                ->GetActiveWebContents()
                                                ->GetPrimaryMainFrame()
                                                ->GetProcess();
  EXPECT_FALSE(tab_process->IsPrivileged());

  content::RenderFrameHost* subframe = AddSubframe(web_contents, subframe_url);
  ASSERT_TRUE(subframe);
  EXPECT_TRUE(subframe->GetProcess()->IsPrivileged());
  // Site isolation is in effect inside the PWC: the cross-origin subframe does
  // not share the privileged main frame's process.
  EXPECT_NE(web_contents->GetPrimaryMainFrame()->GetProcess(),
            subframe->GetProcess());
  EXPECT_NE(subframe->GetProcess(), tab_process);
}

// A renderer-initiated navigation cannot move the main frame off the
// navigation allowlist.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest,
                       RendererInitiatedNavigationIsLocked) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  const GURL off_allowlist = https_server_.GetURL("evil.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  content::TestNavigationManager nav(web_contents, off_allowlist);
  ASSERT_TRUE(content::ExecJs(
      web_contents, content::JsReplace("location.href = $1;", off_allowlist)));
  ASSERT_TRUE(nav.WaitForNavigationFinished());
  EXPECT_FALSE(nav.was_committed());
  EXPECT_EQ(capability, web_contents->GetLastCommittedURL());
}

// An allowlisted navigation that server-redirects off the allowlist is
// cancelled at the redirect: the open redirect cannot smuggle an off-allowlist
// origin into the locked main frame.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest,
                       OpenRedirectOffAllowlistBlocked) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  const GURL off_allowlist = https_server_.GetURL("evil.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  // a.test is allowlisted, so the request starts; the server then redirects to
  // evil.test and the throttle cancels at the redirect. NavigateToURL returns
  // false because the navigation does not commit the requested URL; the main
  // frame stays put.
  const GURL redirector = https_server_.GetURL(
      "a.test", base::StrCat({"/server-redirect?", off_allowlist.spec()}));
  EXPECT_FALSE(content::NavigateToURL(web_contents, redirector));
  EXPECT_EQ(capability, web_contents->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(capability),
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

// The capability bridge binds for the fully-qualified main frame and rejects
// everything else. This mirrors the dedicated binder suite but runs it against
// the same live PWC as the rest of the invariants, so a cross-layer regression
// (e.g. the process model or the navigation lock breaking the gate's
// assumptions) is caught here too.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest, BridgeGating) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  const GURL subframe_url = https_server_.GetURL("c.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  // Qualifying main frame: the bind succeeds (FlushForTesting waits until the
  // browser end accepted or dropped the receiver, so connectivity afterwards
  // is authoritative).
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  {
    mojo::Remote<mojom::PrivilegedBridge> remote;
    BindPrivilegedBridge(web_contents->GetPrimaryMainFrame(),
                         remote.BindNewPipeAndPassReceiver());
    remote.FlushForTesting();
    EXPECT_TRUE(remote.is_connected());
  }

  // A subframe of the PWC is not the primary main frame: the request kills the
  // subframe's renderer.
  content::RenderFrameHost* subframe = AddSubframe(web_contents, subframe_url);
  ASSERT_TRUE(subframe);
  {
    content::RenderProcessHostWatcher exit(
        subframe->GetProcess(),
        content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    mojo::Remote<mojom::PrivilegedBridge> remote;
    BindPrivilegedBridge(subframe, remote.BindNewPipeAndPassReceiver());
    exit.Wait();
    EXPECT_FALSE(subframe->GetProcess()->IsInitializedAndNotDead());
  }
}

// A main frame on a navigation-only (non-capability) origin must not receive
// the bridge; the request kills the renderer.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest, BridgeDeniedOnNavigationOnly) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, nav_only));

  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::RenderProcessHostWatcher exit(
      frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(frame, remote.BindNewPipeAndPassReceiver());
  exit.Wait();
  EXPECT_FALSE(frame->GetProcess()->IsInitializedAndNotDead());
}

// Holding the bridge on the capability origin confers nothing on a successor
// document: after the main frame navigates to a navigation-only origin, a bind
// attempt for the new document's process is unqualified and kills its renderer,
// exactly as if the bridge had never been bound.
IN_PROC_BROWSER_TEST_F(PwcSecurityBrowserTest,
                       BridgeDeniedAfterNavigatingAwayFromCapability) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/title1.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  // Bind successfully on the capability origin.
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(web_contents->GetPrimaryMainFrame(),
                       remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  // Navigate to the navigation-only origin and try to bind for the new main
  // frame: its process is killed.
  ASSERT_TRUE(content::NavigateToURL(web_contents, nav_only));
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  content::RenderProcessHostWatcher exit(
      frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  mojo::Remote<mojom::PrivilegedBridge> second;
  BindPrivilegedBridge(frame, second.BindNewPipeAndPassReceiver());
  exit.Wait();
  EXPECT_FALSE(frame->GetProcess()->IsInitializedAndNotDead());
}

}  // namespace
}  // namespace pwc
