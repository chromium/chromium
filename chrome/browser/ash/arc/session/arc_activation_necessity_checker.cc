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

  // Always activate ARC for unmanaged users who will get some applications
  // installed by Play Auto Install (PAI).
  if (!policy_util::IsAccountManaged(profile_)) {
    OnChecked(std::move(callback), true);
    return;
  }

  // Activate ARC if the package list held by ArcAppListPrefs is not up to date.
  if (!profile_->GetPrefs()->GetBoolean(arc::prefs::kArcPackagesIsUpToDate)) {
    OnChecked(std::move(callback), true);
    return;
  }

  // Activate ARC if there is an app which was not pre-installed.
  ArcAppListPrefs* app_list = ArcAppListPrefs::Get(profile_);
  DCHECK(app_list);
  auto package_names = app_list->GetPackagesFromPrefs();
  for (const auto& package_name : package_names) {
    auto package = app_list->GetPackage(package_name);
    if (package && !package->preinstalled) {
      OnChecked(std::move(callback), true);
      return;
    }
  }

  // If ADB sideloading is enabled, activate ARC. Otherwise, no need to
  // activate.
  adb_sideloading_availability_delegate_->CanChangeAdbSideloading(
      base::BindOnce(&ArcActivationNecessityChecker::OnChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcActivationNecessityChecker::OnChecked(CheckCallback callback,
                                              bool result) {
  base::UmaHistogramBoolean("Arc.DelayedActivation.ActivationShouldBeDelayed",
                            !result);
  // If the ARC on Demand feature is disabled, always activate ARC.
  if (!base::FeatureList::IsEnabled(kArcOnDemandFeature)) {
    result = true;
  }
  std::move(callback).Run(result);
}

}  // namespace arc
