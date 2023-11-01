// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/instant_test_base.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/site_isolation/site_isolation_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_manager.h"
#include "ui/color/color_provider_source.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/test/base/launchservices_utils_mac.h"
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"  // nogncheck
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"  // nogncheck
#include "ui/base/clipboard/clipboard_format_type.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

namespace {

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeContentBrowserClientBrowserTest() {}

  ChromeContentBrowserClientBrowserTest(
      const ChromeContentBrowserClientBrowserTest&) = delete;
  ChromeContentBrowserClientBrowserTest& operator=(
      const ChromeContentBrowserClientBrowserTest&) = delete;

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

class TopChromeChromeContentBrowserClientTest
    : public ChromeContentBrowserClientBrowserTest {
 public:
  TopChromeChromeContentBrowserClientTest() {
    feature_list_.InitAndEnableFeature(
        features::kTopChromeWebUIUsesSpareRenderer);
  }

  // ChromeContentBrowserClientBrowserTest:
  void SetUpOnMainThread() override {
    ChromeContentBrowserClientBrowserTest::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::SetBrowserClientForTesting(nullptr));
    content::SetBrowserClientForTesting(client_);
  }

  ChromeContentBrowserClient* client() { return client_; }

 private:
  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_MAC)
// TODO(https://crbug.com/1497344) Flaky on Mac.
#define MAYBE_ShouldUseSpareRendererWhenNoTopChromePagesPresent \
  DISABLED_ShouldUseSpareRendererWhenNoTopChromePagesPresent
#else
#define MAYBE_ShouldUseSpareRendererWhenNoTopChromePagesPresent \
  ShouldUseSpareRendererWhenNoTopChromePagesPresent
#endif
IN_PROC_BROWSER_TEST_F(
    TopChromeChromeContentBrowserClientTest,
    MAYBE_ShouldUseSpareRendererWhenNoTopChromePagesPresent) {
  const GURL top_chrome_url(chrome::kChromeUITabSearchURL);
  const GURL non_top_chrome_url(chrome::kChromeUINewTabPageURL);

  const auto navigate_browser = [&](const GURL& url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    content::NavigationEntry* entry = browser()
                                          ->tab_strip_model()
                                          ->GetWebContentsAt(0)
                                          ->GetController()
                                          .GetLastCommittedEntry();
    ASSERT_NE(nullptr, entry);
    EXPECT_EQ(url, entry->GetURL());
    EXPECT_EQ(url, entry->GetVirtualURL());
  };

  // Initially there will be no top chrome pages and the client should return
  // true for using the spare renderer.
  EXPECT_TRUE(client()->ShouldUseSpareRenderProcessHost(browser()->profile(),
                                                        top_chrome_url));

  // Navigate to a top chrome URL.
  navigate_browser(top_chrome_url);

  // The browser now hosts a top chrome page and the client should return false
  // for using the spare renderer.
  EXPECT_FALSE(client()->ShouldUseSpareRenderProcessHost(browser()->profile(),
                                                         top_chrome_url));

  // Navigate away from the top chrome page.
  navigate_browser(non_top_chrome_url);

  // There will no longer be any top chrome pages hosted by the browser and the
  // client should return true for using the spare renderer.
  EXPECT_TRUE(client()->ShouldUseSpareRenderProcessHost(browser()->profile(),
                                                        top_chrome_url));
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class IsolatedOriginNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  IsolatedOriginNTPBrowserTest() {}

  IsolatedOriginNTPBrowserTest(const IsolatedOriginNTPBrowserTest&) = delete;
  IsolatedOriginNTPBrowserTest& operator=(const IsolatedOriginNTPBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(https_test_server().InitializeAndListen());

    // Mark ntp.com (with an appropriate port from the test server) as an
    // isolated origin.
    GURL isolated_url(https_test_server().GetURL("ntp.com", "/"));
    command_line->AppendSwitchASCII(switches::kIsolateOrigins,
                                    isolated_url.spec());
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
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
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
            ntp_site_instance->GetSiteURL());

  // Navigating to a non-NTP URL on ntp.com should not result in an Instant
  // process.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), isolated_url));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetPrimaryMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
            site_instance->GetSiteURL());
}

