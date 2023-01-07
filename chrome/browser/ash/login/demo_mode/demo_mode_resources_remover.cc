// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_resources_remover.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/idle_detector.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash {
namespace {

DemoModeResourcesRemover* g_instance = nullptr;

// Name of the pref in local state that indicates whether demo mode resources
// have been removed from the device.
constexpr char kDemoModeResourcesRemoved[] = "demo_mode_resources_removed";

// Key for the pref in local state that tracks accumulated device usage time in
// seconds.
constexpr char kAccumulatedUsagePref[] =
    "demo_mode_resources_remover.accumulated_device_usage_s";

// Regex matching legacy demo retail mode domains.
constexpr char kLegacyDemoRetailModeDomainRegex[] =
    "[[:alpha:]]{2}-retailmode.com";

// An extra legacy demo retail mode domain that does not match
// `kLegacyDemoRetailModeDomainRegex`.
constexpr char kExtraLegacyDemoRetailModeDomain[] = "us2-retailmode.com";

// Deletes directory at `path` from the device.
DemoModeResourcesRemover::RemovalResult RemoveDirectory(
    const base::FilePath& path) {
  if (!base::DirectoryExists(path) || base::IsDirectoryEmpty(path))
    return DemoModeResourcesRemover::RemovalResult::kNotFound;

  if (!base::DeletePathRecursively(path))
    return DemoModeResourcesRemover::RemovalResult::kFailed;

  return DemoModeResourcesRemover::RemovalResult::kSuccess;
}

// Tests whether the session with user `user` is part of legacy demo mode -
// a public session in a legacy demo retail mode domain.
// Note that DemoSession::IsDeviceInDemoMode will return false for these
// sessions.
bool IsLegacyDemoRetailModeSession(const user_manager::User* user) {
  if (user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return false;

  const std::string enrollment_domain = g_browser_process->platform_part()
                                            ->browser_policy_connector_ash()
                                            ->GetEnterpriseEnrollmentDomain();
  return DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
      enrollment_domain);
}

}  // namespace

DemoModeResourcesRemover::UsageAccumulationConfig::UsageAccumulationConfig()
    : resources_removal_threshold(base::Hours(48)),
      update_interval(base::Minutes(5)),
      idle_threshold(base::Seconds(30)) {}

DemoModeResourcesRemover::UsageAccumulationConfig::UsageAccumulationConfig(
    const base::TimeDelta& resources_removal_threshold,
    const base::TimeDelta& update_interval,
    const base::TimeDelta& idle_threshold)
    : resources_removal_threshold(resources_removal_threshold),
      update_interval(update_interval),
      idle_threshold(idle_threshold) {}

// static
void DemoModeResourcesRemover::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kDemoModeResourcesRemoved, false);
  registry->RegisterIntegerPref(kAccumulatedUsagePref, 0);
}

// static
std::unique_ptr<DemoModeResourcesRemover>
DemoModeResourcesRemover::CreateIfNeeded(PrefService* local_state) {
  if (DemoSession::IsDeviceInDemoMode() ||
      local_state->GetBoolean(kDemoModeResourcesRemoved)) {
    return nullptr;
  }

  return base::WrapUnique(new DemoModeResourcesRemover(local_state));
}

// static
DemoModeResourcesRemover* DemoModeResourcesRemover::Get() {
  return g_instance;
}

// static
bool DemoModeResourcesRemover::IsLegacyDemoRetailModeDomain(
    const std::string& domain) {
  return RE2::FullMatch(domain, kLegacyDemoRetailModeDomainRegex) ||
         domain == kExtraLegacyDemoRetailModeDomain;
}

DemoModeResourcesRemover::~DemoModeResourcesRemover() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;

  if (usage_start_.has_value() && usage_end_.has_value())
    UpdateDeviceUsage(*usage_end_ - *usage_start_);

  ChromeUserManager::Get()->RemoveSessionStateObserver(this);
}

void DemoModeResourcesRemover::LowDiskSpace(
    const ::user_data_auth::LowDiskSpace& status) {
  AttemptRemoval(RemovalReason::kLowDiskSpace, RemovalCallback());
}

void DemoModeResourcesRemover::ActiveUserChanged(user_manager::User* user) {
  // Ignore user activity in guest sessions.
  if (user->GetType() == user_manager::USER_TYPE_GUEST)
    return;

  // Attempt resources removal if the device is managed, and not in a retail
  // mode domain.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    if (!IsLegacyDemoRetailModeSession(user))
      AttemptRemoval(RemovalReason::kEnterpriseEnrolled, RemovalCallback());
    return;
  }

  // Start tracking user activity, if it's already not in progress.
  if (!user_activity_observation_.IsObservingSource(
          ui::UserActivityDetector::Get())) {
    if (!AttemptRemovalIfUsageOverThreshold()) {
      user_activity_observation_.Observe(ui::UserActivityDetector::Get());
      OnUserActivity(nullptr);
    }
  }
}

