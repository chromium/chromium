// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
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

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#endif

namespace {
const char kXGeoHeaderName[] = "X-Geo";
}  // namespace

class GeolocationHeaderBrowserTest : public InProcessBrowserTest {
 public:
  GeolocationHeaderBrowserTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(omnibox::kPlatformAgnosticXGeo);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
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

    Profile* profile = browser()->GetProfile();
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile);
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(u"Test DSE");
    data.SetKeyword(u"testdse");
    data.SetURL(test_server_.GetURL("/search?q={searchTerms}").spec());
    data.send_x_geo_header = true;
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

    if (request.relative_url.starts_with("/search?q=redirect-cross-origin")) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_MOVED_PERMANENTLY);
      GURL cross_origin_url =
          test_server_.GetURL("untrusted.com", "/not-search");
      response->AddCustomHeader("Location", cross_origin_url.spec());
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
      {
        base::AutoLock lock(header_lock_);
        xgeo_header_.clear();
        auto it = request.headers.find(kXGeoHeaderName);

        if (it != request.headers.end()) {
          xgeo_header_ = it->second;
        }
      }

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }
    return nullptr;
  }

  std::string GetXGeoHeader() {
    base::AutoLock lock(header_lock_);
    return xgeo_header_;
  }

  void SetDefaultGeolocationPolicy(int setting_value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kDefaultGeolocationSetting,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(setting_value),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  void RunDefaultPolicyTest(int policy_setting, bool expect_header) {
    base::HistogramTester histogram_tester;
    device::ScopedGeolocationOverrider overrider(
        /*latitude=*/12.34, /*longitude=*/56.78);

    Profile* profile = browser()->GetProfile();
    HostContentSettingsMap* settings_map =
        HostContentSettingsMapFactory::GetForProfile(profile);

    // Clear site-specific exception so the default setting applies.
    settings_map->SetContentSettingDefaultScope(
        test_server_.GetURL("/"), test_server_.GetURL("/"),
        ContentSettingsType::GEOLOCATION, CONTENT_SETTING_DEFAULT);

    // Set user default content setting to an intentionally conflicting value
    // (e.g. ALLOW if policy is BLOCK/ASK, BLOCK if policy is ALLOW) to verify
    // that the enterprise policy takes precedence over the user setting.
    ContentSetting conflicting_user_setting = (policy_setting == 1 /* Allow */)
                                                  ? CONTENT_SETTING_BLOCK
                                                  : CONTENT_SETTING_ALLOW;
    settings_map->SetDefaultContentSetting(ContentSettingsType::GEOLOCATION,
                                           conflicting_user_setting);

    SetDefaultGeolocationPolicy(policy_setting);

    GeolocationHeaderService* geo_service =
        GeolocationHeaderServiceFactory::GetForProfile(profile);
    ASSERT_TRUE(geo_service);

    if (expect_header) {
      OmniboxView* omnibox_view = BrowserWindow::FromBrowser(browser())
                                      ->GetLocationBar()
                                      ->GetOmniboxView();
      omnibox_view->OnBeforePossibleChange();
      omnibox_view->SetUserText(u"test");
      omnibox_view->OnAfterPossibleChange(true);

      EXPECT_TRUE(base::test::RunUntil(
          [&]() { return geo_service->HasCachedLocation(); }));
    }

    GURL search_url = test_server_.GetURL("/search?q=test");

    content::OpenURLParams params(
        search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        false);

    content::TestNavigationObserver navigation_observer(
        browser()->tab_strip_model()->GetActiveWebContents());

    browser()->OpenURL(params, /*navigation_handle_callback=*/{});
    navigation_observer.Wait();

    std::string captured_header = GetXGeoHeader();

    if (expect_header) {
      EXPECT_FALSE(captured_header.empty())
          << "X-Geo header should be present when DefaultGeolocationSetting is "
             "ALLOW.";
      EXPECT_TRUE(captured_header.starts_with("w "));
    } else {
      EXPECT_TRUE(captured_header.empty())
          << "X-Geo header should not be present when "
             "DefaultGeolocationSetting "
             "is not ALLOW.";
    }

    histogram_tester.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                        expect_header, 1);

    content_settings::PageSpecificContentSettings* pscs =
        content_settings::PageSpecificContentSettings::GetForFrame(
            browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetPrimaryMainFrame());
    ASSERT_TRUE(pscs);
    EXPECT_EQ(pscs->IsContentAllowed(ContentSettingsType::GEOLOCATION),
              expect_header);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer test_server_;
  base::Lock header_lock_;
  std::string xgeo_header_ GUARDED_BY(header_lock_);
  base::OnceClosure quit_closure_;

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
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
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_FALSE(GetXGeoHeader().empty())
      << "X-Geo header should be present in the request.";
  EXPECT_TRUE(GetXGeoHeader().starts_with("w "))
      << "X-Geo header should start with 'w '.";

  // The Geolocation usage indicator (Omnibox icon) is a desktop UI feature
  // implemented using the Views toolkit. We wrap the UI verification in
  // TOOLKIT_VIEWS to ensure the test compiles and runs correctly on platforms
  // that use Views (Linux, Windows, ChromeOS) while safely skipping it on
  // Mac (if not using Views).
#if defined(TOOLKIT_VIEWS)
  LocationBarTesting* location_bar_testing =
      BrowserWindow::FromBrowser(browser())
          ->GetLocationBar()
          ->GetLocationBarForTesting();
  ASSERT_TRUE(location_bar_testing);
  EXPECT_TRUE(location_bar_testing->IsContentSettingImageVisible(
      ContentSettingImageModel::GetContentSettingImageModelIndexForTesting(
          ContentSettingImageModel::ImageType::kGeolocation)))
      << "Geolocation usage indicator icon should be visible in the Omnibox.";
#endif
  histogram_tester.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached", true,
                                      1);
}

