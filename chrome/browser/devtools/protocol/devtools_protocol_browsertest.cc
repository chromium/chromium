// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_switches.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_saver/data_saver.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/ip_address.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_server_config.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif

using DevToolsProtocolTest = DevToolsProtocolTestBase;
using testing::AllOf;
using testing::Contains;
using testing::Eq;
using testing::Not;

namespace {

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       VisibleSecurityStateChangedNeutralState) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));

  Attach();
  SendCommandAsync("Security.enable");
  base::Value::Dict params =
      WaitForNotification("Security.visibleSecurityStateChanged", true);

  std::string* security_state =
      params.FindStringByDottedPath("visibleSecurityState.securityState");
  ASSERT_TRUE(security_state);
  EXPECT_EQ(std::string("neutral"), *security_state);
  EXPECT_FALSE(params.FindStringByDottedPath(
      "visibleSecurityState.certificateSecurityState"));
  EXPECT_FALSE(
      params.FindStringByDottedPath("visibleSecurityState.safetyTipInfo"));
  const base::Value* security_state_issue_ids =
      params.FindByDottedPath("visibleSecurityState.securityStateIssueIds");
  EXPECT_TRUE(base::Contains(security_state_issue_ids->GetList(),
                             base::Value("scheme-is-not-cryptographic")));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CreateDeleteContext) {
  AttachToBrowserTarget();
  for (int i = 0; i < 2; i++) {
    const base::Value::Dict* result =
        SendCommandSync("Target.createBrowserContext");
    std::string context_id = *result->FindString("browserContextId");

    base::Value::Dict params;
    params.Set("url", "about:blank");
    params.Set("browserContextId", context_id);
    SendCommandSync("Target.createTarget", std::move(params));

    params = base::Value::Dict();
    params.Set("browserContextId", context_id);
    SendCommandSync("Target.disposeBrowserContext", std::move(params));
  }
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NewTabPageInCreatedContextDoesNotCrash) {
  AttachToBrowserTarget();
  const base::Value::Dict* result =
      SendCommandSync("Target.createBrowserContext");
  std::string context_id = *result->FindString("browserContextId");

  base::Value::Dict params;
  params.Set("url", chrome::kChromeUINewTabURL);
  params.Set("browserContextId", context_id);
  content::WebContentsAddedObserver observer;
  SendCommandSync("Target.createTarget", std::move(params));
  content::WebContents* wc = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(wc));
  EXPECT_EQ(chrome::kChromeUINewTabURL, wc->GetLastCommittedURL().spec());

  // Should not crash by this point.
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       CreateBrowserContextAcceptsProxyServer) {
  AttachToBrowserTarget();
  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
            std::make_unique<net::test_server::BasicHttpResponse>());
        http_response->set_code(net::HTTP_OK);
        http_response->set_content_type("text/html");
        http_response->set_content("<title>Hello from proxy server!</title>");
        return std::move(http_response);
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  base::Value::Dict params;
  params.Set("proxyServer",
             embedded_test_server()->host_port_pair().ToString());
  const base::Value::Dict* result =
      SendCommandSync("Target.createBrowserContext", std::move(params));
  std::string context_id = *result->FindString("browserContextId");

  content::WebContentsAddedObserver observer;

  params = base::Value::Dict();
  params.Set("url", "http://this-page-does-not-exist.com/site.html");
  params.Set("browserContextId", context_id);
  result = SendCommandSync("Target.createTarget", std::move(params));

  content::WebContents* wc = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(wc));

  EXPECT_EQ(GURL("http://this-page-does-not-exist.com/site.html"),
            wc->GetURL());

  EXPECT_EQ("Hello from proxy server!", base::UTF16ToUTF8(wc->GetTitle()));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CreateInDefaultContextById) {
  AttachToBrowserTarget();
  const base::Value::Dict* result = SendCommandSync("Target.getTargets");
  const base::Value::List* list = result->FindList("targetInfos");
  ASSERT_TRUE(list->size() == 1);
  const std::string context_id =
      *list->front().GetDict().FindString("browserContextId");

  base::Value::Dict params;
  params.Set("url", "about:blank");
  params.Set("browserContextId", context_id);
  result = SendCommandSync("Target.createTarget", std::move(params));
  ASSERT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       InputDispatchEventsToCorrectTarget) {
  Attach();

  std::string setup_logging = R"(
      window.logs = [];
      ['dragenter', 'keydown', 'mousedown', 'mouseenter', 'mouseleave',
       'mousemove', 'mouseout', 'mouseover', 'mouseup', 'click', 'touchcancel',
       'touchend', 'touchmove', 'touchstart',
      ].forEach((event) =>
        window.addEventListener(event, (e) => logs.push(e.type)));)";
  content::WebContents* target_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* other_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(
      content::EvalJs(target_web_contents, setup_logging).error.empty());
  EXPECT_TRUE(content::EvalJs(other_web_contents, setup_logging).error.empty());

  base::Value::Dict params;
  params.Set("button", "left");
  params.Set("clickCount", 1);
  params.Set("x", 100);
  params.Set("y", 250);
  params.Set("clickCount", 1);

  params.Set("type", "mousePressed");
  SendCommandSync("Input.dispatchMouseEvent", params.Clone());

  params.Set("type", "mouseMoved");
  params.Set("y", 270);
  SendCommandSync("Input.dispatchMouseEvent", params.Clone());

  params.Set("type", "mouseReleased");
  SendCommandSync("Input.dispatchMouseEvent", std::move(params));

  params = base::Value::Dict();
  params.Set("x", 100);
  params.Set("y", 250);
  params.Set("type", "dragEnter");
  params.SetByDottedPath("data.dragOperationsMask", 1);
  params.SetByDottedPath("data.items", base::Value::List());
  SendCommandSync("Input.dispatchDragEvent", std::move(params));

  params = base::Value::Dict();
  params.Set("x", 100);
  params.Set("y", 250);
  SendCommandSync("Input.synthesizeTapGesture", std::move(params));

  params = base::Value::Dict();
  params.Set("type", "keyDown");
  params.Set("key", "a");
  SendCommandSync("Input.dispatchKeyEvent", std::move(params));

  content::EvalJsResult main_target_events =
      content::EvalJs(target_web_contents, "logs.join(' ')");
  content::EvalJsResult other_target_events =
      content::EvalJs(other_web_contents, "logs.join(' ')");
  // mouse events might happen in the other_target if the real mouse pointer
  // happens to be over the browser window
  EXPECT_THAT(
      base::SplitString(main_target_events.ExtractString(), " ",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL),
      AllOf(Contains("mouseover"), Contains("mousedown"), Contains("mousemove"),
            Contains("mouseup"), Contains("click"), Contains("dragenter"),
            Contains("keydown")));
  EXPECT_THAT(base::SplitString(other_target_events.ExtractString(), " ",
                                base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL),
              AllOf(Not(Contains("click")), Not(Contains("dragenter")),
                    Not(Contains("keydown"))));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NoInputEventsSentToBrowserWhenDisallowed) {
  SetIsTrusted(false);
  Attach();

  base::Value::Dict params;
  params.Set("type", "rawKeyDown");
  params.Set("key", "F12");
  params.Set("windowsVirtualKeyCode", 123);
  params.Set("nativeVirtualKeyCode", 123);
  SendCommandSync("Input.dispatchKeyEvent", std::move(params));

  EXPECT_EQ(nullptr, DevToolsWindow::FindDevToolsWindow(agent_host_.get()));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       PreloadEnabledStateUpdatedDefault) {
  Attach();

  SendCommandAsync("Preload.enable");
  const base::Value::Dict result =
      WaitForNotification("Preload.preloadEnabledStateUpdated", true);

  EXPECT_THAT(result.FindBool("disabledByPreference"), false);
  EXPECT_THAT(result.FindBool("disabledByHoldbackPrefetchSpeculationRules"),
              false);
  EXPECT_THAT(result.FindBool("disabledByHoldbackPrerenderSpeculationRules"),
              false);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       PreloadEnabledStateUpdatedDisabledByPreference) {
  Attach();

  prefetch::SetPreloadPagesState(browser()->profile()->GetPrefs(),
                                 prefetch::PreloadPagesState::kNoPreloading);

  SendCommandAsync("Preload.enable");
  const base::Value::Dict result =
      WaitForNotification("Preload.preloadEnabledStateUpdated", true);

  EXPECT_THAT(result.FindBool("disabledByPreference"), true);
}

