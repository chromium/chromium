// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/google/core/common/google_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/safe_search_api/safe_search_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"

namespace {

class YouTubeRestrictionsBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  YouTubeRestrictionsBrowserTest() {
    // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having to
    // disable this feature.
    feature_list_.InitAndDisableFeature(features::kHttpsUpgrades);
  }
  ~YouTubeRestrictionsBrowserTest() override { feature_list_.Reset(); }

  MOCK_METHOD(void, InterceptYoutubeRestrictHeader, (std::string value));
  MOCK_METHOD(void, InterceptRequest, ());

  net::EmbeddedTestServer youtube_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("youtube.com", "127.0.0.1");

    youtube_server_.AddDefaultHandlers(GetChromeTestDataDir());
    youtube_server_.RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          InterceptRequest();
          for (const auto& [key, value] : request.headers) {
            if (key.compare(safe_search_api::kYouTubeRestrictHeaderName) == 0) {
              InterceptYoutubeRestrictHeader(value);
            }
          }
        }));

    ASSERT_TRUE(youtube_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);

    // Note for the google and youtube tests below, the throttles expect that
    // the URLs are to google.com or youtube.com. Networking code also
    // automatically upgrades http requests to these domains to https (see the
    // preload list in https://www.chromium.org/hsts). So as a result we need
    // to make the requests to an https server. Since the HTTPS server only
    // serves a valid cert for localhost, so this is needed to load pages from
    // "www.google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);

    // The production code only allows known ports (80 for http and 443 for
    // https), but the test server runs on a random port.
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
  }

  base::test::ScopedFeatureList feature_list_;
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised}};
};

// TODO(https://crbug.com/1494241): Add more test coverage.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_RestrictionHeaderIsNotSent DISABLED_RestrictionHeaderIsNotSent
#else
#define MAYBE_RestrictionHeaderIsNotSent RestrictionHeaderIsNotSent
#endif
IN_PROC_BROWSER_TEST_F(YouTubeRestrictionsBrowserTest,
                       MAYBE_RestrictionHeaderIsNotSent) {
  GURL youtube_url(youtube_server_.GetURL("youtube.com", "/empty.html"));

  EXPECT_CALL(*this, InterceptRequest())
      .Times(::testing::AtLeast(1));  // Main request + favicon.
  EXPECT_CALL(*this, InterceptYoutubeRestrictHeader(::testing::_)).Times(0);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), youtube_url));
}

}  // namespace
