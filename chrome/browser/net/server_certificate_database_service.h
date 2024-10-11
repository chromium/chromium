// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
#define CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_

#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/net/server_certificate_database.h"
#include "components/keyed_service/core/keyed_service.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/net/server_certificate_database_nss_migrator.h"
#endif

class Profile;

namespace net {

// KeyedService that loads and provides policies around usage of Certificates
// for TLS.
class ServerCertificateDatabaseService : public KeyedService {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NSSMigrationResultHistogram {
    kNssDbEmpty = 0,
    kSuccess = 1,
    kPartialSuccess = 2,
    kFailed = 3,
    kMaxValue = kFailed,
  };

  // Enum that will record migration state in user's preferences. In the
  // current implementation, migration is only attempted once, but saving state
  // about whether there were any errors with the migration might be useful in
  // case there are issues during the rollout and we need to add new code that
  // can try again for anyone that had errors.
  // These values are persisted to prefs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NSSMigrationResultPref : int {
    kNotMigrated = 0,
    kMigratedSuccessfully = 1,
    kMigrationHadErrors = 2,
  };
#endif

  using GetCertificatesCallback = base::OnceCallback<void(
      std::vector<net::ServerCertificateDatabase::CertInformation>)>;

  explicit ServerCertificateDatabaseService(Profile* profile);

  ServerCertificateDatabaseService(const ServerCertificateDatabaseService&) =
      delete;
  ServerCertificateDatabaseService& operator=(
      const ServerCertificateDatabaseService&) = delete;

  ~ServerCertificateDatabaseService() override;

  // Add or update user settings with the included certificate.
  void AddOrUpdateUserCertificate(
      net::ServerCertificateDatabase::CertInformation cert_info,
      base::OnceCallback<void(bool)> callback);

  // Read all certificates from the database.
  void GetAllCertificates(GetCertificatesCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Migrate certificates from NSS and then read all certificates from the
  // database. Migration will only be done once per profile. If called multiple
  // times before migration completes, all the callbacks will be queued and
  // processed once the migration is done. If called after migration is
  // complete it is equivalent to calling `GetAllCertificates`.
  void GetAllCertificatesMigrateFromNSSFirstIfNeeded(
      GetCertificatesCallback callback);
#endif

  // Run callback with `server_cert_database_`. The callback will be run on a
  // thread pool sequence where it is allowed to call methods on the database
  // object. This can be used to do multiple operations on the database without
  // repeated thread hops.
  void PostTaskWithDatabase(
      base::OnceCallback<void(net::ServerCertificateDatabase*)> callback);

 private:
#if BUILDFLAG(IS_CHROMEOS)
  void NSSMigrationComplete(
      ServerCertificateDatabaseNSSMigrator::MigrationResult result);
#endif

  const raw_ptr<Profile> profile_;

  base::SequenceBound<net::ServerCertificateDatabase> server_cert_database_;
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ServerCertificateDatabaseNSSMigrator> nss_migrator_;
  std::vector<GetCertificatesCallback> get_certificates_pending_migration_;
#endif
};

}  // namespace net

#endif  // CHROME_BROWSER_NET_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
