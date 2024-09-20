// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/security_handler.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
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
const char kCertMissingSubjectAltName[] = "cert-missing-subject-alt-name";

std::string SecurityLevelToProtocolSecurityState(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
      return protocol::Security::SecurityStateEnum::Neutral;
    case security_state::WARNING:
      return protocol::Security::SecurityStateEnum::Insecure;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::SECURE:
      return protocol::Security::SecurityStateEnum::Secure;
    case security_state::DANGEROUS:
      return protocol::Security::SecurityStateEnum::InsecureBroken;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED_IN_MIGRATION();
      return protocol::Security::SecurityStateEnum::Neutral;
  }

  NOTREACHED_IN_MIGRATION();
  return protocol::Security::SecurityStateEnum::Neutral;
}

std::unique_ptr<protocol::Security::CertificateSecurityState>
CreateCertificateSecurityState(
    const security_state::VisibleSecurityState& state) {
  auto certificate = std::make_unique<protocol::Array<protocol::String>>();
  if (state.certificate) {
    certificate->push_back(
        base::Base64Encode(net::x509_util::CryptoBufferAsStringPiece(
            state.certificate->cert_buffer())));
    for (const auto& cert : state.certificate->intermediate_buffers()) {
      certificate->push_back(base::Base64Encode(
          net::x509_util::CryptoBufferAsStringPiece(cert.get())));
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
    valid_from = state.certificate->valid_start().InSecondsFSinceUnixEpoch();
    valid_to = state.certificate->valid_expiry().InSecondsFSinceUnixEpoch();
  }

  bool certificate_has_weak_signature =
      (state.cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);

  bool certificate_has_sha1_signature =
      state.certificate &&
      (state.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);

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
          .SetCertificateHasWeakSignature(certificate_has_weak_signature)
          .SetCertificateHasSha1Signature(certificate_has_sha1_signature)
          .SetModernSSL(modern_ssl)
          .SetObsoleteSslProtocol(obsolete_ssl_protocol)
          .SetObsoleteSslKeyExchange(obsolete_ssl_key_exchange)
          .SetObsoleteSslCipher(obsolete_ssl_cipher)
          .SetObsoleteSslSignature(obsolete_ssl_signature)
          .Build();

  if (net::IsCertStatusError(state.cert_status)) {
    certificate_security_state->SetCertificateNetworkError(
        net::ErrorToString(net::MapCertStatusToNetError(state.cert_status)));
  }
  if (key_exchange_group)
    certificate_security_state->SetKeyExchangeGroup(key_exchange_group);
  if (mac)
    certificate_security_state->SetMac(mac);

  return certificate_security_state;
}

std::unique_ptr<protocol::Security::SafetyTipInfo> CreateSafetyTipInfo(
    const security_state::SafetyTipInfo& safety_tip_info) {
  switch (safety_tip_info.status) {
    case security_state::SafetyTipStatus::kLookalike:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
      return protocol::Security::SafetyTipInfo::Create()
          .SetSafetyTipStatus(
              protocol::Security::SafetyTipStatusEnum::Lookalike)
          .SetSafeUrl(safety_tip_info.safe_url.spec())
          .Build();

    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      return nullptr;
  }
}

std::unique_ptr<protocol::Security::VisibleSecurityState>
CreateVisibleSecurityState(SecurityStateTabHelper* helper) {
  DCHECK(helper);
  auto state = helper->GetVisibleSecurityState();
  std::string security_state =
      SecurityLevelToProtocolSecurityState(helper->GetSecurityLevel());

  bool scheme_is_cryptographic =
      security_state::IsSchemeCryptographic(state->url);
  bool malicious_content = state->malicious_content_status !=
                           security_state::MALICIOUS_CONTENT_STATUS_NONE;

  bool secure_origin = scheme_is_cryptographic;
  if (!scheme_is_cryptographic)
    secure_origin = network::IsUrlPotentiallyTrustworthy(state->url);

  bool cert_missing_subject_alt_name =
      state->certificate &&
      !state->certificate->GetSubjectAltName(nullptr, nullptr);

  std::vector<std::string> security_state_issue_ids;
  if (!secure_origin)
    security_state_issue_ids.push_back(kInsecureOriginSecurityStateIssueId);
  if (!scheme_is_cryptographic)
    security_state_issue_ids.push_back(
        kSchemeIsNotCryptographicSecurityStateIssueId);
  if (malicious_content)
    security_state_issue_ids.push_back(kMalicousContentSecurityStateIssueId);
  if (state->displayed_mixed_content)
    security_state_issue_ids.push_back(
        kDisplayedMixedContentSecurityStateIssueId);
  if (state->contained_mixed_form)
    security_state_issue_ids.push_back(kContainedMixedFormSecurityStateIssueId);
  if (state->ran_mixed_content)
    security_state_issue_ids.push_back(kRanMixedContentSecurityStateIssueId);
  if (state->displayed_content_with_cert_errors)
    security_state_issue_ids.push_back(
        kDisplayedContentWithCertErrorsSecurityStateIssueId);
  if (state->ran_content_with_cert_errors)
    security_state_issue_ids.push_back(
        kRanContentWithCertErrorSecurityStateIssueId);
  if (state->pkp_bypassed)
    security_state_issue_ids.push_back(kPkpBypassedSecurityStateIssueId);
  if (state->is_error_page)
    security_state_issue_ids.push_back(kIsErrorPageSecurityStateIssueId);
  if (cert_missing_subject_alt_name)
    security_state_issue_ids.push_back(kCertMissingSubjectAltName);

  auto visible_security_state =
      protocol::Security::VisibleSecurityState::Create()
          .SetSecurityState(security_state)
          .SetSecurityStateIssueIds(
              std::make_unique<protocol::Array<std::string>>(
                  security_state_issue_ids))
          .Build();

  if (state->connection_status != 0) {
    auto certificate_security_state = CreateCertificateSecurityState(*state);
    visible_security_state->SetCertificateSecurityState(
        std::move(certificate_security_state));
  }

  auto safety_tip_info = CreateSafetyTipInfo(state->safety_tip_info);
  if (safety_tip_info)
    visible_security_state->SetSafetyTipInfo(std::move(safety_tip_info));

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

  SecurityStateTabHelper* helper = web_contents() ? SecurityStateTabHelper::FromWebContents(web_contents()) : nullptr;
  if (!helper)
    return;

  auto visible_security_state = CreateVisibleSecurityState(helper);
  frontend_->VisibleSecurityStateChanged(std::move(visible_security_state));
}
