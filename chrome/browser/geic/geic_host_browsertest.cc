// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_host.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/geic/geic.mojom.h"
#include "chrome/browser/geic/geic_browser_host_impl.h"
#include "chrome/browser/geic/geic_pwc_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
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

namespace geic {
namespace {

// Binds GeicApi for `frame` and asserts the frame's renderer is terminated.
void ExpectBindKillsRenderer(content::RenderFrameHost* frame) {
  content::RenderProcessHostWatcher exit_watcher(
      frame->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  mojo::Remote<mojom::GeicApi> remote;
  BindGeicApi(frame, remote.BindNewPipeAndPassReceiver());
  exit_watcher.Wait();
  EXPECT_FALSE(frame->GetProcess()->IsInitializedAndNotDead());
}

class GeicApiBrowserTest : public InProcessBrowserTest {
 public:
  GeicApiBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        pwc::mojom::features::kPrivilegedWebContents);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    // Standard test pages (/title1.html and friends) from chrome/test/data.
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  std::unique_ptr<pwc::PrivilegedWebContents> MakePrivileged(
      pwc::PrivilegedComponent component,
      const GURL& capability) {
    return pwc::PrivilegedWebContents::Create(
        component, browser()->GetProfile(),
        std::make_unique<pwc::FixedPwcPolicyDelegate>(
            std::vector<url::Origin>{url::Origin::Create(capability)},
            std::vector<url::Origin>{url::Origin::Create(capability)}));
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The qualifying main frame of a GEIC PrivilegedWebContents binds GeicApi.
IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, GeicComponentBinds) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      MakePrivileged(pwc::PrivilegedComponent::kGeic, capability);
  // The GEIC controller owns the GeicHost and scopes it to the PWC; here the
  // test plays that role. It registers itself into the PWC and is destroyed
  // before `privileged`.
  GeicHost geic_host(*privileged);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  mojo::Remote<mojom::GeicApi> remote;
  BindGeicApi(web_contents->GetPrimaryMainFrame(),
              remote.BindNewPipeAndPassReceiver());
  // FlushForTesting() waits until the browser end either accepted or dropped
  // the receiver, so connectivity afterwards is an authoritative verdict.
  remote.FlushForTesting();
  EXPECT_TRUE(remote.is_connected());
}

// Receivers are document-scoped (content::DocumentService): the browser
// destroys the binding when its document is destroyed by a cross-document
// navigation, severing the pipe with no reliance on the renderer dropping
// its end.
IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, GeicApiSeveredOnCrossDocumentNav) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL capability2 = https_server_.GetURL("a.test", "/title2.html");
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      MakePrivileged(pwc::PrivilegedComponent::kGeic, capability);
  GeicHost geic_host(*privileged);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  mojo::Remote<mojom::GeicApi> remote;
  BindGeicApi(web_contents->GetPrimaryMainFrame(),
              remote.BindNewPipeAndPassReceiver());
  remote.FlushForTesting();
  ASSERT_TRUE(remote.is_connected());

  ASSERT_TRUE(content::NavigateToURL(web_contents, capability2));
  remote.FlushForTesting();
  EXPECT_FALSE(remote.is_connected());
}

// A qualifying GEIC main frame whose PWC has no GeicHost attached (a
// browser-side setup gap, not an attack) is dropped, not killed.
IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, NoHostDropsWithoutKilling) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      MakePrivileged(pwc::PrivilegedComponent::kGeic, capability);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  // No GeicHost is attached to this PWC.
  content::RenderFrameHost* frame = web_contents->GetPrimaryMainFrame();
  mojo::Remote<mojom::GeicApi> remote;
  BindGeicApi(frame, remote.BindNewPipeAndPassReceiver());
  // The request is dropped: the receiver is never bound, so its end of the pipe
  // closes and the remote disconnects.
  base::RunLoop disconnect_loop;
  remote.set_disconnect_handler(disconnect_loop.QuitClosure());
  disconnect_loop.Run();
  EXPECT_FALSE(remote.is_connected());
  // The renderer is not terminated.
  EXPECT_TRUE(frame->GetProcess()->IsInitializedAndNotDead());
}

// A PrivilegedWebContents that serves a different component (here the test
// component) must not receive GeicApi, even from its qualifying main frame: the
// interface is keyed on the component, not merely on being privileged.
IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, NonGeicComponentKilled) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      MakePrivileged(pwc::PrivilegedComponent::kTestComponent, capability);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, capability));

  ExpectBindKillsRenderer(web_contents->GetPrimaryMainFrame());
}

