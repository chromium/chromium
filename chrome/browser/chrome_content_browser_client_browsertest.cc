// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/page_colors_controller.h"
#include "chrome/browser/accessibility/page_colors_controller_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/test/fake_clipboard_request_handler.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/frame_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/extension_features.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/launchservices_utils_mac.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#include "third_party/blink/public/common/features.h"
#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"  // nogncheck
#include "ui/base/clipboard/clipboard_format_type.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#endif

namespace {

std::vector<uint8_t> StringToVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }
};

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetWebContentsAt(0)
                                        ->GetController()
                                        .GetLastCommittedEntry();

  ASSERT_TRUE(entry != nullptr);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class IsolatedOriginNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    https_test_server().SetCertHostnames({"ntp.com"});
    ASSERT_TRUE(https_test_server().InitializeAndListen());

    // Mark ntp.com (with an appropriate port from the test server) as an
    // isolated origin.
    GURL isolated_url(https_test_server().GetURL("ntp.com", "/"));
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    isolated_url.spec());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().StartAcceptingConnections();
  }
};

// Verifies that when the remote NTP URL has an origin which is also marked as
// an isolated origin (i.e., requiring a dedicated process), the NTP URL
// still loads successfully, and the resulting process is marked as an Instant
// process.  See https://crbug.com/755595.
IN_PROC_BROWSER_TEST_F(IsolatedOriginNTPBrowserTest,
                       IsolatedOriginDoesNotInterfereWithNTP) {
  GURL base_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), base_url, ntp_url);

  // Sanity check that a SiteInstance for a generic ntp.com URL requires a
  // dedicated process.
  content::BrowserContext* context = browser()->profile();
  GURL isolated_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  scoped_refptr<content::SiteInstance> site_instance =
      content::SiteInstance::CreateForURL(context, isolated_url);
  EXPECT_TRUE(site_instance->RequiresDedicatedProcess());
  // Verify the isolated origin does not receive an NTP site URL scheme.
  EXPECT_FALSE(
      site_instance->GetSiteURL().SchemeIs(chrome::kChromeSearchScheme));

  // The site URL for the NTP URL should resolve to a chrome-search:// URL via
  // GetEffectiveURL(), even if the NTP URL matches an isolated origin.
  scoped_refptr<content::SiteInstance> ntp_site_instance =
      content::SiteInstance::CreateForURL(context, ntp_url);
  EXPECT_TRUE(
      ntp_site_instance->GetSiteURL().SchemeIs(chrome::kChromeSearchScheme));

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
            ntp_site_instance->GetSiteURL());

  // Navigating to a non-NTP URL on ntp.com should not result in an Instant
  // process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), isolated_url));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
            site_instance->GetSiteURL());
}

// Helper class to test window creation from NTP.
class OpenWindowFromNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().SetCertHostnames({"ntp.com"});
    ASSERT_TRUE(https_test_server().InitializeAndListen());
    https_test_server().StartAcceptingConnections();
  }
};

// Test checks that navigations from NTP tab to URLs with same host as NTP but
// different path do not reuse NTP SiteInstance. See https://crbug.com/859062
// for details.
IN_PROC_BROWSER_TEST_F(OpenWindowFromNTPBrowserTest,
                       TransferFromNTPCreateNewTab) {
  GURL search_url =
      https_test_server().GetURL("ntp.com", "/instant_extended.html");
  GURL ntp_url =
      https_test_server().GetURL("ntp.com", "/instant_extended_ntp.html");
  SetupInstant(browser()->profile(), search_url, ntp_url);

  // Navigate to the NTP URL and verify that the resulting process is marked as
  // an Instant process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ntp_url));
  content::WebContents* ntp_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_tab->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));

  // Execute script that creates new window from ntp tab with
  // ntp.com/title1.html as target url. Host is same as remote-ntp host, yet
  // path is different.
  GURL generic_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  content::TestNavigationObserver opened_tab_observer(nullptr);
  opened_tab_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(ExecJs(ntp_tab, "window.open('" + generic_url.spec() + "');"));
  opened_tab_observer.Wait();
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* opened_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Wait until newly opened tab is fully loaded.
  EXPECT_TRUE(WaitForLoadStop(opened_tab));

  EXPECT_NE(opened_tab, ntp_tab);
  EXPECT_EQ(generic_url, opened_tab->GetLastCommittedURL());
  // New created tab should not reside in an Instant process.
  EXPECT_FALSE(instant_service->IsInstantProcess(
      opened_tab->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID()));
}

// Test for the state of Forced Colors Mode for a given WebContents across
// various scenarios.
class ForcedColorsTest : public testing::WithParamInterface<bool>,
                         public InProcessBrowserTest {
 protected:
  static bool ForcedColorsActive() { return GetParam(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ForcedColors");
  }

  ui::MockOsSettingsProvider& os_settings_provider() {
    return os_settings_provider_;
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
};

IN_PROC_BROWSER_TEST_P(ForcedColorsTest, ForcedColors) {
  os_settings_provider().SetForcedColorsActive(ForcedColorsActive());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), chrome_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("forced-colors.html")))));
  std::u16string tab_title;
  const char* expected = ForcedColorsActive() ? "active" : "none";
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(expected), tab_title);
}

IN_PROC_BROWSER_TEST_P(ForcedColorsTest, ForcedColorsWithBlockList) {
  os_settings_provider().SetForcedColorsActive(ForcedColorsActive());

  const char* url = "https://foo.com";
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));

  // Add url to the page colors block list.
  base::Value::List list;
  list.Append(url);
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetList(prefs::kPageColorsBlockList, list.Clone());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

  // Forced colors should be `none` when a site is added to the block list.
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         base::StringPrintf(
                             "window.matchMedia('(forced-colors: %s)').matches",
                             "none")));

  // Remove url from the page colors block list.
  list.EraseValue(base::Value(url));
  profile->GetPrefs()->SetList(prefs::kPageColorsBlockList, list.Clone());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

  // Forced colors should respect the OS when a site is removed from the block
  // list.
  const char* expected = ForcedColorsActive() ? "active" : "none";
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         base::StringPrintf(
                             "window.matchMedia('(forced-colors: %s)').matches",
                             expected)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ForcedColorsTest,
    testing::Bool(),
    [](const ::testing::TestParamInfo<ForcedColorsTest::ParamType>& info) {
      return base::StrCat({"ForcedColors", info.param ? "Active" : "Inactive"});
    });

