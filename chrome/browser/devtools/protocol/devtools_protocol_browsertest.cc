// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/test_switches.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "url/origin.h"

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
  SendCommand("Security.enable");
  base::Value params =
      WaitForNotification("Security.visibleSecurityStateChanged");

  std::string* security_state =
      params.FindStringPath("visibleSecurityState.securityState");
  ASSERT_TRUE(security_state);
  ASSERT_EQ(std::string("neutral"), *security_state);
  ASSERT_FALSE(
      params.FindPath("visibleSecurityState.certificateSecurityState"));
  ASSERT_FALSE(params.FindPath("visibleSecurityState.safetyTipInfo"));
  const base::Value* security_state_issue_ids =
      params.FindListPath("visibleSecurityState.securityStateIssueIds");
  ASSERT_TRUE(std::find(security_state_issue_ids->GetListDeprecated().begin(),
                        security_state_issue_ids->GetListDeprecated().end(),
                        base::Value("scheme-is-not-cryptographic")) !=
              security_state_issue_ids->GetListDeprecated().end());
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, CreateDeleteContext) {
  AttachToBrowser();
  for (int i = 0; i < 2; i++) {
    SendCommandSync("Target.createBrowserContext");
    std::string* context_id_value = result_.FindStringPath("browserContextId");
    ASSERT_TRUE(context_id_value);
    std::string context_id = *context_id_value;

    base::DictionaryValue params;
    params.SetStringPath("url", "about:blank");
    params.SetStringPath("browserContextId", context_id);
    SendCommandSync("Target.createTarget", std::move(params));

    params = base::DictionaryValue();
    params.SetStringPath("browserContextId", context_id);
    SendCommandSync("Target.disposeBrowserContext", std::move(params));
  }
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NewTabPageInCreatedContextDoesNotCrash) {
  AttachToBrowser();
  SendCommandSync("Target.createBrowserContext");
  std::string* context_id_value = result_.FindStringPath("browserContextId");
  ASSERT_TRUE(context_id_value);
  std::string context_id = *context_id_value;

  base::DictionaryValue params;
  params.SetStringPath("url", chrome::kChromeUINewTabURL);
  params.SetStringPath("browserContextId", context_id);
  content::WebContentsAddedObserver observer;
  SendCommandSync("Target.createTarget", std::move(params));
  content::WebContents* wc = observer.GetWebContents();
  ASSERT_TRUE(content::WaitForLoadStop(wc));
  EXPECT_EQ(chrome::kChromeUINewTabURL, wc->GetLastCommittedURL().spec());

  // Should not crash by this point.
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

  base::DictionaryValue params;
  params.SetStringKey("button", "left");
  params.SetIntKey("clickCount", 1);
  params.SetIntKey("x", 100);
  params.SetIntKey("y", 250);
  params.SetIntKey("clickCount", 1);

  params.SetStringKey("type", "mousePressed");
  SendCommandSync("Input.dispatchMouseEvent", params.Clone());

  params.SetStringKey("type", "mouseMoved");
  params.SetIntKey("y", 270);
  SendCommandSync("Input.dispatchMouseEvent", params.Clone());

  params.SetStringKey("type", "mouseReleased");
  SendCommandSync("Input.dispatchMouseEvent", std::move(params));

  params = base::DictionaryValue();
  params.SetIntKey("x", 100);
  params.SetIntKey("y", 250);
  params.SetStringPath("type", "dragEnter");
  params.SetIntPath("data.dragOperationsMask", 1);
  params.SetPath("data.items", base::ListValue());
  SendCommandSync("Input.dispatchDragEvent", std::move(params));

  params = base::DictionaryValue();
  params.SetIntKey("x", 100);
  params.SetIntKey("y", 250);
  SendCommandSync("Input.synthesizeTapGesture", std::move(params));

  params = base::DictionaryValue();
  params.SetStringKey("type", "keyDown");
  params.SetStringKey("key", "a");
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
  may_send_input_event_to_browser_ = false;
  Attach();

  base::DictionaryValue params;
  params.SetStringKey("type", "rawKeyDown");
  params.SetStringKey("key", "F12");
  params.SetIntKey("windowsVirtualKeyCode", 123);
  params.SetIntKey("nativeVirtualKeyCode", 123);
  SendCommandSync("Input.dispatchKeyEvent", std::move(params));

  EXPECT_EQ(nullptr, DevToolsWindow::FindDevToolsWindow(agent_host_.get()));
}

