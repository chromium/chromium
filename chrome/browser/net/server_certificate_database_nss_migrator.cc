// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_nss_migrator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/net/server_certificate_database_service.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "net/cert/internal/trust_store_nss.h"
#include "net/cert/nss_cert_database.h"

namespace net {

namespace {

auto MapTrust(const bssl::CertificateTrust& trust) {
  if (trust.IsDistrusted()) {
    return chrome_browser_server_certificate_database::CertificateTrust::
        CERTIFICATE_TRUST_TYPE_DISTRUSTED;
  }
  if (trust.IsTrustAnchor() || trust.IsTrustLeaf()) {
    return chrome_browser_server_certificate_database::CertificateTrust::
        CERTIFICATE_TRUST_TYPE_TRUSTED;
  }
  return chrome_browser_server_certificate_database::CertificateTrust::
      CERTIFICATE_TRUST_TYPE_UNSPECIFIED;
}

void MigrateCertsOnBackgroundThread(
    std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate,
    ServerCertificateDatabaseNSSMigrator::ResultCallback callback,
    net::ServerCertificateDatabase* server_cert_database) {
  ServerCertificateDatabaseNSSMigrator::MigrationResult result;
  result.cert_count = certs_to_migrate.size();
  for (net::PlatformTrustStore::CertWithTrust& cert_to_migrate :
       certs_to_migrate) {
    net::ServerCertificateDatabase::CertInformation cert_info;
    cert_info.sha256hash_hex =
        base::HexEncode(crypto::SHA256Hash(cert_to_migrate.cert_bytes));
    cert_info.der_cert = std::move(cert_to_migrate.cert_bytes);
    cert_info.cert_metadata.mutable_trust()->set_trust_type(
        MapTrust(cert_to_migrate.trust));

    bool ok = server_cert_database->InsertOrUpdateCert(cert_info);
    if (!ok) {
      result.error_count++;
      LOG(ERROR) << "error importing cert " << cert_info.sha256hash_hex;
    }
  }

  std::move(callback).Run(std::move(result));
}

void ReadNSSCertsOnBackgroundThread(
    crypto::ScopedPK11Slot slot,
    base::OnceCallback<
        void(std::vector<net::PlatformTrustStore::CertWithTrust>)> callback) {
  TrustStoreNSS trust_store_nss(
      TrustStoreNSS::UserSlotTrustSetting(std::move(slot)));
  std::move(callback).Run(trust_store_nss.GetAllUserAddedCerts());
}

void GotNSSCertDatabaseOnIOThread(
    base::OnceCallback<
        void(std::vector<net::PlatformTrustStore::CertWithTrust>)> callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadNSSCertsOnBackgroundThread, cert_db->GetPublicSlot(),
                     std::move(callback)));
}

void GetNSSCertDatabaseOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<
        void(std::vector<net::PlatformTrustStore::CertWithTrust>)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&GotNSSCertDatabaseOnIOThread, std::move(callback)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db) {
    std::move(split_callback.second).Run(cert_db);
  }
}

}  // namespace

ServerCertificateDatabaseNSSMigrator::ServerCertificateDatabaseNSSMigrator(
    Profile* profile)
    : profile_(profile) {}

ServerCertificateDatabaseNSSMigrator::~ServerCertificateDatabaseNSSMigrator() =
    default;

void ServerCertificateDatabaseNSSMigrator::MigrateCerts(
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetNSSCertDatabaseOnIOThread,
          NssServiceFactory::GetForContext(profile_)
              ->CreateNSSCertDatabaseGetterForIOThread(),
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &ServerCertificateDatabaseNSSMigrator::GotCertsFromNSS,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)))));
}

void ServerCertificateDatabaseNSSMigrator::GotCertsFromNSS(
    ResultCallback callback,
    std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  net::ServerCertificateDatabaseService* cert_db_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          profile_);

  cert_db_service->PostTaskWithDatabase(base::BindOnce(
      &MigrateCertsOnBackgroundThread, std::move(certs_to_migrate),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ServerCertificateDatabaseNSSMigrator::FinishedMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)))));
}

void ServerCertificateDatabaseNSSMigrator::FinishedMigration(
    ResultCallback callback,
    MigrationResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(std::move(result));
  // `this` object may be deleted by the callback, do not access object after
  // this point.
}

}  // namespace net
