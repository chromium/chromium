// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/kcer/nssdb_migration/pkcs12_migrator.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/kcer/cert_cache.h"
#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/kcer/kcer_factory_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/certificate_helper.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/x509_util_nss.h"

namespace kcer {
namespace {

using ListCertsCallback =
    base::OnceCallback<void(bool success,
                            net::ScopedCERTCertificateList certs)>;

void RecordUmaEvent(KcerPkcs12MigrationEvent event) {
  base::UmaHistogramEnumeration(kKcerPkcs12MigrationUma, event);
}

void FilterClientCerts(ListCertsCallback callback,
                       net::ScopedCERTCertificateList certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  net::ScopedCERTCertificateList filtered_certs;
  for (net::ScopedCERTCertificate& cert : certs) {
    if (ash::certificate::GetCertType(cert.get()) == net::USER_CERT) {
      filtered_certs.push_back(std::move(cert));
    }
  }

  std::move(callback).Run(/*success=*/true, std::move(filtered_certs));
}

void ListPublicSlotClientCertsWithDb(ListCertsCallback callback,
                                     net::NSSCertDatabase* nss_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!nss_db) {
    return std::move(callback).Run(/*success=*/false, {});
  }

  nss_db->ListCertsInSlot(
      base::BindOnce(&FilterClientCerts, std::move(callback)),
      nss_db->GetPublicSlot().get());
}

void ListPublicSlotClientCertsOnIOThread(NssCertDatabaseGetter database_getter,
                                         ListCertsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&ListPublicSlotClientCertsWithDb, std::move(callback)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  // If the NSS database was already available, |cert_db| is non-null and
  // |did_get_cert_db_callback| has not been called. Call it explicitly.
  if (cert_db) {
    std::move(split_callback.second).Run(cert_db);
  }
}

void ExportCertOnWorkerThread(net::ScopedCERTCertificate cert,
                              base::OnceCallback<void(Pkcs12Blob)> callback) {
  net::ScopedCERTCertificateList cert_list;
  cert_list.push_back(std::move(cert));
  std::string pkcs12;
  net::NSSCertDatabase::ExportToPKCS12(cert_list,
                                       /*password=*/std::u16string(), &pkcs12);
  std::move(callback).Run(
      Pkcs12Blob(std::vector<uint8_t>(pkcs12.begin(), pkcs12.end())));
}

}  // namespace

//==============================================================================

Pkcs12MigratorFactory::Pkcs12MigratorFactory()
    : ProfileKeyedServiceFactory(
          "Pkcs12Migrator",
          ProfileSelections::Builder()
              // This factory needs to only create the service for the profiles
              // of the ChromeOS users because only those have a persistent NSS
              // Database ("public slot") that can be used as a source for the
              // migration.
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(NssServiceFactory::GetInstance());
  DependsOn(KcerFactoryAsh::GetInstance());
}

// static
Pkcs12MigratorFactory* Pkcs12MigratorFactory::GetInstance() {
  static base::NoDestructor<Pkcs12MigratorFactory> factory;
  return factory.get();
}

bool Pkcs12MigratorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
Pkcs12MigratorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!ash::features::IsCopyClientKeysCertsToChapsEnabled()) {
    return nullptr;
  }
  // The public slot contains the data from the software NSS database, which is
  // just a file on disk, the primary profile is associated with the current
  // ChromeOS user and conceptually owns files of the user, so it should be
  // responsible for working with them. Other profiles for the same ChromeOS
  // user would try to migrate the same certs, which could lead to problems.
  if (!ash::ProfileHelper::IsPrimaryProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }
  auto service = std::make_unique<Pkcs12Migrator>(context);
  service->Start();
  return service;
}

//==============================================================================

Pkcs12Migrator::Pkcs12Migrator(content::BrowserContext* context)
    : context_(context) {}

Pkcs12Migrator::~Pkcs12Migrator() = default;

void Pkcs12Migrator::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  RecordUmaEvent(KcerPkcs12MigrationEvent::kMigrationStarted);
  // Delay the migration a bit, so it doesn't slow down ChromeOS at the
  // beginning of a user session.
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Pkcs12Migrator::StartAfterDelay,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(30));
}

void Pkcs12Migrator::StartAfterDelay() {
  auto callback =
      base::BindPostTask(content::GetUIThreadTaskRunner({}),
                         base::BindOnce(&Pkcs12Migrator::MigrateCerts,
                                        weak_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ListPublicSlotClientCertsOnIOThread,
                                NssServiceFactory::GetForContext(context_)
                                    ->CreateNSSCertDatabaseGetterForIOThread(),
                                std::move(callback)));
}

