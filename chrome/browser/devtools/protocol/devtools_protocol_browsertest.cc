// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/test_switches.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/custom_handlers/protocol_handler_registry.h"
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
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
  {
    base::Value::Dict params;
    params.Set("enabled", false);
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
    base::Value::Dict params;
    params.Set("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
  {
    base::Value::Dict params;
    params.Set("enabled", true);
    SendCommandSync("Emulation.setAutomationOverride", std::move(params));
  }
  EXPECT_EQ(static_cast<int>(manager->infobar_count()), 1);
}

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest, UntrustedClient) {
  std::unique_ptr<base::Value::Dict> params(new base::Value::Dict());
  SetIsTrusted(false);
  Attach();
  EXPECT_FALSE(SendCommandSync("HeapProfiler.enable"));  // Implemented in V8
  EXPECT_FALSE(SendCommandSync("LayerTree.enable"));     // Implemented in blink
  EXPECT_FALSE(SendCommandSync(
      "Memory.prepareForLeakDetection"));        // Implemented in content
  EXPECT_FALSE(SendCommandSync("Cast.enable"));  // Implemented in content
  EXPECT_FALSE(SendCommandSync("Storage.getCookies"));
  EXPECT_TRUE(SendCommandSync("Accessibility.enable"));
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
    ExtensionTestMessageListener activated_listener("WORKER_ACTIVATED");
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
    if (*target.FindStringKey("type") == "service_worker") {
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
    params.Set("targetId", *targetInfo->FindStringKey("targetId"));
    params.Set("waitForDebuggerOnStart", false);
    SendCommandSync("Target.autoAttachRelated", std::move(params));
  }
  auto detached = WaitForNotification("Target.detachedFromTarget", true);
  EXPECT_THAT(*detached.FindString("sessionId"), Eq("sessionId"));
}

}  // namespace
