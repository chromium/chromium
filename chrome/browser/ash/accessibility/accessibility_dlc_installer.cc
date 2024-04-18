// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_dlc_installer.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kFaceGazeAssetsInstallDurationMetric[] =
    "Accessibility.DlcInstallerFaceGazeAssetsInstallationDuration";

constexpr char kFaceGazeAssetsInstallationMetric[] =
    "Accessibility.DlcInstallerFaceGazeAssetsSuccess";

constexpr char kPumpkinInstallationMetric[] =
    "PumpkinInstaller.InstallationSuccess";

constexpr char kPumpkinInstallDurationMetric[] =
    "Accessibility.DlcInstallerPumpkinInstallationDuration";
}  // namespace

namespace ash {

AccessibilityDlcInstaller::Callbacks::Callbacks(InstalledCallback on_installed,
                                                ProgressCallback on_progress,
                                                ErrorCallback on_error) {
  on_installed_ = std::move(on_installed);
  on_progress_ = std::move(on_progress);
  on_error_ = std::move(on_error);
}

AccessibilityDlcInstaller::Callbacks::~Callbacks() = default;

void AccessibilityDlcInstaller::Callbacks::RunOnInstalled(
    const bool success,
    std::string root_path) {
  DCHECK(!on_installed_.is_null());
  std::move(on_installed_).Run(success, root_path);
}

void AccessibilityDlcInstaller::Callbacks::RunOnProgress(double progress) {
  DCHECK(!on_progress_.is_null());
  on_progress_.Run(progress);
}

void AccessibilityDlcInstaller::Callbacks::RunOnError(std::string_view error) {
  DCHECK(!on_error_.is_null());
  std::move(on_error_).Run(error);
}

AccessibilityDlcInstaller::AccessibilityDlcInstaller() = default;
AccessibilityDlcInstaller::~AccessibilityDlcInstaller() = default;

void AccessibilityDlcInstaller::MaybeInstall(DlcType type,
                                             InstalledCallback on_installed,
                                             ProgressCallback on_progress,
                                             ErrorCallback on_error) {
  if (pending_requests_[type]) {
    std::move(on_error).Run(GetPendingDlcRequestErrorMessage(type));
    return;
  }

  callbacks_.insert_or_assign(
      type,
      std::make_unique<Callbacks>(std::move(on_installed),
                                  std::move(on_progress), std::move(on_error)));
  pending_requests_.insert_or_assign(type, true);
  DlcserviceClient::Get()->GetDlcState(
      GetDlcName(type),
      base::BindOnce(&AccessibilityDlcInstaller::MaybeInstallHelper,
                     GetWeakPtr(), type));
}

void AccessibilityDlcInstaller::MaybeInstallHelper(
    DlcType type,
    std::string_view error,
    const dlcservice::DlcState& dlc_state) {
  pending_requests_.insert_or_assign(type, false);
  if (error != dlcservice::kErrorNone) {
    if (GetCallbacks(type)) {
      GetCallbacks(type)->RunOnError(error);
    }
    return;
  }

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLING:
      if (GetCallbacks(type)) {
        GetCallbacks(type)->RunOnError(GetDlcInstallingErrorMessage(type));
      }
      return;
    case dlcservice::DlcState_State_INSTALLED:
      installed_dlcs_.insert(type);
      if (GetCallbacks(type)) {
        GetCallbacks(type)->RunOnInstalled(true, dlc_state.root_path());
      }
      return;
    default:
      break;
  }

  // Install DLC.
  pending_requests_.insert_or_assign(type, true);
  dlcservice::InstallRequest install_request;
  install_request.set_id(GetDlcName(type));
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&AccessibilityDlcInstaller::OnInstalled, GetWeakPtr(),
                     type, base::Time::Now()),
      base::BindRepeating(&AccessibilityDlcInstaller::OnProgress, GetWeakPtr(),
                          type));
}

void AccessibilityDlcInstaller::OnInstalled(
    DlcType type,
    const base::Time start_time,
    const DlcserviceClient::InstallResult& install_result) {
  pending_requests_.insert_or_assign(type, false);

  // Record success metric.
  switch (type) {
    case DlcType::kFaceGazeAssets:
      base::UmaHistogramBoolean(kFaceGazeAssetsInstallationMetric,
                                install_result.error == dlcservice::kErrorNone);
      break;
    case DlcType::kPumpkin:
      base::UmaHistogramBoolean(kPumpkinInstallationMetric,
                                install_result.error == dlcservice::kErrorNone);
      break;
  }

  if (install_result.error != dlcservice::kErrorNone) {
    if (GetCallbacks(type)) {
      GetCallbacks(type)->RunOnError(install_result.error);
    }
    return;
  }

  // Record install duration metric.
  const base::TimeDelta install_duration = base::Time::Now() - start_time;
  switch (type) {
    case DlcType::kFaceGazeAssets:
      base::UmaHistogramTimes(kFaceGazeAssetsInstallDurationMetric,
                              install_duration);
      break;
    case DlcType::kPumpkin:
      base::UmaHistogramTimes(kPumpkinInstallDurationMetric, install_duration);
      break;
  }

  installed_dlcs_.insert(type);
  if (GetCallbacks(type)) {
    GetCallbacks(type)->RunOnInstalled(true, install_result.root_path);
  }
}

void AccessibilityDlcInstaller::OnProgress(DlcType type, double progress) {
  if (GetCallbacks(type)) {
    GetCallbacks(type)->RunOnProgress(progress);
  }
}

AccessibilityDlcInstaller::Callbacks* AccessibilityDlcInstaller::GetCallbacks(
    DlcType type) {
  if (!callbacks_[type]) {
    return nullptr;
  }

  return callbacks_[type].get();
}

std::string AccessibilityDlcInstaller::GetDlcName(DlcType type) {
  switch (type) {
    case DlcType::kFaceGazeAssets:
      return "facegaze-assets";
    case DlcType::kPumpkin:
      return "pumpkin";
  }
}

std::string AccessibilityDlcInstaller::GetDlcInstallingErrorMessage(
    DlcType type) {
  return base::StringPrintf("%s already installing.", GetDlcName(type).c_str());
}

std::string AccessibilityDlcInstaller::GetPendingDlcRequestErrorMessage(
    DlcType type) {
  return base::StringPrintf("Cannot install %s, DLC request in progress.",
                            GetDlcName(type).c_str());
}

bool AccessibilityDlcInstaller::IsFaceGazeAssetsInstalled() const {
  return base::Contains(installed_dlcs_, DlcType::kFaceGazeAssets);
}

bool AccessibilityDlcInstaller::IsPumpkinInstalled() const {
  return base::Contains(installed_dlcs_, DlcType::kPumpkin);
}

base::WeakPtr<AccessibilityDlcInstaller>
AccessibilityDlcInstaller::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
