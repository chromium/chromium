// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_vm_data_migration_necessity_checker.h"

#include <string>
#include <vector>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_client_adapter.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"
#include "components/account_id/account_id.h"

namespace arc {

namespace {

std::string GetChromeOsUser(Profile* profile) {
  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile));
  return cryptohome::CreateAccountIdentifierFromAccountId(account).account_id();
}

}  // namespace

ArcVmDataMigrationNecessityChecker::ArcVmDataMigrationNecessityChecker(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

ArcVmDataMigrationNecessityChecker::~ArcVmDataMigrationNecessityChecker() =
    default;

void ArcVmDataMigrationNecessityChecker::Check(CheckCallback callback) {
  DCHECK(base::FeatureList::IsEnabled(kEnableArcVmDataMigration));
  if (base::FeatureList::IsEnabled(kEnableVirtioBlkForData)) {
    // No migration needs to be performed if virtio-blk /data is forciblly
    // enabled via a feature.
    std::move(callback).Run(false);
    return;
  }

  if (GetArcVmDataMigrationStatus(profile_->GetPrefs()) ==
      ArcVmDataMigrationStatus::kFinished) {
    // No migration needs to be performed if it's already finished.
    std::move(callback).Run(false);
    return;
  }

  std::vector<std::string> environment = {"CHROMEOS_USER=" +
                                          GetChromeOsUser(profile_)};
  std::deque<JobDesc> jobs{JobDesc{kArcVmDataMigratorJobName,
                                   UpstartOperation::JOB_STOP_AND_START,
                                   std::move(environment)}};
  ConfigureUpstartJobs(
      std::move(jobs),
      base::BindOnce(
          &ArcVmDataMigrationNecessityChecker::OnArcVmDataMigratorStarted,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcVmDataMigrationNecessityChecker::OnArcVmDataMigratorStarted(
    CheckCallback callback,
    bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to start arcvm-data-migrator";
    std::move(callback).Run(std::nullopt);
    return;
  }

  DCHECK(ash::ArcVmDataMigratorClient::Get());
  data_migrator::HasDataToMigrateRequest request;
  request.set_username(GetChromeOsUser(profile_));
  ash::ArcVmDataMigratorClient::Get()->HasDataToMigrate(
      request,
      base::BindOnce(
          &ArcVmDataMigrationNecessityChecker::OnHasDataToMigrateResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcVmDataMigrationNecessityChecker::OnHasDataToMigrateResponse(
    CheckCallback callback,
    std::optional<bool> response) {
  if (!response.has_value()) {
    LOG(ERROR) << "Failed to check whether /data has any content: "
               << "No valid D-Bus response";
  }

  std::move(callback).Run(response);
}

}  // namespace arc
