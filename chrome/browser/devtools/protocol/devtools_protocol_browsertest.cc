// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace {

const char kIdParam[] = "id";
const char kMethodParam[] = "method";

}  // namespace

class DevToolsProtocolTest : public InProcessBrowserTest,
                             public content::DevToolsAgentHostClient {
 public:
  DevToolsProtocolTest() : last_sent_id_(0) {}

 protected:
  typedef base::RepeatingCallback<bool(const base::Value&)> NotificationMatcher;

  // InProcessBrowserTest  interface
  void TearDownOnMainThread() override { Detach(); }

  // DevToolsAgentHostClient interface
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    auto parsed_message = base::JSONReader::Read(message);
    auto id = parsed_message->FindIntPath("id");
    if (id) {
      // TODO: implement handling of results from method calls (when needed).
    } else {
      std::string* notification = parsed_message->FindStringPath("method");
      EXPECT_TRUE(notification);
      notifications_.push_back(*notification);
      base::Value* params = parsed_message->FindPath("params");
      notification_params_.push_back(params ? params->Clone() : base::Value());
      if (waiting_for_notification_ == *notification &&
          (waiting_for_notification_matcher_.is_null() ||
           waiting_for_notification_matcher_.Run(
               notification_params_.back()))) {
        waiting_for_notification_ = std::string();
        waiting_for_notification_matcher_ = NotificationMatcher();
        waiting_for_notification_params_ = notification_params_.back().Clone();
        std::move(run_loop_quit_closure_).Run();
      }
    }
  }

  void SendCommand(const std::string& method) {
    base::Value command(base::Value::Type::DICTIONARY);
    command.SetKey(kIdParam, base::Value(++last_sent_id_));
    command.SetKey(kMethodParam, base::Value(method));
    std::string json_command;
    base::JSONWriter::Write(command, &json_command);
    agent_host_->DispatchProtocolMessage(this, json_command);
  }

  void RunLoopUpdatingQuitClosure() {
    base::RunLoop run_loop;
    CHECK(!run_loop_quit_closure_);
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void Attach() {
    agent_host_ = content::DevToolsAgentHost::GetOrCreateFor(web_contents());
    agent_host_->AttachClient(this);
  }

  void Detach() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  base::Value WaitForNotification(const std::string& notification) {
    auto always_match = base::Bind([](const base::Value&) { return true; });
    return WaitForMatchingNotification(notification, always_match);
  }

  base::Value WaitForMatchingNotification(const std::string& notification,
                                          const NotificationMatcher& matcher) {
    for (size_t i = 0; i < notifications_.size(); ++i) {
      if (notifications_[i] == notification &&
          matcher.Run(notification_params_[i])) {
        base::Value result = std::move(notification_params_[i]);
        notifications_.erase(notifications_.begin() + i);
        notification_params_.erase(notification_params_.begin() + i);
        return result;
      }
    }
    waiting_for_notification_ = notification;
    waiting_for_notification_matcher_ = matcher;
    RunLoopUpdatingQuitClosure();
    return std::move(waiting_for_notification_params_);
  }

 private:
  // DevToolsAgentHostClient interface
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}

  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  int last_sent_id_;
  base::OnceClosure run_loop_quit_closure_;
  std::vector<std::string> notifications_;
  std::vector<base::Value> notification_params_;
  std::string waiting_for_notification_;
  NotificationMatcher waiting_for_notification_matcher_;
  base::Value waiting_for_notification_params_;
};

IN_PROC_BROWSER_TEST_F(DevToolsProtocolTest,
                       VisibleSecurityStateChangedNeutralState) {
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  content::WaitForLoadStop(web_contents());

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
  const base::Value* security_state_issue_ids =
      params.FindListPath("visibleSecurityState.securityStateIssueIds");
  ASSERT_TRUE(std::find(security_state_issue_ids->GetList().begin(),
                        security_state_issue_ids->GetList().end(),
                        base::Value("scheme-is-not-cryptographic")) !=
              security_state_issue_ids->GetList().end());
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

  bool page_certificate_has_weak_signature =
      (entry->GetSSL().cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);

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
  base::Value params = WaitForMatchingNotification(
      "Security.visibleSecurityStateChanged", base::Bind(has_certificate));

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

  auto certificate_has_weak_signature =
      certificate_security_state->FindBoolPath("certifcateHasWeakSignature");
  ASSERT_TRUE(certificate_has_weak_signature);
  ASSERT_EQ(*certificate_has_weak_signature,
            page_certificate_has_weak_signature);

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
  for (const auto& cert : certificate_value->GetList()) {
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
  EXPECT_EQ(security_state_issue_ids->GetList().size(), 0u);
}
