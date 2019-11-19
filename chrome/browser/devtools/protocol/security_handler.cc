// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/security_handler.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/content/content_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace {

const char kInsecureOriginSecurityStateIssueId[] = "insecure-origin";
const char kSchemeIsNotCryptographicSecurityStateIssueId[] =
    "scheme-is-not-cryptographic";
const char kMalicousContentSecurityStateIssueId[] = "malicious-content";
const char kDisplayedMixedContentSecurityStateIssueId[] =
    "displayed-mixed-content";
const char kContainedMixedFormSecurityStateIssueId[] = "contained-mixed-form";
const char kRanMixedContentSecurityStateIssueId[] = "ran-mixed-content";
const char kDisplayedContentWithCertErrorsSecurityStateIssueId[] =
    "displayed-content-with-cert-errors";
const char kRanContentWithCertErrorSecurityStateIssueId[] =
    "ran-content-with-cert-error";
const char kPkpBypassedSecurityStateIssueId[] = "pkp-bypassed";
const char kIsErrorPageSecurityStateIssueId[] = "is-error-page";
const char kInsecureInputEventsSecurityStateIssueId[] = "insecure-input-events";

std::string SecurityLevelToProtocolSecurityState(
    security_state::SecurityLevel security_level,
    GURL url) {
  switch (security_level) {
    case security_state::NONE:
    case security_state::WARNING:
      if (security_state::ShouldDowngradeNeutralStyling(
              security_level, url,
              base::BindRepeating(&content::IsOriginSecure)))
        return protocol::Security::SecurityStateEnum::Insecure;
      return protocol::Security::SecurityStateEnum::Neutral;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return protocol::Security::SecurityStateEnum::Secure;
    case security_state::DANGEROUS:
      return protocol::Security::SecurityStateEnum::InsecureBroken;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return protocol::Security::SecurityStateEnum::Neutral;
  }

  NOTREACHED();
  return protocol::Security::SecurityStateEnum::Neutral;
}

std::unique_ptr<protocol::Security::CertificateSecurityState>
CreateCertificateSecurityState(
    const security_state::VisibleSecurityState& state) {
  auto certificate = std::make_unique<protocol::Array<protocol::String>>();
  if (state.certificate) {
    certificate->emplace_back();
    base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(
                           state.certificate->cert_buffer()),
                       &certificate->back());
    for (const auto& cert : state.certificate->intermediate_buffers()) {
      certificate->emplace_back();
      base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(cert.get()),
                         &certificate->back());
    }
  }

  int ssl_version = net::SSLConnectionStatusToVersion(state.connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);

  const char* key_exchange_str;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(state.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange_str, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  std::string key_exchange;
  if (key_exchange_str)
    key_exchange = key_exchange_str;

  const char* key_exchange_group = SSL_get_curve_name(state.key_exchange_group);

  std::string subject_name;
  std::string issuer_name;
  double valid_from = 0.0;
  double valid_to = 0.0;
  if (state.certificate) {
    subject_name = state.certificate->subject().common_name;
    issuer_name = state.certificate->issuer().common_name;
    valid_from = state.certificate->valid_start().ToDoubleT();
    valid_to = state.certificate->valid_expiry().ToDoubleT();
  }

  bool certificate_has_weak_signature =
      (state.cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);

  int status = net::ObsoleteSSLStatus(state.connection_status,
                                      state.peer_signature_algorithm);
  bool modern_ssl = status == net::OBSOLETE_SSL_NONE;
  bool obsolete_ssl_protocol = status & net::OBSOLETE_SSL_MASK_PROTOCOL;
  bool obsolete_ssl_key_exchange = status & net::OBSOLETE_SSL_MASK_KEY_EXCHANGE;
  bool obsolete_ssl_cipher = status & net::OBSOLETE_SSL_MASK_CIPHER;
  bool obsolete_ssl_signature = status & net::OBSOLETE_SSL_MASK_SIGNATURE;

  auto certificate_security_state =
      protocol::Security::CertificateSecurityState::Create()
          .SetProtocol(protocol)
          .SetKeyExchange(key_exchange)
          .SetCipher(cipher)
          .SetCertificate(std::move(certificate))
          .SetSubjectName(subject_name)
          .SetIssuer(issuer_name)
          .SetValidFrom(valid_from)
          .SetValidTo(valid_to)
          .SetCertifcateHasWeakSignature(certificate_has_weak_signature)
          .SetModernSSL(modern_ssl)
          .SetObsoleteSslProtocol(obsolete_ssl_protocol)
          .SetObsoleteSslKeyExchange(obsolete_ssl_key_exchange)
          .SetObsoleteSslCipher(obsolete_ssl_cipher)
          .SetObsoleteSslSignature(obsolete_ssl_signature)
          .Build();

  if (key_exchange_group)
    certificate_security_state->SetKeyExchangeGroup(key_exchange_group);
  if (mac)
    certificate_security_state->SetMac(mac);

  return certificate_security_state;
}