void DemoModeResourcesRemover::OnUserActivity(const ui::Event* event) {
  base::TimeTicks now = tick_clock_->NowTicks();

  if (!usage_start_.has_value()) {
    usage_start_ = now;
    usage_end_ = now;
    return;
  }

  bool was_idle =
      usage_end_.has_value() &&
      (now - *usage_end_) > usage_accumulation_config_.idle_threshold;
  base::TimeTicks interval_end = was_idle ? *usage_end_ : now;
  base::TimeDelta duration = interval_end - *usage_start_;

  // If enough time has passed, or the current usage interval was interrupted by
  // idle period, record the usage.
  if (was_idle || duration >= usage_accumulation_config_.update_interval) {
    UpdateDeviceUsage(duration);

    // Attempt to remove resources will stop observing user activity, so no need
    // to start the next usage interval.
    if (AttemptRemovalIfUsageOverThreshold())
      return;

    // Start tracking the next active usage interval.
    usage_start_ = now;
  }

  usage_end_ = now;
}

void DemoModeResourcesRemover::AttemptRemoval(RemovalReason reason,
                                              RemovalCallback callback) {
  if (local_state_->GetBoolean(kDemoModeResourcesRemoved)) {
    if (callback)
      std::move(callback).Run(RemovalResult::kAlreadyRemoved);
    return;
  }

  if (DemoSession::IsDeviceInDemoMode()) {
    if (callback)
      std::move(callback).Run(RemovalResult::kNotAllowed);
    return;
  }

  if (callback)
    removal_callbacks_.push_back(std::move(callback));

  if (removal_in_progress_)
    return;
  removal_in_progress_ = true;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RemoveDirectory, DemoComponents::GetPreInstalledPath()),
      base::BindOnce(&DemoModeResourcesRemover::OnRemovalDone,
                     weak_ptr_factory_.GetWeakPtr(), reason));
}

void DemoModeResourcesRemover::OverrideTimeForTesting(
    base::TickClock* tick_clock,
    const UsageAccumulationConfig& config) {
  tick_clock_ = tick_clock;

  usage_start_ = absl::nullopt;
  usage_end_ = absl::nullopt;

  usage_accumulation_config_ = config;
}

DemoModeResourcesRemover::DemoModeResourcesRemover(PrefService* local_state)
    : local_state_(local_state),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  CHECK(!g_instance);
  g_instance = this;

  userdataauth_observation_.Observe(UserDataAuthClient::Get());
  ChromeUserManager::Get()->AddSessionStateObserver(this);
}

void DemoModeResourcesRemover::UpdateDeviceUsage(
    const base::TimeDelta& duration) {
  int accumulated_activity = local_state_->GetInteger(kAccumulatedUsagePref);
  int64_t removal_threshold_s =
      usage_accumulation_config_.resources_removal_threshold.InSeconds();
  if (accumulated_activity < removal_threshold_s)
    accumulated_activity += std::min(duration.InSeconds(), removal_threshold_s);

  local_state_->SetInteger(kAccumulatedUsagePref, accumulated_activity);

  usage_start_ = absl::nullopt;
  usage_end_ = absl::nullopt;
}

bool DemoModeResourcesRemover::AttemptRemovalIfUsageOverThreshold() {
  int accumulated_activity = local_state_->GetInteger(kAccumulatedUsagePref);
  int64_t removal_threshold_s =
      usage_accumulation_config_.resources_removal_threshold.InSeconds();

  if (accumulated_activity < removal_threshold_s)
    return false;

  // Stop observing usage.
  user_activity_observation_.Reset();
  AttemptRemoval(RemovalReason::kRegularUsage, RemovalCallback());
  return true;
}

void DemoModeResourcesRemover::OnRemovalDone(RemovalReason reason,
                                             RemovalResult result) {
  DCHECK(removal_in_progress_);
  removal_in_progress_ = false;

  if (result == RemovalResult::kNotFound || result == RemovalResult::kSuccess) {
    local_state_->SetBoolean(kDemoModeResourcesRemoved, true);
    local_state_->ClearPref(kAccumulatedUsagePref);

    userdataauth_observation_.Reset();
    ChromeUserManager::Get()->RemoveSessionStateObserver(this);

    user_activity_observation_.Reset();
    usage_start_ = absl::nullopt;
    usage_end_ = absl::nullopt;
  }

  // Only report metrics when the resources were found; otherwise this is
  // reported on almost every sign-in.
  // Only report metrics once per resources directory removal task.
  // Concurrent removal requests should not be reported multiple times.
  if (result == RemovalResult::kSuccess || result == RemovalResult::kFailed) {
    UMA_HISTOGRAM_ENUMERATION("DemoMode.ResourcesRemoval.Reason", reason);
    UMA_HISTOGRAM_ENUMERATION("DemoMode.ResourcesRemoval.Result", result);
  }

  std::vector<RemovalCallback> callbacks;
  callbacks.swap(removal_callbacks_);

  for (auto& callback : callbacks)
    std::move(callback).Run(result);
}

}  // namespace ash