IN_PROC_BROWSER_TEST_F(
    DevToolsProtocolTest,
    NoPendingUrlShownWhenAttachedToBrowserInitiatedFailedNavigation) {
  GURL url("invalid.scheme:for-sure");
  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;

  content::WebContents* web_contents =
      browser()->OpenURL(content::OpenURLParams(
          url, content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui::PAGE_TRANSITION_TYPED, false));
  tab_added_waiter.Wait();
  // WaitForLoadStop() checks for the existence of the last committed
  // NavigationEntry, which will only be there if we have initial
  // NavigationEntries.
  ASSERT_EQ(WaitForLoadStop(web_contents),
            blink::features::IsInitialNavigationEntryEnabled());
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
  if (blink::features::IsInitialNavigationEntryEnabled()) {
    EXPECT_EQ(GURL(""), navigation_controller.GetVisibleEntry()->GetURL());
  } else {
    EXPECT_EQ(nullptr, navigation_controller.GetVisibleEntry());
  }
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       NoPendingUrlShownForPageNavigateFromChromeExtension) {
  GURL url("https://example.com");
  // DevTools protocol use cases that have an initiator origin (e.g., for
  // extensions) should use renderer-initiated navigations and be subject to URL
  // spoof defenses.
  navigation_initiator_origin_ =
      url::Origin::Create(GURL("chrome-extension://abc123/"));

  // Attach DevTools and start a navigation but don't wait for it to finish.
  Attach();
  SendCommandSync("Page.enable");
  base::DictionaryValue params;
  params.SetStringKey("url", url.spec());
  SendCommand("Page.navigate", std::move(params), false);
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

class DevToolsProtocolTest_AppId : public DevToolsProtocolTest {
 public:
  DevToolsProtocolTest_AppId() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kWebAppEnableManifestId);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId, ReturnsManifestAppId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_with_id.json"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  SendCommandSync("Page.getAppId");
  EXPECT_EQ(*result_.FindStringPath("appId"),
            embedded_test_server()->GetURL("/some_id"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId,
                       ReturnsStartUrlAsManifestAppIdIfNotSet) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  SendCommandSync("Page.getAppId");
  EXPECT_EQ(*result_.FindStringPath("appId"),
            embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  EXPECT_EQ(*result_.FindStringPath("recommendedId"),
            "/web_apps/no_service_worker.html");
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest_AppId, ReturnsNoAppIdIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(embedded_test_server()->GetURL("/empty.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  Attach();

  SendCommandSync("Page.getAppId");
  ASSERT_TRUE(result_.FindPath("appId") == nullptr);
  ASSERT_TRUE(result_.FindPath("recommendedId") == nullptr);
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
    page_valid_from = entry->GetSSL().certificate->valid_start().ToDoubleT();
    page_valid_to = entry->GetSSL().certificate->valid_expiry().ToDoubleT();
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
  SendCommand("Security.enable");
  auto has_certificate = [](const base::Value& params) {
    return params.FindListPath(
               "visibleSecurityState.certificateSecurityState.certificate") !=
           nullptr;
  };
  base::Value params =
      WaitForMatchingNotification("Security.visibleSecurityStateChanged",
                                  base::BindRepeating(has_certificate));

  // Verify that the visibleSecurityState payload matches the SSL status data.
  std::string* security_state =
      params.FindStringPath("visibleSecurityState.securityState");
  ASSERT_TRUE(security_state);
  ASSERT_EQ(std::string("secure"), *security_state);

  base::Value* certificate_security_state =
      params.FindPath("visibleSecurityState.certificateSecurityState");
  ASSERT_TRUE(certificate_security_state);

  std::string* protocol =
      certificate_security_state->FindStringPath("protocol");
  ASSERT_TRUE(protocol);
  ASSERT_EQ(*protocol, page_protocol);

  std::string* key_exchange =
      certificate_security_state->FindStringPath("keyExchange");
  ASSERT_TRUE(key_exchange);
  ASSERT_EQ(*key_exchange, page_key_exchange);

  std::string* key_exchange_group =
      certificate_security_state->FindStringPath("keyExchangeGroup");
  if (key_exchange_group) {
    ASSERT_EQ(*key_exchange_group, page_key_exchange_group);
  }

  std::string* mac = certificate_security_state->FindStringPath("mac");
  if (mac) {
    ASSERT_EQ(*mac, page_mac);
  }

  std::string* cipher = certificate_security_state->FindStringPath("cipher");
  ASSERT_TRUE(cipher);
  ASSERT_EQ(*cipher, page_cipher);

  std::string* subject_name =
      certificate_security_state->FindStringPath("subjectName");
  ASSERT_TRUE(subject_name);
  ASSERT_EQ(*subject_name, page_subject_name);

  std::string* issuer = certificate_security_state->FindStringPath("issuer");
  ASSERT_TRUE(issuer);
  ASSERT_EQ(*issuer, page_issuer_name);

  auto valid_from = certificate_security_state->FindDoublePath("validFrom");
  ASSERT_TRUE(valid_from);
  ASSERT_EQ(*valid_from, page_valid_from);

  auto valid_to = certificate_security_state->FindDoublePath("validTo");
  ASSERT_TRUE(valid_to);
  ASSERT_EQ(*valid_to, page_valid_to);

  std::string* certificate_network_error =
      certificate_security_state->FindStringPath("certificateNetworkError");
  if (certificate_network_error) {
    ASSERT_EQ(*certificate_network_error, page_certificate_network_error);
  }

  auto certificate_has_weak_signature =
      certificate_security_state->FindBoolPath("certificateHasWeakSignature");
  ASSERT_TRUE(certificate_has_weak_signature);
  ASSERT_EQ(*certificate_has_weak_signature,
            page_certificate_has_weak_signature);

  auto certificate_has_sha1_signature_present =
      certificate_security_state->FindBoolPath("certificateHasSha1Signature");
  ASSERT_TRUE(certificate_has_sha1_signature_present);
  ASSERT_EQ(*certificate_has_sha1_signature_present,
            page_certificate_has_sha1_signature_present);

  auto modern_ssl = certificate_security_state->FindBoolPath("modernSSL");
  ASSERT_TRUE(modern_ssl);
  ASSERT_EQ(*modern_ssl, page_modern_ssl);

  auto obsolete_ssl_protocol =
      certificate_security_state->FindBoolPath("obsoleteSslProtocol");
  ASSERT_TRUE(obsolete_ssl_protocol);
  ASSERT_EQ(*obsolete_ssl_protocol, page_obsolete_ssl_protocol);

  auto obsolete_ssl_key_exchange =
      certificate_security_state->FindBoolPath("obsoleteSslKeyExchange");
  ASSERT_TRUE(obsolete_ssl_key_exchange);
  ASSERT_EQ(*obsolete_ssl_key_exchange, page_obsolete_ssl_key_exchange);

  auto obsolete_ssl_cipher =
      certificate_security_state->FindBoolPath("obsoleteSslCipher");
  ASSERT_TRUE(obsolete_ssl_cipher);
  ASSERT_EQ(*obsolete_ssl_cipher, page_obsolete_ssl_cipher);

  auto obsolete_ssl_signature =
      certificate_security_state->FindBoolPath("obsoleteSslSignature");
  ASSERT_TRUE(obsolete_ssl_signature);
  ASSERT_EQ(*obsolete_ssl_signature, page_obsolete_ssl_signature);

  const base::Value* certificate_value =
      certificate_security_state->FindListPath("certificate");
  std::vector<std::string> der_certs;
  for (const auto& cert : certificate_value->GetListDeprecated()) {
    std::string decoded;
    ASSERT_TRUE(base::Base64Decode(cert.GetString(), &decoded));
    der_certs.push_back(decoded);
  }
  std::vector<base::StringPiece> cert_string_piece;
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
      params.FindListPath("visibleSecurityState.securityStateIssueIds");
  EXPECT_EQ(security_state_issue_ids->GetListDeprecated().size(), 0u);

  ASSERT_FALSE(params.FindPath("visibleSecurityState.safetyTipInfo"));
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       AutomationOverrideShowsAndRemovesInfoBar) {
  Attach();
  auto* manager = infobars::ContentInfoBarManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetBoolKey("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetBoolKey("enabled", false);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 0);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       AutomationOverrideAddsOneInfoBarOnly) {
  Attach();
  auto* manager = infobars::ContentInfoBarManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetBoolKey("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetBoolKey("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
}

class NetworkResponseProtocolTest : public DevToolsProtocolTest {
 protected:
  base::Value FetchAndWaitForResponse(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string script =
        content::JsReplace("fetch($1).then(r => r.status)", url.spec());
    content::EvalJsResult status = content::EvalJs(web_contents, script);
    EXPECT_EQ(200, status);
    if (!(200 == status)) {
      return base::Value();
    }

    // Look for the requestId.
    auto matches_url = [](const GURL& url, const base::Value& params) {
      const std::string* got_url = params.FindStringPath("request.url");
      return got_url && *got_url == url.spec();
    };
    base::Value request = WaitForMatchingNotification(
        "Network.requestWillBeSent", base::BindRepeating(matches_url, url));
    const std::string* request_id = request.FindStringPath("requestId");
    if (!request_id) {
      ADD_FAILURE() << "Could not find request ID";
      return base::Value();
    }

    // Look for the response.
    auto matches_id = [](const std::string& request_id,
                         const base::Value& params) {
      const std::string* id = params.FindStringPath("requestId");
      return id && *id == request_id;
    };
    return WaitForMatchingNotification(
        "Network.responseReceived",
        base::BindRepeating(matches_id, *request_id));
  }
};

// Test that the SecurityDetails field of the resource response matches the
// server.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetails) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
  server_config.cipher_suite_for_testing = 0xc02f;
  server_config.curves_for_testing = {NID_X25519};
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(server.Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), server.GetURL("/title1.html"), 1);

  Attach();
  SendCommand("Network.enable");

  base::Value response = FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* protocol =
      response.FindStringPath("response.securityDetails.protocol");
  ASSERT_TRUE(protocol);
  EXPECT_EQ("TLS 1.2", *protocol);

  const std::string* key_exchange =
      response.FindStringPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("ECDHE_RSA", *key_exchange);

  const std::string* cipher =
      response.FindStringPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  EXPECT_EQ("AES_128_GCM", *cipher);

  // AEAD ciphers should not report a MAC.
  EXPECT_FALSE(response.FindStringPath("response.securityDetails.mac"));

  const std::string* group =
      response.FindStringPath("response.securityDetails.keyExchangeGroup");
  ASSERT_TRUE(group);
  EXPECT_EQ("X25519", *group);

  const std::string* subject =
      response.FindStringPath("response.securityDetails.subjectName");
  ASSERT_TRUE(subject);
  EXPECT_EQ(server.GetCertificate()->subject().common_name, *subject);

  const std::string* issuer =
      response.FindStringPath("response.securityDetails.issuer");
  ASSERT_TRUE(issuer);
  EXPECT_EQ(server.GetCertificate()->issuer().common_name, *issuer);

  // The default certificate has a single SAN, 127.0.0.1.
  const base::Value* sans =
      response.FindListPath("response.securityDetails.sanList");
  ASSERT_TRUE(sans);
  ASSERT_EQ(1u, sans->GetListDeprecated().size());
  EXPECT_EQ(base::Value("127.0.0.1"), sans->GetListDeprecated()[0]);

  absl::optional<double> valid_from =
      response.FindDoublePath("response.securityDetails.validFrom");
  EXPECT_EQ(server.GetCertificate()->valid_start().ToDoubleT(), valid_from);

  absl::optional<double> valid_to =
      response.FindDoublePath("response.securityDetails.validTo");
  EXPECT_EQ(server.GetCertificate()->valid_expiry().ToDoubleT(), valid_to);
}

// Test SecurityDetails, but with a TLS 1.3 cipher suite, which should not
// report a key exchange component.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetailsTLS13) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_3;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_3;
  server_config.curves_for_testing = {NID_X25519};
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(server.Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), server.GetURL("/title1.html"), 1);

  Attach();
  SendCommand("Network.enable");

  base::Value response = FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* protocol =
      response.FindStringPath("response.securityDetails.protocol");
  ASSERT_TRUE(protocol);
  EXPECT_EQ("TLS 1.3", *protocol);

  const std::string* key_exchange =
      response.FindStringPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("", *key_exchange);

  const std::string* cipher =
      response.FindStringPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  // Depending on whether the host machine has AES hardware, the server may
  // pick AES-GCM or ChaCha20-Poly1305.
  EXPECT_TRUE(*cipher == "AES_128_GCM" || *cipher == "CHACHA20_POLY1305");

  // AEAD ciphers should not report a MAC.
  EXPECT_FALSE(response.FindStringPath("response.securityDetails.mac"));

  const std::string* group =
      response.FindStringPath("response.securityDetails.keyExchangeGroup");
  ASSERT_TRUE(group);
  EXPECT_EQ("X25519", *group);
}

// Test SecurityDetails, but with a legacy cipher suite, which should report a
// separate MAC component and no group.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest,
                       SecurityDetailsLegacyCipher) {
  // Configure a specific TLS configuration to compare against.
  net::SSLServerConfig server_config;
  server_config.version_min = net::SSL_PROTOCOL_VERSION_TLS1_2;
  server_config.version_max = net::SSL_PROTOCOL_VERSION_TLS1_2;
  // TLS_RSA_WITH_AES_128_CBC_SHA
  server_config.cipher_suite_for_testing = 0x002f;
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(net::EmbeddedTestServer::ServerCertificate::CERT_OK,
                      server_config);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(server.Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), server.GetURL("/title1.html"), 1);

  Attach();
  SendCommand("Network.enable");

  base::Value response = FetchAndWaitForResponse(server.GetURL("/empty.html"));

  const std::string* key_exchange =
      response.FindStringPath("response.securityDetails.keyExchange");
  ASSERT_TRUE(key_exchange);
  EXPECT_EQ("RSA", *key_exchange);

  const std::string* cipher =
      response.FindStringPath("response.securityDetails.cipher");
  ASSERT_TRUE(cipher);
  EXPECT_EQ("AES_128_CBC", *cipher);

  const std::string* mac =
      response.FindStringPath("response.securityDetails.mac");
  ASSERT_TRUE(mac);
  EXPECT_EQ("HMAC-SHA1", *mac);

  // RSA ciphers should not report a MAC.
  EXPECT_FALSE(
      response.FindStringPath("response.securityDetails.keyExchangeGroup"));
}

