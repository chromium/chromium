// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/adb_sideloading_availability_delegate.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace arc {

ArcActivationNecessityChecker::ArcActivationNecessityChecker(
    Profile* profile,
    AdbSideloadingAvailabilityDelegate* adb_sideloading_availability_delegate)
    : profile_(profile),
      adb_sideloading_availability_delegate_(
          adb_sideloading_availability_delegate) {}

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

  // If ADB sideloading is enabled, activate ARC. Otherwise, no need to
  // activate.
  adb_sideloading_availability_delegate_->CanChangeAdbSideloading(
      base::BindOnce(&ArcActivationNecessityChecker::OnChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcActivationNecessityChecker::OnChecked(CheckCallback callback,
                                              bool result) {
  // Activate ARC if the user has launched any apps.
  ArcAppListPrefs* app_list = ArcAppListPrefs::Get(profile_);
  DCHECK(app_list);
  bool is_app_installed = false;
  bool is_app_launched = false;
  const auto app_ids = app_list->GetAppIds();
  for (const auto& app_id : app_ids) {
    const auto package_name = app_list->GetAppPackageName(app_id);
    const auto package = app_list->GetPackage(package_name);
    if (package && !package->preinstalled) {
      is_app_installed = true;
      const auto app = app_list->GetApp(app_id);
      if (app->last_launch_time.ToDeltaSinceWindowsEpoch().InMicroseconds() >
          0) {
        // Launch time is stored as time since the Windows epoch
        // A value of greater than 0 means that the app has been launched.
        is_app_launched = true;
        break;
      }
    }
  }
  base::UmaHistogramBoolean("Arc.ArcOnDemandV2.ActivationShouldBeDelayed",
                            !result && !is_app_launched);

  if (!base::FeatureList::IsEnabled(kArcOnDemandV2)) {
    // For V1, activate ARC if the user is an unmanaged user or if any app is
    // installed.
    result |= !policy_util::IsAccountManaged(profile_) || is_app_installed;
  } else {
    // For V2, activate ARC if the user has launched apps before.
    result |= is_app_launched;
  }
  std::move(callback).Run(result);
}

}  // namespace arc
