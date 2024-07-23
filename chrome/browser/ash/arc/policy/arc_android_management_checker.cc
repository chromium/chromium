// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"

namespace arc {

namespace {

constexpr base::TimeDelta kRetryDelayMin = base::Seconds(10);
constexpr base::TimeDelta kRetryDelayMax = base::Hours(1);

}  // namespace

ArcAndroidManagementChecker::ArcAndroidManagementChecker(
    Profile* profile,
    signin::IdentityManager* identity_manager,
    const CoreAccountId& device_account_id,
    bool retry_on_error,
    std::unique_ptr<policy::AndroidManagementClient> android_management_client)
    : profile_(profile),
      identity_manager_(identity_manager),
      device_account_id_(device_account_id),
      retry_on_error_(retry_on_error),
      retry_delay_(kRetryDelayMin),
      android_management_client_(std::move(android_management_client)) {}

ArcAndroidManagementChecker::~ArcAndroidManagementChecker() {
  identity_manager_->RemoveObserver(this);
}

void ArcAndroidManagementChecker::StartCheck(CheckCallback callback) {
  DCHECK(callback_.is_null());

  // No need to check Android Management if the user is a Chrome OS managed
  // user, or belongs to a well-known non-enterprise domain.
  if (policy_util::IsAccountManaged(profile_) ||
      !signin::AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
          profile_->GetProfileUserName())) {
    std::move(callback).Run(CheckResult::ALLOWED);
    return;
  }

  callback_ = std::move(callback);
  EnsureRefreshTokenLoaded();
}

void ArcAndroidManagementChecker::EnsureRefreshTokenLoaded() {
  if (identity_manager_->HasAccountWithRefreshToken(device_account_id_)) {
    // If the refresh token is already available, just start the management
    // check immediately.
    StartCheckInternal();
    return;
  }

  // Set the observer to the token service so the callback will be called
  // when the token is loaded.
  identity_manager_->AddObserver(this);
}

void ArcAndroidManagementChecker::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  if (account_info.account_id != device_account_id_)
    return;
  OnRefreshTokensLoaded();
}

void ArcAndroidManagementChecker::OnRefreshTokensLoaded() {
  identity_manager_->RemoveObserver(this);
  StartCheckInternal();
}

void ArcAndroidManagementChecker::StartCheckInternal() {
  DCHECK(!callback_.is_null());

  if (!identity_manager_->HasAccountWithRefreshToken(device_account_id_)) {
    LOG(ERROR) << "No refresh token is available for android management check.";
    std::move(callback_).Run(CheckResult::ERROR);
    return;
  }

  VLOG(2) << "Start android management check.";
  android_management_client_->StartCheckAndroidManagement(
      base::BindOnce(&ArcAndroidManagementChecker::OnAndroidManagementChecked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcAndroidManagementChecker::OnAndroidManagementChecked(
    policy::AndroidManagementClient::Result management_result) {
  DCHECK(!callback_.is_null());
  VLOG(2) << "Android management check done " << management_result << ".";
  if (retry_on_error_ &&
      management_result == policy::AndroidManagementClient::Result::ERROR) {
    ScheduleRetry();
    return;
  }

  CheckResult check_result = CheckResult::ERROR;
  switch (management_result) {
    case policy::AndroidManagementClient::Result::MANAGED:
      check_result = CheckResult::DISALLOWED;
      break;
    case policy::AndroidManagementClient::Result::UNMANAGED:
      check_result = CheckResult::ALLOWED;
      break;
    case policy::AndroidManagementClient::Result::ERROR:
      check_result = CheckResult::ERROR;
      break;
  }
  std::move(callback_).Run(check_result);
}

void ArcAndroidManagementChecker::ScheduleRetry() {
  DCHECK(retry_on_error_);
  DCHECK(!callback_.is_null());
  VLOG(2) << "Schedule next android management check in " << retry_delay_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ArcAndroidManagementChecker::StartCheckInternal,
                     weak_ptr_factory_.GetWeakPtr()),
      retry_delay_);
  retry_delay_ = std::min(retry_delay_ * 2, kRetryDelayMax);
}

}  // namespace arc