class DevToolsProtocolTest_PreloadEnabledStateUpdatedDisabledByHoldback
    : public DevToolsProtocolTest {
 protected:
  void SetUp() override {
    // This holds back speculation rules prefetch and prerender. Note that
    // directly using enums (instead of strings) in the call to SetHoldback is
    // usually preferred, but this is not possible here because
    // content::content_preloading_predictor::kSpeculationRules, which is not
    // exposed outside of content.
    preloading_config_override_.SetHoldback("Prefetch", "SpeculationRules",
                                            true);
    preloading_config_override_.SetHoldback("Prerender", "SpeculationRules",
                                            true);

    DevToolsProtocolTest::SetUp();
  }

 private:
  content::test::PreloadingConfigOverride preloading_config_override_;
};

IN_PROC_BROWSER_TEST_F(
    DevToolsProtocolTest_PreloadEnabledStateUpdatedDisabledByHoldback,
    PreloadEnabledStateUpdatedDisabledByHoldback) {
  Attach();

  SendCommandAsync("Preload.enable");
  const base::Value::Dict result =
      WaitForNotification("Preload.preloadEnabledStateUpdated", true);

  EXPECT_THAT(result.FindBool("disabledByHoldbackPrefetchSpeculationRules"),
              true);
  EXPECT_THAT(result.FindBool("disabledByHoldbackPrerenderSpeculationRules"),
              true);
}

class DevToolsProtocolTest_PrefetchHoldbackDisabledIfCDPClientConnected
    : public DevToolsProtocolTest {
 protected:
  void SetUp() override {
    preloading_config_override_.SetHoldback("Prefetch", "SpeculationRules",
                                            true);

    DevToolsProtocolTest::SetUp();
  }

 private:
  content::test::PreloadingConfigOverride preloading_config_override_;
};

// Check that prefetch is enabled if DevToolsAgentHost exists even if it is
// disabled by PreloadingConfig.
IN_PROC_BROWSER_TEST_F(
    DevToolsProtocolTest_PrefetchHoldbackDisabledIfCDPClientConnected,
    PrefetchHoldbackDisabledIfCDPClientConnected) {
  Attach();

  {
    SendCommandAsync("Preload.enable");
    const base::Value::Dict result =
        WaitForNotification("Preload.preloadEnabledStateUpdated", true);

    EXPECT_THAT(result.FindBool("disabledByHoldbackPrefetchSpeculationRules"),
                true);
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  const std::string add_specrules = R"(
    const specrules = document.createElement("script");
    specrules.type = "speculationrules";
    specrules.text = `
      {
        "prefetch":[
          {
            "source": "list",
            "urls": ["title1.html"]
          }
        ]
      }`;
    document.body.appendChild(specrules);
  )";

  EXPECT_TRUE(content::EvalJs(web_contents(), add_specrules).error.empty());

  {
    base::Value::Dict result;
    while (true) {
      result = WaitForNotification("Preload.prefetchStatusUpdated", true);
      if (*result.FindString("status") == "Ready") {
        break;
      }
    }
  }
}

