// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_tab_helper.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/mock_fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace {

using testing::_;
using testing::Eq;
using testing::SaveArg;
using testing::StrictMock;

// Creates the same fake http response for every request.
std::unique_ptr<net::test_server::HttpResponse> CreateFakeResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("Dummy response.");
  response->set_content_type("text/html");
  return response;
}

class FastCheckoutTabHelperBrowserTest : public AndroidBrowserTest {
 public:
  void SetUpOnMainThread() override {
    Profile* profile = ProfileManager::GetLastUsedProfileIfLoaded();
    CHECK(profile);
    fetcher_ =
        FastCheckoutCapabilitiesFetcherFactory::GetInstance()
            ->SetTestingSubclassFactoryAndUse(
                profile, base::BindOnce([](content::BrowserContext*) {
                  return std::make_unique<
                      StrictMock<MockFastCheckoutCapabilitiesFetcher>>();
                }));

    // Set up the service to be tested - normally this is called during
    // `WebContents` creation.
    FastCheckoutTabHelper::CreateForWebContents(GetActiveWebContents());

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&CreateFakeResponse));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  MockFastCheckoutCapabilitiesFetcher* fetcher() { return fetcher_; }

  autofill::MockFastCheckoutClient* fast_checkout_client() {
    return static_cast<autofill::MockFastCheckoutClient*>(
        autofill_client().GetFastCheckoutClient());
  }

  autofill::TestContentAutofillClient& autofill_client() {
    return *autofill_client_injector_[GetActiveWebContents()];
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void NavigateToUrl(const GURL& url) {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL(url.host(), url.path())));
    base::RunLoop().RunUntilIdle();
  }

 private:
  raw_ptr<MockFastCheckoutCapabilitiesFetcher> fetcher_ = nullptr;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      autofill_client_injector_;
};

IN_PROC_BROWSER_TEST_F(
    FastCheckoutTabHelperBrowserTest,
    DidStartNavigation_NoShoppingURL_NoFetchCapabilitiesCall) {
  // No availability request was started or the `StrickMock` would have failed.
  GURL no_shopping_url("http://www.example.com/empty.html");
  EXPECT_CALL(*fast_checkout_client(), OnNavigation);
  NavigateToUrl(no_shopping_url);
}

IN_PROC_BROWSER_TEST_F(
    FastCheckoutTabHelperBrowserTest,
    DidStartNavigation_CheckoutURL_MakesFetchCapabilitiesCall) {
  GURL shopping_url("http://www.example2.co.uk/checkout.html");
  EXPECT_CALL(*fetcher(), FetchCapabilities);
  EXPECT_CALL(*fast_checkout_client(), OnNavigation);
  NavigateToUrl(shopping_url);
}

IN_PROC_BROWSER_TEST_F(
    FastCheckoutTabHelperBrowserTest,
    DidStartNavigation_CartShoppingURL_MakesFetchCapabilitiesCall) {
  GURL shopping_cart_url("http://www.example2.co.uk/cart.html");
  EXPECT_CALL(*fetcher(), FetchCapabilities);
  EXPECT_CALL(*fast_checkout_client(), OnNavigation);
  NavigateToUrl(shopping_cart_url);
}

}  // namespace
