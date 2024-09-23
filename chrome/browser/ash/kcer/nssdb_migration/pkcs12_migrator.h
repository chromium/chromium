// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_PKCS12_MIGRATOR_H_
#define CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_PKCS12_MIGRATOR_H_

#include <memory>

#include "ash/components/kcer/kcer.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/cert/scoped_nss_types.h"

namespace content {
class BrowserContext;
}

namespace kcer {

const char kKcerPkcs12MigrationUma[] = "Ash.KcerPkcs12Migration.Events";

// Used for UMA counters, the entries should not be re-numbered or re-used.
enum class KcerPkcs12MigrationEvent {
  kMigrationStarted = 0,
  kkNothingToMigrate = 1,
  kMigrationFinishedSuccess = 2,
  kMigrationFinishedFailure = 3,
  kCertMigratedSuccess = 4,
  kFailedToReimportCert = 5,
  kExportedPkcs12EmptyError = 6,
  kFailedToGetKcerCerts = 7,
  kFailedToGetNssCerts = 8,
  kMaxValue = kFailedToGetNssCerts,
};

// On Profile creation evaluates whether the migration of client certificates
// from the public slot to the private slot is enabled, and if yes, creates and
// starts a Pkcs12Migrator instance.
class Pkcs12MigratorFactory : public ProfileKeyedServiceFactory {
 public:
  static Pkcs12MigratorFactory* GetInstance();

 private:
  friend class base::NoDestructor<Pkcs12MigratorFactory>;

  Pkcs12MigratorFactory();
  ~Pkcs12MigratorFactory() override = default;

  // Implements BrowserStateKeyedServiceFactory.
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

// Copies all client certificates from the NSS public slot of the `context` into
// its private slot (provided by Chaps). It never deletes migrated certificates
// to reduce the risk of breakages and allow a simpler rollback.
class Pkcs12Migrator : public KeyedService {
 public:
  explicit Pkcs12Migrator(content::BrowserContext* context);
  ~Pkcs12Migrator() override;

  // Waits for 30 sec to avoid slowdowns during user session initialization.
  // Finds all NSS public slot client certificates that don't exist on the Chaps
  // token. Copies the NSS public slot client certificates to the Chaps token.
  void Start();

 private:
  void StartAfterDelay();
  void MigrateCerts(bool success, net::ScopedCERTCertificateList certs);
  void MigrateCertsWithKcerCerts(
      net::ScopedCERTCertificateList nss_certs,
      std::vector<scoped_refptr<const Cert>> kcer_certs,
      base::flat_map<Token, Error> kcer_errors);
  void MigrateEachCert(net::ScopedCERTCertificateList certs);
  void ExportedOneCert(net::ScopedCERTCertificateList certs, Pkcs12Blob pkcs12);
  void ImportedOneCert(net::ScopedCERTCertificateList certs,
                       base::expected<void, Error> result);

  bool had_failures_ = false;
  raw_ptr<content::BrowserContext> context_;
  base::WeakPtrFactory<Pkcs12Migrator> weak_factory_{this};
};

}  // namespace kcer

#endif  // CHROME_BROWSER_ASH_KCER_NSSDB_MIGRATION_PKCS12_MIGRATOR_H_