IN_PROC_BROWSER_TEST_F(
    DevToolsProtocolTest,
    NoPendingUrlShownWhenAttachedToBrowserInitiatedFailedNavigation) {
  GURL url("invalid.scheme:for-sure");
  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;

  content::WebContents* web_contents = browser()->OpenURL(
      content::OpenURLParams(url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  tab_added_waiter.Wait();
  ASSERT_TRUE(WaitForLoadStop(web_contents));

  content::NavigationController& navigation_controller =
      web_contents->GetController();
  content::NavigationEntry* pending_entry =
      navigation_controller.GetPendingEntry();
  ASSERT_NE(nullptr, pending_entry);
  EXPECT_EQ(url, pending_entry->GetURL());

  EXPECT_EQ(pending_entry, navigation_controller.GetVisibleEntry());
  agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(web_contents);
  agent_host_->AttachClient(this);
  SendCommandSync("Page.enable");

  // Ensure that a failed pending entry is cleared when the DevTools protocol
  // attaches, so that any modified page content is not attributed to the failed
  // URL. (crbug/1192417)
  EXPECT_EQ(nullptr, navigation_controller.GetPendingEntry());
  EXPECT_EQ(GURL(""), navigation_controller.GetVisibleEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NoPendingUrlShownForPageNavigateFromChromeExtension) {
  GURL url("https://example.com");
  // DevTools protocol use cases that have an initiator origin (e.g., for
  // extensions) should use renderer-initiated navigations and be subject to URL
  // spoof defenses.
  SetNavigationInitiatorOrigin(
      url::Origin::Create(GURL("chrome-extension://abc123/")));

  // Attach DevTools and start a navigation but don't wait for it to finish.
  Attach();
  SendCommandSync("Page.enable");
  base::Value::Dict params;
  params.Set("url", url.spec());
  SendCommandAsync("Page.navigate", std::move(params));
  content::NavigationController& navigation_controller =
      web_contents()->GetController();
  content::NavigationEntry* pending_entry =
      navigation_controller.GetPendingEntry();
  ASSERT_NE(nullptr, pending_entry);
  EXPECT_EQ(url, pending_entry->GetURL());

  // Attaching the DevTools protocol to the initial empty document of a new tab
  // should prevent the pending URL from being visible, since the protocol
  // allows modifying the initial empty document in a way that could be useful
  // for URL spoofs.
  EXPECT_NE(pending_entry, navigation_controller.GetVisibleEntry());
  EXPECT_NE(nullptr, navigation_controller.GetPendingEntry());
  EXPECT_EQ(GURL("about:blank"),
            navigation_controller.GetVisibleEntry()->GetURL());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, SetRPHRegistrationMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  // Initial value
  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  EXPECT_EQ(custom_handlers::RphRegistrationMode::kNone,
            registry->registration_mode());

  // Set a value not defined in AutoResponseMode enum
  base::Value::Dict params_invalid_enum;
  params_invalid_enum.Set("mode", "accept");
  SendCommandAsync("Page.setRPHRegistrationMode",
                   std::move(params_invalid_enum));
  EXPECT_EQ(custom_handlers::RphRegistrationMode::kNone,
            registry->registration_mode());

  // Set a invalid value, but defined in the AutoResponseMode enum
  base::Value::Dict params_invalid;
  params_invalid.Set("mode", "autoOptOut");
  SendCommandAsync("Page.setRPHRegistrationMode", std::move(params_invalid));
  EXPECT_EQ(custom_handlers::RphRegistrationMode::kNone,
            registry->registration_mode());

  // Set a valid value
  base::Value::Dict params;
  params.Set("mode", "autoAccept");
  SendCommandAsync("Page.setRPHRegistrationMode", std::move(params));
  EXPECT_EQ(custom_handlers::RphRegistrationMode::kAutoAccept,
            registry->registration_mode());
}

class DevToolsProtocolTest_BounceTrackingMitigations
    : public DevToolsProtocolTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kDIPS,
                               {{"delete", "true"},
                                {"triggering_action", "stateful_bounce"}}}},
        /*disabled_features=*/{});

    DevToolsProtocolTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    DevToolsProtocolTest::SetUpOnMainThread();
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_BounceTrackingMitigations,
                       RunBounceTrackingMitigations) {
  SetBlockThirdPartyCookies(true);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  const GURL bouncer(
      embedded_test_server()->GetURL("example.test", "/title1.html"));

  // Record a stateful bounce for `bouncer`.
  ASSERT_TRUE(SimulateDipsBounce(
      web_contents(), embedded_test_server()->GetURL("a.test", "/empty.html"),
      bouncer, embedded_test_server()->GetURL("b.test", "/empty.html"),
      embedded_test_server()->GetURL("c.test", "/empty.html")));

  SendCommandSync("Storage.runBounceTrackingMitigations");

  const base::Value::List* deleted_sites_list =
      result()->FindList("deletedSites");
  ASSERT_TRUE(deleted_sites_list);

  std::vector<std::string> deleted_sites;
  for (const auto& site : *deleted_sites_list) {
    deleted_sites.push_back(site.GetString());
  }

  EXPECT_THAT(deleted_sites, testing::ElementsAre("example.test"));
}