// Helper class to test window creation from NTP.
class OpenWindowFromNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  OpenWindowFromNTPBrowserTest() {}

  OpenWindowFromNTPBrowserTest(const OpenWindowFromNTPBrowserTest&) = delete;
  OpenWindowFromNTPBrowserTest& operator=(const OpenWindowFromNTPBrowserTest&) =
      delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
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
      ntp_tab->GetPrimaryMainFrame()->GetProcess()->GetID()));

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
      opened_tab->GetPrimaryMainFrame()->GetProcess()->GetID()));
}

// Tests for the preferred color scheme for a given WebContents. The first param
// controls whether the web NativeTheme is light or dark the second controls
// whether the color mode on the associated color provider is light or dark.
class PrefersColorSchemeTest
    : public testing::WithParamInterface<std::tuple<bool, bool>>,
      public InProcessBrowserTest {
 protected:
  PrefersColorSchemeTest()
      : theme_client_(&test_theme_),
        color_provider_source_(GetIsDarkColorProviderColorMode()) {
    test_theme_.SetDarkMode(GetIsDarkNativeTheme());
  }
  ~PrefersColorSchemeTest() override {
    CHECK_EQ(&theme_client_, SetBrowserClientForTesting(original_client_));
  }

  const char* ExpectedColorScheme() const {
    // WebUI's preferred color scheme should reflect the color mode of their
    // associated ColorProvider, and not the preferred color scheme of the web
    // NativeTheme.
    const GURL& last_committed_url = browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetLastCommittedURL();
    if (last_committed_url.SchemeIs(content::kChromeUIScheme)) {
      return GetIsDarkColorProviderColorMode() ? "dark" : "light";
    }
    return GetIsDarkNativeTheme() ? "dark" : "light";
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&theme_client_);
    test_theme_.SetDarkMode(GetIsDarkNativeTheme());
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->SetColorProviderSource(&color_provider_source_);
  }

 protected:
  bool GetIsDarkNativeTheme() const { return std::get<0>(GetParam()); }
  bool GetIsDarkColorProviderColorMode() const {
    return std::get<1>(GetParam());
  }

  ui::TestNativeTheme test_theme_;

 private:
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const raw_ptr<const ui::NativeTheme> theme_;
  };

  class MockColorProviderSource : public ui::ColorProviderSource {
   public:
    explicit MockColorProviderSource(bool is_dark) {
      key_.color_mode = is_dark ? ui::ColorProviderKey::ColorMode::kDark
                                : ui::ColorProviderKey::ColorMode::kLight;
      provider_.GenerateColorMap();
    }
    MockColorProviderSource(const MockColorProviderSource&) = delete;
    MockColorProviderSource& operator=(const MockColorProviderSource&) = delete;
    ~MockColorProviderSource() override = default;

    // ui::ColorProviderSource:
    const ui::ColorProvider* GetColorProvider() const override {
      return &provider_;
    }
    ui::ColorProviderKey GetColorProviderKey() const override { return key_; }

   private:
    ui::ColorProvider provider_;
    ui::ColorProviderKey key_;
  };

  base::test::ScopedFeatureList feature_list_;
  ChromeContentBrowserClientWithWebTheme theme_client_;
  MockColorProviderSource color_provider_source_;
};

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, PrefersColorScheme) {
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-color-scheme.html")))));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedColorScheme()), tab_title);
}

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesChromeSchemes) {
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIDownloadsURL)));

  EXPECT_EQ(
      true,
      EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
             base::StringPrintf(
                 "window.matchMedia('(prefers-color-scheme: %s)').matches",
                 ExpectedColorScheme())));
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesPdfUI) {
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

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

INSTANTIATE_TEST_SUITE_P(All,
                         PrefersColorSchemeTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class PrefersContrastTest
    : public testing::WithParamInterface<ui::NativeTheme::PreferredContrast>,
      public InProcessBrowserTest {
 protected:
  PrefersContrastTest() : theme_client_(&test_theme_) {}

  ~PrefersContrastTest() override {
    CHECK_EQ(&theme_client_, SetBrowserClientForTesting(original_client_));
  }

  const char* ExpectedPrefersContrast() const {
    switch (GetParam()) {
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PrefersContrast");
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "ForcedColors");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&theme_client_);
  }

 protected:
  ui::TestNativeTheme test_theme_;

 private:
  raw_ptr<content::ContentBrowserClient> original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const raw_ptr<const ui::NativeTheme> theme_;
  };

  ChromeContentBrowserClientWithWebTheme theme_client_;
};

