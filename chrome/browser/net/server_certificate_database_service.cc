// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/server_certificate_database_service.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

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
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::GetAllCertificates(
    base::OnceCallback<
        void(std::vector<net::ServerCertificateDatabase::CertInformation>)>
        callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveAllCertificates)
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::PostTaskWithDatabase(
    base::OnceCallback<void(net::ServerCertificateDatabase*)> callback) {
  server_cert_database_.PostTaskWithThisObject(std::move(callback));
}

}  // namespace net