class DIPSStatusDevToolsProtocolTest
    : public DevToolsProtocolTest,
      public testing::WithParamInterface<std::tuple<bool, bool, std::string>> {
  // The fields of `GetParam()` indicate/control the following:
  //   `std::get<0>(GetParam())` => `features::kDIPS`
  //   `std::get<1>(GetParam())` => `features::kDIPSDeletionEnabled`
  //   `std::get<2>(GetParam())` => `features::kDIPSTriggeringAction`
  //
  // In order for Bounce Tracking Mitigations to take effect, `features::kDIPS`
  // must be true/enabled, `kDeletionEnabled` must be true, and
  // `kTriggeringAction` must NOT be `none`.
  //
  // Note: Bounce Tracking Mitigations issues only report sites that would
  // be affected when `kTriggeringAction` is set to 'stateful_bounce'.

 protected:
  void SetUp() override {
    if (std::get<0>(GetParam())) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kDIPS,
          {{"delete", (std::get<1>(GetParam()) ? "true" : "false")},
           {"triggering_action", std::get<2>(GetParam())}});
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kDIPS);
    }

    DevToolsProtocolTest::SetUp();
  }

  bool ShouldBeEnabled() {
    return (std::get<0>(GetParam()) && std::get<1>(GetParam()) &&
            (std::get<2>(GetParam()) != "none"));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DIPSStatusDevToolsProtocolTest,
                       TrueWhenEnabledAndDeleting) {
  AttachToBrowserTarget();

  base::Value::Dict paramsDIPS;
  paramsDIPS.Set("featureState", "DIPS");

  SendCommand("SystemInfo.getFeatureState", std::move(paramsDIPS));
  EXPECT_EQ(result()->FindBool("featureEnabled"), ShouldBeEnabled());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DIPSStatusDevToolsProtocolTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Values("none", "storage", "bounce", "stateful_bounce")));

