// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "chromeos/network/onc/certificate_scope.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/policy_certificate_provider.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/common/policy_service.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}  // namespace base

namespace chromeos {
class ManagedNetworkConfigurationHandler;
}  // namespace chromeos

namespace policy {

class PolicyMap;

// Implements the common part of tracking a OpenNetworkConfiguration device or
// user policy. Pushes the network configs to the
// ManagedNetworkConfigurationHandler, which in turn writes configurations to
// Shill. Certificates are imported with the chromeos::onc::CertificateImporter.
// For user policies the subclass UserNetworkConfigurationUpdater must be used.
// Does not handle proxy settings.
class NetworkConfigurationUpdater : public chromeos::PolicyCertificateProvider,
                                    public PolicyService::Observer {
 public:
  ~NetworkConfigurationUpdater() override;

  // PolicyService::Observer overrides
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;
  void OnPolicyServiceInitialized(PolicyDomain domain) override;

  // chromeos::PolicyCertificateProvider:
  void AddPolicyProvidedCertsObserver(
      chromeos::PolicyCertificateProvider::Observer* observer) override;
  void RemovePolicyProvidedCertsObserver(
      chromeos::PolicyCertificateProvider::Observer* observer) override;
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
  NetworkConfigurationUpdater(
      onc::ONCSource onc_source,
      std::string policy_key,
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler);

  virtual void Init();

  // Called in the subclass to import client certificates provided by the ONC
  // policy. The client certificates to be imported can be obtained using
  // |GetClientcertificates()|.
  virtual void ImportClientCertificates() = 0;

  // Pushes the network part of the policy to the
  // ManagedNetworkConfigurationHandler. This can be overridden by subclasses to
  // modify |network_configs_onc| before the actual application.
  virtual void ApplyNetworkPolicy(
      base::ListValue* network_configs_onc,
      base::DictionaryValue* global_network_config) = 0;

  // Parses the current value of the ONC policy. Clears |network_configs|,
  // |global_network_config| and |certificates| and fills them with the
  // validated NetworkConfigurations, GlobalNetworkConfiguration and
  // Certificates of the current policy. Callers can pass nullptr to any of
  // |network_configs|, |global_network_config|, |certificates| if they don't
  // need that specific part of the ONC policy.
  void ParseCurrentPolicy(base::ListValue* network_configs,
                          base::DictionaryValue* global_network_config,
                          base::ListValue* certificates);

  // Determines if |policy_map| contains an ONC policy under |policy_key| that
  // mandates that at least one additional certificate should be used and
  // assignd 'Web' trust.
  static bool PolicyHasWebTrustedAuthorityCertificate(
      const PolicyMap& policy_map,
      onc::ONCSource onc_source,
      const std::string& policy_key);

  const std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>&
  GetClientCertificates() const;

  onc::ONCSource onc_source_;

  // Pointer to the global singleton or a test instance.
  chromeos::ManagedNetworkConfigurationHandler* network_config_handler_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Called if the ONC policy changed.
  void OnPolicyChanged(const base::Value* previous, const base::Value* current);

  // Apply the observed policy, i.e. both networks and certificates.
  void ApplyPolicy();

  // Marks IP Address config fields as "Recommended" for Ethernet network
  // configs without authentication. The reason is that Chrome OS used to treat
  // Ethernet networks without authentication as unmanaged, so users were able
  // to edit the IP address even if there was a policy for Ethernet. This
  // behavior should be preserved for now to not break existing use cases.
  // TODO(https://crbug.com/931412): Remove this when the server sets
  // "Recommended".
  void MarkFieldsAsRecommendedForBackwardsCompatibility(
      base::Value* network_configs_onc);

  // Sets the "Recommended" list of recommended field names in |onc_value|,
  // which must be a dictionary, to |recommended_field_names|. If a
  // "Recommended" list already existed in |onc_value|, it's replaced.
  void SetRecommended(
      base::Value* onc_value,
      std::initializer_list<base::StringPiece> recommended_field_names);

  std::string LogHeader() const;

  // Imports the certificates part of the policy.
  void ImportCertificates(const base::ListValue& certificates_onc);

  void NotifyPolicyProvidedCertsChanged();

  std::string policy_key_;

  // Used to register for notifications from the |policy_service_|.
  PolicyChangeRegistrar policy_change_registrar_;

  // Used to retrieve the policies.
  PolicyService* policy_service_;

  // Holds certificates from the last parsed ONC policy.
  std::unique_ptr<chromeos::onc::OncParsedCertificates> certs_;
  std::set<std::string> extension_ids_with_policy_certificates_;

  // Observer list for notifying about ONC-provided server and CA certificate
  // changes.
  base::ObserverList<chromeos::PolicyCertificateProvider::Observer,
                     true>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_NETWORK_CONFIGURATION_UPDATER_H_