// Helper class to test the Page colors feature. Page colors is a feature that
// simulates Forced colors mode via a browser setting.
using PageColorsBrowserClientTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PageColorsBrowserClientTest,
                       PageColorsAffectsWebContents) {
  PageColorsControllerFactory::GetForProfile(browser()->profile())
      ->SetRequestedPageColors(PageColors::kDusk);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("http://foo.com")));

  // Check that the page colors are applied when Forced Colors is enabled. For
  // the Dusk theme, the color value for Window is 0x2D3236 which corresponds to
  // rgb(45, 50, 54).
  std::string expected_bg_color = "rgb(45, 50, 54)";
  EXPECT_EQ(expected_bg_color,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "window.getComputedStyle(document.body).getPropertyValue('"
                   "background-color').toString()"));
}

IN_PROC_BROWSER_TEST_F(PageColorsBrowserClientTest,
                       PageColorsAffectsCssPseudoElements) {
  PageColorsControllerFactory::GetForProfile(browser()->profile())
      ->SetRequestedPageColors(PageColors::kDesert);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), chrome_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("system-colors.html")))));

  // Check that the right system color is applied for Pseudo elements when
  // Forced Colors is enabled. For the Desert theme, the color value for
  // WindowText is 0x3D3D3D which corresponds to rgb(61, 61, 61).
  std::string expected_element_color = "rgb(61, 61, 61)";
  EXPECT_EQ(expected_element_color,
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "window.getComputedStyle(document.getElementById('icon'), "
                   "'::before')."
                   "getPropertyValue('color').toString()"));
}

// Tests for the preferred color scheme for a given WebContents. The first param
// controls whether the web NativeTheme is light or dark the second controls
// whether the color mode on the associated color provider is light or dark.
class PrefersColorSchemeTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    os_settings_provider_.SetPreferredColorScheme(
        DarkOs() ? ui::NativeTheme::PreferredColorScheme::kDark
                 : ui::NativeTheme::PreferredColorScheme::kLight);

#if BUILDFLAG(ENABLE_GLIC)
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(embedded_test_server()->Start());
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ::switches::kGlicGuestURL,
        embedded_test_server()->GetURL("/glic/test_client/index.html").spec());
#endif

    guest_view_manager_ =
        guest_view_manager_factory_.GetOrCreateTestGuestViewManager(
            browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                      ->CreateGuestViewManagerDelegate());

    auto* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    web_contents->SetColorProviderSource(&color_provider_source_);
    web_contents->OnWebPreferencesChanged();
  }

  void TearDownOnMainThread() override {
    guest_view_manager_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::string_view ExpectedColorScheme() const {
    // Most web content should follow the browser theme, with the exception of
    // non-WebUI incognito pages, which follow the device theme directly.
    const auto* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const bool use_os_theme =
        browser()->profile()->IsIncognitoProfile() &&
        !web_contents->GetLastCommittedURL().SchemeIs(content::kChromeUIScheme);
    const bool dark_mode = use_os_theme ? DarkOs() : DarkColorProvider();
    return dark_mode ? "dark" : "light";
  }

  guest_view::TestGuestViewManager* guest_view_manager() const {
    return guest_view_manager_;
  }

 private:
  class MockColorProviderSource : public ui::ColorProviderSource {
   public:
    explicit MockColorProviderSource(bool is_dark) {
      key_.color_mode = is_dark ? ui::ColorProviderKey::ColorMode::kDark
                                : ui::ColorProviderKey::ColorMode::kLight;
    }
    MockColorProviderSource(const MockColorProviderSource&) = delete;
    MockColorProviderSource& operator=(const MockColorProviderSource&) = delete;
    ~MockColorProviderSource() override = default;

    // ui::ColorProviderSource:
    const ui::ColorProvider* GetColorProvider() const override {
      return &provider_;
    }

    ui::RendererColorMap GetRendererColorMap(
        ui::ColorProviderKey::ColorMode color_mode,
        ui::ColorProviderKey::ForcedColors forced_colors) const override {
      ui::ColorProviderKey key = GetColorProviderKey();
      key.color_mode = color_mode;
      key.forced_colors = forced_colors;
      return ui::CreateRendererColorMap(
          *ui::ColorProviderManager::Get().GetColorProviderFor(key));
    }

    ui::ColorProviderKey GetColorProviderKey() const override { return key_; }

   private:
    ui::ColorProvider provider_;
    ui::ColorProviderKey key_;
  };

  static bool DarkOs() { return std::get<0>(GetParam()); }
  static bool DarkColorProvider() { return std::get<1>(GetParam()); }

  ui::MockOsSettingsProvider os_settings_provider_;
  MockColorProviderSource color_provider_source_{DarkColorProvider()};
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicTestEnvironment glic_test_environment_;
#endif
  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;
  raw_ptr<guest_view::TestGuestViewManager> guest_view_manager_ = nullptr;
};

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, PrefersColorScheme) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      chrome_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-color-scheme.html")))));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedColorScheme()), tab_title);
}

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesChromeSchemes) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIDownloadsURL)));

  EXPECT_EQ(
      true,
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             base::StringPrintf(
                 "window.matchMedia('(prefers-color-scheme: %s)').matches",
                 ExpectedColorScheme())));
}

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, PrefersColorSchemeGlic) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIGlicURL)));

  guest_view::GuestViewBase* guest_view =
      guest_view_manager()->WaitForSingleGuestViewCreated();
  // Intentionally ignore the return value. It seems that on Windows and Linux
  // the guest contents could have already been loaded by the time we get here.
  std::ignore = guest_view_manager()->WaitUntilAttachedAndLoaded(guest_view);

  EXPECT_EQ(
      true,
      EvalJs(guest_view->GetGuestMainFrame(),
             base::StringPrintf(
                 "window.matchMedia('(prefers-color-scheme: %s)').matches",
                 ExpectedColorScheme())));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesPdfUI) {
  std::string pdf_extension_url(extensions::kExtensionScheme);
  pdf_extension_url.append(url::kStandardSchemeSeparator);
  pdf_extension_url.append(extension_misc::kPdfExtensionId);
  GURL pdf_index = GURL(pdf_extension_url).Resolve("/index.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), pdf_index));

  EXPECT_EQ(
      true,
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             base::StringPrintf(
                 "window.matchMedia('(prefers-color-scheme: %s)').matches",
                 ExpectedColorScheme())));
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefersColorSchemeTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const ::testing::TestParamInfo<PrefersColorSchemeTest::ParamType>&
           info) {
      return base::StrCat({std::get<0>(info.param) ? "Dark" : "Light", "OS_",
                           std::get<1>(info.param) ? "Dark" : "Light",
                           "ColorProvider"});
    });

