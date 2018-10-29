// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chromeos/policy_certificate_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace user_manager {
class UserManager;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace network {
class CertVerifierWithTrustAnchors;
}

namespace policy {
class TempCertsCacheNSS;

// This service is the counterpart of PolicyCertVerifier on the UI thread. It's
// responsible for pushing the current list of trust anchors to the CertVerifier
// and marking the profile's prefs if any of the trust anchors was used.
// Except for unit tests, PolicyCertVerifier should only be created through this
// class.
class PolicyCertService : public KeyedService,
                          public chromeos::PolicyCertificateProvider::Observer {
 public:
  PolicyCertService(Profile* profile,
                    const std::string& user_id,
                    UserNetworkConfigurationUpdater* net_conf_updater,
                    user_manager::UserManager* user_manager);
  ~PolicyCertService() override;

  // Creates an associated PolicyCertVerifier. The returned object must only be
  // used on the IO thread and must outlive this object.
  // This can only be called if the network service is disabled.
  std::unique_ptr<network::CertVerifierWithTrustAnchors>
  CreatePolicyCertVerifier();

  // Start listening for trust anchor changes to push to the network service.
  // This only needs to be called with the network service.
  void StartObservingPolicyCerts();

  // Returns true if the profile that owns this service has used certificates
  // installed via policy to establish a secure connection before. This means
  // that it may have cached content from an untrusted source.
  bool UsedPolicyCertificates() const;

  bool has_policy_certificates() const { return !trust_anchors_.empty(); }

  const net::CertificateList& trust_anchors() const { return trust_anchors_; }

  // UserNetworkConfigurationUpdater::PolicyProvidedCertsObserver:
  void OnPolicyProvidedCertsChanged(
      const net::CertificateList& all_server_and_authority_certs,
      const net::CertificateList& trust_anchors) override;

  // KeyedService:
  void Shutdown() override;

  static std::unique_ptr<PolicyCertService> CreateForTesting(
      const std::string& user_id,
      network::CertVerifierWithTrustAnchors* verifier,
      user_manager::UserManager* user_manager);

 private:
  PolicyCertService(const std::string& user_id,
                    network::CertVerifierWithTrustAnchors* verifier,
                    user_manager::UserManager* user_manager);

  void StartObservingPolicyCertsInternal(bool notify);
  void OnPolicyProvidedCertsChangedInternal(
      const net::CertificateList& all_server_and_authority_certs,
      const net::CertificateList& trust_anchors,
      bool notify);

  Profile* profile_ = nullptr;
  network::CertVerifierWithTrustAnchors* cert_verifier_;
  std::string user_id_;
  UserNetworkConfigurationUpdater* net_conf_updater_;
  user_manager::UserManager* user_manager_;
  net::CertificateList trust_anchors_;

  // Holds all policy-provided server and authority certificates and makes them
  // available to NSS as temp certificates. This is needed so they can be used
  // as intermediates when NSS verifies a certificate.
  std::unique_ptr<TempCertsCacheNSS> temp_policy_provided_certs_;

  // Weak pointers to handle callbacks from PolicyCertVerifier on the IO thread.
  // The factory and the created WeakPtrs must only be used on the UI thread.
  base::WeakPtrFactory<PolicyCertService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCertService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_H_