// Test that complex certificate SAN lists are reported in SecurityDetails.
IN_PROC_BROWSER_TEST_F(NetworkResponseProtocolTest, SecurityDetailsSAN) {
  net::EmbeddedTestServer::ServerCertificateConfig cert_config;
  cert_config.dns_names = {"a.example", "b.example", "*.c.example"};
  cert_config.ip_addresses = {net::IPAddress::IPv4Localhost(),
                              net::IPAddress::IPv6Localhost(),
                              net::IPAddress(1, 2, 3, 4)};
  net::EmbeddedTestServer server(net::EmbeddedTestServer::TYPE_HTTPS);
  server.SetSSLConfig(cert_config);
  server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(server.Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), server.GetURL("/title1.html"), 1);

  Attach();
  SendCommand("Network.enable");

  base::Value response = FetchAndWaitForResponse(server.GetURL("/empty.html"));
  const base::Value* sans =
      response.FindListPath("response.securityDetails.sanList");
  ASSERT_TRUE(sans);
  ASSERT_EQ(6u, sans->GetListDeprecated().size());
  EXPECT_EQ(base::Value("a.example"), sans->GetListDeprecated()[0]);
  EXPECT_EQ(base::Value("b.example"), sans->GetListDeprecated()[1]);
  EXPECT_EQ(base::Value("*.c.example"), sans->GetListDeprecated()[2]);
  EXPECT_EQ(base::Value("127.0.0.1"), sans->GetListDeprecated()[3]);
  EXPECT_EQ(base::Value("::1"), sans->GetListDeprecated()[4]);
  EXPECT_EQ(base::Value("1.2.3.4"), sans->GetListDeprecated()[5]);
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

  const extensions::Extension* LoadExtension(base::FilePath extension_path) {
    extensions::TestExtensionRegistryObserver observer(extension_registry_);
    ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED", false);
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

  void ReloadExtension(const std::string extension_id) {
    extensions::TestExtensionRegistryObserver observer(extension_registry_);
    extension_service_->ReloadExtension(extension_id);
    observer.WaitForExtensionLoaded();
  }

 private:
  extensions::ExtensionService* extension_service_;
  extensions::ExtensionRegistry* extension_registry_;
  content::WebContents* background_web_contents_;
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
  base::DictionaryValue params;
  params.SetStringPath("categories", "-*");
  SendCommandSync("Tracing.start", std::move(params));
  SendCommand("Tracing.end");
  base::Value tracing_complete = WaitForNotification("Tracing.tracingComplete");
}

