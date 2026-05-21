// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace glic {

class GlicWebUiBrowserTest : public glic::GlicBrowserTest {
 public:
  GlicWebUiBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kGlicCSPConfig, {}},
        },
        {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    glic::GlicBrowserTest::SetUpCommandLine(command_line);
    // Allow b.com to load in the webview, but not have Glic API access
    command_line->AppendSwitchASCII(::switches::kGlicAllowedOrigins,
                                    "https://gemini.google.com http://b.com");
    command_line->AppendSwitch(::switches::kGlicSkipReloadAfterNavigation);
  }

  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("b.com", "127.0.0.1");
    SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicWebUiBrowserTest,
                       NavigatingToUntrustedOriginRevokesApi) {
  // 1. Open Glic on the primary guest URL (which is API allowed)
  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());
  EXPECT_TRUE(instance->host().IsWebClientConnected());

  // 2. Obtain the guest WebContents and trigger navigation to b.com (untrusted)
  content::WebContents* guest_contents =
      GetGlicGuestWebContents(instance->host().webui_contents());
  ASSERT_TRUE(guest_contents);

  GURL untrusted_guest_url = embedded_test_server()->GetURL(
      "b.com", "/glic/browser_tests/minimal_client.html");
  ASSERT_TRUE(content::NavigateToURL(guest_contents, untrusted_guest_url));

  // 3. Verify Glic API/Mojo connection is immediately revoked
  EXPECT_TRUE(
      RunUntil([&]() { return !instance->host().IsWebClientConnected(); },
               "Wait for Glic WebClient to disconnect"));

  // 4. Ensure Glic API remains disconnected
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(500));
  run_loop.Run();
  EXPECT_FALSE(instance->host().IsWebClientConnected());
}

}  // namespace glic