// A subframe of a GEIC PrivilegedWebContents is not the primary main frame and
// must not receive GeicApi (the shared privileged-frame gate rejects it).
IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, SubframeKilled) {
  const GURL capability = https_server_.GetURL("a.test", "/title1.html");
  const GURL subframe_url = https_server_.GetURL("b.test", "/title1.html");
  std::unique_ptr<pwc::PrivilegedWebContents> privileged =
      MakePrivileged(pwc::PrivilegedComponent::kGeic, capability);
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

IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest, OpenAndCloseSignInTabLifecycle) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);
  tabs::TabInterface* initial_tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  GeicBrowserHostImpl host_impl(initial_tab);
  mojo::Remote<mojom::GeicBrowserHost> host_remote;
  host_impl.BindBrowserHost(host_remote.BindNewPipeAndPassReceiver());

  // Open sign-in tab.
  GURL signin_url("https://accounts.google.com/signin");
  host_remote->OpenSignInTab(signin_url);
  host_remote.FlushForTesting();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(), signin_url);

  // Close sign-in tab.
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  host_remote->CloseSignInTab(close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);

  // Verify sign-in tab is closed and focus is restored to the original tab.
  EXPECT_EQ(tab_strip->count(), 1);
  EXPECT_EQ(tab_strip->active_index(), 0);
  EXPECT_EQ(tab_strip->GetActiveTab(), initial_tab);
}

IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest,
                       OpenSignInTabReusesExistingSignInTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);
  tabs::TabInterface* initial_tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  GeicBrowserHostImpl host_impl(initial_tab);
  mojo::Remote<mojom::GeicBrowserHost> host_remote;
  host_impl.BindBrowserHost(host_remote.BindNewPipeAndPassReceiver());

  GURL signin_url("https://accounts.google.com/signin");
  host_remote->OpenSignInTab(signin_url);
  host_remote.FlushForTesting();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // Switch back to tab 0:
  tab_strip->ActivateTabAt(0);
  EXPECT_EQ(tab_strip->active_index(), 0);

  // Second OpenSignInTab should activate tab 1 without opening a new tab:
  host_remote->OpenSignInTab(signin_url);
  host_remote.FlushForTesting();
  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  host_remote->CloseSignInTab(close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);
  EXPECT_EQ(tab_strip->count(), 1);
}

IN_PROC_BROWSER_TEST_F(GeicApiBrowserTest,
                       CloseSignInTabReturnsAlreadyClosedWhenUserClosedTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);
  tabs::TabInterface* initial_tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  GeicBrowserHostImpl host_impl(initial_tab);
  mojo::Remote<mojom::GeicBrowserHost> host_remote;
  host_impl.BindBrowserHost(host_remote.BindNewPipeAndPassReceiver());

  GURL signin_url("https://accounts.google.com/signin");
  host_remote->OpenSignInTab(signin_url);
  host_remote.FlushForTesting();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->active_index(), 1);

  // User closes sign-in tab manually:
  tab_strip->CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);
  EXPECT_EQ(tab_strip->count(), 1);

  // CloseSignInTab should report kAlreadyClosed:
  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  host_remote->CloseSignInTab(close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kAlreadyClosed);
}

class GeicBrowserHostCustomGuestUrlBrowserTest : public GeicApiBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GeicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        kGeicGuestURLSwitch,
        "https://localhost.corp.google.com:10443/side-panel");
  }
};

IN_PROC_BROWSER_TEST_F(GeicBrowserHostCustomGuestUrlBrowserTest,
                       AllowsConfiguredGuestOrigin) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip->count(), 1);
  tabs::TabInterface* initial_tab = tab_strip->GetActiveTab();
  ASSERT_TRUE(initial_tab);

  GeicBrowserHostImpl host_impl(initial_tab);
  mojo::Remote<mojom::GeicBrowserHost> host_remote;
  host_impl.BindBrowserHost(host_remote.BindNewPipeAndPassReceiver());

  GURL guest_signin_url("https://localhost.corp.google.com:10443/auth/signin");
  host_remote->OpenSignInTab(guest_signin_url);
  host_remote.FlushForTesting();

  EXPECT_EQ(tab_strip->count(), 2);
  EXPECT_EQ(tab_strip->GetActiveWebContents()->GetVisibleURL(),
            guest_signin_url);

  base::test::TestFuture<mojom::CloseSignInTabResult> close_future;
  host_remote->CloseSignInTab(close_future.GetCallback());
  EXPECT_EQ(close_future.Take(), mojom::CloseSignInTabResult::kSuccess);
  EXPECT_EQ(tab_strip->count(), 1);
}

}  // namespace
}  // namespace geic
