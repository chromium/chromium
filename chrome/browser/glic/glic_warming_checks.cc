// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_warming_checks.h"

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace glic {
namespace {

bool g_prewarming_enabled_for_testing = true;
std::optional<net::NetworkChangeNotifier::ConnectionType>
    g_forced_connection_type;

// Synchronously checks profile lifecycle, readiness, policy, and UI prefs.
GlicPrewarmingChecksResult CheckProfileEligibility(Profile* profile) {
  if (!profile || IsProfileDirectoryMarkedForDeletion(profile->GetPath())) {
    return GlicPrewarmingChecksResult::kProfileGone;
  }
  if (profile->ShutdownStarted()) {
    return GlicPrewarmingChecksResult::kBrowserShuttingDown;
  }

  switch (GlicEnabling::GetProfileReadyState(profile)) {
    case mojom::ProfileReadyState::kReady:
      break;
    case mojom::ProfileReadyState::kUnknownError:
      return GlicPrewarmingChecksResult::kProfileNotReadyUnknown;
    case mojom::ProfileReadyState::kSignInRequired:
      return GlicPrewarmingChecksResult::kProfileRequiresSignIn;
    case mojom::ProfileReadyState::kIneligible:
      return GlicPrewarmingChecksResult::kProfileNotEligible;
    case mojom::ProfileReadyState::kDisabledByAdmin:
      return GlicPrewarmingChecksResult::kProfileDisallowedByAdmin;
    case mojom::ProfileReadyState::kLocationMismatch:
      return GlicPrewarmingChecksResult::kProfileNotEligibleLocationMismatch;
    case mojom::ProfileReadyState::kIneligibleAccount:
      return GlicPrewarmingChecksResult::kProfileNotEligibleAccountCapabilities;
  }

  if (!profile->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip)) {
    return GlicPrewarmingChecksResult::kNotPinnedToTabstrip;
  }

  return GlicPrewarmingChecksResult::kSuccess;
}

// Synchronously checks hardware RAM and test override.
GlicPrewarmingChecksResult CheckDeviceConstraints() {
  if (base::SysInfo::AmountOfTotalPhysicalMemory() <
      base::MiB(features::kGlicWarmingMinRequiredRamMb.Get())) {
    return GlicPrewarmingChecksResult::kDeviceLowMemory;
  }
  if (!g_prewarming_enabled_for_testing) {
    return GlicPrewarmingChecksResult::kPrewarmingDisabledForTesting;
  }
  return GlicPrewarmingChecksResult::kSuccess;
}

void PostResult(ShouldPreloadCallback callback,
                GlicPrewarmingChecksResult result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

// Asynchronously checks network connection (avoids prewarming over cellular).
void CheckNetworkAndComplete(ShouldPreloadCallback callback) {
  auto on_got_connection_type =
      [](ShouldPreloadCallback callback,
         net::NetworkChangeNotifier::ConnectionType type) {
        std::move(callback).Run(
            network::NetworkConnectionTracker::IsConnectionCellular(type)
                ? GlicPrewarmingChecksResult::kCellularConnection
                : GlicPrewarmingChecksResult::kSuccess);
      };

  net::NetworkChangeNotifier::ConnectionType connection_type;
  if (g_forced_connection_type) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(on_got_connection_type, std::move(callback),
                                  *g_forced_connection_type));
    return;
  }

  auto callbacks = base::SplitOnceCallback(std::move(callback));
  if (content::GetNetworkConnectionTracker()->GetConnectionType(
          &connection_type,
          base::BindOnce(on_got_connection_type, std::move(callbacks.first)))) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(on_got_connection_type, std::move(callbacks.second),
                       connection_type));
  }
}

}  // namespace

void SetPrewarmingEnabledForTesting(bool enabled) {
  g_prewarming_enabled_for_testing = enabled;
}

void ForceConnectionTypeForTesting(
    std::optional<net::NetworkChangeNotifier::ConnectionType> connection_type) {
  g_forced_connection_type = connection_type;
}

void ShouldPreloadForProfile(Profile* profile, ShouldPreloadCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kGlicWarming)) {
    PostResult(std::move(callback),
               GlicPrewarmingChecksResult::kWarmingDisabled);
    return;
  }

  // 1. Profile eligibility check.
  if (auto result = CheckProfileEligibility(profile);
      result != GlicPrewarmingChecksResult::kSuccess) {
    PostResult(std::move(callback), result);
    return;
  }

  // 2. Hardware / device constraint check.
  if (auto result = CheckDeviceConstraints();
      result != GlicPrewarmingChecksResult::kSuccess) {
    PostResult(std::move(callback), result);
    return;
  }

  // 3. Network connection check.
  CheckNetworkAndComplete(std::move(callback));
}

}  // namespace glic
