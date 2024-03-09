// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/network_configuration_updater.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

using chromeos::onc::CertificateScope;
using chromeos::onc::OncParsedCertificates;

namespace policy {

namespace {

// A predicate used for filtering server or authority certificates.
using ServerOrAuthorityCertPredicate = base::RepeatingCallback<bool(
    const OncParsedCertificates::ServerOrAuthorityCertificate& cert)>;

// Returns a filtered copy of |sever_or_authority_certificates|. The filtered
// copy will contain  a certificate from the input iff it matches |scope| and
// executing |predicate| on it returned true.
net::CertificateList GetFilteredCertificateListFromOnc(
    const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
        server_or_authority_certificates,
    const CertificateScope& scope,
    ServerOrAuthorityCertPredicate predicate) {
  net::CertificateList certificates;
  for (const auto& server_or_authority_cert :
       server_or_authority_certificates) {
    if (server_or_authority_cert.scope() == scope &&
        predicate.Run(server_or_authority_cert))
      certificates.push_back(server_or_authority_cert.certificate());
  }
  return certificates;
}

// Returns all extension IDs that were used in a Scope of a one of the
// |server_or_authority_certificates|.
std::set<std::string> CollectExtensionIds(
    const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
        server_or_authority_certificates) {
  std::set<std::string> extension_ids;
  for (const auto& cert : server_or_authority_certificates) {
    if (cert.scope().is_extension_scoped())
      extension_ids.insert(cert.scope().extension_id());
  }
  return extension_ids;
}

}  // namespace

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  for (auto& observer : observer_list_) {
    observer.OnPolicyCertificateProviderDestroying();
  }
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void NetworkConfigurationUpdater::OnPolicyUpdated(const PolicyNamespace& ns,
                                                  const PolicyMap& previous,
                                                  const PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void NetworkConfigurationUpdater::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain != POLICY_DOMAIN_CHROME)
    return;

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " initialized.";
    policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
    ApplyPolicy();
  }
}

void NetworkConfigurationUpdater::AddPolicyProvidedCertsObserver(
    ash::PolicyCertificateProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NetworkConfigurationUpdater::RemovePolicyProvidedCertsObserver(
    ash::PolicyCertificateProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

net::CertificateList
NetworkConfigurationUpdater::GetAllServerAndAuthorityCertificates(
    const CertificateScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(), scope,
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return true;
          }));
}

net::CertificateList NetworkConfigurationUpdater::GetAllAuthorityCertificates(
    const CertificateScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(), scope,
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return cert.type() ==
                   OncParsedCertificates::ServerOrAuthorityCertificate::Type::
                       kAuthority;
          }));
}

net::CertificateList NetworkConfigurationUpdater::GetWebTrustedCertificates(
    const CertificateScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(), scope,
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return cert.web_trust_requested();
          }));
}

net::CertificateList
NetworkConfigurationUpdater::GetCertificatesWithoutWebTrust(
    const CertificateScope& scope) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(), scope,
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return !cert.web_trust_requested();
          }));
}

const std::set<std::string>&
NetworkConfigurationUpdater::GetExtensionIdsWithPolicyCertificates() const {
  return extension_ids_with_policy_certificates_;
}

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    onc::ONCSource onc_source,
    std::string policy_key,
    PolicyService* policy_service)
    : onc_source_(onc_source),
      policy_key_(policy_key),
      policy_change_registrar_(
          policy_service,
          PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      policy_service_(policy_service),
      certs_(std::make_unique<OncParsedCertificates>()) {}

void NetworkConfigurationUpdater::Init() {
  policy_change_registrar_.Observe(
      policy_key_,
      base::BindRepeating(&NetworkConfigurationUpdater::OnPolicyChanged,
                          base::Unretained(this)));

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " is already initialized.";
    ApplyPolicy();
  } else {
    policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void NetworkConfigurationUpdater::ParseCurrentPolicy(
    base::Value::List* network_configs,
    base::Value::Dict* global_network_config,
    base::Value::List* certificates) {
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value =
      policies.GetValue(policy_key_, base::Value::Type::STRING);

  if (!policies.IsPolicySet(policy_key_))
    VLOG(2) << LogHeader() << " is not set.";
  else if (!policy_value)
    LOG(ERROR) << LogHeader() << " is not a string value.";

  const std::string onc_blob = policy_value && policy_value->is_string()
                                   ? policy_value->GetString()
                                   : std::string();
  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source_, std::string() /* no passphrase */, network_configs,
      global_network_config, certificates);
}

const std::vector<OncParsedCertificates::ClientCertificate>&
NetworkConfigurationUpdater::GetClientCertificates() const {
  return certs_->client_certificates();
}

void NetworkConfigurationUpdater::OnPolicyChanged(const base::Value* previous,
                                                  const base::Value* current) {
  ApplyPolicy();
}

void NetworkConfigurationUpdater::ApplyPolicy() {
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  ParseCurrentPolicy(&network_configs, &global_network_config, &certificates);

  ImportCertificates(std::move(certificates));
  ApplyNetworkPolicy(network_configs, global_network_config);
}

std::string NetworkConfigurationUpdater::LogHeader() const {
  return chromeos::onc::GetSourceAsString(onc_source_);
}

void NetworkConfigurationUpdater::ImportCertificates(
    base::Value::List certificates_onc) {
  std::unique_ptr<OncParsedCertificates> incoming_certs =
      std::make_unique<OncParsedCertificates>(certificates_onc);

  bool server_or_authority_certs_changed =
      certs_->server_or_authority_certificates() !=
      incoming_certs->server_or_authority_certificates();
  bool client_certs_changed =
      certs_->client_certificates() != incoming_certs->client_certificates();

  if (!server_or_authority_certs_changed && !client_certs_changed)
    return;

  certs_ = std::move(incoming_certs);
  extension_ids_with_policy_certificates_ =
      CollectExtensionIds(certs_->server_or_authority_certificates());

  if (client_certs_changed)
    ImportClientCertificates();

  if (server_or_authority_certs_changed)
    NotifyPolicyProvidedCertsChanged();
}

void NetworkConfigurationUpdater::NotifyPolicyProvidedCertsChanged() {
  for (auto& observer : observer_list_)
    observer.OnPolicyProvidedCertsChanged();
}

}  // namespace policy
