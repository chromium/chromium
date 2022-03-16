// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/task/bind_post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_cert_loader.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "chromeos/network/onc/onc_certificate_importer_impl.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_source.h"

namespace policy {

namespace {

void GetNssCertDatabaseOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
}

}  // namespace

UserNetworkConfigurationUpdaterAsh::~UserNetworkConfigurationUpdaterAsh() {
  // NetworkCertLoader may be not initialized in tests.
  if (chromeos::NetworkCertLoader::IsInitialized()) {
    chromeos::NetworkCertLoader::Get()->SetUserPolicyCertificateProvider(
        nullptr);
  }
}

// static
std::unique_ptr<UserNetworkConfigurationUpdaterAsh>
UserNetworkConfigurationUpdaterAsh::CreateForUserPolicy(
    Profile* profile,
    const user_manager::User& user,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler) {
  std::unique_ptr<UserNetworkConfigurationUpdaterAsh> updater(
      new UserNetworkConfigurationUpdaterAsh(profile, user, policy_service,
                                             network_config_handler));
  updater->Init();
  return updater;
}

void UserNetworkConfigurationUpdaterAsh::SetClientCertificateImporterForTest(
    std::unique_ptr<chromeos::onc::CertificateImporter>
        client_certificate_importer) {
  SetClientCertificateImporter(std::move(client_certificate_importer));
}

// static
bool UserNetworkConfigurationUpdaterAsh::
    PolicyHasWebTrustedAuthorityCertificate(const PolicyMap& policy_map) {
  const base::Value* policy_value = policy_map.GetValue(
      key::kOpenNetworkConfiguration, base::Value::Type::STRING);

  if (!policy_value)
    return false;

  base::ListValue certificates_value;
  chromeos::onc::ParseAndValidateOncForImport(
      policy_value->GetString(), onc::ONC_SOURCE_USER_POLICY,
      /*passphrase=*/std::string(),
      /*network_configs=*/nullptr, /*global_network_config=*/nullptr,
      &certificates_value);
  chromeos::onc::OncParsedCertificates onc_parsed_certificates(
      certificates_value);
  for (const auto& server_or_authority_cert :
       onc_parsed_certificates.server_or_authority_certificates()) {
    if (server_or_authority_cert.type() ==
            chromeos::onc::OncParsedCertificates::ServerOrAuthorityCertificate::
                Type::kAuthority &&
        server_or_authority_cert.web_trust_requested()) {
      return true;
    }
  }
  return false;
}

UserNetworkConfigurationUpdaterAsh::UserNetworkConfigurationUpdaterAsh(
    Profile* profile,
    const user_manager::User& user,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler)
    : UserNetworkConfigurationUpdater(policy_service),
      user_(&user),
      network_config_handler_(network_config_handler) {
  // The updater is created with |client_certificate_importer_| unset and is
  // responsible for creating it. This requires |GetNSSCertDatabaseForProfile|
  // call, which is not safe before the profile initialization is finalized.
  // Thus, listen for PROFILE_ADDED notification, on which |cert_importer_|
  // creation should start. https://crbug.com/171406
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_ADDED,
                 content::Source<Profile>(profile));

  // Make sure that the |NetworkCertLoader| which makes certificates available
  // to the chromeos network code gets policy-pushed certificates from the
  // primary profile. This assumes that a |UserNetworkConfigurationUpdaterAsh|
  // is only created for the primary profile. NetworkCertLoader may be not
  // initialized in tests.
  if (chromeos::NetworkCertLoader::IsInitialized())
    chromeos::NetworkCertLoader::Get()->SetUserPolicyCertificateProvider(this);
}

void UserNetworkConfigurationUpdaterAsh::ImportClientCertificates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If certificate importer is not yet set, the import of client certificates
  // will be re-triggered in SetClientCertificateImporter.
  if (client_certificate_importer_) {
    client_certificate_importer_->ImportClientCertificates(
        GetClientCertificates(), base::DoNothing());
  }
}

void UserNetworkConfigurationUpdaterAsh::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(user_);
  chromeos::onc::ExpandStringPlaceholdersInNetworksForUser(user_,
                                                           network_configs_onc);

  // Call on UserSessionManager to send the user's password to session manager
  // if the password substitution variable exists in the ONC.
  bool save_password =
      chromeos::onc::HasUserPasswordSubsitutionVariable(network_configs_onc);
  ash::UserSessionManager::GetInstance()->VoteForSavingLoginPassword(
      ash::UserSessionManager::PasswordConsumingService::kNetwork,
      save_password);

  network_config_handler_->SetPolicy(onc_source_, user_->username_hash(),
                                     *network_configs_onc,
                                     *global_network_config);
}

void UserNetworkConfigurationUpdaterAsh::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_ADDED);
  Profile* profile = content::Source<Profile>(source).ptr();

  // Note: This unsafely grabs a persistent reference to the `NssService`'s
  // `NSSCertDatabase`, which may be invalidated once `profile` is shut down.
  // TODO(https://crbug.com/1186373): Provide better lifetime guarantees and
  // pass the `NssCertDatabaseGetter` to the `CertificateImporterImpl`.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetNssCertDatabaseOnIOThread,
          NssServiceFactory::GetForContext(profile)
              ->CreateNSSCertDatabaseGetterForIOThread(),
          base::BindPostTask(
              base::SequencedTaskRunnerHandle::Get(),
              base::BindOnce(&UserNetworkConfigurationUpdaterAsh::
                                 CreateAndSetClientCertificateImporter,
                             weak_factory_.GetWeakPtr()))));
}

void UserNetworkConfigurationUpdaterAsh::CreateAndSetClientCertificateImporter(
    net::NSSCertDatabase* database) {
  DCHECK(database);
  SetClientCertificateImporter(
      std::make_unique<chromeos::onc::CertificateImporterImpl>(
          content::GetIOThreadTaskRunner({}), database));
}

void UserNetworkConfigurationUpdaterAsh::SetClientCertificateImporter(
    std::unique_ptr<chromeos::onc::CertificateImporter>
        client_certificate_importer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool initial_client_certificate_importer =
      client_certificate_importer_ == nullptr;
  client_certificate_importer_ = std::move(client_certificate_importer);

  if (initial_client_certificate_importer && !GetClientCertificates().empty()) {
    client_certificate_importer_->ImportClientCertificates(
        GetClientCertificates(), base::DoNothing());
  }
}

}  // namespace policy
