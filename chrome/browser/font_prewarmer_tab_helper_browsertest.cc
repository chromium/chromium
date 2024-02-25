// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_prewarmer_tab_helper.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/google/core/common/google_switches.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/testing_search_terms_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"

class FontPrewarmerTabHelperTest : public InProcessBrowserTest {
 public:
  TemplateURLService* LoadTemplateUrlService() {
    TemplateURLService* service =
        TemplateURLServiceFactory::GetInstance()->GetForProfile(
            browser()->profile());
    if (service->loaded())
      return service;
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        service->RegisterOnLoadedCallback(
            base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    service->Load();
    run_loop.Run();
    return service;
  }

  // BrowserTest:
  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    // Setup the server to allow serving separate sites, so we can perform
    // cross-process navigation.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Configure the test server to generate a certificate valid for
    // www.google.com.
    net::EmbeddedTestServer::ServerCertificateConfig https_server_cert_config;
    https_server_cert_config.dns_names = {"www.google.com"};
    https_server_.SetSSLConfig(https_server_cert_config);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &FontPrewarmerTabHelperTest::OnHandleRequest, base::Unretained(this)));

    // Needed for explicit ports to work (which embedded test uses).
    command_line->AppendSwitch(switches::kIgnoreGooglePortNumbers);
    ASSERT_TRUE(https_server_.Start());
    // Change the google url so that the default search engine picks up the
    // port used by the test server.
    command_line->AppendSwitchASCII(
        switches::kGoogleBaseURL,
        https_server_.GetURL("www.google.com", "/").spec().c_str());
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

 protected:
  std::string GetSearchResultsPageFontsPref() {
    return FontPrewarmerTabHelper::GetSearchResultsPageFontsPref();
  }

  std::vector<std::string> GetFontNames() {
    return FontPrewarmerTabHelper::GetFontNames(browser()->profile());
  }

  std::unique_ptr<net::test_server::HttpResponse> OnHandleRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_content_type("text/html");
    response->set_code(net::HTTP_OK);
    response->set_content("<html><body style='font-family:Arial'>Hello");
    return response;
  }

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(FontPrewarmerTabHelperTest, Basic) {
  TemplateURLService* service = LoadTemplateUrlService();
  ASSERT_TRUE(service);
  const GURL search_results_page_url =
      service->GetDefaultSearchProvider()->GenerateSearchURL(
          UIThreadSearchTermsData());
  ASSERT_TRUE(!search_results_page_url.is_empty());
  NavigateParams params(browser(), search_results_page_url,
                        ui::PAGE_TRANSITION_LINK);
  base::RunLoop run_loop;
  PrefChangeRegistrar prefs_registrar;
  prefs_registrar.Init(browser()->profile()->GetPrefs());
  prefs_registrar.Add(GetSearchResultsPageFontsPref(),
                      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  Navigate(&params);
  run_loop.Run();
  auto font_names = GetFontNames();
  std::vector<std::string> expected = {"Arial"};
  EXPECT_EQ(expected, font_names);
}
