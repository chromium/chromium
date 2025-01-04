// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "net/cert/internal/platform_trust_store.h"

namespace net {

class NSSCertDatabase;
class ServerCertificateDatabaseService;

// Migrates server-related certificates from an NSS user database into
// ServerCertificateDatabase.
// Does not migrate client certificates as those are handled by Kcer and a
// separate migration process handles those.
class ServerCertificateDatabaseNSSMigrator {
 public:
  struct MigrationResult {
    // The number of certs that were read from the user's NSS database.
    int cert_count = 0;

    // Count of how many certs failed to import into the user's
    // ServerCertDatabase.
    int error_count = 0;
  };
  using ResultCallback = base::OnceCallback<void(MigrationResult)>;

  // NssCertDatabaseGetter here has the same definition and usage requirements
  // as in chrome/browser/net/nss_service.h but the alias is redefined here
  // because we can't include that file directly.
  using NssCertDatabaseGetter = base::OnceCallback<net::NSSCertDatabase*(
      base::OnceCallback<void(net::NSSCertDatabase*)> callback)>;

  explicit ServerCertificateDatabaseNSSMigrator(
      ServerCertificateDatabaseService* cert_db_service,
      NssCertDatabaseGetter nss_cert_db_getter);
  ~ServerCertificateDatabaseNSSMigrator();

  // Begins migration process. `callback` will be run on the calling thread when
  // the migration is complete. Must be called on the UI thread.
  // If the ServerCertificateDatabaseNSSMigrator is deleted before the callback
  // has been run, the callback will not be run and migrated certs may or may
  // not be written to the ServerCertificateDatabase.
  void MigrateCerts(ResultCallback callback);

 private:
  void GotCertsFromNSS(
      ResultCallback callback,
      std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate);
  void FinishedMigration(ResultCallback callback, MigrationResult result);

  raw_ptr<ServerCertificateDatabaseService> cert_db_service_;
  NssCertDatabaseGetter nss_cert_db_getter_;
  base::WeakPtrFactory<ServerCertificateDatabaseNSSMigrator> weak_ptr_factory_{
      this};
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_
