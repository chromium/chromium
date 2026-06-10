// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/byte_size.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/experiences/arc/arc_util.h"
#include "chromeos/ash/experiences/arc/session/arc_management_transition.h"
#include "components/prefs/pref_service.h"

namespace arc {

ArcActivationNecessityChecker::ArcActivationNecessityChecker(Profile* profile)
    : profile_(profile) {}

ArcActivationNecessityChecker::~ArcActivationNecessityChecker() = default;

void ArcActivationNecessityChecker::Check(CheckCallback callback) {
  // If ARC is running in a container, it's already started on the login screen
  // as a mini instance. In that case, just activate it.
  if (!IsArcVmEnabled()) {
    OnChecked(std::move(callback), true);
    return;
  }

  // Activate ARC if the package list held by ArcAppListPrefs is not up to date.
  if (!profile_->GetPrefs()->GetBoolean(arc::prefs::kArcPackagesIsUpToDate)) {
    OnChecked(std::move(callback), true);
    return;
  }

  const ArcManagementTransition management_transition =
      static_cast<ArcManagementTransition>(
          profile_->GetPrefs()->GetInteger(prefs::kArcManagementTransition));
  // Activate ARC if a management transitioning is happening.
  if (management_transition != ArcManagementTransition::NO_TRANSITION) {
    OnChecked(std::move(callback), true);
    return;
  }

  // Activate ARC if Always ON VPN is enabled.
  if (!profile_->GetPrefs()->GetString(prefs::kAlwaysOnVpnPackage).empty()) {
    OnChecked(std::move(callback), true);
    return;
  }

  // Activate ARC if Coral feature is enabled, since it depends on the on-device
  // safety service which is powered inside arc.
  if (ash::features::IsCoralFeatureEnabled()) {
    OnChecked(std::move(callback), true);
    return;
  }

  // If ADB sideloading is enabled, activate ARC. Otherwise, no need to
  // activate.
  ash::SessionManagerClient* client = ash::SessionManagerClient::Get();
  if (!client) {
    OnChecked(std::move(callback), false);
    return;
  }

  client->QueryAdbSideload(
      base::BindOnce(&ArcActivationNecessityChecker::OnQueryAdbSideload,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcActivationNecessityChecker::OnQueryAdbSideload(
    CheckCallback callback,
    ash::SessionManagerClient::AdbSideloadResponseCode response_code,
    bool is_allowed) {
  if (response_code !=
      ash::SessionManagerClient::AdbSideloadResponseCode::SUCCESS) {
    LOG(ERROR) << "Failed to query ADB sideload status";
    is_allowed = false;
  }
  OnChecked(std::move(callback), is_allowed);
}

void ArcActivationNecessityChecker::OnChecked(CheckCallback callback,
                                              bool result) {
  // Check if the user installed any apps and the last launch time if any.
  ArcAppListPrefs* app_list = ArcAppListPrefs::Get(profile_);
  DCHECK(app_list);
  std::optional<base::Time> last_launch;
  bool is_app_installed = false;
  const auto app_ids = app_list->GetAppIds();
  for (const auto& app_id : app_ids) {
    const auto package_name = app_list->GetAppPackageName(app_id);
    const auto package = app_list->GetPackage(package_name);
    if (package && !package->preinstalled) {
      is_app_installed = true;
      const auto app = app_list->GetApp(app_id);
      // Launch time is stored as time since the Windows epoch. A value of
      // greater than 0 means that the app has been launched.
      if (app->last_launch_time.ToDeltaSinceWindowsEpoch().InMicroseconds() >
          0) {
        if (!last_launch.has_value() ||
            app->last_launch_time > last_launch.value()) {
          last_launch = app->last_launch_time;
        }
      }
    }
  }

  // If delay_on_app_launch is true, activate ARC if the user has launched any
  // app. Othsewise, activate ARC if the user has launched any apps within
  // inactive_interval.
  bool is_app_recently_launched = false;
  if (kArcOnDemandActivateOnAppLaunch.Get()) {
    is_app_recently_launched = last_launch.has_value();
  } else if (last_launch.has_value()) {
    base::ByteSize ram_size = base::SysInfo::AmountOfTotalPhysicalMemory();
    bool is_4gb_device = ram_size < base::GiBU(4.5);

    base::TimeDelta inactive_interval =
        is_4gb_device ? kArcOnDemandInactiveIntervalFor4GiB.Get()
                      : kArcOnDemandInactiveInterval.Get();
    is_app_recently_launched =
        (base::Time::Now() - last_launch.value()) < inactive_interval;
  }
  base::UmaHistogramBoolean("Arc.ArcOnDemandV2.ActivationShouldBeDelayed",
                            !result && !is_app_recently_launched);

  if (!base::FeatureList::IsEnabled(kArcOnDemandV2)) {
    // For V1, activate ARC if the user is an unmanaged user or if any app is
    // installed.
    result |= !policy_util::IsAccountManaged(profile_) || is_app_installed;
  } else {
    // For V2, activate ARC if the user has launched apps before.
    result |= is_app_recently_launched;
  }
  std::move(callback).Run(result);
}

}  // namespace arc
