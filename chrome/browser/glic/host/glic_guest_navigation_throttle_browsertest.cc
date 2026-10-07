// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_navigation_throttle.h"

#include <memory>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class GlicGuestNavigationThrottleBrowserTest : public GlicBrowserTest {
 public:
  GlicGuestNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeatures({features::kGlicDisconnectedWebview},
                                          {features::kGlicNoWebview});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kGlicSkipReloadAfterNavigation);
  }

  void SetUpOnMainThread() override {
    GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("b.com", "127.0.0.1");
    SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A subframe in the guest cannot move an off-allowlist origin into the guest's
// main frame by navigating it to about:blank. Such a navigation commits without
// a URL loader and would inherit the subframe's origin; the throttle cancels
// it via WillCommitWithoutUrlLoader.
IN_PROC_BROWSER_TEST_F(GlicGuestNavigationThrottleBrowserTest,
                       CancelsAboutBlankMainFrameNavigationFromSubframe) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  GURL initial_guest_url = guest_contents->GetLastCommittedURL();
  url::Origin initial_guest_origin =
      guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  GURL subframe_url = embedded_https_test_server().GetURL(
      "b.com", "/glic/browser_tests/minimal_client.html");

  // Embed an off-allowlist cross-origin subframe in the guest.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, content::JsReplace(R"(
    new Promise((resolve) => {
      const f = document.createElement('iframe');
      f.src = $1;
      f.onload = () => resolve(true);
      document.body.appendChild(f);
    })
  )",
                                                         subframe_url)));

  content::RenderFrameHost* subframe =
      content::ChildFrameAt(guest_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(url::Origin::Create(subframe_url),
            subframe->GetLastCommittedOrigin());

  // The subframe navigates the main frame to about:blank. The throttle cancels
  // it, so the main frame stays on the allowlisted origin.
  content::TestNavigationManager nav_manager(guest_contents,
                                             GURL("about:blank"));
  ASSERT_TRUE(content::ExecJs(subframe, "top.location.href = 'about:blank';"));
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());
  EXPECT_EQ(initial_guest_url, guest_contents->GetLastCommittedURL());
  EXPECT_EQ(initial_guest_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
}

// Same-document navigations in the guest main frame commit without a URL loader
// and should proceed normally.
IN_PROC_BROWSER_TEST_F(GlicGuestNavigationThrottleBrowserTest,
                       AllowsSameDocumentNavigation) {
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  url::Origin initial_origin =
      guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();

  ASSERT_TRUE(content::ExecJs(guest_contents,
                              "window.location.hash = '#test-section';"));
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

  EXPECT_TRUE(
      guest_contents->GetLastCommittedURL().spec().ends_with("#test-section"));
  EXPECT_EQ(initial_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
}

}  // namespace
}  // namespace glic
