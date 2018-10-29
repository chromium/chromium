// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

const char kAValidUrl[] = "http://example.com/";
const char kAnInvalidUrl[] = "the quick brown fox jumps over the lazy dog";

bool ReturnValidXml(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
  std::string xml =
      "<rules version=\"1\"><docMode><domain docMode=\"9\">"
      "docs.google.com</domain></docMode></rules>";
  content::URLLoaderInterceptor::WriteResponse(headers, xml,
                                               params->client.get());
  return true;
}

bool FailToDownload(content::URLLoaderInterceptor::RequestParams* params) {
  std::string headers = "HTTP/1.1 500 Internal Server Error\n\n";
  content::URLLoaderInterceptor::WriteResponse(headers, "",
                                               params->client.get());
  return true;
}

}  // namespace

class BrowserSwitcherServiceTest : public InProcessBrowserTest {
 public:
  BrowserSwitcherServiceTest() = default;
  ~BrowserSwitcherServiceTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserSwitcherServiceTest);
};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, NotEnabledByPolicy) {
  // Only load the IEEM sitelist if the 'use_ie_sitelist' pref is set to true.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, false);
  base::RunLoop run_loop;
  BrowserSwitcherService::SetIeemFetchDelayForTesting(base::TimeDelta());
  BrowserSwitcherService::SetXmlParsedCallbackForTesting(
      run_loop.QuitClosure());
  BrowserSwitcherService::SetIeemSitelistUrlForTesting(GURL(kAValidUrl));

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams*) {
        *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  run_loop.Run();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IeemSitelistInvalidUrl) {
  // Only load the IEEM sitelist if the 'use_ie_sitelist' pref is set to true.
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, false);
  base::RunLoop run_loop;
  BrowserSwitcherService::SetIeemFetchDelayForTesting(base::TimeDelta());
  BrowserSwitcherService::SetXmlParsedCallbackForTesting(
      run_loop.QuitClosure());
  BrowserSwitcherService::SetIeemSitelistUrlForTesting(GURL(kAnInvalidUrl));

  bool fetch_happened = false;
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](bool* happened, content::URLLoaderInterceptor::RequestParams*) {
        *happened = true;
        return false;
      },
      &fetch_happened));

  // Execute everything and make sure we didn't get to the fetch step.
  BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  run_loop.Run();
  EXPECT_FALSE(fetch_happened);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, FetchAndParseAfterStartup) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, true);
  base::RunLoop run_loop;
  BrowserSwitcherService::SetIeemFetchDelayForTesting(base::TimeDelta());
  BrowserSwitcherService::SetXmlParsedCallbackForTesting(
      run_loop.QuitClosure());
  BrowserSwitcherService::SetIeemSitelistUrlForTesting(GURL(kAValidUrl));

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(ReturnValidXml));

  // Execute everything and make sure the rules are applied correctly.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  run_loop.Run();
  EXPECT_FALSE(service->sitelist()->ShouldSwitch(GURL("http://google.com/")));
  EXPECT_TRUE(
      service->sitelist()->ShouldSwitch(GURL("http://docs.google.com/")));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherServiceTest, IgnoresFailedDownload) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUseIeSitelist, true);
  base::RunLoop run_loop;
  BrowserSwitcherService::SetIeemFetchDelayForTesting(base::TimeDelta());
  BrowserSwitcherService::SetXmlParsedCallbackForTesting(
      run_loop.QuitClosure());
  BrowserSwitcherService::SetIeemSitelistUrlForTesting(GURL(kAValidUrl));

  content::URLLoaderInterceptor interceptor(
      base::BindRepeating(FailToDownload));

  // Execute everything and make sure no rules are applied.
  auto* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(browser()->profile());
  run_loop.Run();
  EXPECT_FALSE(service->sitelist()->ShouldSwitch(GURL("http://google.com/")));
  EXPECT_FALSE(
      service->sitelist()->ShouldSwitch(GURL("http://docs.google.com/")));
}

}  // namespace browser_switcher