std::unique_ptr<protocol::Security::VisibleSecurityState>
CreateVisibleSecurityState(const security_state::VisibleSecurityState& state,
                           content::WebContents* web_contents) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  std::string security_state = SecurityLevelToProtocolSecurityState(
      helper->GetSecurityLevel(), state.url);

  bool scheme_is_cryptographic =
      security_state::IsSchemeCryptographic(state.url);
  bool malicious_content = state.malicious_content_status !=
                           security_state::MALICIOUS_CONTENT_STATUS_NONE;
  bool insecure_input_events =
      state.insecure_input_events.insecure_field_edited;

  bool secure_origin = scheme_is_cryptographic;
  if (!scheme_is_cryptographic)
    secure_origin = content::IsOriginSecure(state.url);

  std::vector<std::string> security_state_issue_ids;
  if (!secure_origin)
    security_state_issue_ids.push_back(kInsecureOriginSecurityStateIssueId);
  if (!scheme_is_cryptographic)
    security_state_issue_ids.push_back(
        kSchemeIsNotCryptographicSecurityStateIssueId);
  if (malicious_content)
    security_state_issue_ids.push_back(kMalicousContentSecurityStateIssueId);
  if (state.displayed_mixed_content)
    security_state_issue_ids.push_back(
        kDisplayedMixedContentSecurityStateIssueId);
  if (state.contained_mixed_form)
    security_state_issue_ids.push_back(kContainedMixedFormSecurityStateIssueId);
  if (state.ran_mixed_content)
    security_state_issue_ids.push_back(kRanMixedContentSecurityStateIssueId);
  if (state.displayed_content_with_cert_errors)
    security_state_issue_ids.push_back(
        kDisplayedContentWithCertErrorsSecurityStateIssueId);
  if (state.ran_content_with_cert_errors)
    security_state_issue_ids.push_back(
        kRanContentWithCertErrorSecurityStateIssueId);
  if (state.pkp_bypassed)
    security_state_issue_ids.push_back(kPkpBypassedSecurityStateIssueId);
  if (state.is_error_page)
    security_state_issue_ids.push_back(kIsErrorPageSecurityStateIssueId);
  if (insecure_input_events)
    security_state_issue_ids.push_back(
        kInsecureInputEventsSecurityStateIssueId);

  auto visible_security_state =
      protocol::Security::VisibleSecurityState::Create()
          .SetSecurityState(security_state)
          .SetSecurityStateIssueIds(
              std::make_unique<protocol::Array<std::string>>(
                  security_state_issue_ids))
          .Build();

  if (state.connection_status != 0) {
    auto certificate_security_state = CreateCertificateSecurityState(state);
    visible_security_state->SetCertificateSecurityState(
        std::move(certificate_security_state));
  }
  return visible_security_state;
}

}  // namespace

SecurityHandler::SecurityHandler(content::WebContents* web_contents,
                                 protocol::UberDispatcher* dispatcher)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
  frontend_ =
      std::make_unique<protocol::Security::Frontend>(dispatcher->channel());
  protocol::Security::Dispatcher::wire(dispatcher, this);
}

SecurityHandler::~SecurityHandler() {}

protocol::Response SecurityHandler::Enable() {
  if (enabled_)
    return protocol::Response::FallThrough();
  enabled_ = true;
  DidChangeVisibleSecurityState();
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

protocol::Response SecurityHandler::Disable() {
  enabled_ = false;
  // Do not mark the command as handled. Let it fall through instead, so that
  // the handler in content gets a chance to process the command.
  return protocol::Response::FallThrough();
}

void SecurityHandler::DidChangeVisibleSecurityState() {
  if (!enabled_)
    return;

  auto state = security_state::GetVisibleSecurityState(web_contents());
  auto visible_security_state =
      CreateVisibleSecurityState(*state.get(), web_contents());
  frontend_->VisibleSecurityStateChanged(std::move(visible_security_state));
}
