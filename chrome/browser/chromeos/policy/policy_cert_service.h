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
#include "chromeos/network/policy_certificate_provider.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class FilePath;
}

namespace user_manager {
class UserManager;
}

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace network {
class NSSTempCertsCacheChromeOS;
}

namespace policy {

// This service is responsible for pushing the current list of policy-provided
// certificates to ProfileNetworkContextService.
// This service / its factory keep track of which Profile has used a
// policy-provided trust anchor.
class PolicyCertService : public KeyedService,
                          public chromeos::PolicyCertificateProvider::Observer {
 public:
  // Constructs a PolicyCertService for |profile| using
  // |policy_certificate_provider| as the source of certificates.
  // If |may_use_profile_wide_trust_anchors| is true, certificates from
  // |policy_certificate_provider| that have requested "Web" trust and have
  // profile-wide scope will be used for |profile|.
  // |user_id| is used to remember if policy-provided trust anchors have been
  // used in this user Profile and should be an empty string if this is the
  // PolicyCertService for a Profile which is not associated with a user.
  // If |user_id| is empty, |may_use_profile_wide_trust_anchors| must be false.
  PolicyCertService(
      Profile* profile,
      chromeos::PolicyCertificateProvider* policy_certificate_provider,
      bool may_use_profile_wide_trust_anchors,
      const std::string& user_id,
      user_manager::UserManager* user_manager);
  ~PolicyCertService() override;

  // Returns true if the profile that owns this service has used certificates
  // installed via policy to establish a secure connection before. This means
  // that it may have cached content from an untrusted source.
  bool UsedPolicyCertificates() const;

  // Returns true if the profile that owns this service has at least one
  // policy-provided trust anchor configured.
  bool has_policy_certificates() const {
    return !profile_wide_trust_anchors_.empty();
  }

  // PolicyCertificateProvider::Observer:
  void OnPolicyProvidedCertsChanged() override;

  // Fills *|out_all_server_and_authority_certificates| and *|out_trust_anchors|
  // with policy-provided certificates that should be used when verifying a
  // server certificate for Web requests from the StoragePartition identified by
  // |partition_path|.
  void GetPolicyCertificatesForStoragePartition(
      const base::FilePath& partition_path,
      net::CertificateList* out_all_server_and_authority_certificates,
      net::CertificateList* out_trust_anchors) const;

  static std::unique_ptr<PolicyCertService> CreateForTesting(
      const std::string& user_id,
      user_manager::UserManager* user_manager);

  // Sets the profile-wide policy-provided trust anchors reported by this
  // PolicyCertService. This is only callable for instances created through
  // CreateForTesting.
  void SetPolicyTrustAnchorsForTesting(
      const net::CertificateList& trust_anchors);

 private:
  // Constructor used by CreateForTesting.
  PolicyCertService(const std::string& user_id,
                    user_manager::UserManager* user_manager);

  // Returns all allowed policy-provided certificates that have requested "Web"
  // trust and have profile-wide scope. If |may_use_profile_wide_trust_anchors_|
  // is false, always returns an empty list.
  net::CertificateList GetAllowedProfileWideTrustAnchors();

  Profile* const profile_;

  // The source of certificates for this PolicyCertService.
  chromeos::PolicyCertificateProvider* const policy_certificate_provider_;

  // If true, CA certificates |policy_certificate_provider_| that have requested
  // "Web" trust and have profile-wide scope may be used for |profile_|.
  const bool may_use_profile_wide_trust_anchors_;

  // This will be an empty string for a PolicyCertService which is tied to a
  // Profile without user association (e.g. the sign-in screen Profile).
  const std::string user_id_;
  user_manager::UserManager* const user_manager_;

  // Caches all server and CA certificates that have profile-wide scope from
  // |policy_certificate_provider_|.
  net::CertificateList profile_wide_all_server_and_authority_certs_;
  // Caches CA certificates that have requested "Web" trust and have
  // profile-wide scope from |policy_certificate_provider_|.
  net::CertificateList profile_wide_trust_anchors_;

  // Holds all policy-provided server and authority certificates and makes them
  // available to NSS as temp certificates. This is needed so they can be used
  // as intermediates when NSS verifies a certificate.
  std::unique_ptr<network::NSSTempCertsCacheChromeOS>
      temp_policy_provided_certs_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCertService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_SERVICE_H_
