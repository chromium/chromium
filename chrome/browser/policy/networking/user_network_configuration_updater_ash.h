// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_ASH_H_
#define CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_ASH_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/cert/scoped_nss_types.h"

class Profile;

namespace ash {
class ManagedNetworkConfigurationHandler;
namespace onc {
class CertificateImporter;
}  // namespace onc
}  // namespace ash

namespace base {
class Value;
}

namespace user_manager {
class User;
}

namespace net {
class NSSCertDatabase;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate>> CertificateList;
}  // namespace net

namespace policy {

class PolicyMap;
class PolicyService;

// Implements additional special handling of ONC user policies. Namely string
// expansion with the user's name (or email address, etc.) and handling of "Web"
// trust of certificates.
class UserNetworkConfigurationUpdaterAsh
    : public UserNetworkConfigurationUpdater,
      public content::NotificationObserver {
 public:
  UserNetworkConfigurationUpdaterAsh(
      const UserNetworkConfigurationUpdaterAsh&) = delete;
  UserNetworkConfigurationUpdaterAsh& operator=(
      const UserNetworkConfigurationUpdaterAsh&) = delete;

  ~UserNetworkConfigurationUpdaterAsh() override;

  // Creates an updater that applies the ONC user policy from |policy_service|
  // for user |user| once the policy service is completely initialized and on
  // each policy change.  A reference to |user| is stored. It must outlive the
  // returned updater.
  static std::unique_ptr<UserNetworkConfigurationUpdaterAsh>
  CreateForUserPolicy(
      Profile* profile,
      const user_manager::User& user,
      PolicyService* policy_service,
      ash::ManagedNetworkConfigurationHandler* network_config_handler);

  // Helper method to expose |SetClientCertificateImporter| for usage in tests.
  // Note that the CertificateImporter is only used for importing client
  // certificates.
  void SetClientCertificateImporterForTest(
      std::unique_ptr<ash::onc::CertificateImporter> certificate_importer);

  // Determines if |policy_map| contains a OpenNetworkConfiguration policy that
  // mandates that at least one additional certificate should be used and
  // assigned 'Web' trust.
  static bool PolicyHasWebTrustedAuthorityCertificate(
      const PolicyMap& policy_map);

 private:
  class CrosTrustAnchorProvider;

  UserNetworkConfigurationUpdaterAsh(
      Profile* profile,
      const user_manager::User& user,
      PolicyService* policy_service,
      ash::ManagedNetworkConfigurationHandler* network_config_handler);

  // NetworkConfigurationUpdater:
  void ImportClientCertificates() override;

  void ApplyNetworkPolicy(base::Value::List network_configs_onc,
                          base::Value::Dict global_network_config) override;

  // content::NotificationObserver implementation. Observes the profile to which
  // |this| belongs to for PROFILE_ADDED notification.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Creates onc::CertImporter with |database| and passes it to
  // |SetClientCertificateImporter|.
  void CreateAndSetClientCertificateImporter(net::NSSCertDatabase* database);

  // Sets the certificate importer that should be used to import certificate
  // policies. If there is |pending_certificates_onc_|, it gets imported.
  void SetClientCertificateImporter(
      std::unique_ptr<ash::onc::CertificateImporter> certificate_importer);

  // The user for whom the user policy will be applied.
  const raw_ptr<const user_manager::User> user_;

  // Pointer to the global singleton or a test instance.
  const raw_ptr<ash::ManagedNetworkConfigurationHandler>
      network_config_handler_;

  // Certificate importer to be used for importing policy defined client
  // certificates. Set by |SetClientCertificateImporter|.
  std::unique_ptr<ash::onc::CertificateImporter> client_certificate_importer_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<UserNetworkConfigurationUpdaterAsh> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_ASH_H_
