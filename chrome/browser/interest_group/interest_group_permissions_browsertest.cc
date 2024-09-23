// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace interest_group {

class InterestGroupPermissionsBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  InterestGroupPermissionsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         features::kPrivacySandboxAdsAPIsOverride},
        /*disabled_features=*/
        {blink::features::kFencedFrames,
         blink::features::kFledgeEnforceKAnonymity});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_->Start());

    PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
        ->SetAllPrivacySandboxAllowedForTesting();
    // Prime the interest groups if the API is enabled.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
    if (HasInterestGroupApi(web_contents()) &&
        HasRunAdAuctionApi(web_contents())) {
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

  bool HasRunAdAuctionApi(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      navigator.runAdAuction instanceof Function
    )")
        .ExtractBool();
  }

  bool HasCreateAdRequestApi(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      navigator.createAdRequest instanceof Function
    )")
        .ExtractBool();
  }

  bool HasFinalizeAdApi(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      navigator.finalizeAd instanceof Function
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
              trustedBiddingSignalsURL: $3,
              trustedBiddingSignalsKeys: ['key1'],
              userBiddingSignals: {some: 'json', data: {here: [1, 2, 3]}},
              ads: [{
                renderURL: $4,
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
    decisionLogicURL: $2,
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
    EXPECT_TRUE(base::StartsWith(auction_result.ExtractString(), "urn:uuid:",
                                 base::CompareCase::INSENSITIVE_ASCII))
        << auction_result.ExtractString();
    return true;
  }

  void WaitUntilCanRunAuction(const content::ToRenderFrameHost& adapter) {
    // wait for that to complete
    base::RunLoop run_loop;
    base::RetainingOneShotTimer check_done;
    check_done.Start(
        FROM_HERE, base::Microseconds(10),
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
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  privacy_sandbox::PrivacySandboxAttestationsMixin
      privacy_sandbox_attestations_mixin_{&mixin_host_};
  base::test::ScopedFeatureList scoped_feature_list_;
};

class InterestGroupOffBrowserTest : public InterestGroupPermissionsBrowserTest {
 public:
  InterestGroupOffBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInterestGroupStorage},
        {blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         blink::features::kParakeet, features::kPrivacySandboxAdsAPIsOverride});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupOffBrowserTest, AdAPIsOffWithoutFlags) {
  // No APIs should be exposed when all features are off.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_FALSE(HasInterestGroupApi(web_contents()));
  ASSERT_FALSE(HasRunAdAuctionApi(web_contents()));
  ASSERT_FALSE(HasCreateAdRequestApi(web_contents()));
  ASSERT_FALSE(HasFinalizeAdApi(web_contents()));
}

class InterestGroupFledgeOnBrowserTest
    : public InterestGroupPermissionsBrowserTest {
 public:
  InterestGroupFledgeOnBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInterestGroupStorage, blink::features::kFledge,
         features::kPrivacySandboxAdsAPIsOverride},
        {blink::features::kAdInterestGroupAPI, blink::features::kParakeet});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupFledgeOnBrowserTest, FledgeOnWithAPIFlag) {
  // kFledge should turn on the runAdAuction and correspondingly the
  // interestgroup API.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  ASSERT_TRUE(HasRunAdAuctionApi(web_contents()));
  ASSERT_FALSE(HasCreateAdRequestApi(web_contents()));
  ASSERT_FALSE(HasFinalizeAdApi(web_contents()));
}

class InterestGroupParakeetOnBrowserTest
    : public InterestGroupPermissionsBrowserTest {
 public:
  InterestGroupParakeetOnBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInterestGroupStorage, blink::features::kParakeet},
        {blink::features::kAdInterestGroupAPI, blink::features::kFledge,
         features::kPrivacySandboxAdsAPIsOverride});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupParakeetOnBrowserTest,
                       ParakeetOnWithAPIFlag) {
  // kParakeet should turn on the createAdRequest/finalizeAd and correspondingly
  // the interestgroup API.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  ASSERT_FALSE(HasRunAdAuctionApi(web_contents()));
  ASSERT_TRUE(HasCreateAdRequestApi(web_contents()));
  ASSERT_TRUE(HasFinalizeAdApi(web_contents()));
}

class InterestGroupAPIOnBrowserTest
    : public InterestGroupPermissionsBrowserTest {
 public:
  InterestGroupAPIOnBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI},
        {blink::features::kParakeet, blink::features::kFledge,
         features::kPrivacySandboxAdsAPIsOverride});
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupAPIOnBrowserTest,
                       InterestGroupsOnWithAPIFlag) {
  // kAdInterestGroupAPI should turn on only the interestgroup API.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  ASSERT_FALSE(HasRunAdAuctionApi(web_contents()));
  ASSERT_FALSE(HasCreateAdRequestApi(web_contents()));
  ASSERT_FALSE(HasFinalizeAdApi(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest, CookiesAllowed) {
  // With cookies, API works.
  SetGlobalCookiesAllowed(true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest,
                       CookiesAllowedForSite) {
  // With cookies, API works.
  SetGlobalCookiesAllowed(false);
  SetAllowCookiesForURL(test_url(), true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest,
                       ThirdPartyCookiesAllowedForSite) {
  // With cookies, API works.
  SetAllowThirdPartyCookies(false);
  SetAllowThirdPartyCookiesForURL(test_url(), true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest, CookiesBlocked) {
  // With no cookies, API does nothing.
  SetGlobalCookiesAllowed(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest,
                       CookiesBlockedForSite) {
  // With no cookies, API does nothing.
  SetAllowCookiesForURL(test_url(), false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest,
                       ThirdPartyCookiesBlocked) {
  // With no 3PC cookies, API still works.
  SetAllowThirdPartyCookies(false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_TRUE(CanRunAuction(web_contents()));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPermissionsBrowserTest,
                       ThirdPartyCookiesBlockedForSite) {
  // With no cookies, API does nothing.
  SetAllowThirdPartyCookiesForURL(test_url(), false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_TRUE(HasInterestGroupApi(web_contents()));
  EXPECT_FALSE(CanRunAuction(web_contents()));
}

class FledgePermissionBrowserTestBaseFeatureDisabled
    : public InterestGroupPermissionsBrowserTest {
 public:
  FledgePermissionBrowserTestBaseFeatureDisabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        blink::features::kInterestGroupStorage);
  }
};

IN_PROC_BROWSER_TEST_F(FledgePermissionBrowserTestBaseFeatureDisabled,
                       CookiesAllowed) {
  // Even with cookies no feature means no API.
  SetGlobalCookiesAllowed(true);
  SetAllowCookiesForURL(test_url(), true);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  ASSERT_FALSE(HasInterestGroupApi(web_contents()));
}

}  // namespace interest_group
