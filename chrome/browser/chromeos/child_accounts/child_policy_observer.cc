// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_policy_observer.h"

#include "base/optional.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

namespace chromeos {

ChildPolicyObserver::ChildPolicyObserver(Profile* profile) : profile_(profile) {
  policy::CloudPolicyService* cloud_policy_service =
      GetUserCloudPolicyManager()->core()->service();
  base::Optional<bool> initial_policy_refresh_result =
      cloud_policy_service->initial_policy_refresh_result();
  if (initial_policy_refresh_result) {
    OnPolicyReady(*initial_policy_refresh_result
                      ? InitialPolicyRefreshResult::kPolicyRefreshed
                      : InitialPolicyRefreshResult::kPolicyRefreshError);
  }
  cloud_policy_service->AddObserver(this);
}

ChildPolicyObserver::~ChildPolicyObserver() {
  GetUserCloudPolicyManager()->core()->service()->RemoveObserver(this);
}

void ChildPolicyObserver::NotifyWhenPolicyReady(
    PolicyReadyCallback on_policy_ready,
    base::TimeDelta timeout) {
  DCHECK(!on_policy_ready_);

  if (IsChildPolicyReady()) {
    std::move(on_policy_ready).Run(profile_, refresh_result_);
    return;
  }

  on_policy_ready_ = std::move(on_policy_ready);
  refresh_timeout_timer_ = std::make_unique<base::OneShotTimer>();
  refresh_timeout_timer_->Start(
      FROM_HERE, timeout,
      base::BindOnce(&ChildPolicyObserver::OnPolicyReady,
                     base::Unretained(this),
                     InitialPolicyRefreshResult::kPolicyRefreshTimeout));
}

void ChildPolicyObserver::OnCloudPolicyServiceInitializationCompleted() {}

void ChildPolicyObserver::OnPolicyRefreshed(bool success) {
  OnPolicyReady(success ? InitialPolicyRefreshResult::kPolicyRefreshed
                        : InitialPolicyRefreshResult::kPolicyRefreshError);
}

bool ChildPolicyObserver::IsChildPolicyReady() const {
  return refresh_result_ != InitialPolicyRefreshResult::kUnknown;
}

void ChildPolicyObserver::OnPolicyReady(
    InitialPolicyRefreshResult refresh_result) {
  DCHECK_NE(InitialPolicyRefreshResult::kUnknown, refresh_result);

  refresh_timeout_timer_.reset();

  if (refresh_result_ == InitialPolicyRefreshResult::kUnknown)
    refresh_result_ = refresh_result;

  if (on_policy_ready_)
    std::move(on_policy_ready_).Run(profile_, refresh_result_);
}

policy::UserCloudPolicyManagerChromeOS*
ChildPolicyObserver::GetUserCloudPolicyManager() {
  policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile_->GetUserCloudPolicyManagerChromeOS();
  DCHECK(user_cloud_policy_manager);
  return user_cloud_policy_manager;
}

}  // namespace chromeos
