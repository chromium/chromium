// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
const char kXGeoHeaderName[] = "X-Geo";
}  // namespace

class GeolocationHeaderBrowserTest : public InProcessBrowserTest {
 public:
  GeolocationHeaderBrowserTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(omnibox::kPlatformAgnosticXGeo);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &GeolocationHeaderBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    host_resolver()->AddRule("untrusted.com", "127.0.0.1");

    Profile* profile = browser()->profile();
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(u"Test DSE");
    data.SetKeyword(u"testdse");
    data.SetURL(test_server_.GetURL("/search?q={searchTerms}").spec());
    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);
    settings_map->SetContentSettingDefaultScope(
        test_server_.GetURL("/"), test_server_.GetURL("/"),
        ContentSettingsType::GEOLOCATION, CONTENT_SETTING_ALLOW);
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.starts_with("/search?q=redirect-non-dse")) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->AddCustomHeader("Location", "/not-search");
      return response;
    }

    if (request.relative_url.starts_with("/search?q=redirect-same-origin")) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      response->AddCustomHeader("Location", "/search?q=final");
      return response;
    }

    if (request.relative_url.starts_with("/search") ||
        request.relative_url.starts_with("/not-search")) {
      xgeo_header_.clear();
      auto it = request.headers.find(kXGeoHeaderName);

      if (it != request.headers.end()) {
        xgeo_header_ = it->second;
      }
      if (quit_closure_) {
        std::move(quit_closure_).Run();
      }
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }
    return nullptr;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
  base::Lock header_lock_;
  std::string xgeo_header_ GUARDED_BY(header_lock_);
  base::OnceClosure quit_closure_;
};

class GeolocationHeaderFencedFrameBrowserTest
    : public GeolocationHeaderBrowserTest {
 public:
  GeolocationHeaderFencedFrameBrowserTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {omnibox::kPlatformAgnosticXGeo, blink::features::kFencedFrames,
         features::kPrivacySandboxAdsAPIsOverride},
        {});
  }
};

// Test that the X-Geo header is correctly appended for allowed searches.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, AppendsXGeoHeader) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Perform navigation to the search provider mimicking the Omnibox.
  GURL search_url = test_server_.GetURL("/search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_FALSE(xgeo_header_.empty())
      << "X-Geo header should be present in the request.";
  EXPECT_TRUE(xgeo_header_.starts_with("w "))
      << "X-Geo header should start with 'w '.";
}

// Test that the X-Geo header is NOT appended in Incognito mode.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, NoHeaderInIncognito) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Browser* incognito_browser = CreateIncognitoBrowser();

  // Perform navigation in incognito mimicking the Omnibox.
  GURL search_url = test_server_.GetURL("/search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  incognito_browser->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_TRUE(xgeo_header_.empty())
      << "X-Geo header should not be present in Incognito.";
}

// Test that the X-Geo header is NOT appended when geolocation permission is
// denied.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       NoHeaderWithoutPermission) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Revoke permission.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  settings_map->SetContentSettingDefaultScope(
      test_server_.GetURL("/"), test_server_.GetURL("/"),
      ContentSettingsType::GEOLOCATION, CONTENT_SETTING_BLOCK);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Perform navigation to the search provider mimicking the Omnibox.
  GURL search_url = test_server_.GetURL("/search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_TRUE(xgeo_header_.empty())
      << "X-Geo header should not be present when permission is denied.";
}

// Test that the X-Geo header is NOT appended for navigations to non-search
// URLs.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, NoHeaderForNonDse) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Perform navigation to a non-search URL.
  GURL search_url = test_server_.GetURL("/not-search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_TRUE(xgeo_header_.empty())
      << "X-Geo header should not be present for non-search navigations.";
}

// Test that the X-Geo header is removed when redirecting from a search URL to a
// non-search URL.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, RedirectToNonDse) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Perform navigation to a URL that redirects to a non-search URL.
  GURL redirect_url = test_server_.GetURL("/search?q=redirect-non-dse");

  content::OpenURLParams params(
      redirect_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_TRUE(xgeo_header_.empty())
      << "X-Geo header should be removed when redirecting to a non-search URL.";
}

// Test that the X-Geo header is retained when redirecting from a search URL to
// another search URL.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, RedirectToSameOrigin) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Perform navigation to a URL that redirects to another DSE URL (same
  // origin).
  GURL redirect_url = test_server_.GetURL("/search?q=redirect-same-origin");

  content::OpenURLParams params(
      redirect_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  run_loop.Run();

  EXPECT_FALSE(xgeo_header_.empty())
      << "X-Geo header should be kept when redirecting to a DSE URL.";
  EXPECT_TRUE(xgeo_header_.starts_with("w "));
}

IN_PROC_BROWSER_TEST_F(GeolocationHeaderFencedFrameBrowserTest,
                       NoHeaderForFencedFrame) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->profile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Load a main page first to ensure service is active and primed.
  GURL main_url = test_server_.GetURL("/search?q=main");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Create a Fenced Frame manually via JS.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();

  EXPECT_TRUE(
      content::ExecJs(main_frame,
                      "const ff = document.createElement('fencedframe');"
                      "ff.id = 'my_fenced_frame';"
                      "document.body.appendChild(ff);"));

  // Find the Fenced Frame RFH.
  content::RenderFrameHost* fenced_frame_rfh = nullptr;
  main_frame->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (rfh->IsFencedFrameRoot()) {
      fenced_frame_rfh = rfh;
    }
  });
  ASSERT_TRUE(fenced_frame_rfh);

  // Navigate the Fenced Frame manually via JS.
  GURL fenced_frame_url = test_server_.GetURL("/search?q=fenced");

  content::TestFrameNavigationObserver navigation_observer(fenced_frame_rfh);
  EXPECT_TRUE(content::ExecJs(
      fenced_frame_rfh, base::StringPrintf("location.href = '%s';",
                                           fenced_frame_url.spec().c_str())));
  navigation_observer.Wait();

  // Verify that the header was NOT sent for the fenced frame request.
  std::string captured_header;
  {
    base::AutoLock lock(header_lock_);
    captured_header = xgeo_header_;
  }
  EXPECT_TRUE(captured_header.empty())
      << "X-Geo header should not be sent for Fenced Frame navigations.";
}