using DevToolsProtocolTest_AppId = DevToolsProtocolTest;

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId, ReturnsManifestAppId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_with_id.json"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  const base::Value::Dict* result = SendCommandSync("Page.getAppId");
  EXPECT_EQ(*result->FindString("appId"),
            embedded_test_server()->GetURL("/some_id"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId,
                       ReturnsStartUrlAsManifestAppIdIfNotSet) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  const base::Value::Dict* result = SendCommandSync("Page.getAppId");
  EXPECT_EQ(*result->FindString("appId"),
            embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  EXPECT_EQ(*result->FindString("recommendedId"),
            "/web_apps/no_service_worker.html");
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId, ReturnsNoAppIdIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  const base::Value::Dict* result = SendCommandSync("Page.getAppId");
  EXPECT_FALSE(result->Find("appId"));
  EXPECT_FALSE(result->Find("recommendedId"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, VisibleSecurityStateSecureState) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), https_server.GetURL("/title1.html"), 1);
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  // Extract SSL status data from the navigation entry.
  scoped_refptr<net::X509Certificate> page_cert = entry->GetSSL().certificate;
  ASSERT_TRUE(page_cert);

  int ssl_version =
      net::SSLConnectionStatusToVersion(entry->GetSSL().connection_status);
  const char* page_protocol;
  net::SSLVersionToString(&page_protocol, ssl_version);

  const char* page_key_exchange_str;
  const char* page_cipher;
  const char* page_mac;
  bool is_aead;
  bool is_tls13;
  uint16_t page_cipher_suite =
      net::SSLConnectionStatusToCipherSuite(entry->GetSSL().connection_status);
  net::SSLCipherSuiteToStrings(&page_key_exchange_str, &page_cipher, &page_mac,
                               &is_aead, &is_tls13, page_cipher_suite);
  std::string page_key_exchange;
  if (page_key_exchange_str)
    page_key_exchange = page_key_exchange_str;

  const char* page_key_exchange_group =
      SSL_get_curve_name(entry->GetSSL().key_exchange_group);

  std::string page_subject_name;
  std::string page_issuer_name;
  double page_valid_from = 0.0;
  double page_valid_to = 0.0;
  if (entry->GetSSL().certificate) {
    page_subject_name = entry->GetSSL().certificate->subject().common_name;
    page_issuer_name = entry->GetSSL().certificate->issuer().common_name;
    page_valid_from =
        entry->GetSSL().certificate->valid_start().InSecondsFSinceUnixEpoch();
    page_valid_to =
        entry->GetSSL().certificate->valid_expiry().InSecondsFSinceUnixEpoch();
  }

  std::string page_certificate_network_error;
  if (net::IsCertStatusError(entry->GetSSL().cert_status)) {
    page_certificate_network_error = net::ErrorToString(
        net::MapCertStatusToNetError(entry->GetSSL().cert_status));
  }

  bool page_certificate_has_weak_signature =
      (entry->GetSSL().cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);

  bool page_certificate_has_sha1_signature_present =
      (entry->GetSSL().cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);

  int status = net::ObsoleteSSLStatus(entry->GetSSL().connection_status,
                                      entry->GetSSL().peer_signature_algorithm);
  bool page_modern_ssl = status == net::OBSOLETE_SSL_NONE;
  bool page_obsolete_ssl_protocol = status & net::OBSOLETE_SSL_MASK_PROTOCOL;
  bool page_obsolete_ssl_key_exchange =
      status & net::OBSOLETE_SSL_MASK_KEY_EXCHANGE;
  bool page_obsolete_ssl_cipher = status & net::OBSOLETE_SSL_MASK_CIPHER;
  bool page_obsolete_ssl_signature = status & net::OBSOLETE_SSL_MASK_SIGNATURE;

  Attach();
  SendCommandAsync("Security.enable");
  auto has_certificate = [](const base::Value::Dict& params) {
    return params.FindListByDottedPath(
               "visibleSecurityState.certificateSecurityState.certificate") !=
           nullptr;
  };
  base::Value::Dict params =
      WaitForMatchingNotification("Security.visibleSecurityStateChanged",
                                  base::BindRepeating(has_certificate));

  // Verify that the visibleSecurityState payload matches the SSL status data.
  std::string* security_state =
      params.FindStringByDottedPath("visibleSecurityState.securityState");
  ASSERT_TRUE(security_state);
  ASSERT_EQ(std::string("secure"), *security_state);

  base::Value* certificate_security_state =
      params.FindByDottedPath("visibleSecurityState.certificateSecurityState");
  ASSERT_TRUE(certificate_security_state);
  base::Value::Dict& dict = certificate_security_state->GetDict();

  std::string* protocol = dict.FindString("protocol");
  ASSERT_TRUE(protocol);
  ASSERT_EQ(*protocol, page_protocol);

  std::string* key_exchange = dict.FindString("keyExchange");
  ASSERT_TRUE(key_exchange);
  ASSERT_EQ(*key_exchange, page_key_exchange);

  std::string* key_exchange_group = dict.FindString("keyExchangeGroup");
  if (key_exchange_group) {
    ASSERT_EQ(*key_exchange_group, page_key_exchange_group);
  }

  std::string* mac = dict.FindString("mac");
  if (mac) {
    ASSERT_EQ(*mac, page_mac);
  }

  std::string* cipher = dict.FindString("cipher");
  ASSERT_TRUE(cipher);
  ASSERT_EQ(*cipher, page_cipher);

  std::string* subject_name = dict.FindString("subjectName");
  ASSERT_TRUE(subject_name);
  ASSERT_EQ(*subject_name, page_subject_name);

  std::string* issuer = dict.FindString("issuer");
  ASSERT_TRUE(issuer);
  ASSERT_EQ(*issuer, page_issuer_name);

  auto valid_from = dict.FindDouble("validFrom");
  ASSERT_TRUE(valid_from);
  ASSERT_EQ(*valid_from, page_valid_from);

  auto valid_to = dict.FindDouble("validTo");
  ASSERT_TRUE(valid_to);
  ASSERT_EQ(*valid_to, page_valid_to);

  std::string* certificate_network_error =
      dict.FindString("certificateNetworkError");
  if (certificate_network_error) {
    ASSERT_EQ(*certificate_network_error, page_certificate_network_error);
  }

  auto certificate_has_weak_signature =
      dict.FindBool("certificateHasWeakSignature");
  ASSERT_TRUE(certificate_has_weak_signature);
  ASSERT_EQ(*certificate_has_weak_signature,
            page_certificate_has_weak_signature);

  auto certificate_has_sha1_signature_present =
      dict.FindBool("certificateHasSha1Signature");
  ASSERT_TRUE(certificate_has_sha1_signature_present);
  ASSERT_EQ(*certificate_has_sha1_signature_present,
            page_certificate_has_sha1_signature_present);

  auto modern_ssl = dict.FindBool("modernSSL");
  ASSERT_TRUE(modern_ssl);
  ASSERT_EQ(*modern_ssl, page_modern_ssl);

  auto obsolete_ssl_protocol = dict.FindBool("obsoleteSslProtocol");
  ASSERT_TRUE(obsolete_ssl_protocol);
  ASSERT_EQ(*obsolete_ssl_protocol, page_obsolete_ssl_protocol);

  auto obsolete_ssl_key_exchange = dict.FindBool("obsoleteSslKeyExchange");
  ASSERT_TRUE(obsolete_ssl_key_exchange);
  ASSERT_EQ(*obsolete_ssl_key_exchange, page_obsolete_ssl_key_exchange);

  auto obsolete_ssl_cipher = dict.FindBool("obsoleteSslCipher");
  ASSERT_TRUE(obsolete_ssl_cipher);
  ASSERT_EQ(*obsolete_ssl_cipher, page_obsolete_ssl_cipher);

  auto obsolete_ssl_signature = dict.FindBool("obsoleteSslSignature");
  ASSERT_TRUE(obsolete_ssl_signature);
  ASSERT_EQ(*obsolete_ssl_signature, page_obsolete_ssl_signature);

  const base::Value::List* certificate_value = dict.FindList("certificate");
  ASSERT_TRUE(certificate_value);
  std::vector<std::string> der_certs;
  for (const auto& cert : *certificate_value) {
    std::string decoded;
    ASSERT_TRUE(base::Base64Decode(cert.GetString(), &decoded));
    der_certs.push_back(decoded);
  }
  std::vector<std::string_view> cert_string_piece;
  for (const auto& str : der_certs) {
    cert_string_piece.push_back(str);
  }

  // Check that the certificateSecurityState.certificate matches.
  net::SHA256HashValue page_cert_chain_fingerprint =
      page_cert->CalculateChainFingerprint256();
  scoped_refptr<net::X509Certificate> certificate =
      net::X509Certificate::CreateFromDERCertChain(cert_string_piece);
  ASSERT_TRUE(certificate);
  EXPECT_EQ(page_cert_chain_fingerprint,
            certificate->CalculateChainFingerprint256());
  const base::Value* security_state_issue_ids =
      params.FindByDottedPath("visibleSecurityState.securityStateIssueIds");
  ASSERT_TRUE(security_state_issue_ids->is_list());
  EXPECT_EQ(security_state_issue_ids->GetList().size(), 0u);

  EXPECT_FALSE(params.FindByDottedPath("visibleSecurityState.safetyTipInfo"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       AutomationOverrideShowsAndRemovesInfoBar) {
  Attach();
  auto* manager = infobars::ContentInfoBarManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  {
    base::Value::Dict params;
    params.Set("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(manager->infobars().size(), 1u);
  {
    base::Value::Dict params;
    params.Set("enabled", false);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(manager->infobars().size(), 0u);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       AutomationOverrideAddsOneInfoBarOnly) {
  Attach();
  auto* manager = infobars::ContentInfoBarManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  {
    base::Value::Dict params;
    params.Set("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(manager->infobars().size(), 1u);
  {
    base::Value::Dict params;
    params.Set("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(manager->infobars().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, UntrustedClient) {
  SetIsTrusted(false);
  Attach();
  EXPECT_FALSE(SendCommandSync("HeapProfiler.enable"));  // Implemented in V8
  EXPECT_FALSE(SendCommandSync("LayerTree.enable"));     // Implemented in blink
  EXPECT_FALSE(SendCommandSync(
      "Memory.prepareForLeakDetection"));        // Implemented in content
  EXPECT_FALSE(SendCommandSync("Cast.enable"));  // Implemented in content
  EXPECT_TRUE(SendCommandSync("Accessibility.enable"));
}

class DevToolsProtocolScreenshotTest : public DevToolsProtocolTest {
 protected:
  void SetUp() override {
    EnablePixelOutput();
    DevToolsProtocolTest::SetUp();
  }

  SkBitmap CaptureScreenshot() {
    SendCommandSync("Page.captureScreenshot");
    CHECK(!error());
    const std::string* base64_data = result()->FindString("data");
    CHECK(base64_data);
    std::string png_data;
    CHECK(base::Base64Decode(*base64_data, &png_data));
    SkBitmap bitmap;
    CHECK(gfx::PNGCodec::Decode(
        reinterpret_cast<unsigned const char*>(png_data.data()),
        png_data.size(), &bitmap));
    return bitmap;
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolScreenshotTest, ScreenshotInactiveTab) {
  static constexpr char kBluePageURL[] =
      R"(data:text/html,<body style="background-color: blue"></body>)";
  static constexpr char kRedPageURL[] =
      R"(data:text/html,<body style="background-color: red"></body>)";
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL(kBluePageURL), 1);
  EXPECT_TRUE(WaitForLoadStop(web_contents()));
  Attach();
  constexpr int kIndex = 1;
  ASSERT_TRUE(AddTabAtIndex(kIndex, GURL(kRedPageURL),
                            ui::PageTransition::PAGE_TRANSITION_TYPED));

  SkBitmap bitmap = CaptureScreenshot();
  SkColor pixel_color = bitmap.getColor(100, 100);
  EXPECT_EQ(SK_ColorBLUE, pixel_color);
}

class ExtensionProtocolTest : public DevToolsProtocolTest {
 protected:
  void SetUpOnMainThread() override {
    DevToolsProtocolTest::SetUpOnMainThread();
    Profile* profile = browser()->profile();
    extension_service_ =
        extensions::ExtensionSystem::Get(profile)->extension_service();
    extension_registry_ = extensions::ExtensionRegistry::Get(profile);
  }

  content::WebContents* web_contents() override {
    return background_web_contents_;
  }

  const extensions::Extension* LoadExtensionOrApp(
      const base::FilePath& extension_path) {
    extensions::TestExtensionRegistryObserver observer(extension_registry_);
    extensions::UnpackedInstaller::Create(extension_service_)
        ->Load(extension_path);
    observer.WaitForExtensionLoaded();
    const extensions::Extension* extension = nullptr;
    for (const auto& enabled_extension :
         extension_registry_->enabled_extensions()) {
      if (enabled_extension->path() == extension_path) {
        extension = enabled_extension.get();
        break;
      }
    }
    CHECK(extension) << "Failed to find loaded extension " << extension_path;
    return extension;
  }

  const extensions::Extension* LoadExtension(
      const base::FilePath& extension_path) {
    ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");
    const extensions::Extension* extension = LoadExtensionOrApp(extension_path);
    auto* process_manager =
        extensions::ProcessManager::Get(browser()->profile());
    if (extensions::BackgroundInfo::IsServiceWorkerBased(extension)) {
      EXPECT_TRUE(activated_listener.WaitUntilSatisfied());
      auto worker_ids =
          process_manager->GetServiceWorkersForExtension(extension->id());
      CHECK_EQ(1lu, worker_ids.size());
    } else {
      extensions::ExtensionHost* host =
          process_manager->GetBackgroundHostForExtension(extension->id());
      background_web_contents_ = host->host_contents();
    }

    return extension;
  }

  void LaunchApp(const std::string& app_id) {
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest);
    apps::AppServiceProxyFactory::GetForProfile(browser()->profile())
        ->BrowserAppLauncher()
        ->LaunchAppWithParamsForTesting(std::move(params));
  }

  void ReloadExtension(const std::string& extension_id) {
    extensions::TestExtensionRegistryObserver observer(extension_registry_);
    extension_service_->ReloadExtension(extension_id);
    observer.WaitForExtensionLoaded();
  }

 private:
  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_;
  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged> extension_registry_;
  raw_ptr<content::WebContents, DanglingUntriaged> background_web_contents_;
#if BUILDFLAG(IS_WIN)
  // This is needed to stop ExtensionProtocolTestsfrom creating a
  // shortcut in the Windows start menu. The override needs to last until the
  // test is destroyed, because Windows shortcut tasks which create the shortcut
  // can run after the test body returns.
  base::ScopedPathOverride override_start_dir{base::DIR_START_MENU};
#endif  // BUILDFLAG(IS_WIN
};

IN_PROC_BROWSER_TEST_F(ExtensionProtocolTest, ReloadTracedExtension) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("simple_background_page");
  auto* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);
  Attach();
  ReloadExtension(extension->id());
  base::Value::Dict params;
  params.Set("categories", "-*");
  SendCommandSync("Tracing.start", std::move(params));
  SendCommandAsync("Tracing.end");
  WaitForNotification("Tracing.tracingComplete", true);
}

IN_PROC_BROWSER_TEST_F(ExtensionProtocolTest,
                       DISABLED_ReloadServiceWorkerExtension) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("service_worker");
  std::string extension_id;
  {
    // `extension` is stale after reload.
    auto* extension = LoadExtension(extension_path);
    ASSERT_THAT(extension, testing::NotNull());
    extension_id = extension->id();
  }
  AttachToBrowserTarget();
  const base::Value::Dict* result = SendCommandSync("Target.getTargets");

  std::string target_id;
  base::Value::Dict ext_target;
  for (const auto& target : *result->FindList("targetInfos")) {
    if (*target.GetDict().FindString("type") == "service_worker") {
      ext_target = target.Clone().TakeDict();
      break;
    }
  }
  {
    base::Value::Dict params;
    params.Set("targetId", CHECK_DEREF(ext_target.FindString("targetId")));
    params.Set("waitForDebuggerOnStart", false);
    SendCommandSync("Target.autoAttachRelated", std::move(params));
  }
  ReloadExtension(extension_id);
  base::Value::Dict attached =
      WaitForNotification("Target.attachedToTarget", true);
  base::Value* targetInfo = attached.Find("targetInfo");
  ASSERT_THAT(targetInfo, testing::NotNull());
  EXPECT_THAT(
      targetInfo->GetDict(),
      base::test::DictionaryHasValue("type", base::Value("service_worker")));
  EXPECT_THAT(targetInfo->GetDict(),
              base::test::DictionaryHasValue(
                  "url", CHECK_DEREF(ext_target.Find("url"))));
  EXPECT_THAT(attached.FindBool("waitingForDebugger"),
              testing::Optional(false));

  {
    base::Value::Dict params;
    params.Set("targetId", *targetInfo->GetDict().FindString("targetId"));
    params.Set("waitForDebuggerOnStart", false);
    SendCommandSync("Target.autoAttachRelated", std::move(params));
  }
  auto detached = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_THAT(*detached.FindString("sessionId"), Eq("sessionId"));
}

// Accepts a list of URL predicates and allows awaiting for all matching
// WebContents to load.
class WebContentsBarrier {
 public:
  using Predicate = base::FunctionRef<bool(const GURL& url)>;

  WebContentsBarrier(std::initializer_list<Predicate> predicates)
      : predicates_(predicates) {}

  std::vector<raw_ptr<content::WebContents, VectorExperimental>> Await() {
    if (!IsReady()) {
      base::RunLoop run_loop;
      ready_callback_ = run_loop.QuitClosure();
      run_loop.Run();
      CHECK(IsReady());
    }
    return std::move(ready_web_contents_);
  }

 private:
  class LoadObserver : public content::WebContentsObserver {
   public:
    LoadObserver(content::WebContents& wc, WebContentsBarrier& owner)
        : WebContentsObserver(&wc), owner_(owner) {}

   private:
    void DidFinishLoad(content::RenderFrameHost* host,
                       const GURL& url) override {
      if (host != web_contents()->GetPrimaryMainFrame()) {
        return;
      }
      owner_->OnWebContentsLoaded(web_contents(), url);
    }
    const raw_ref<WebContentsBarrier> owner_;
  };

  bool IsReady() const { return !pending_contents_count_; }

  void OnWebContentsCreated(content::WebContents* wc) {
    observers_.push_back(std::make_unique<LoadObserver>(*wc, *this));
  }

  void OnWebContentsLoaded(content::WebContents* wc, const GURL& url) {
    CHECK(!IsReady());
    for (size_t i = 0; i < predicates_.size(); ++i) {
      if (!predicates_[i](url)) {
        continue;
      }
      CHECK(!ready_web_contents_[i])
          << " predicate #" << i << " matches "
          << ready_web_contents_[i]->GetLastCommittedURL() << " and " << url;
      ready_web_contents_[i] = wc;
      --pending_contents_count_;
    }
    if (IsReady() && ready_callback_) {
      std::move(ready_callback_).Run();
    }
  }

  const std::vector<Predicate> predicates_;

  std::vector<raw_ptr<content::WebContents, VectorExperimental>>
      ready_web_contents_{predicates_.size(), nullptr};
  size_t pending_contents_count_{predicates_.size()};
  base::CallbackListSubscription creation_subscription_{
      content::RegisterWebContentsCreationCallback(
          base::BindRepeating(&WebContentsBarrier::OnWebContentsCreated,
                              base::Unretained(this)))};
  std::vector<std::unique_ptr<LoadObserver>> observers_;
  base::OnceClosure ready_callback_;
};

IN_PROC_BROWSER_TEST_F(ExtensionProtocolTest, TabTargetWithGuestView) {
  base::FilePath extension_path =
      base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
          .AppendASCII("devtools")
          .AppendASCII("extensions")
          .AppendASCII("app_with_webview");
  auto* extension = LoadExtensionOrApp(extension_path);
  ASSERT_THAT(extension, testing::NotNull());

  WebContentsBarrier barrier(
      {[](const GURL& url) -> bool {
         return base::EndsWith(url.path(), "host.html");
       },
       [](const GURL& url) -> bool { return url.SchemeIs(url::kDataScheme); }});

  LaunchApp(extension->id());

  std::vector<raw_ptr<content::WebContents, VectorExperimental>> wcs =
      barrier.Await();
  ASSERT_THAT(wcs, testing::SizeIs(2));
  EXPECT_NE(wcs[0], wcs[1]);
  // Assure host and view have different DevTools hosts.
  EXPECT_NE(content::DevToolsAgentHost::GetOrCreateForTab(wcs[0]),
            content::DevToolsAgentHost::GetOrCreateForTab(wcs[1]));
  // Assure host does not auto-attach view.
  AttachToTabTarget(wcs[0]);
  base::Value::Dict command_params;
  command_params = base::Value::Dict();
  command_params.Set("autoAttach", true);
  command_params.Set("waitForDebuggerOnStart", false);
  command_params.Set("flatten", true);
  SendCommandSync("Target.setAutoAttach", std::move(command_params));
  EXPECT_FALSE(HasExistingNotificationMatching(
      [](const base::Value::Dict& notification) {
        if (*notification.FindString("method") != "Target.attachedToTarget") {
          return false;
        }
        const std::string* url =
            notification.FindStringByDottedPath("params.targetInfo.url");
        return url && base::StartsWith(*url, "data:");
      }));
}

class PrerenderDataSaverProtocolTest : public DevToolsProtocolTest {
 public:
  PrerenderDataSaverProtocolTest()
      : prerender_helper_(base::BindRepeating(
            &PrerenderDataSaverProtocolTest::GetActiveWebContents,
            base::Unretained(this))) {}

 protected:
  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    data_saver::OverrideIsDataSaverEnabledForTesting(true);
    DevToolsProtocolTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

  void TearDown() override {
    data_saver::ResetIsDataSaverEnabledForTesting();
    InProcessBrowserTest::TearDown();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_F(PrerenderDataSaverProtocolTest,
                       CheckReportedDisabledByDataSaverPreloadingState) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(embedded_test_server()->Start());

  Attach();
  SendCommandSync("Runtime.enable");

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  const GURL prerendering_url =
      embedded_test_server()->GetURL("/empty.html?prerender");

  content::test::PrerenderHostRegistryObserver observer(
      *GetActiveWebContents());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  prerender_helper()->AddPrerenderAsync(prerendering_url);
  observer.WaitForTrigger(prerendering_url);

  content::FrameTreeNodeId host_id =
      prerender_helper()->GetHostForUrl(prerendering_url);
  EXPECT_TRUE(host_id.is_null());

  SendCommandAsync("Preload.enable");
  const base::Value::Dict result =
      WaitForNotification("Preload.preloadEnabledStateUpdated", true);

  EXPECT_THAT(result.FindBool("disabledByDataSaver"), true);
  histogram_tester.ExpectUniqueSample(
      "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
      /*PrerenderFinalStatus::kDataSaverEnabled=*/38, 1);
}

class PrivacySandboxAttestationsOverrideTest : public DevToolsProtocolTest {
 public:
  PrivacySandboxAttestationsOverrideTest() = default;

 private:
  privacy_sandbox::PrivacySandboxAttestationsMixin
      privacy_sandbox_attestations_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsOverrideTest,
                       PrivacySandboxEnrollmentOverride) {
  Attach();

  base::Value::Dict paramsDIPS;
  const std::string attestation_url = "https://google.com";
  paramsDIPS.Set("url", attestation_url);

  SendCommand("Browser.addPrivacySandboxEnrollmentOverride",
              std::move(paramsDIPS));

  EXPECT_TRUE(
      privacy_sandbox::PrivacySandboxAttestations::GetInstance()->IsOverridden(
          net::SchemefulSite(GURL(attestation_url))));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAttestationsOverrideTest,
                       PrivacySandboxEnrollmentOverrideInvalidUrl) {
  Attach();

  base::Value::Dict paramsDIPS;
  const std::string attestation_url = "this is a bad url";
  paramsDIPS.Set("url", attestation_url);

  SendCommand("Browser.addPrivacySandboxEnrollmentOverride",
              std::move(paramsDIPS));

  EXPECT_TRUE(error());
  EXPECT_FALSE(
      privacy_sandbox::PrivacySandboxAttestations::GetInstance()->IsOverridden(
          net::SchemefulSite(GURL(attestation_url))));
}

class DevToolsProtocolTest_RelatedWebsiteSets : public DevToolsProtocolTest {
 protected:
  const char* kPrimarySite = "https://a.test";
  const char* kAssociatedSite = "https://b.test";
  const char* kServiceSite = "https://c.test";
  const char* kPrimaryCcTLD = "https://a.cctld";

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StringPrintf(R"({"primary": "%s",)"
                           R"("associatedSites": ["%s"],)"
                           R"("serviceSites": ["%s"],)"
                           R"("ccTLDs": {"%s": ["%s"]}})",
                           kPrimarySite, kAssociatedSite, kServiceSite,
                           kPrimarySite, kPrimaryCcTLD));
  }
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_RelatedWebsiteSets,
                       GetRelatedWebsiteSets) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  SendCommandSync("Storage.getRelatedWebsiteSets");

  if (result()) {
    const base::Value::List* set_list = result()->FindList("sets");
    ASSERT_TRUE(set_list);

    base::Value::List expected =
        base::Value::List()  //
            .Append(base::Value::Dict()
                        .Set("associatedSites",
                             base::Value::List().Append(kAssociatedSite))
                        .Set("primarySites", base::Value::List()
                                                 .Append(kPrimaryCcTLD)
                                                 .Append(kPrimarySite))
                        .Set("serviceSites",
                             base::Value::List().Append(kServiceSite)));

    EXPECT_EQ(*set_list, expected);
  } else if (error()) {
    EXPECT_EQ(*error()->FindString("message"),
              "Failed fetching RelatedWebsiteSets");
  }
}

}  // namespace