class PreferredRootScrollbarColorSchemeChromeClientTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public InProcessBrowserTest {
 public:
  PreferredRootScrollbarColorSchemeChromeClientTest()
      : theme_color_(DarkMode() ? SK_ColorDKGRAY : SK_ColorLTGRAY) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
    feature_list_.InitAndEnableFeature(
        blink::features::kRootScrollbarFollowsBrowserTheme);
#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    os_settings_provider_.SetPreferredColorScheme(
        DarkMode() ? ui::NativeTheme::PreferredColorScheme::kDark
                   : ui::NativeTheme::PreferredColorScheme::kLight);
    ThemeService* const theme_service =
        ThemeServiceFactory::GetForProfile(browser()->profile());
    if (UsesCustomTheme()) {
      // Browser themes adapt their hue to match the used color scheme (light
      // or dark), however autogenerated themes don't take color schemes into
      // consideration. So we select which color to use based on the color
      // scheme to simulate this behavior.
      theme_service->BuildAutogeneratedThemeFromColor(theme_color_);
    } else {
      theme_service->UseDefaultTheme();
    }
  }

 protected:
  blink::mojom::PreferredColorScheme ExpectedColorScheme() const {
    return DarkMode() ? blink::mojom::PreferredColorScheme::kDark
                      : blink::mojom::PreferredColorScheme::kLight;
  }

  bool ThemeColorMatches() const {
    const std::optional<SkColor> root_scrollbar_pref =
        browser()
            ->tab_strip_model()
            ->GetActiveWebContents()
            ->GetOrCreateWebPreferences()
            .root_scrollbar_theme_color;
    // If not using a custom theme, `root_scrollbar_theme_color` shouldn't be
    // set.
    if (!UsesCustomTheme()) {
      return !root_scrollbar_pref.has_value();
    }
    EXPECT_TRUE(root_scrollbar_pref.has_value());
    const SkColor root_scrollbar_color = root_scrollbar_pref.value();
    // `root_scrollbar_theme_color` is set based off the toolbar color, which is
    // generated using the theme's color. Because of this, we can't directly
    // compare equality between the two colors and we check that they are
    // similar enough.
    const double r_diff =
        std::abs(SkColorGetR(root_scrollbar_color) -
                 static_cast<double>(SkColorGetR(theme_color_)));
    const double g_diff =
        std::abs(SkColorGetG(root_scrollbar_color) -
                 static_cast<double>(SkColorGetG(theme_color_)));
    const double b_diff =
        std::abs(SkColorGetB(root_scrollbar_color) -
                 static_cast<double>(SkColorGetB(theme_color_)));

    const int kMaxDiff = 20;
    return r_diff < kMaxDiff && b_diff < kMaxDiff && g_diff < kMaxDiff;
  }

 private:
  static bool DarkMode() { return std::get<0>(GetParam()); }
  static bool UsesCustomTheme() { return std::get<1>(GetParam()); }

  ui::MockOsSettingsProvider os_settings_provider_;
  const SkColor theme_color_;
  base::test::ScopedFeatureList feature_list_;
};

// This test verifies that the preferred color scheme for root scrollbars is set
// appropriately following the web content's color scheme and the presence of
// a custom theme.
IN_PROC_BROWSER_TEST_P(PreferredRootScrollbarColorSchemeChromeClientTest,
                       ScrollbarFollowsPreferredColorScheme) {
  auto* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(web_contents->GetOrCreateWebPreferences()
                .preferred_root_scrollbar_color_scheme,
            ExpectedColorScheme());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
// This test verifies that the root scrollbar color theme is set correctly only
// when using a custom theme.
IN_PROC_BROWSER_TEST_P(PreferredRootScrollbarColorSchemeChromeClientTest,
                       VerifyRootScrollbarColorTheme) {
  EXPECT_TRUE(ThemeColorMatches());
}
#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

INSTANTIATE_TEST_SUITE_P(
    All,
    PreferredRootScrollbarColorSchemeChromeClientTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const ::testing::TestParamInfo<
        PreferredRootScrollbarColorSchemeChromeClientTest::ParamType>& info) {
      return base::StrCat({std::get<0>(info.param) ? "Dark" : "Light", "Mode_",
                           std::get<1>(info.param) ? "Custom" : "Default",
                           "Theme"});
    });

class PrefersContrastTest
    : public testing::WithParamInterface<ui::NativeTheme::PreferredContrast>,
      public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PrefersContrast");
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ForcedColors");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    os_settings_provider_.SetPreferredContrast(PreferredContrast());
  }

 protected:
  std::string_view ExpectedPrefersContrast() const {
    switch (PreferredContrast()) {
      case ui::NativeTheme::PreferredContrast::kNoPreference:
        return "no-preference";
      case ui::NativeTheme::PreferredContrast::kMore:
        return "more";
      case ui::NativeTheme::PreferredContrast::kLess:
        return "less";
      case ui::NativeTheme::PreferredContrast::kCustom:
        return "custom";
    }
  }

 private:
  static ui::NativeTheme::PreferredContrast PreferredContrast() {
    return GetParam();
  }

  ui::MockOsSettingsProvider os_settings_provider_;
};