void Pkcs12Migrator::MigrateCerts(bool success,
                                  net::ScopedCERTCertificateList nss_certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!success) {
    return RecordUmaEvent(KcerPkcs12MigrationEvent::kFailedToGetNssCerts);
  }

  if (nss_certs.empty()) {
    return RecordUmaEvent(KcerPkcs12MigrationEvent::kkNothingToMigrate);
  }

  base::WeakPtr<Kcer> kcer =
      KcerFactoryAsh::GetKcer(Profile::FromBrowserContext(context_));
  kcer->ListCerts(
      {Token::kUser},
      base::BindOnce(&Pkcs12Migrator::MigrateCertsWithKcerCerts,
                     weak_factory_.GetWeakPtr(), std::move(nss_certs)));
}

void Pkcs12Migrator::MigrateCertsWithKcerCerts(
    net::ScopedCERTCertificateList nss_certs,
    std::vector<scoped_refptr<const Cert>> kcer_certs,
    base::flat_map<Token, Error> kcer_errors) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!kcer_errors.empty()) {
    return RecordUmaEvent(KcerPkcs12MigrationEvent::kFailedToGetKcerCerts);
  }

  kcer::internal::CertCache kcer_cert_cache(std::move(kcer_certs));
  net::ScopedCERTCertificateList nss_certs_to_migrate;

  for (net::ScopedCERTCertificate& nss_cert : nss_certs) {
    const base::span<const uint8_t> cert_span(
        net::x509_util::CERTCertificateAsSpan(nss_cert.get()));
    if (!kcer_cert_cache.FindCert(cert_span)) {
      nss_certs_to_migrate.push_back(std::move(nss_cert));
    }
  }

  if (nss_certs_to_migrate.empty()) {
    return RecordUmaEvent(KcerPkcs12MigrationEvent::kkNothingToMigrate);
  }

  MigrateEachCert(std::move(nss_certs_to_migrate));
}

// This method is called repeatedly until `certs` is empty.
void Pkcs12Migrator::MigrateEachCert(net::ScopedCERTCertificateList certs) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (certs.empty()) {
    if (had_failures_) {
      RecordUmaEvent(KcerPkcs12MigrationEvent::kMigrationFinishedFailure);
    } else {
      RecordUmaEvent(KcerPkcs12MigrationEvent::kMigrationFinishedSuccess);
    }
    return;
  }

  net::ScopedCERTCertificate cur_cert = std::move(certs.back());
  certs.pop_back();

  auto callback = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindOnce(&Pkcs12Migrator::ExportedOneCert,
                     weak_factory_.GetWeakPtr(), std::move(certs)));

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ExportCertOnWorkerThread, std::move(cur_cert),
                     std::move(callback)));
}

void Pkcs12Migrator::ExportedOneCert(net::ScopedCERTCertificateList certs,
                                     Pkcs12Blob pkcs12) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pkcs12->empty()) {
    had_failures_ = true;
    RecordUmaEvent(KcerPkcs12MigrationEvent::kExportedPkcs12EmptyError);
    return MigrateEachCert(std::move(certs));
  }

  base::WeakPtr<Kcer> kcer =
      KcerFactoryAsh::GetKcer(Profile::FromBrowserContext(context_));

  auto callback = base::BindOnce(&Pkcs12Migrator::ImportedOneCert,
                                 weak_factory_.GetWeakPtr(), std::move(certs));
  // Set the flag that some certs now exist in both NSS public slot and Chaps.
  // It might be needed for the rollback.
  kcer::KcerFactoryAsh::RecordPkcs12CertDualWritten();
  kcer->ImportPkcs12Cert(Token::kUser, std::move(pkcs12),
                         /*password=*/std::string(),
                         /*hardware_backed=*/false, /*mark_as_migrated=*/true,
                         std::move(callback));
}

void Pkcs12Migrator::ImportedOneCert(net::ScopedCERTCertificateList certs,
                                     base::expected<void, Error> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!result.has_value()) {
    had_failures_ = true;
    RecordUmaEvent(KcerPkcs12MigrationEvent::kFailedToReimportCert);
  } else {
    RecordUmaEvent(KcerPkcs12MigrationEvent::kCertMigratedSuccess);
  }
  MigrateEachCert(std::move(certs));
}

}  // namespace kcer
