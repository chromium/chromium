// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_POLICY_NETWORKING_NETWORK_CONFIGURATION_UPDATER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

class PolicyMap;

// Implements the common part of tracking the OpenNetworkConfiguration device
// and user policy. Implements the handling of server and authority certificates
// (that will be propagated to the network service). Provides entry points for
// handling client certificates and network configurations in subclasses.
// Does not handle proxy settings.
class NetworkConfigurationUpdater : public ash::PolicyCertificateProvider,
                                    public PolicyService::Observer {
 public:
  NetworkConfigurationUpdater(const NetworkConfigurationUpdater&) = delete;
  NetworkConfigurationUpdater& operator=(const NetworkConfigurationUpdater&) =
      delete;

  ~NetworkConfigurationUpdater() override;

  // PolicyService::Observer overrides
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

  // The observer interface sends notifications about changes in server and
  // authority certificates.
  // ash::PolicyCertificateProvider:
  void AddPolicyProvidedCertsObserver(
      ash::PolicyCertificateProvider::Observer* observer) override;
  void RemovePolicyProvidedCertsObserver(
      ash::PolicyCertificateProvider::Observer* observer) override;
  net::CertificateList GetAllServerAndAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override;
  net::CertificateList GetAllAuthorityCertificates(
      const chromeos::onc::CertificateScope& scope) const override;
  net::CertificateList GetWebTrustedCertificates(
      const chromeos::onc::CertificateScope& scope) const override;
  net::CertificateList GetCertificatesWithoutWebTrust(
      const chromeos::onc::CertificateScope& scope) const override;

  const std::set<std::string>& GetExtensionIdsWithPolicyCertificates()
      const override;

 protected:
  NetworkConfigurationUpdater(onc::ONCSource onc_source,
                              std::string policy_key,
                              PolicyService* policy_service);

  virtual void Init();

  // Called in the subclass to import client certificates provided by the ONC
  // policy. The client certificates to be imported can be obtained using
  // |GetClientcertificates()|.
  virtual void ImportClientCertificates() = 0;

  // Parses the incoming policy, applies server and authority certificates.
  // Calls the specialized methods from subclasses to handle client certificates
  // and network configs.
  virtual void ApplyNetworkPolicy(
      const base::Value::List& network_configs_onc,
      const base::Value::Dict& global_network_config) = 0;

  // Parses the current value of the ONC policy. Clears |network_configs|,
  // |global_network_config| and |certificates| and fills them with the
  // validated NetworkConfigurations, GlobalNetworkConfiguration and
  // Certificates of the current policy. Callers can pass nullptr to any of
  // |network_configs|, |global_network_config|, |certificates| if they don't
  // need that specific part of the ONC policy.
  void ParseCurrentPolicy(base::Value::List* network_configs,
                          base::Value::Dict* global_network_config,
                          base::Value::List* certificates);

  const std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>&
  GetClientCertificates() const;

  onc::ONCSource onc_source_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Called if the ONC policy changed.
  void OnPolicyChanged(const base::Value* previous, const base::Value* current);

  // Apply the observed policy, i.e. both networks and certificates.
  void ApplyPolicy();

  std::string LogHeader() const;

  // Imports the certificates part of the policy.
  void ImportCertificates(base::Value::List certificates_onc);

  void NotifyPolicyProvidedCertsChanged();

  std::string policy_key_;

  // Used to register for notifications from the |policy_service_|.
  PolicyChangeRegistrar policy_change_registrar_;

  // Used to retrieve the policies.
  raw_ptr<PolicyService> policy_service_;

  // Holds certificates from the last parsed ONC policy.
  std::unique_ptr<chromeos::onc::OncParsedCertificates> certs_;
  std::set<std::string> extension_ids_with_policy_certificates_;

  // Observer list for notifying about ONC-provided server and CA certificate
  // changes.
  base::ObserverList<ash::PolicyCertificateProvider::Observer, true>::Unchecked
      observer_list_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_NETWORK_CONFIGURATION_UPDATER_H_
