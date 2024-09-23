// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_policy_observer.h"

#include <optional>

#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

namespace ash {

ChildPolicyObserver::ChildPolicyObserver(Profile* profile) : profile_(profile) {
  policy::CloudPolicyService* cloud_policy_service =
      GetUserCloudPolicyManager()->core()->service();
  std::optional<bool> initial_policy_refresh_result =
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
    std::move(on_policy_ready).Run(profile_.get(), refresh_result_);
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
    std::move(on_policy_ready_).Run(profile_.get(), refresh_result_);
}

policy::UserCloudPolicyManagerAsh*
ChildPolicyObserver::GetUserCloudPolicyManager() {
  policy::UserCloudPolicyManagerAsh* user_cloud_policy_manager =
      profile_->GetUserCloudPolicyManagerAsh();
  DCHECK(user_cloud_policy_manager);
  return user_cloud_policy_manager;
}

std::string_view ChildPolicyObserver::name() const {
  return "ChildPolicyObserver";
}

}  // namespace ash
