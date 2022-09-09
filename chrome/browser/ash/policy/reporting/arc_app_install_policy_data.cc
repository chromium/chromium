// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_policy_data.h"

#include <mutex>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace {

// Timeout to record metrics even if all apps are not installed yet.
constexpr base::TimeDelta kTimeout = base::Minutes(30);

std::mutex data_mutex;

}  // namespace

namespace policy {

ArcAppInstallPolicyData::ArcAppInstallPolicyData(
    base::TimeTicks time_created,
    const std::set<std::string>& requested_apps_pending,
    std::int64_t success_count) {
  std::int64_t num_total_apps = success_count + requested_apps_pending.size();
  DCHECK(num_total_apps != 0);  // prevent divide by 0
  time_created_ = time_created;
  requested_apps_pending_ = requested_apps_pending;
  success_count_ = success_count;
  requested_apps_total_ = num_total_apps;
  is_success_rate_recorded_ = false;
}

ArcAppInstallPolicyData::~ArcAppInstallPolicyData() = default;

ArcAppInstallPolicyData::ArcAppInstallPolicyData(
    const ArcAppInstallPolicyData& other) {
  time_created_ = other.time_created_;
  requested_apps_pending_ = other.requested_apps_pending_;
  success_count_ = other.success_count_;
  requested_apps_total_ = other.requested_apps_total_;
  is_success_rate_recorded_ = other.is_success_rate_recorded_;
}

bool ArcAppInstallPolicyData::operator<(
    ArcAppInstallPolicyData const& other) const {
  return time_created_ < other.time_created_;
}

bool ArcAppInstallPolicyData::UpdatePackageInstallResult(
    const std::string& package,
    bool success) const {
  if (requested_apps_pending_.erase(package) == 1 && success)
    ++success_count_;

  return MaybeRecordSuccessRate();
}

bool ArcAppInstallPolicyData::MaybeRecordSuccessRate() const {
  const std::lock_guard<std::mutex> lock(data_mutex);
  if (requested_apps_pending_.empty() ||
      base::TimeTicks::Now() - time_created_ >= kTimeout) {
    if (!is_success_rate_recorded_) {
      std::int64_t rate = (double)success_count_ / requested_apps_total_ * 100;
      base::UmaHistogramPercentage("Arc.AppInstall.PolicySuccessRate", rate);
      is_success_rate_recorded_ = true;
    }
    return true;
  }
  return false;
}

}  // namespace policy
