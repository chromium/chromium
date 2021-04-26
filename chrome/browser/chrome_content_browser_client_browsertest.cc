// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
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
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
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
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/test_native_theme.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "url/url_constants.h"
#endif

#if defined(OS_MAC)
#include "chrome/test/base/launchservices_utils_mac.h"
#endif

namespace {

// Use a test class with SetUpCommandLine to ensure the flag is sent to the
// first renderer process.
class ChromeContentBrowserClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeContentBrowserClientBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientBrowserTest);
};

// Test that a basic navigation works in --site-per-process mode.  This prevents
// regressions when that mode calls out into the ChromeContentBrowserClient,
// such as http://crbug.com/164223.
IN_PROC_BROWSER_TEST_F(ChromeContentBrowserClientBrowserTest,
                       SitePerProcessNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/title1.html"));

  ui_test_utils::NavigateToURL(browser(), url);
  content::NavigationEntry* entry = browser()
                                        ->tab_strip_model()
                                        ->GetWebContentsAt(0)
                                        ->GetController()
                                        .GetLastCommittedEntry();

  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url, entry->GetURL());
  EXPECT_EQ(url, entry->GetVirtualURL());
}

// Helper class to mark "https://ntp.com/" as an isolated origin.
class IsolatedOriginNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  IsolatedOriginNTPBrowserTest() {}

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

 private:
  DISALLOW_COPY_AND_ASSIGN(IsolatedOriginNTPBrowserTest);
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
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(contents->GetMainFrame()->GetSiteInstance()->GetSiteURL(),
            ntp_site_instance->GetSiteURL());

  // Navigating to a non-NTP URL on ntp.com should not result in an Instant
  // process.
  ui_test_utils::NavigateToURL(browser(), isolated_url);
  EXPECT_FALSE(instant_service->IsInstantProcess(
      contents->GetMainFrame()->GetProcess()->GetID()));
  EXPECT_EQ(contents->GetMainFrame()->GetSiteInstance()->GetSiteURL(),
            site_instance->GetSiteURL());
}

// Helper class to test window creation from NTP.
class OpenWindowFromNTPBrowserTest : public InProcessBrowserTest,
                                     public InstantTestBase {
 public:
  OpenWindowFromNTPBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_test_server().InitializeAndListen());
    https_test_server().StartAcceptingConnections();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenWindowFromNTPBrowserTest);
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
  ui_test_utils::NavigateToURL(browser(), ntp_url);
  content::WebContents* ntp_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_tab->GetMainFrame()->GetProcess()->GetID()));

  // Execute script that creates new window from ntp tab with
  // ntp.com/title1.html as target url. Host is same as remote-ntp host, yet
  // path is different.
  GURL generic_url(https_test_server().GetURL("ntp.com", "/title1.html"));
  content::TestNavigationObserver opened_tab_observer(nullptr);
  opened_tab_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(
      ExecuteScript(ntp_tab, "window.open('" + generic_url.spec() + "');"));
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
      opened_tab->GetMainFrame()->GetProcess()->GetID()));
}

class PrefersColorSchemeTest : public testing::WithParamInterface<bool>,
                               public InProcessBrowserTest {
 protected:
  PrefersColorSchemeTest() : theme_client_(&test_theme_) {
    feature_list_.InitWithFeatureState(features::kWebUIDarkMode, GetParam());
  }

  ~PrefersColorSchemeTest() override {
    CHECK_EQ(&theme_client_, SetBrowserClientForTesting(original_client_));
  }

  const char* ExpectedColorScheme() const {
    return GetParam() ? "dark" : "light";
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "MediaQueryPrefersColorScheme");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    original_client_ = SetBrowserClientForTesting(&theme_client_);
  }

 protected:
  ui::TestNativeTheme test_theme_;

 private:
  content::ContentBrowserClient* original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const ui::NativeTheme* const theme_;
  };

  base::test::ScopedFeatureList feature_list_;
  ChromeContentBrowserClientWithWebTheme theme_client_;
};

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, PrefersColorScheme) {
  test_theme_.SetDarkMode(GetParam());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();
  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-color-scheme.html"))));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedColorScheme()), tab_title);
}

IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesChromeSchemes) {
  test_theme_.SetDarkMode(true);
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIDownloadsURL));

  bool matches;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.domAutomationController.send(window."
                         "matchMedia('(prefers-color-scheme: %s)').matches)",
                         ExpectedColorScheme()),
      &matches));
  EXPECT_TRUE(matches);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
