// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/persisted_state_db/profile_proto_db.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/search/ntp_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/window_open_disposition.h"

namespace {
cart_db::ChromeCartContentProto BuildProto(const char* domain,
                                           const char* merchant_url) {
  cart_db::ChromeCartContentProto proto;
  proto.set_key(domain);
  proto.set_merchant_cart_url(merchant_url);
  proto.set_timestamp(base::Time::Now().ToDoubleT());
  return proto;
}

const char kMockMerchant[] = "walmart.com";
const char kMockMerchantURL[] = "https://www.walmart.com";
using ShoppingCarts =
    std::vector<ProfileProtoDB<cart_db::ChromeCartContentProto>::KeyAndValue>;
}  // namespace

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("dummy");
  response->set_content_type("text/html");
  return response;
}

// Tests CartService.
class CartServiceBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kModules, ntp_features::kNtpChromeCartModule,
         optimization_guide::features::kOptimizationHints},
        {});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_ = Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    service_ = CartServiceFactory::GetForProfile(profile_);

    // This is necessary to test non-localhost domains.
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.ServeFilesFromSourceDirectory("chrome/test/data/cart/");
    https_server_.RegisterRequestHandler(base::BindRepeating(&BasicResponse));
    ASSERT_TRUE(https_server_.InitializeAndListen());
    https_server_.StartAcceptingConnections();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This bloom filter rejects "walmart.com" as a shopping site.
    command_line->AppendSwitchASCII("optimization_guide_hints_override",
                                    "Eg8IDxILCBsQJxoFiUzKeE4=");
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void OperationEvaluation(base::OnceClosure closure,
                           bool expected_success,
                           bool actual_success) {
    EXPECT_EQ(expected_success, actual_success);
    std::move(closure).Run();
  }

  void GetEvaluationURL(base::OnceClosure closure,
                        ShoppingCarts expected,
                        bool result,
                        ShoppingCarts found) {
    EXPECT_EQ(found.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(found[i].first, expected[i].first);
      EXPECT_EQ(found[i].second.merchant_cart_url(),
                expected[i].second.merchant_cart_url());
      for (int j = 0; j < expected[i].second.product_image_urls().size(); j++) {
        EXPECT_EQ(expected[i].second.product_image_urls()[j],
                  found[i].second.product_image_urls()[j]);
      }
    }
    std::move(closure).Run();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToURL(
      const GURL& url,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    // TODO(crbug.com/1206094): Investigate TabStripModelObserver-based waiting
    // mechanism.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(2));
    base::RunLoop().RunUntilIdle();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CartService* service_;
  Profile* profile_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(CartServiceBrowserTest, TestNotShowSkippedMerchants) {
  CartDB* cart_db_ = service_->GetDB();
  base::RunLoop run_loop[6];
  cart_db::ChromeCartContentProto merchant_proto =
      BuildProto(kMockMerchant, kMockMerchantURL);
  ShoppingCarts merchant_res = {{kMockMerchant, merchant_proto}};
  ShoppingCarts empty_res = {};

  cart_db_->AddCart(
      kMockMerchant, merchant_proto,
      base::BindOnce(&CartServiceBrowserTest::OperationEvaluation,
                     base::Unretained(this), run_loop[0].QuitClosure(), true));
  run_loop[0].Run();

  service_->LoadAllActiveCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[1].QuitClosure(), empty_res));
  run_loop[1].Run();

  cart_db_->LoadAllCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[2].QuitClosure(), empty_res));
  run_loop[2].Run();

  merchant_proto.set_is_removed(true);
  cart_db_->AddCart(
      kMockMerchant, merchant_proto,
      base::BindOnce(&CartServiceBrowserTest::OperationEvaluation,
                     base::Unretained(this), run_loop[3].QuitClosure(), true));
  run_loop[3].Run();

  service_->LoadAllActiveCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[4].QuitClosure(), empty_res));
  run_loop[4].Run();

  cart_db_->LoadAllCarts(base::BindOnce(
      &CartServiceBrowserTest::GetEvaluationURL, base::Unretained(this),
      run_loop[5].QuitClosure(), merchant_res));
  run_loop[5].Run();
}

IN_PROC_BROWSER_TEST_F(CartServiceBrowserTest, TestNavigationUKMCollection) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  GURL foo_url("https://www.foo.com/cart.html");
  GURL bar_url("https://www.bar.com/cart.html");
  foo_url = https_server_.GetURL(foo_url.host(), foo_url.path());
  bar_url = https_server_.GetURL(bar_url.host(), bar_url.path());

  // Not record UKM when no prepared URL.
  NavigateToURL(foo_url);
  auto entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Not record UKM when prepared URL is different from navigation URL.
  service_->PrepareForNavigation(foo_url);
  NavigateToURL(bar_url);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(0u, entries.size());
  NavigateToURL(foo_url);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Record UKM when prepared URL matches with navigation URL.
  service_->PrepareForNavigation(foo_url);
  NavigateToURL(foo_url);
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(1u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), foo_url);

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  service_->PrepareForNavigation(bar_url);
  NavigateToURL(bar_url, WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(2u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), bar_url);

  BrowserList* active_browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1u, active_browser_list->size());
  service_->PrepareForNavigation(foo_url);
  NavigateToURL(foo_url, WindowOpenDisposition::NEW_WINDOW);
  EXPECT_EQ(2u, active_browser_list->size());
  entries = ukm_recorder.GetEntriesByName(
      ukm::builders::Shopping_ChromeCart::kEntryName);
  EXPECT_EQ(3u, entries.size());
  ukm_recorder.ExpectEntrySourceHasUrl(entries.back(), foo_url);
}