IN_PROC_BROWSER_TEST_P(PrefersContrastTest, PrefersContrast) {
  test_theme_.SetPreferredContrast(GetParam());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(
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
                    ui::NativeTheme::PreferredContrast::kCustom));

class ProtocolHandlerTest : public InProcessBrowserTest {
 public:
  ProtocolHandlerTest() = default;

  ProtocolHandlerTest(const ProtocolHandlerTest&) = delete;
  ProtocolHandlerTest& operator=(const ProtocolHandlerTest&) = delete;

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

// TODO(https://crbug.com/1454691): Enable test when MacOS flake is fixed.
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif

#if !BUILDFLAG(IS_ANDROID)
class KeepaliveDurationOnShutdownTest : public InProcessBrowserTest,
                                        public InstantTestBase {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    client_ = static_cast<ChromeContentBrowserClient*>(
        content::SetBrowserClientForTesting(nullptr));
    content::SetBrowserClientForTesting(client_);
  }
  void TearDownOnMainThread() override {
    client_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  raw_ptr<ChromeContentBrowserClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DefaultValue) {
  Profile* profile = browser()->profile();
  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile), base::TimeDelta());
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, PolicySettings) {
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile), base::Seconds(2));
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DynamicUpdate) {
  Profile* profile = browser()->profile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile), base::Seconds(2));

  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 3);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile), base::Seconds(3));
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

