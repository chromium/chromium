// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/net/server_certificate_database_nss_migrator.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

namespace net {

ServerCertificateDatabaseService::ServerCertificateDatabaseService(
    Profile* profile)
    : profile_(profile) {
  if (base::FeatureList::IsEnabled(
          ::features::kEnableCertManagementUIV2Write)) {
    server_cert_database_ = base::SequenceBound<net::ServerCertificateDatabase>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
        profile->GetPath());
  }
}

ServerCertificateDatabaseService::~ServerCertificateDatabaseService() = default;

void ServerCertificateDatabaseService::AddOrUpdateUserCertificate(
    net::ServerCertificateDatabase::CertInformation cert_info,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::InsertOrUpdateCert)
      .WithArgs(std::move(cert_info))
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServerCertificateDatabaseService::GetAllCertificates(
    base::OnceCallback<
        void(std::vector<net::ServerCertificateDatabase::CertInformation>)>
        callback) {
#if BUILDFLAG(IS_CHROMEOS)
  // Migrate certificates from NSS and then read all certificates from the
  // database. Migration will only be done once per profile. If called multiple
  // times before migration completes, all the callbacks will be queued and
  // processed once the migration is done.
  if (profile_->GetPrefs()->GetInteger(
          prefs::kNSSCertsMigratedToServerCertDb) ==
      static_cast<int>(NSSMigrationResultPref::kNotMigrated)) {
    if (!nss_migrator_) {
      DVLOG(1) << "starting migration for profile "
               << profile_->GetPath().AsUTF8Unsafe();
      nss_migrator_ =
          std::make_unique<ServerCertificateDatabaseNSSMigrator>(profile_);
      // Unretained is safe as ServerCertificateDatabaseNSSMigrator will not
      // run the callback after it is deleted.
      nss_migrator_->MigrateCerts(base::BindOnce(
          &ServerCertificateDatabaseService::NSSMigrationComplete,
          base::Unretained(this)));
    }
    DVLOG(1) << "queuing migration request";
    get_certificates_pending_migration_.push_back(std::move(callback));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveAllCertificates)
      .Then(std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS)
void ServerCertificateDatabaseService::NSSMigrationComplete(
    ServerCertificateDatabaseNSSMigrator::MigrationResult result) {
  DVLOG(1) << "Migration for " << profile_->GetPath().AsUTF8Unsafe()
           << " finished: nss cert count=" << result.cert_count
           << " errors=" << result.error_count;
  NSSMigrationResultHistogram result_for_histogram;
  if (result.cert_count == 0) {
    result_for_histogram = NSSMigrationResultHistogram::kNssDbEmpty;
  } else if (result.error_count == 0) {
    result_for_histogram = NSSMigrationResultHistogram::kSuccess;
  } else if (result.error_count < result.cert_count) {
    result_for_histogram = NSSMigrationResultHistogram::kPartialSuccess;
  } else {
    result_for_histogram = NSSMigrationResultHistogram::kFailed;
  }
  base::UmaHistogramEnumeration("Net.CertVerifier.NSSCertMigrationResult",
                                result_for_histogram);
  base::UmaHistogramCounts100(
      "Net.CertVerifier.NSSCertMigrationQueuedRequestsWhenFinished",
      get_certificates_pending_migration_.size());

  profile_->GetPrefs()->SetInteger(
      prefs::kNSSCertsMigratedToServerCertDb,
      static_cast<int>((result.error_count == 0)
                           ? NSSMigrationResultPref::kMigratedSuccessfully
                           : NSSMigrationResultPref::kMigrationHadErrors));
  for (GetCertificatesCallback& callback :
       get_certificates_pending_migration_) {
    // TODO(https://crbug.com/40928765): kinda silly to start multiple
    // simultaneous reads here, but dunno if it actually occurs enough to be
    // worth optimizing. Evaluate the histograms to see if this seems worth
    // addressing.
    GetAllCertificates(std::move(callback));
  }
  get_certificates_pending_migration_.clear();
  nss_migrator_.reset();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ServerCertificateDatabaseService::PostTaskWithDatabase(
    base::OnceCallback<void(net::ServerCertificateDatabase*)> callback) {
  server_cert_database_.PostTaskWithThisObject(std::move(callback));
}

void ServerCertificateDatabaseService::GetCertificatesCount(
    base::OnceCallback<void(uint32_t)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveCertificatesCount)
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::DeleteCertificate(
    const std::string& sha256hash_hex,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::DeleteCertificate)
      .WithArgs(sha256hash_hex)
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

base::CallbackListSubscription ServerCertificateDatabaseService::AddObserver(
    base::RepeatingClosure callback) {
  return observers_.Add(std::move(callback));
}

void ServerCertificateDatabaseService::HandleModificationResult(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  std::move(callback).Run(success);
  if (success) {
    observers_.Notify();
  }
}

}  // namespace net
