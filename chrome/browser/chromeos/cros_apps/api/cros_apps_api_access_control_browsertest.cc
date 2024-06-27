// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/cros_apps/api/cros_apps_api_mutable_registry.h"
#include "chrome/browser/chromeos/cros_apps/cros_apps_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/runtime_feature_state/runtime_feature_state_context.h"
#include "third_party/blink/public/mojom/chromeos/diagnostics/cros_diagnostics.mojom.h"

class CrosAppsApiAccessControlBrowsertestBase : public InProcessBrowserTest {
 public:
  CrosAppsApiAccessControlBrowsertestBase() {
    features_.InitAndEnableFeature({chromeos::features::kBlinkExtension});
  }

  CrosAppsApiAccessControlBrowsertestBase(
      const CrosAppsApiAccessControlBrowsertestBase&) = delete;
  CrosAppsApiAccessControlBrowsertestBase& operator=(
      const CrosAppsApiAccessControlBrowsertestBase&) = delete;

  ~CrosAppsApiAccessControlBrowsertestBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    // Expose MojoJS so we can ask the renderer to request the test interface to
    // test Mojo binder behavior.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures, "MojoJS");

    // Prevent creation of the startup browser. The test suite needs to set up
    // API info on the Profile before the RenderFrameHost is created, so the
    // RenderFrameHost has updated API info to register Mojo binders on
    // creation.
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  void SetUpOnMainThread() override {
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {std::string(kFooHost), std::string(kBarHost)};
    https_server_.SetSSLConfig(cert_config);
    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  net::EmbeddedTestServer& test_server() { return https_server_; }

 protected:
  // Sets up test API info, and creates a browser with a default opened tab.
  // Must be called once at the beginning of the test body.
  void SetUpTestApi(
      const std::vector<url::Origin>& allowlisted_origins,
      const std::initializer_list<std::reference_wrapper<const base::Feature>>&
          required_features) {
    auto* profile = ProfileManager::GetPrimaryUserProfile();
    CHECK(profile);

    CrosAppsApiMutableRegistry::GetInstance(profile).AddOrReplaceForTesting(
        std::move(CrosAppsApiInfo(CrosAppsApiId::kBlinkExtensionDiagnostics,
                                  &blink::RuntimeFeatureStateContext::
                                      SetBlinkExtensionDiagnosticsEnabled)
                      .AddAllowlistedOrigins(allowlisted_origins)
                      .SetRequiredFeatures(required_features)));

    // Create the browser with a default WebContents for running the tests.
    Browser::CreateParams params(profile, /*user_gesture=*/true);
    Browser::Create(params);
    CHECK_EQ(1u, BrowserList::GetInstance()->size());
    SelectFirstBrowser();
    browser()->window()->Show();

    std::unique_ptr<content::WebContents> web_contents_to_add =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    browser()->tab_strip_model()->AddWebContents(
        std::move(web_contents_to_add), -1, ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
        AddTabTypes::ADD_ACTIVE);
    CHECK_EQ(1, browser()->tab_strip_model()->count());
  }

  bool IsTestApiExposed(const content::ToRenderFrameHost& to_rfh) {
    return IsIdentifierDefined(to_rfh, "chromeos.diagnostics").ExtractBool();
  }

  bool IsChromeOSGlobalExposed(const content::ToRenderFrameHost& to_rfh) {
    return IsIdentifierDefined(to_rfh, "chromeos").ExtractBool();
  }

  void ExpectRendererCrashOnBindTestInterface(
      const content::ToRenderFrameHost& to_rfh,
      std::string_view expected_error) {
// TOOD(b/320182347): Remove the #if condition when we have the a test interface
// that's available in Ash.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crash(
        to_rfh.render_frame_host());
    content::RenderProcessHostBadMojoMessageWaiter crash_watcher(
        to_rfh.render_frame_host()->GetProcess());

    EXPECT_TRUE(content::ExecJs(
        to_rfh, content::JsReplace(
                    "const {handle0, handle1} = Mojo.createMessagePipe();"
                    "Mojo.bindInterface($1, handle0);",
                    kTestMojoInterfaceName)));
    auto crash_message = crash_watcher.Wait();
    EXPECT_TRUE(crash_message);
    EXPECT_EQ(crash_message.value(), expected_error);
#endif
  }