class ClipboardTestContentAnalysisDelegate
    : public enterprise_connectors::test::FakeContentAnalysisDelegate {
 public:
  ClipboardTestContentAnalysisDelegate(base::RepeatingClosure delete_closure,
                                       StatusCallback status_callback,
                                       std::string dm_token,
                                       content::WebContents* web_contents,
                                       Data data,
                                       CompletionCallback callback)
      : enterprise_connectors::test::FakeContentAnalysisDelegate(
            delete_closure,
            std::move(status_callback),
            std::move(dm_token),
            web_contents,
            std::move(data),
            std::move(callback)) {}

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
    return ret;
  }

 private:
  void UploadTextForDeepScanning(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override {
    ASSERT_EQ(request->reason(),
              enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE);

    enterprise_connectors::test::FakeContentAnalysisDelegate::
        UploadTextForDeepScanning(std::move(request));
  }

  void UploadImageForDeepScanning(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override {
    ASSERT_EQ(request->reason(),
              enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE);

    enterprise_connectors::test::FakeContentAnalysisDelegate::
        UploadImageForDeepScanning(std::move(request));
  }

  void FakeUploadFileForDeepScanning(
      safe_browsing::BinaryUploadService::Result result,
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

class IsClipboardPasteContentAllowedTest : public InProcessBrowserTest {
 public:
  IsClipboardPasteContentAllowedTest() {
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
        content::SetBrowserClientForTesting(nullptr));
    content::SetBrowserClientForTesting(client_);
  }

  void TearDownOnMainThread() override {
    client_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ChromeContentBrowserClient* client() const { return client_; }

  std::string CreateTestFile(const base::FilePath::StringType& filename,
                             const std::string& content) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path = temp_dir_.GetPath().Append(filename);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(content.data(), content.size());
    return path.AsUTF8Unsafe();
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

IN_PROC_BROWSER_TEST_F(IsClipboardPasteContentAllowedTest, TextAllowed) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data =
      ChromeContentBrowserClient::ClipboardPasteData("allowed", std::string(),
                                                     {});

  client()->IsClipboardPasteContentAllowed(
      contents, GURL("google.com"), ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data,
      base::BindOnce(
          [](absl::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteContentAllowedTest, TextBlocked) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data =
      ChromeContentBrowserClient::ClipboardPasteData("blocked", std::string(),
                                                     {});

  client()->IsClipboardPasteContentAllowed(
      contents, GURL("google.com"), ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data,
      base::BindOnce(
          [](absl::optional<ChromeContentBrowserClient::ClipboardPasteData>
                 clipboard_paste_data) {
#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
            EXPECT_FALSE(clipboard_paste_data.has_value());
#else
            // Platforms that don't support local content analysis shouldn't
            // block anything, even when the policy is set to a local service
            // provider value.
            EXPECT_TRUE(clipboard_paste_data.has_value());
#endif
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteContentAllowedTest, AllFilesAllowed) {
  std::vector<std::string> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow1"), "data"));
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data =
      ChromeContentBrowserClient::ClipboardPasteData(std::string(),
                                                     std::string(), paths);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteContentAllowed(
      contents, GURL("google.com"), ui::ClipboardFormatType::FilenamesType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](absl::optional<ChromeContentBrowserClient::ClipboardPasteData>
                      clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(paths[0], clipboard_paste_data->file_paths[0]);
            EXPECT_EQ(paths[1], clipboard_paste_data->file_paths[1]);
          }));
}

IN_PROC_BROWSER_TEST_F(IsClipboardPasteContentAllowedTest, AllFilesBlocked) {
  std::vector<std::string> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block1"), "data"));

  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data =
      ChromeContentBrowserClient::ClipboardPasteData(std::string(),
                                                     std::string(), paths);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteContentAllowed(
      contents, GURL("google.com"), ui::ClipboardFormatType::FilenamesType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](absl::optional<ChromeContentBrowserClient::ClipboardPasteData>
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

IN_PROC_BROWSER_TEST_F(IsClipboardPasteContentAllowedTest, SomeFilesBlocked) {
  std::vector<std::string> paths;
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("allow0"), "data"));
  paths.push_back(CreateTestFile(FILE_PATH_LITERAL("block1"), "data"));
  ChromeContentBrowserClient::ClipboardPasteData clipboard_paste_data =
      ChromeContentBrowserClient::ClipboardPasteData(std::string(),
                                                     std::string(), paths);

  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  client()->IsClipboardPasteContentAllowed(
      contents, GURL("google.com"), ui::ClipboardFormatType::FilenamesType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [paths](absl::optional<ChromeContentBrowserClient::ClipboardPasteData>
                      clipboard_paste_data) {
            EXPECT_TRUE(clipboard_paste_data.has_value());
            EXPECT_EQ(clipboard_paste_data->file_paths[0], paths[0]);
          }));
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

class AutomaticBeaconCredentialsBrowserTest : public InProcessBrowserTest,
                                              public InstantTestBase {
 public:
  AutomaticBeaconCredentialsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{privacy_sandbox::
                                  kOverridePrivacySandboxSettingsLocalTesting},
        /*disabled_features=*/{});
  }

  AutomaticBeaconCredentialsBrowserTest(
      const AutomaticBeaconCredentialsBrowserTest&) = delete;
  AutomaticBeaconCredentialsBrowserTest& operator=(
      const AutomaticBeaconCredentialsBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
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

  constexpr char kReportingURL[] = "/_report_event_server.html";
  constexpr char kBeaconMessage[] = "this is the message";

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
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kTrackingProtection3pcdEnabled, true);

  // Verify automatic beacons no longer are sent with cookie data.
  EXPECT_TRUE(
      ExecJs(fenced_frame_host,
             content::JsReplace("window.open($1, '_blank');", top_nav_url)));
  second_response.WaitForRequest();
  EXPECT_EQ(0U, second_response.http_request()->headers.count("Cookie"));
}

}  // namespace