IN_PROC_BROWSER_TEST_P(PrefersContrastTest, PrefersContrast) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      chrome_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-contrast.html")))));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedPrefersContrast()), tab_title);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefersContrastTest,
    testing::Values(ui::NativeTheme::PreferredContrast::kNoPreference,
                    ui::NativeTheme::PreferredContrast::kMore,
                    ui::NativeTheme::PreferredContrast::kLess,
                    ui::NativeTheme::PreferredContrast::kCustom),
    ([](const ::testing::TestParamInfo<PrefersContrastTest::ParamType>& info) {
      using enum ui::NativeTheme::PreferredContrast;
      static constexpr auto kContrastValues =
          base::MakeFixedFlatMap<ui::NativeTheme::PreferredContrast,
                                 std::string_view>({{kNoPreference, "Default"},
                                                    {kMore, "More"},
                                                    {kLess, "Less"},
                                                    {kCustom, "Custom"}});
      return base::StrCat({kContrastValues.at(info.param), "Contrast"});
    }));

class ProtocolHandlerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void AddProtocolHandler(const std::string& scheme,
                          const std::string& redirect_template) {
    protocol_handler_registry()->OnAcceptRegisterProtocolHandler(
        custom_handlers::ProtocolHandler::CreateProtocolHandler(
            scheme, GURL(redirect_template)));
  }

  custom_handlers::ProtocolHandlerRegistry* protocol_handler_registry() {
    return ProtocolHandlerRegistryFactory::GetInstance()->GetForBrowserContext(
        browser()->profile());
  }
};

// TODO(crbug.com/40917055): Enable test when MacOS flake is fixed.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CustomHandler DISABLED_CustomHandler
#else
#define MAYBE_CustomHandler CustomHandler
#endif
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, MAYBE_CustomHandler) {
#if BUILDFLAG(IS_MAC)
  ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif
  AddProtocolHandler("news", "https://abc.xyz/?url=%s");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("news:something")));

  std::u16string expected_title = u"abc.xyz";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// This is a regression test for crbug.com/969177.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, HandlersIgnoredWhenDisabled) {
  AddProtocolHandler("bitcoin", "https://abc.xyz/?url=%s");
  protocol_handler_registry()->Disable();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("bitcoin:something")));

  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(u"about:blank", tab_title);
}

#if BUILDFLAG(IS_CHROMEOS)
// Tests that if a protocol handler is registered for a scheme, an external
// program (another Chrome tab in this case) is not launched to handle the
// navigation. This is a regression test for crbug.com/963133.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, ExternalProgramNotLaunched) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("mailto:bob@example.com")));

  // If an external program (Chrome) was launched, it will result in a second
  // tab being opened.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  // Make sure the protocol handler redirected the navigation.
  std::u16string expected_title = u"mail.google.com";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

namespace {
class FakeExternalProtocolHandlerWorker
    : public shell_integration::DefaultSchemeClientWorker {
 public:
  FakeExternalProtocolHandlerWorker(
      const GURL& url,
      shell_integration::DefaultWebClientState os_state,
      const std::u16string& program_name)
      : shell_integration::DefaultSchemeClientWorker(url),
        os_state_(os_state),
        program_name_(program_name) {}

 protected:
  ~FakeExternalProtocolHandlerWorker() override = default;

 private:
  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return os_state_;
  }

  std::u16string GetDefaultClientNameImpl() override { return program_name_; }

  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
    std::move(on_finished_callback).Run();
  }

  shell_integration::DefaultWebClientState os_state_;
  std::u16string program_name_;
};

class ScopedFakeExternalProtocolHandlerDelegate
    : public ExternalProtocolHandler::Delegate {
 public:
  ScopedFakeExternalProtocolHandlerDelegate() {
    ExternalProtocolHandler::SetDelegateForTesting(this);
  }
  ~ScopedFakeExternalProtocolHandlerDelegate() override {
    ExternalProtocolHandler::SetDelegateForTesting(nullptr);
  }

  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return new FakeExternalProtocolHandlerWorker(
        url, shell_integration::UNKNOWN_DEFAULT, program_name_);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::UNKNOWN;
  }

  void BlockRequest() override {
    FAIL() << "Unexpected BlockRequest call received";
  }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    EXPECT_EQ(program_name_, program_name);
    external_protocol_dialog_called_ = true;
    launched_url_with_security_check_ = url.spec();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    launched_url_without_security_check_ = url.spec();
    launch_url_run_loop_.Quit();
  }

  void FinishedProcessingCheck() override { launch_url_run_loop_.Quit(); }

  void WaitExternalUrlLaunchCompleted() { launch_url_run_loop_.Run(); }

  bool external_protocol_dialog_called() const {
    return external_protocol_dialog_called_;
  }
  std::string_view launched_url_without_security_check() const {
    return launched_url_without_security_check_;
  }
  std::string_view launched_url_with_security_check() const {
    return launched_url_with_security_check_;
  }

 private:
  base::RunLoop launch_url_run_loop_;
  const std::u16string program_name_ = u"custom";
  bool external_protocol_dialog_called_ = false;
  std::string launched_url_without_security_check_;
  std::string launched_url_with_security_check_;
};

}  // namespace

// URLs which are explicitly allowlisted by policy can bypass security checks.
// TODO: https://crbug.com/434758587 - Re-enable this test.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest,
                       DISABLED_SecurityCheckExceptionForAllowlistedUrls) {
  ProtocolHandlerRegistryFactory::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->OnAcceptRegisterProtocolHandler(
          custom_handlers::ProtocolHandler::CreateProtocolHandler(
              "map", GURL("geo://%s")));

  ScopedFakeExternalProtocolHandlerDelegate delegate;

  base::Value::List allowlist;
  allowlist.Append("geo://*");
  browser()->profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlAllowlist,
                                            std::move(allowlist));
  // The call to update the internal allowlist value is async.
  base::RunLoop().RunUntilIdle();

  const char kGeoUrl[] = "geo:48.2082,16.3738";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kGeoUrl)));
  delegate.WaitExternalUrlLaunchCompleted();

  EXPECT_FALSE(delegate.external_protocol_dialog_called());
  EXPECT_EQ(delegate.launched_url_without_security_check(), kGeoUrl);
  EXPECT_EQ(delegate.launched_url_with_security_check(), "");
}