IN_PROC_BROWSER_TEST_F(ExtensionProtocolTest, ReloadServiceWorkerExtension) {
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
  AttachToBrowser();
  SendCommandSync("Target.getTargets");

  std::string target_id;
  base::Value ext_target;
  for (auto& target : result_.FindListKey("targetInfos")->GetListDeprecated()) {
    if (*target.FindStringKey("type") == "service_worker") {
      ext_target = target.Clone();
      break;
    }
  }
  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetStringKey("targetId", *ext_target.FindStringKey("targetId"));
    params.SetBoolKey("waitForDebuggerOnStart", false);
    SendCommandSync("Target.autoAttachRelated", std::move(params));
  }
  ReloadExtension(extension_id);
  auto attached = WaitForNotification("Target.attachedToTarget");
  base::Value* targetInfo = attached.FindDictKey("targetInfo");
  ASSERT_THAT(targetInfo, testing::NotNull());
  EXPECT_THAT(*targetInfo, base::test::DictionaryHasValue(
                               "type", base::Value("service_worker")));
  EXPECT_THAT(*targetInfo, base::test::DictionaryHasValue(
                               "url", *ext_target.FindKey("url")));
  EXPECT_THAT(attached, base::test::DictionaryHasValue("waitingForDebugger",
                                                       base::Value(false)));

  {
    base::Value params(base::Value::Type::DICTIONARY);
    params.SetStringKey("targetId", *targetInfo->FindStringKey("targetId"));
    params.SetBoolKey("waitForDebuggerOnStart", false);
    SendCommandSync("Target.autoAttachRelated", std::move(params));
  }
  auto detached = WaitForNotification("Target.detachedFromTarget");
  EXPECT_THAT(detached, base::test::DictionaryHasValue(
                            "sessionId",
                            base::Value(*attached.FindStringKey("sessionId"))));
}

}  // namespace
