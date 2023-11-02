// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_policy_data_helper.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"

namespace policy {

ArcAppInstallPolicyDataHelper::ArcAppInstallPolicyDataHelper() = default;

ArcAppInstallPolicyDataHelper::~ArcAppInstallPolicyDataHelper() = default;

void ArcAppInstallPolicyDataHelper::AddPolicyData(
    const std::set<std::string>& current_pending,
    std::int64_t num_apps_previously_installed) {
  if (!current_pending.empty()) {
    ArcAppInstallPolicyData data(base::TimeTicks::Now(), current_pending,
                                 num_apps_previously_installed);
    policy_data_.insert(data);
    if (!policy_data_timer_.IsRunning()) {
      policy_data_timer_.Start(
          FROM_HERE, base::Minutes(1), this,
          &ArcAppInstallPolicyDataHelper::CheckForPolicyDataTimeout);
    }
  }
}

void ArcAppInstallPolicyDataHelper::UpdatePolicySuccessRate(
    const std::string& package,
    bool success) {
  std::set<ArcAppInstallPolicyData>::iterator it;
  for (it = policy_data_.begin(); it != policy_data_.end();) {
    bool isPolicyTrackingComplete =
        it->UpdatePackageInstallResult(package, success);
    if (isPolicyTrackingComplete) {
      it = policy_data_.erase(it);
    } else {
      it++;
    }
  }
}

void ArcAppInstallPolicyDataHelper::UpdatePolicySuccessRateForPackages(
    const std::set<std::string>& packages,
    bool success) {
  for (std::string package : packages) {
    UpdatePolicySuccessRate(package, success);
  }
}

void ArcAppInstallPolicyDataHelper::CheckForPolicyDataTimeout() {
  std::set<ArcAppInstallPolicyData>::iterator it;
  for (it = policy_data_.begin(); it != policy_data_.end();) {
    bool isPolicyTimedOut = it->MaybeRecordSuccessRate();
    if (isPolicyTimedOut) {
      it = policy_data_.erase(it);
    } else {
      it++;
    }
  }

  if (policy_data_.empty())
    policy_data_timer_.Stop();
}

}  // namespace policy