// Regardless of the value of the UrlAllowlist policy, intent:// URLs should
// always be deferred to the external protocol dialog (which currently defers
// the call to ARC).
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest,
                       IntentSchemeBypassSecurityExceptions) {
  ProtocolHandlerRegistryFactory::GetInstance()
      ->GetForBrowserContext(browser()->profile())
      ->OnAcceptRegisterProtocolHandler(
          custom_handlers::ProtocolHandler::CreateProtocolHandler(
              "search", GURL("intent://%s")));

  ScopedFakeExternalProtocolHandlerDelegate delegate;

  base::Value::List allowlist;
  allowlist.Append("intent://*");
  browser()->profile()->GetPrefs()->SetList(policy::policy_prefs::kUrlAllowlist,
                                            std::move(allowlist));
  // The call to update the internal allowlist value is async.
  base::RunLoop().RunUntilIdle();

  const char kIntentUrl[] =
      "intent://www.google.com/"
      "#Intent;scheme=http;package=com.android.chrome;end";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kIntentUrl)));
  delegate.WaitExternalUrlLaunchCompleted();

  EXPECT_TRUE(delegate.external_protocol_dialog_called());
  // intent:// URLs should not skip security checks.
  EXPECT_EQ(delegate.launched_url_without_security_check(), "");
  EXPECT_EQ(delegate.launched_url_with_security_check(), kIntentUrl);
}
#endif

#if !BUILDFLAG(IS_ANDROID)
class KeepaliveDurationOnShutdownTest : public InProcessBrowserTest,
                                        public InstantTestBase {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::GetContentClientForTesting()->browser());
  }

  void TearDownOnMainThread() override {
    client_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  const ChromeContentBrowserClient* client() const { return client_; }

 private:
  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DefaultValue) {
  Profile* profile = browser()->profile();
  EXPECT_EQ(client()->GetKeepaliveTimerTimeout(profile), base::TimeDelta());
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, PolicySettings) {
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client()->GetKeepaliveTimerTimeout(profile), base::Seconds(2));
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DynamicUpdate) {
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client()->GetKeepaliveTimerTimeout(profile), base::Seconds(2));

  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 3);

  EXPECT_EQ(client()->GetKeepaliveTimerTimeout(profile), base::Seconds(3));
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

class ClipboardTestContentAnalysisDelegate
    : public enterprise_connectors::test::FakeContentAnalysisDelegate {
 public:
  static std::unique_ptr<ContentAnalysisDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<ClipboardTestContentAnalysisDelegate>(
        delete_closure, std::move(status_callback), std::move(dm_token),
        web_contents, std::move(data), std::move(callback));
    enterprise_connectors::FilesRequestHandler::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeFilesRequestHandler::Create,
            base::BindRepeating(&ClipboardTestContentAnalysisDelegate::
                                    FakeUploadFileForDeepScanning,
                                base::Unretained(ret.get()))));
    enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeClipboardRequestHandler::Create,
            base::Unretained(ret.get())));
    return ret;
  }

  using FakeContentAnalysisDelegate::FakeContentAnalysisDelegate;

 protected:
  void FakeUploadFileForDeepScanning(
      enterprise_connectors::ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      enterprise_connectors::test::FakeFilesRequestHandler::
          FakeFileRequestCallback callback) override {
    ASSERT_EQ(request->reason(),
              enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE);

    enterprise_connectors::test::FakeContentAnalysisDelegate::
        FakeUploadFileForDeepScanning(result, path, std::move(request),
                                      std::move(callback));
  }
};

class IsClipboardPasteAllowedTest : public InProcessBrowserTest {
 public:
  IsClipboardPasteAllowedTest() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Make sure enterprise policies are set to turn on content analysis.
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(),
        enterprise_connectors::BULK_DATA_ENTRY, kBulkDataEntryPolicyValue);
    enterprise_connectors::test::SetAnalysisConnector(
        browser()->profile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
        kFileAttachedPolicyValue);

    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &ClipboardTestContentAnalysisDelegate::Create, base::DoNothing(),
            base::BindRepeating([](const std::string& contents,
                                   const base::FilePath& path) {
              bool success = false;
              if (!contents.empty()) {
                success = contents.substr(0, 5) == "allow";
              } else {
                success =
                    path.BaseName().AsUTF8Unsafe().substr(0, 5) == "allow";
              }
              return success
                         ? enterprise_connectors::test::
                               FakeContentAnalysisDelegate::SuccessfulResponse(
                                   {"dlp"})
                         : enterprise_connectors::test::
                               FakeContentAnalysisDelegate::DlpResponse(
                                   enterprise_connectors::
                                       ContentAnalysisResponse::Result::SUCCESS,
                                   "rule-name",
                                   enterprise_connectors::
                                       ContentAnalysisResponse::Result::
                                           TriggeredRule::BLOCK);
            }),
            /*dm_token=*/std::string()));

    client_ = static_cast<ChromeContentBrowserClient*>(
        content::GetContentClientForTesting()->browser());
  }

  void TearDownOnMainThread() override {
    client_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ChromeContentBrowserClient* client() { return client_; }

  base::FilePath CreateTestFile(const base::FilePath::StringType& filename,
                                const std::string& content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path = temp_dir_.GetPath().Append(filename);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(base::as_byte_span(content));
    return path;
  }

 private:
  static constexpr char kBulkDataEntryPolicyValue[] = R"(
  {
    "service_provider": "local_system_agent",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1,
    "minimum_data_size": 1
  })";

  static constexpr char kFileAttachedPolicyValue[] = R"(
  {
    "service_provider": "local_system_agent",
    "enable": [
      {
        "url_list": ["*"],
        "tags": ["dlp"]
      }
    ],
    "block_until_verdict": 1
  })";

  base::ScopedTempDir temp_dir_;
  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  enterprise_connectors::FakeContentAnalysisSdkManager sdk_manager_;