  static constexpr char kFooHost[] = "foo.com";
  static constexpr char kBarHost[] = "bar.com";
  const GURL KDataUrl = GURL("data:text/html,<body></body>");

  const std::string kTestMojoInterfaceName =
      blink::mojom::CrosDiagnostics::Name_;
  const std::string kTestApiId =
      base::ToString(CrosAppsApiId::kBlinkExtensionDiagnostics);
  const std::string kNoBinderFoundError = base::StringPrintf(
      "Received bad user message: No binder found for interface %s for the "
      "frame/document scope",
      kTestMojoInterfaceName.c_str());
  const std::string kNotAllowedToAccessApiError = base::StringPrintf(
      "Received bad user message: The requesting context isn't allowed to "
      "access interface %s because it isn't allowed to access the "
      "corresponding API: %s",
      kTestMojoInterfaceName.c_str(),
      base::ToString(kTestApiId).c_str());

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::test::ScopedFeatureList features_;
};

// Test fixture with no base::Feature flags enabled. Mostly used for tests where
// the API doesn't require any feature flags.
using CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest =
    CrosAppsApiAccessControlBrowsertestBase;

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       EmptyAllowlistDoesNotEnableApi) {
  SetUpTestApi(/*allowlisted_origins=*/{}, /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/iframe.html")));
  // Main frame doesn't get the API.
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));

  // Child frame doesn't get the API.
  auto* child_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  EXPECT_FALSE(IsChromeOSGlobalExposed(child_frame));

  // The renderer is terminated if it requests the API's Mojo interface.
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       OnlyAllowlistedOriginHasApi) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The allowlisted origin get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  EXPECT_TRUE(IsTestApiExposed(web_contents));

  // The non-allowlisted origin doesn't get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kBarHost, "/empty.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       OnlyEnableApiInMainFrame) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The allowlisted origin's top-level frame get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/iframe.html")));
  EXPECT_TRUE(IsTestApiExposed(web_contents));

  // The allowlisted origin's child frame doesn't get the API.
  auto* child_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  EXPECT_FALSE(IsChromeOSGlobalExposed(child_frame));
  ExpectRendererCrashOnBindTestInterface(child_frame,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       AllowlistedOriginIsGatedByBaseFeature) {
  SetUpTestApi(
      /*allowlisted_origin*/ {test_server().GetOrigin(kFooHost)},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The allowlisted origin doesn't get the API when the API is gated by a
  // base::Feature that's not enabled.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents, kNoBinderFoundError);

  // The non-allowlisted origin doesn't get the API when the test API is gated
  // by a base::Feature that's not enabled.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kBarHost, "/empty.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents, kNoBinderFoundError);
}

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       DataUrlDoesNotHaveApi) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Pages with an data: scheme shouldn't get the API.
  ASSERT_TRUE(NavigateToURL(web_contents, KDataUrl));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(CrosAppsApiAccessControlWithNoFeatureFlagsBrowsertest,
                       BlobUrlDoesNotHaveApi) {
  const auto kAllowlistedOrigin = test_server().GetOrigin(kFooHost);
  SetUpTestApi(
      /*allowlisted_origins=*/{kAllowlistedOrigin},
      /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  ASSERT_TRUE(IsTestApiExposed(web_contents));

  // Create a blob: URL that can be opened in another WebContents.
  GURL blob_url =
      GURL(content::EvalJs(
               web_contents,
               "URL.createObjectURL(new Blob(['<html><body></body></html>']));")
               .ExtractString());
  ASSERT_TRUE(kAllowlistedOrigin.IsSameOriginWith(blob_url));

  // Navigate to the blob URL in a new WebContents.
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      /*foreground=*/true);
  auto* blob_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(blob_web_contents, blob_url));
  ASSERT_EQ(kAllowlistedOrigin,
            blob_web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Verify the API isn't enabled.
  EXPECT_FALSE(IsChromeOSGlobalExposed(blob_web_contents));
  ExpectRendererCrashOnBindTestInterface(blob_web_contents,
                                         kNotAllowedToAccessApiError);
}