// Test that the X-Geo header is NOT appended in Incognito mode.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, NoHeaderInIncognito) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();

  // Perform navigation in incognito mimicking the Omnibox.
  GURL search_url = test_server_.GetURL("/search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  content::TestNavigationObserver navigation_observer(
      incognito_browser->GetTabStripModel()->GetActiveWebContents());

  incognito_browser->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_TRUE(GetXGeoHeader().empty())
      << "X-Geo header should not be present in Incognito.";
  histogram_tester.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 0);
}

// Test that the X-Geo header is NOT appended when geolocation permission is
// denied.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       NoHeaderWithoutPermission) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
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
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_TRUE(GetXGeoHeader().empty())
      << "X-Geo header should not be present when permission is denied.";
  histogram_tester.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached",
                                      false, 1);
}

// Test that the X-Geo header is NOT appended for navigations to non-search
// URLs.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, NoHeaderForNonDse) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_TRUE(GetXGeoHeader().empty())
      << "X-Geo header should not be present for non-search navigations.";
  histogram_tester.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 0);
}

// Test that the X-Geo header is removed when redirecting from a search URL to a
// non-search URL.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, RedirectToNonDse) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_TRUE(GetXGeoHeader().empty())
      << "X-Geo header should be removed when redirecting to a non-search URL.";
  histogram_tester.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached", true,
                                      1);
}

// Test that the X-Geo header is retained when redirecting from a search URL to
// another search URL.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest, RedirectToSameOrigin) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  EXPECT_FALSE(GetXGeoHeader().empty())
      << "X-Geo header should be kept when redirecting to a DSE URL.";
  EXPECT_TRUE(GetXGeoHeader().starts_with("w "));
  histogram_tester.ExpectBucketCount("Omnibox.Search.XGeoHeaderAttached", true,
                                     2);
}

