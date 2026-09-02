// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/pwc_api_binder.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc.mojom.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content("<html><body>ok</body></html>");
  return response;
}

// Binds the bridge for `frame` and asserts the receiver was accepted.
// FlushForTesting() waits until the browser end either acknowledged the pipe
// or dropped it, so connectivity afterwards is an authoritative verdict.
void ExpectBindSucceeds(content::RenderFrameHost* frame) {
  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(frame, remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

// Binds the bridge for `frame` and asserts the pending receiver is silently
// dropped -- the remote disconnects -- while the renderer stays alive.
void ExpectBindSilentlyDropped(content::RenderFrameHost* frame) {
  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(frame, remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
  EXPECT_TRUE(frame->GetProcess()->IsInitializedAndNotDead());
}

// Binds the bridge for `frame` and asserts the frame's renderer process is
// terminated (via bad_message).
void ExpectBindKillsRenderer(content::RenderFrameHost* frame) {
  content::RenderProcessHostWatcher exit_watcher(
      frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(frame, remote.BindNewPipeAndPassReceiver());
  exit_watcher.Wait();
  EXPECT_FALSE(frame->GetProcess()->IsInitializedAndNotDead());
}

class PwcApiBinderBrowserTest : public InProcessBrowserTest {
 public:
  PwcApiBinderBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  // Builds a PrivilegedWebContents whose capability allowlist is {`capability`}
  // and whose navigation allowlist is {`capability`, `navigation_only`}. The
  // navigation-only origin is reachable by the main frame but never receives
  // the bridge.
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

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The qualifying frame -- the primary main frame of a PrivilegedWebContents
// committed over HTTPS to a capability origin -- binds the bridge. Privileged
// navigations force an origin-keyed process in every configuration, so the
// gate's origin-keyed requirement always holds for a qualifying frame.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, BindsQualifyingMainFrame) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  ExpectBindSucceeds(web_contents->GetPrimaryMainFrame());
}

// A main frame committed to a navigation-only (non-capability) origin -- the
// "sorry page" tier -- must not receive the bridge; the request kills the
// renderer.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, NavigationOnlyOriginKilled) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, nav_only));
  ExpectBindKillsRenderer(web_contents->GetPrimaryMainFrame());
}

// A subframe of a PrivilegedWebContents -- even one on the capability origin --
// is not the primary main frame and must not receive the bridge.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, SubframeKilled) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  const GURL subframe_url = https_server_.GetURL("c.test", "/sub.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  ASSERT_TRUE(content::ExecJs(web_contents, content::JsReplace(R"(
    new Promise((resolve) => {
      const f = document.createElement('iframe');
      f.src = $1;
      f.onload = () => resolve(true);
      document.body.appendChild(f);
    })
  )",
                                                               subframe_url)));
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ExpectBindKillsRenderer(subframe);
}

// Runs with origin-keyed processes unavailable: site isolation is turned off
// and the Origin-Agent-Cluster header machinery is disabled, so the forced
// origin isolation a privileged navigation requests is not applied and the
// privileged process stays site-keyed.
class PwcApiBinderNoOriginIsolationBrowserTest
    : public PwcApiBinderBrowserTest {
 public:
  PwcApiBinderNoOriginIsolationBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kOriginIsolationHeader);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PwcApiBinderBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Where OAC process isolation is unavailable (low-end Android, or site
// isolation turned off), privileged frames are origin-keyed directly through
// their AgentClusterKey, so the qualifying frame still runs in an
// origin-keyed process and the bind succeeds -- the gate's requirement holds
// without a special case.
IN_PROC_BROWSER_TEST_F(PwcApiBinderNoOriginIsolationBrowserTest,
                       QualifyingFrameBindsWithoutOACProcessIsolation) {
  ASSERT_FALSE(content::SiteIsolationPolicy::
                   IsProcessIsolationForOriginAgentClusterEnabled());
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  ExpectBindSucceeds(web_contents->GetPrimaryMainFrame());
}

// An ordinary tab (not a PrivilegedWebContents) must not receive the bridge,
// even at the gemini origin: the gate is context-keyed, not URL-keyed.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, OrdinaryTabKilled) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), capability));
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ExpectBindKillsRenderer(tab->GetPrimaryMainFrame());
}

// The fail-closed inheriting-scheme rule -- capability requires a committed
// HTTPS *URL*, not merely an HTTPS origin, so an about:blank/srcdoc/blob:/data:
// document that inherited a capability origin is still rejected -- is enforced
// by the gate but not exercised here: the navigation throttle prevents every
// non-HTTPS main-frame commit, and the one reachable non-HTTPS state (the
// initial, never-navigated empty document) has no live renderer to drive a
// realistic interface request, so binding against it is not a meaningful
// browsertest scenario.