#endif
};

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, BitmapAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.png = StringToVector("allowed");

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.png.size(),
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->png, StringToVector("allowed"));
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, BitmapBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.png = StringToVector("blocked");

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.png.size(),
          .format_type = ui::ClipboardFormatType::BitmapType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->png, StringToVector("blocked"));
#endif
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, TextAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"allowed";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.text.size(),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->text, u"allowed");
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, TextBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"blocked";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.text.size(),
          .format_type = ui::ClipboardFormatType::PlainTextType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->text, u"blocked");
#endif
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, HtmlAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.html = u"allowed";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.html.size(),
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->html, u"allowed");
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, HtmlBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.html = u"blocked";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.html.size(),
          .format_type = ui::ClipboardFormatType::HtmlType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->html, u"blocked");
#endif
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, SvgAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.svg = u"allowed";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.svg.size(),
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->svg, u"allowed");
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, SvgBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.svg = u"blocked";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.svg.size(),
          .format_type = ui::ClipboardFormatType::SvgType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->svg, u"blocked");
#endif
          }));
}
IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, RtfAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.rtf = "allowed";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.rtf.size(),
          .format_type = ui::ClipboardFormatType::RtfType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->rtf, "allowed");
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, RtfBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.rtf = "blocked";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.rtf.size(),
          .format_type = ui::ClipboardFormatType::RtfType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->rtf, "blocked");
#endif
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, CustomDataAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.custom_data[u"custom/data"] = u"allowed";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.custom_data[u"custom/data"].size(),
          .format_type = ui::ClipboardFormatType::DataTransferCustomType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->custom_data.size(), 1u);
            EXPECT_TRUE(
                clipboard_paste_data->custom_data.count(u"custom/data"));
            EXPECT_EQ(clipboard_paste_data->custom_data[u"custom/data"],
                      u"allowed");
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, CustomDataBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.custom_data[u"custom/data"] = u"blocked";

  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .size = clipboard_paste_data.custom_data[u"custom/data"].size(),
          .format_type = ui::ClipboardFormatType::DataTransferCustomType(),
      },
      clipboard_paste_data,
      base::BindOnce(
          [](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->custom_data.size(), 1u);
            EXPECT_TRUE(
                clipboard_paste_data->custom_data.count(u"custom/data"));
            EXPECT_EQ(clipboard_paste_data->custom_data[u"custom/data"],
                      u"blocked");
#endif
          }));
}

// TODO(crbug.com/391682998): Enable.
IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, DISABLED_AllFilesAllowed) {
  std::vector<base::FilePath> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow1"), "data"));
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.file_paths = paths;

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .format_type = ui::ClipboardFormatType::FilenamesType(),
      },
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                      clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(paths[0], clipboard_paste_data->file_paths[0]);
            EXPECT_EQ(paths[1], clipboard_paste_data->file_paths[1]);
          }));
}

// TODO(crbug.com/391682998): Enable.
IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, DISABLED_AllFilesBlocked) {
  std::vector<base::FilePath> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block1"), "data"));

  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.file_paths = paths;

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .format_type = ui::ClipboardFormatType::FilenamesType(),
      },
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                      clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->file_paths[0], paths[0]);
            EXPECT_EQ(clipboard_paste_data->file_paths[1], paths[1]);
#endif
          }));
}

// TODO(crbug.com/391682998): Re-enable this test
IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest, DISABLED_SomeFilesBlocked) {
  std::vector<base::FilePath> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block1"), "data"));
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.file_paths = paths;

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteAllowedByPolicy(
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com"))),
      content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [contents] { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      {
          .format_type = ui::ClipboardFormatType::FilenamesType(),
      },
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](std::optional<ChromeContentBrowserClient::ClipboardPasteData>
                      clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->file_paths[0], paths[0]);
          }));
}

// Verifies that the available clipboard types are correctly reported when
// clipboard data is replaced by a Data Controls policy.
IN_PROC_BROWSER_TEST_F(IsClipboardPasteAllowedTest,
                       GetClipboardTypesIfPolicyApplied) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Set policy that blocks copying to the OS clipboard for a specific source.
  data_controls::SetDataControls(browser()->profile()->GetPrefs(), {R"({
      "name": "test rule",
      "sources": {
        "urls": ["https://google.com"]
      },
      "destinations": {
        "os_clipboard": true
      },
      "restrictions": [
        {"class": "CLIPBOARD", "level": "BLOCK"}
      ]
    })"});

  content::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"foo";
  clipboard_paste_data.custom_data[u"custom/data"] = u"custom data";

  base::test::TestFuture<const ui::ClipboardFormatType&,
                         const content::ClipboardPasteData&,
                         std::optional<std::u16string>>
      future;

  // Initiates a copy request and verify that replacement is written as per the
  // policy.
  client()->IsClipboardCopyAllowedByPolicy(
      /*source=*/content::ClipboardEndpoint(
          ui::DataTransferEndpoint(GURL("https://google.com")),
          base::BindLambdaForTesting(
              [&contents]() { return contents->GetBrowserContext(); }),
          *contents->GetPrimaryMainFrame()),
      /*metadata=*/{.size = 1234}, clipboard_paste_data,
      future.GetCallback());
  auto replacement = future.Get<std::optional<std::u16string>>();
  EXPECT_TRUE(replacement.has_value());

  // Triggers the clipboard observer started by `IsClipboardCopyAllowedByPolicy`
  // so that it's aware of the new seqno.
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Verifies that the last replaced clipboard types are retrieved correctly.
  auto types = client()->GetClipboardTypesIfPolicyApplied(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste));

  ASSERT_TRUE(types.has_value());
  ASSERT_EQ(types->size(), 2u);
  EXPECT_EQ((*types)[0], u"text/plain");
  EXPECT_EQ((*types)[1], u"custom/data");
}

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