IN_PROC_BROWSER_TEST_F(GeolocationHeaderFencedFrameBrowserTest,
                       NoHeaderForFencedFrame) {
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
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
      browser()->GetTabStripModel()->GetActiveWebContents();
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

IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       RedirectToCrossOriginNonDse) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  // Trigger priming by typing in the Omnibox.
  OmniboxView* omnibox_view =
      BrowserWindow::FromBrowser(browser())->GetLocationBar()->GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(u"test");
  omnibox_view->OnAfterPossibleChange(true);

  // Wait until the geolocation service completes the query and caches it.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return geo_service->HasCachedLocation(); }));

  // Perform navigation to a URL that redirects to a cross-origin non-DSE URL.
  GURL redirect_url = test_server_.GetURL("/search?q=redirect-cross-origin");

  content::OpenURLParams params(
      redirect_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  std::string captured_header;
  {
    base::AutoLock lock(header_lock_);
    captured_header = xgeo_header_;
  }
  EXPECT_TRUE(captured_header.empty()) << "X-Geo header should be removed when "
                                          "redirecting to a cross-origin URL.";

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          browser()
              ->GetTabStripModel()
              ->GetActiveWebContents()
              ->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);
  EXPECT_FALSE(pscs->IsContentAllowed(ContentSettingsType::GEOLOCATION));
  histogram_tester.ExpectUniqueSample("Omnibox.Search.XGeoHeaderAttached", true,
                                      1);
}

// Tests that when the DefaultGeolocationSetting enterprise policy is set to
// 2 (BLOCK), Omnibox searches do not attach the X-Geo header.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       DefaultGeolocationPolicyBlocked) {
  RunDefaultPolicyTest(2 /* Block */, /*expect_header=*/false);
}

// Tests that when the DefaultGeolocationSetting enterprise policy is set to
// 1 (ALLOW), Omnibox searches attach the X-Geo header without requiring site
// exceptions.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       DefaultGeolocationPolicyAllowed) {
  RunDefaultPolicyTest(1 /* Allow */, /*expect_header=*/true);
}

// Tests that when the DefaultGeolocationSetting enterprise policy is set to
// 3 (ASK), Omnibox searches without site exceptions do not attach the X-Geo
// header.
IN_PROC_BROWSER_TEST_F(GeolocationHeaderBrowserTest,
                       DefaultGeolocationPolicyAsk) {
  RunDefaultPolicyTest(3 /* Ask */, /*expect_header=*/false);
}

class GeolocationHeaderDisabledBrowserTest : public InProcessBrowserTest {
 public:
  GeolocationHeaderDisabledBrowserTest()
      : test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndDisableFeature(omnibox::kPlatformAgnosticXGeo);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &GeolocationHeaderDisabledBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.starts_with("/search")) {
      base::AutoLock lock(header_lock_);
      xgeo_header_.clear();
      auto it = request.headers.find("X-Geo");
      if (it != request.headers.end()) {
        xgeo_header_ = it->second;
      }
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      return response;
    }
    return nullptr;
  }

 protected:
  net::EmbeddedTestServer test_server_;
  base::Lock header_lock_;
  std::string xgeo_header_ GUARDED_BY(header_lock_);
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GeolocationHeaderDisabledBrowserTest,
                       NoHeaderWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;
  device::ScopedGeolocationOverrider overrider(
      /*latitude=*/12.34, /*longitude=*/56.78);

  Profile* profile = browser()->GetProfile();
  GeolocationHeaderService* geo_service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(geo_service);

  GURL search_url = test_server_.GetURL("/search?q=test");

  content::OpenURLParams params(
      search_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
      false);

  content::TestNavigationObserver navigation_observer(
      browser()->GetTabStripModel()->GetActiveWebContents());

  browser()->OpenURL(params, /*navigation_handle_callback=*/{});

  navigation_observer.Wait();

  std::string captured_header;
  {
    base::AutoLock lock(header_lock_);
    captured_header = xgeo_header_;
  }
  EXPECT_TRUE(captured_header.empty())
      << "X-Geo header should not be present when feature is disabled.";

  histogram_tester.ExpectTotalCount("Omnibox.Search.XGeoHeaderAttached", 0);

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          browser()
              ->GetTabStripModel()
              ->GetActiveWebContents()
              ->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);
  EXPECT_FALSE(pscs->IsContentAllowed(ContentSettingsType::GEOLOCATION));
}
