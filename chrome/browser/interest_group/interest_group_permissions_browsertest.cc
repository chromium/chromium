// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace interest_group {

class FledgePermissionsBrowserTest : public InProcessBrowserTest {
 public:
  FledgePermissionsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kFledgeInterestGroups,
         blink::features::kFledgeInterestGroupAPI},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    // Prime the interest groups if the API is enabled.
    ui_test_utils::NavigateToURL(browser(), test_url());
    if (HasInterestGroupApi(web_contents())) {
      JoinInterestGroup(web_contents());
      WaitUntilCanRunAuction(web_contents());
    }
  }

  void SetGlobalCookiesAllowed(bool allowed) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetDefaultCookieSetting(allowed ? CONTENT_SETTING_ALLOW
                                          : CONTENT_SETTING_BLOCK);
  }

  void SetAllowCookiesForURL(const GURL& url, bool allowed) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetCookieSetting(
            url, allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }

  void SetAllowThirdPartyCookies(bool allowed) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            allowed ? content_settings::CookieControlsMode::kOff
                    : content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void SetAllowThirdPartyCookiesForURL(const GURL& url, bool allowed) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetThirdPartyCookieSetting(
            url, allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
  }

  bool HasInterestGroupApi(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      navigator.joinAdInterestGroup instanceof Function
    )")
        .ExtractBool();
  }

  void JoinInterestGroup(const content::ToRenderFrameHost& adapter) {
    // join interest group
    EXPECT_EQ(
        "Success",
        EvalJs(adapter, content::JsReplace(
                            R"(
    (function() {
      try {
        navigator.joinAdInterestGroup(
            {
              name: 'cars',
              owner: $1,
              biddingLogicUrl: $2,
              trustedBiddingSignalsUrl: $3,
              trustedBiddingSignalsKeys: ['key1'],
              userBiddingSignals: {some: 'json', data: {here: [1, 2, 3]}},
              ads: [{
                renderUrl: $4,
                metadata: {ad: 'metadata', here: [1, 2, 3]},
              }],
            },
            /*joinDurationSec=*/ 1000);
      } catch (e) {
        return e.toString();
      }
      return "Success";
    })())",
                            https_server_->GetURL("a.test", "/"),
                            https_server_->GetURL(
                                "a.test", "/interest_group/bidding_logic.js"),
                            https_server_->GetURL(
                                "a.test",
                                "/interest_group/trusted_bidding_signals.json"),
                            render_url())));
  }

  bool CanRunAuction(const content::ToRenderFrameHost& adapter) {
    // run auction
    auto auction_result = EvalJs(
        adapter, content::JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
  });
})())",
                     https_server_->GetURL("a.test", "/"),
                     https_server_->GetURL(
                         "a.test", "/interest_group/decision_logic.js")));
    if (nullptr == auction_result) {
      return false;
    }
    EXPECT_EQ(render_url(), auction_result);
    return true;
  }

  void WaitUntilCanRunAuction(const content::ToRenderFrameHost& adapter) {
    // wait for that to complete
    base::RunLoop run_loop;
    base::RetainingOneShotTimer check_done;
    check_done.Start(
        FROM_HERE, base::TimeDelta::FromMicroseconds(10),
        base::BindLambdaForTesting([&adapter, &check_done, &run_loop, this]() {
          if (!CanRunAuction(adapter)) {
            check_done.Reset();
          } else {
            run_loop.Quit();
          }
        }));
    run_loop.Run();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL test_url() { return https_server_->GetURL("a.test", "/echo"); }
  GURL render_url() { return GURL("https://example.com/render"); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest, CookiesAllowed) {
  // With cookies, API works.
  SetGlobalCookiesAllowed(true);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest, CookiesAllowedForSite) {
  // With cookies, API works.
  SetGlobalCookiesAllowed(false);
  SetAllowCookiesForURL(test_url(), true);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest,
                       ThirdPartyCookiesAllowedForSite) {
  // With cookies, API works.
  SetAllowThirdPartyCookies(false);
  SetAllowThirdPartyCookiesForURL(test_url(), true);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest, CookiesBlocked) {
  // With no cookies, API does nothing.
  SetGlobalCookiesAllowed(false);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest, CookiesBlockedForSite) {
  // With no cookies, API does nothing.
  SetAllowCookiesForURL(test_url(), false);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest, ThirdPartyCookiesBlocked) {
  // With no cookies, API does nothing.
  SetAllowThirdPartyCookies(false);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(FledgePermissionsBrowserTest,
                       ThirdPartyCookiesBlockedForSite) {
  // With no cookies, API does nothing.
  SetAllowThirdPartyCookiesForURL(test_url(), false);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

class FledgePermissionBrowserTestBaseFeatureDisabled
    : public FledgePermissionsBrowserTest {
 public:
  FledgePermissionBrowserTestBaseFeatureDisabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kFledgeInterestGroups);
  }
};

IN_PROC_BROWSER_TEST_F(FledgePermissionBrowserTestBaseFeatureDisabled,
                       CookiesAllowed) {
  // Even with cookies no feature means no API.
  SetGlobalCookiesAllowed(true);
  SetAllowCookiesForURL(test_url(), true);
  ui_test_utils::NavigateToURL(browser(), test_url());

  ASSERT_FALSE(HasInterestGroupApi(web_contents()));
}

}  // namespace interest_group