class AutomaticBeaconCredentialsBrowserTest : public InProcessBrowserTest,
                                              public InstantTestBase {
 public:
  AutomaticBeaconCredentialsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{privacy_sandbox::
                                  kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{
            content_settings::features::kTrackingProtection3pcd});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_test_server().SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  }

 protected:
  content::RenderFrameHost* primary_main_frame_host() {
    auto* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutomaticBeaconCredentialsBrowserTest,
                       3PCEnabledAndDisabled) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  static constexpr char kReportingURL[] = "/_report_event_server.html";
  static constexpr char kBeaconMessage[] = "this is the message";

  net::test_server::ControllableHttpResponse first_response(
      &https_test_server(), kReportingURL);
  net::test_server::ControllableHttpResponse second_response(
      &https_test_server(), kReportingURL);

  ASSERT_TRUE(https_test_server().Start());

  // Set up the document.cookie for credentialed automatic beacons. Automatic
  // beacons are set up in chrome/test/data/interest_group/bidding_logic.js to
  // send to "d.test/_report_event_server.html".
  auto cookie_url = https_test_server().GetURL("d.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), cookie_url));
  EXPECT_TRUE(
      ExecJs(primary_main_frame_host(),
             "document.cookie = 'name=foobarbaz; SameSite=None; Secure';"));

  auto initial_url = https_test_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(
      https_test_server().GetURL("a.test", "/fenced_frames/title1.html"));
  GURL new_tab_url(https_test_server().GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  auto* fenced_frame_host =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());
  content::TestFrameNavigationObserver observer(fenced_frame_host);
  fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
      primary_main_frame_host(), fenced_frame_url, "fenced_frame");
  observer.Wait();

  // The navigation will change the fenced frame node. Get the handle to the new
  // node.
  fenced_frame_host =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());

  // Set the automatic beacon
  EXPECT_TRUE(
      ExecJs(fenced_frame_host,
             content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['seller', 'buyer']
      });
    )",
                                "reserved.top_navigation", kBeaconMessage)));

  // Trigger the first automatic beacon and verify it was sent with cookie data.
  auto top_nav_url = https_test_server().GetURL("a.test", "/empty.html");
  EXPECT_TRUE(
      ExecJs(fenced_frame_host,
             content::JsReplace("window.open($1, '_blank');", top_nav_url)));
  first_response.WaitForRequest();
  EXPECT_EQ(1U, first_response.http_request()->headers.count("Cookie"));
  EXPECT_EQ("name=foobarbaz",
            first_response.http_request()->headers.at("Cookie"));

  // Disable 3rd party cookies.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  // Verify automatic beacons no longer are sent with cookie data.
  EXPECT_TRUE(
      ExecJs(fenced_frame_host,
             content::JsReplace("window.open($1, '_blank');", top_nav_url)));
  second_response.WaitForRequest();
  EXPECT_EQ(0U, second_response.http_request()->headers.count("Cookie"));
}