// Test fixture that enabled the base::Feature corresponding to the test API.
// Mostly used for tests where the API requires a feature flag.
class CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest
    : public CrosAppsApiAccessControlBrowsertestBase {
 public:
  CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest() {
    features_.InitAndEnableFeature(
        chromeos::features::kBlinkExtensionDiagnostics);
  }

  ~CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest() override =
      default;

 protected:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    EmptyAllowlistDoesNotEnableApi) {
  SetUpTestApi(
      /*allowlisted_origins=*/{},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The top-level frame doesn't get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/iframe.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));

  // The child frame doesn't get the API.
  auto* child_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  EXPECT_FALSE(IsChromeOSGlobalExposed(child_frame));

  // The renderer is terminated if it requests the API's Mojo interface.
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    DataUrlDoesNotHaveApi) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Page with an data: scheme doesn't get the API.
  ASSERT_TRUE(NavigateToURL(web_contents, KDataUrl));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    BlobUrlDoesNotHaveApi) {
  const auto kAllowlistedOrigin = test_server().GetOrigin(kFooHost);
  SetUpTestApi(
      /*allowlisted_origins=*/{kAllowlistedOrigin},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  ASSERT_TRUE(IsTestApiExposed(web_contents));

  // Create a blob: URL that can be opened in another WebContents.
  GURL blob_url =
      GURL(content::EvalJs(
               web_contents,
               "URL.createObjectURL(new Blob(['<html><body></body></html>']));")
               .ExtractString());
  ASSERT_TRUE(kAllowlistedOrigin.IsSameOriginWith(blob_url));

  // Navigate to the blob URL in a new WebContents.
  browser()->tab_strip_model()->AppendWebContents(
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile())),
      /*foreground=*/true);
  auto* blob_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(blob_web_contents, blob_url));
  ASSERT_EQ(kAllowlistedOrigin,
            blob_web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // Verify the API isn't enabled.
  EXPECT_FALSE(IsChromeOSGlobalExposed(blob_web_contents));
  ExpectRendererCrashOnBindTestInterface(blob_web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    OnlyAllowlistedOriginHasApi) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The allowlisted origin get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  EXPECT_TRUE(IsTestApiExposed(web_contents));

  // The non-allowlisted origin doesn't get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kBarHost, "/empty.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    OnlyEnableApiInMainFrame) {
  SetUpTestApi(
      /*allowlisted_origins=*/{test_server().GetOrigin(kFooHost)},
      /*required_features=*/{chromeos::features::kBlinkExtensionDiagnostics});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The allowlisted origin's top-level frame get the API.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/iframe.html")));
  EXPECT_TRUE(IsTestApiExposed(web_contents));

  // The allowlisted origin's child frame doesn't get the API.
  auto* child_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  EXPECT_FALSE(IsChromeOSGlobalExposed(child_frame));
  ExpectRendererCrashOnBindTestInterface(child_frame,
                                         kNotAllowedToAccessApiError);
}

IN_PROC_BROWSER_TEST_F(
    CrosAppsApiAccessControlWithEnabledTestBaseFeatureBrowsertest,
    ApiWithoutBaseFeatureRequirement) {
  SetUpTestApi(/*allowlisted_origins=*/{}, /*required_features=*/{});

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // The API isn't enabled.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            test_server().GetURL(kFooHost, "/empty.html")));
  EXPECT_FALSE(IsChromeOSGlobalExposed(web_contents));
  ExpectRendererCrashOnBindTestInterface(web_contents,
                                         kNotAllowedToAccessApiError);
}