// The host does not proactively tear down receivers on navigation: a bound
// pipe stays live across a main-frame navigation. (In production the renderer
// holds the remote, so a real cross-document navigation severs the pipe
// naturally when the document is destroyed; there is no navigation-driven drop
// in the browser. Same-document navigations, which do not destroy the document,
// must keep the bridge.) The receiver here is held browser-side, so it isolates
// the "no proactive drop" behavior from the renderer-side teardown.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, BridgeSurvivesMainFrameNav) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL capability2 = https_server_.GetURL("a.test", "/cap2.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  mojo::Remote<mojom::PrivilegedBridge> remote;
  BindPrivilegedBridge(web_contents->GetPrimaryMainFrame(),
                       remote.BindNewPipeAndPassReceiver());
  // Confirm the pipe is live before navigating.
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  // Navigate the main frame to another capability URL. The host does not
  // clear the receiver, so the browser-side pipe stays connected.
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability2));
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

// A legitimate bind request can be in flight while the browser commits a
// cross-document navigation that moves the requesting document out of the
// primary page (pending deletion). That request is fully structurally
// qualified -- it is not a compromised renderer -- so it must be dropped
// without terminating the renderer.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest,
                       PendingDeletionFrameDroppedWithoutKill) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  // Keep the capability document alive in pending deletion across a
  // cross-document navigation (a cross-origin one, which always swaps
  // RenderFrameHosts) instead of letting it be destroyed.
  content::RenderFrameHostWrapper old_frame(
      web_contents->GetPrimaryMainFrame());
  content::LeaveInPendingDeletionState(old_frame.get());
  ASSERT_TRUE(content::NavigateToURL(web_contents, nav_only));
  ASSERT_FALSE(old_frame.IsDestroyed());
  ASSERT_NE(old_frame.get(), web_contents->GetPrimaryMainFrame());
  ASSERT_FALSE(old_frame.get()->IsInPrimaryMainFrame());

  // The old document still satisfies every static requirement (capability
  // origin, HTTPS, outermost main frame of the PWC), so this exercises
  // exactly the lifecycle tier of the gate.
  ExpectBindSilentlyDropped(old_frame.get());
}

// IsCapabilityQualifiedFrame() mirrors the gate as a pure predicate: true
// only for the fully qualifying main frame, false for a subframe, an
// ordinary tab, and a navigation-only origin -- and, having no side effects,
// it never terminates a renderer.
IN_PROC_BROWSER_TEST_F(PwcApiBinderBrowserTest, QualifiedFramePredicate) {
  const GURL capability = https_server_.GetURL("a.test", "/cap.html");
  const GURL nav_only = https_server_.GetURL("b.test", "/sorry.html");
  const GURL subframe_url = https_server_.GetURL("c.test", "/sub.html");
  std::unique_ptr<PrivilegedWebContents> privileged =
      MakePrivileged(capability, nav_only);
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  if (content::SiteIsolationPolicy::
          IsProcessIsolationForOriginAgentClusterEnabled()) {
    EXPECT_TRUE(IsCapabilityQualifiedFrame(main_frame));
  }

  // A subframe of the PWC -- even on the capability origin -- does not
  // qualify, and the predicate does not kill its renderer.
  ASSERT_TRUE(content::ExecJs(web_contents, content::JsReplace(R"(
    new Promise((resolve) => {
      const f = document.createElement('iframe');
      f.src = $1;
      f.onload = () => resolve(true);
      document.body.appendChild(f);
    })
  )",
                                                               subframe_url)));
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  EXPECT_FALSE(IsCapabilityQualifiedFrame(subframe));
  EXPECT_TRUE(subframe->GetProcess()->IsInitializedAndNotDead());

  // An ordinary tab at the capability URL does not qualify.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), capability));
  content::RenderFrameHost* tab_frame = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetPrimaryMainFrame();
  EXPECT_FALSE(IsCapabilityQualifiedFrame(tab_frame));
  EXPECT_TRUE(tab_frame->GetProcess()->IsInitializedAndNotDead());

  // The PWC main frame on a navigation-only origin does not qualify.
  ASSERT_TRUE(content::NavigateToURL(web_contents, nav_only));
  content::RenderFrameHost* nav_only_frame =
      web_contents->GetPrimaryMainFrame();
  EXPECT_FALSE(IsCapabilityQualifiedFrame(nav_only_frame));
  EXPECT_TRUE(nav_only_frame->GetProcess()->IsInitializedAndNotDead());
}

}  // namespace
}  // namespace pwc