class TopChromeChromeContentBrowserClientTest
    : public ChromeContentBrowserClientBrowserTest {
 public:
  // ChromeContentBrowserClientBrowserTest:
  void SetUpOnMainThread() override {
    ChromeContentBrowserClientBrowserTest::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::GetContentClientForTesting()->browser());
  }

  ChromeContentBrowserClient* client() { return client_; }

 private:
  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TopChromeChromeContentBrowserClientTest,
                       UnboundRequestDoesNothing) {
#if BUILDFLAG(IS_ANDROID)
  network::URLLoaderFactoryBuilder factory_builder;
  client()->MaybeProxyNetworkBoundRequest(browser()->profile(),
                                          net::handles::kInvalidNetworkHandle,
                                          factory_builder, nullptr);
  EXPECT_EQ(
      client()
          ->get_target_network_for_network_bound_network_context_for_testing(),
      net::handles::kInvalidNetworkHandle);
  EXPECT_FALSE(
      client()->get_network_bound_network_context_for_testing().is_bound());
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "proxying bound requests is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(TopChromeChromeContentBrowserClientTest,
                       BoundRequestCreatesNetworkContext) {
#if BUILDFLAG(IS_ANDROID)
  constexpr net::handles::NetworkHandle network = 1;
  network::URLLoaderFactoryBuilder factory_builder;
  client()->MaybeProxyNetworkBoundRequest(browser()->profile(), network,
                                          factory_builder, nullptr);
  EXPECT_EQ(
      client()
          ->get_target_network_for_network_bound_network_context_for_testing(),
      network);
  EXPECT_TRUE(
      client()->get_network_bound_network_context_for_testing().is_bound());
  EXPECT_TRUE(
      client()->get_network_bound_network_context_for_testing().is_connected());
  {
    base::RunLoop run_loop;
    client()
        ->get_network_bound_network_context_for_testing()
        ->GetBoundNetworkForTesting(base::BindOnce(
            [](base::OnceClosure callback,
               net::handles::NetworkHandle bound_network) {
              EXPECT_EQ(bound_network, network);
              std::move(callback).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "proxying bound requests is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(TopChromeChromeContentBrowserClientTest,
                       BoundRequestWithOverrideCreatesNetworkContext) {
#if BUILDFLAG(IS_ANDROID)
  constexpr net::handles::NetworkHandle network = 1;
  network::URLLoaderFactoryBuilder factory_builder;
  network::mojom::URLLoaderFactoryOverridePtr factory_override;
  EXPECT_FALSE(factory_override);
  client()->MaybeProxyNetworkBoundRequest(browser()->profile(), network,
                                          factory_builder, &factory_override);
  EXPECT_EQ(
      client()
          ->get_target_network_for_network_bound_network_context_for_testing(),
      network);
  EXPECT_TRUE(
      client()->get_network_bound_network_context_for_testing().is_bound());
  EXPECT_TRUE(
      client()->get_network_bound_network_context_for_testing().is_connected());
  EXPECT_TRUE(factory_override->overriding_factory);
  mojo::Remote<network::mojom::URLLoaderFactory> overridden_factory;
  overridden_factory.Bind(std::move(factory_override->overriding_factory));
  {
    base::RunLoop run_loop;
    client()
        ->get_network_bound_network_context_for_testing()
        ->GetBoundNetworkForTesting(base::BindOnce(
            [](base::OnceClosure callback,
               net::handles::NetworkHandle bound_network) {
              EXPECT_EQ(bound_network, network);
              std::move(callback).Run();
            },
            run_loop.QuitClosure()));
    run_loop.Run();
  }
#else   // !BUILDFLAG(IS_ANDROID)
  GTEST_SKIP() << "proxying bound requests is supported only on Android";
#endif  // BUILDFLAG(IS_ANDROID)
}

IN_PROC_BROWSER_TEST_F(TopChromeChromeContentBrowserClientTest,
                       ShouldReuseRendererWhenTopChromePagesPresent) {
  const GURL top_chrome_url(chrome::kChromeUITabSearchURL);
  const GURL top_chrome_url2(chrome::kChromeUIReadLaterURL);
  const GURL random_url(GURL("www.google.com"));

  const auto navigate_browser = [&](const GURL& url, int index) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::NavigationEntry* entry = browser()
                                          ->tab_strip_model()
                                          ->GetWebContentsAt(index)
                                          ->GetController()
                                          .GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(url, entry->GetURL());
    EXPECT_EQ(url, entry->GetVirtualURL());
  };

  // Navigate to a top chrome URL.
  navigate_browser(top_chrome_url, 0);

  // The browser now hosts a top chrome page and the client should return true
  // to reuse the current renderer Host for the other Top Chrome UI pages.
  EXPECT_TRUE(client()->ShouldTryToUseExistingProcessHost(browser()->profile(),
                                                          top_chrome_url2));

  // Should not reuse existing process host for pages that are not Top Chrome
  // UI.
  EXPECT_FALSE(client()->ShouldTryToUseExistingProcessHost(browser()->profile(),
                                                           random_url));
}

class BundledCodeCacheChromeContentBrowserClientTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  BundledCodeCacheChromeContentBrowserClientTest() {
    feature_list_.InitWithFeatureState(features::kWebUIBundledCodeCache,
                                       IsBundledCodeCacheEnabled());
  }

  bool IsBundledCodeCacheEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Assert top-chrome webui renderers disallow v8 feature flag overrides only
// when the bundled webui code cache is enabled.
IN_PROC_BROWSER_TEST_P(BundledCodeCacheChromeContentBrowserClientTest,
                       ConfiguresRendererForDisallowV8FeatureOverrides) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL top_chrome_url1(chrome::kChromeUITabSearchURL);
  const GURL top_chrome_url2(chrome::kChromeUIReadLaterURL);
  const GURL non_top_chrome_url1(chrome::kChromeUINewTabPageURL);
  const GURL non_top_chrome_url2(
      embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(top_chrome_url1.DomainIs(chrome::kChromeUITopChromeDomain));
  EXPECT_TRUE(top_chrome_url2.DomainIs(chrome::kChromeUITopChromeDomain));
  EXPECT_FALSE(non_top_chrome_url1.DomainIs(chrome::kChromeUITopChromeDomain));
  EXPECT_FALSE(non_top_chrome_url1.DomainIs(chrome::kChromeUITopChromeDomain));

  // Disallow V8 feature flag overrides should only apply to top-chrome URLs
  // when bundled code caching is enabled.
  auto navigate_and_expect_policy_result = [this](const GURL& url,
                                                  bool expectation) {
    content::RenderFrameHost* rfh =
        ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_EQ(expectation, rfh->GetProcess()->DisallowV8FeatureFlagOverrides());
  };
  navigate_and_expect_policy_result(top_chrome_url1,
                                    IsBundledCodeCacheEnabled());
  navigate_and_expect_policy_result(top_chrome_url2,
                                    IsBundledCodeCacheEnabled());
  navigate_and_expect_policy_result(non_top_chrome_url1, false);
  navigate_and_expect_policy_result(non_top_chrome_url1, false);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    BundledCodeCacheChromeContentBrowserClientTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<
        BundledCodeCacheChromeContentBrowserClientTest::ParamType>& info) {
      return base::StrCat(
          {"BundledCodeCache", info.param ? "Enabled" : "Disabled"});
    });

class DevToolsOverridesThirdPartyCookiesBrowserTest
    : public InProcessBrowserTest,
      public content::TestDevToolsProtocolClient {
 public:
  DevToolsOverridesThirdPartyCookiesBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    // This feature must be enabled to align the behavior in the test with the
    // actual behavior in the branded-build. Related bug: crbug.com/385032014.
    feature_list_.InitAndEnableFeature(
        extensions_features::kForceWebRequestProxyForTest);
#endif
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    // Open DevTools and enable network agent.
    AttachToWebContents(web_contents());
    SendCommandAsync("Network.enable");
  }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetURL(std::string_view host) { return https_server_.GetURL(host, "/"); }

  void NavigateToPageWithFrame(std::string_view host,
                               Browser* browser_ptr = nullptr) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser_ptr ? browser_ptr : browser(), main_url));
  }

  void NavigateFrameTo(std::string_view host, std::string_view path) {
    GURL page = https_server_.GetURL(host, path);
    EXPECT_TRUE(NavigateIframeToURL(web_contents(), "test", page));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(DevToolsOverridesThirdPartyCookiesBrowserTest,
                       DevToolsForceDisableTPC) {
  const std::string_view kHostA = "a.test";
  const std::string_view kHostB = "b.test";
  // Apply devtools overrides to enable 3pc restriction.
  base::Value::Dict command_params;
  command_params.Set("enableThirdPartyCookieRestriction", true);
  command_params.Set("disableThirdPartyCookieMetadata", false);
  command_params.Set("disableThirdPartyCookieHeuristics", false);
  SendCommandSync("Network.setCookieControls", std::move(command_params));

  NavigateToPageWithFrame(kHostA);
  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // setting 3pc should be blocked due to devtools overrides.
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)), "");

  SendCommandAsync("Network.disable");
  // The override should stop working and setting 3pc is re-allowed after
  // devtools is disabled.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");
}
}  // namespace