IN_PROC_BROWSER_TEST_P(PrefersColorSchemeTest, FeatureOverridesPdfUI) {
  test_theme_.SetDarkMode(true);
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();

  std::string pdf_extension_url(extensions::kExtensionScheme);
  pdf_extension_url.append(url::kStandardSchemeSeparator);
  pdf_extension_url.append(extension_misc::kPdfExtensionId);
  GURL pdf_index = GURL(pdf_extension_url).Resolve("/index.html");
  ui_test_utils::NavigateToURL(browser(), pdf_index);

  bool matches;
  ASSERT_TRUE(ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(),
      base::StringPrintf("window.domAutomationController.send(window."
                         "matchMedia('(prefers-color-scheme: %s)').matches)",
                         ExpectedColorScheme()),
      &matches));
  EXPECT_TRUE(matches);
}
#endif

INSTANTIATE_TEST_SUITE_P(All, PrefersColorSchemeTest, testing::Bool());

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
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
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
  content::ContentBrowserClient* original_client_ = nullptr;

  class ChromeContentBrowserClientWithWebTheme
      : public ChromeContentBrowserClient {
   public:
    explicit ChromeContentBrowserClientWithWebTheme(
        const ui::NativeTheme* theme)
        : theme_(theme) {}

   protected:
    const ui::NativeTheme* GetWebTheme() const override { return theme_; }

   private:
    const ui::NativeTheme* const theme_;
  };

  ChromeContentBrowserClientWithWebTheme theme_client_;
};

IN_PROC_BROWSER_TEST_P(PrefersContrastTest, PrefersContrast) {
  test_theme_.set_preferred_contrast(GetParam());
  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->OnWebPreferencesChanged();
  ui_test_utils::NavigateToURL(
      browser(),
      ui_test_utils::GetTestUrl(
          base::FilePath(base::FilePath::kCurrentDirectory),
          base::FilePath(FILE_PATH_LITERAL("prefers-contrast.html"))));
  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(base::ASCIIToUTF16(ExpectedPrefersContrast()), tab_title);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefersContrastTest,
    testing::Values(ui::NativeTheme::PreferredContrast::kNoPreference,
                    ui::NativeTheme::PreferredContrast::kMore,
                    ui::NativeTheme::PreferredContrast::kLess));

class ProtocolHandlerTest : public InProcessBrowserTest {
 public:
  ProtocolHandlerTest() = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void AddProtocolHandler(const std::string& scheme,
                          const std::string& redirect_template) {
    protocol_handler_registry()->OnAcceptRegisterProtocolHandler(
        ProtocolHandler::CreateProtocolHandler(scheme,
                                               GURL(redirect_template)));
  }

  ProtocolHandlerRegistry* protocol_handler_registry() {
    return ProtocolHandlerRegistryFactory::GetInstance()->GetForBrowserContext(
        browser()->profile());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerTest);
};

IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, CustomHandler) {
#if defined(OS_MAC)
  ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif
  AddProtocolHandler("news", "https://abc.xyz/?url=%s");

  ui_test_utils::NavigateToURL(browser(), GURL("news:something"));

  std::u16string expected_title = u"abc.xyz";
  content::TitleWatcher title_watcher(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// This is a regression test for crbug.com/969177.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, HandlersIgnoredWhenDisabled) {
  AddProtocolHandler("bitcoin", "https://abc.xyz/?url=%s");
  protocol_handler_registry()->Disable();

  ui_test_utils::NavigateToURL(browser(), GURL("bitcoin:something"));

  std::u16string tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(u"about:blank", tab_title);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that if a protocol handler is registered for a scheme, an external
// program (another Chrome tab in this case) is not launched to handle the
// navigation. This is a regression test for crbug.com/963133.
IN_PROC_BROWSER_TEST_F(ProtocolHandlerTest, ExternalProgramNotLaunched) {
  ui_test_utils::NavigateToURL(browser(), GURL("mailto:bob@example.com"));

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

#if !defined(OS_ANDROID)
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

  ChromeContentBrowserClient* client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DefaultValue) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile), base::TimeDelta());
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, PolicySettings) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile),
            base::TimeDelta::FromSeconds(2));
}

IN_PROC_BROWSER_TEST_F(KeepaliveDurationOnShutdownTest, DynamicUpdate) {
  Profile* profile =
      g_browser_process->profile_manager()->GetPrimaryUserProfile();
  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 2);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile),
            base::TimeDelta::FromSeconds(2));

  profile->GetPrefs()->SetInteger(prefs::kFetchKeepaliveDurationOnShutdown, 3);

  EXPECT_EQ(client_->GetKeepaliveTimerTimeout(profile),
            base::TimeDelta::FromSeconds(3));
}

#endif  // !defined(OS_ANDROID)

}  // namespace
