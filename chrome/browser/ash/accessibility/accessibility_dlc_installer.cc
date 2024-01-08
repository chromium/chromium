// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_dlc_installer.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "ui/accessibility/accessibility_features.h"

namespace {
constexpr char kPumpkinInstallationMetricName[] =
    "PumpkinInstaller.InstallationSuccess";
}  // namespace

namespace ash {

AccessibilityDlcInstaller::AccessibilityDlcInstaller() = default;
AccessibilityDlcInstaller::~AccessibilityDlcInstaller() = default;

void AccessibilityDlcInstaller::MaybeInstall(DlcType type,
                                             InstalledCallback on_installed,
                                             ProgressCallback on_progress,
                                             ErrorCallback on_error) {
  if (pending_dlc_request_) {
    std::move(on_error).Run(GetPendingDlcRequestErrorMessage(type));
    return;
  }

  on_installed_ = std::move(on_installed);
  on_progress_ = std::move(on_progress);
  on_error_ = std::move(on_error);

  pending_dlc_request_ = true;
  DlcserviceClient::Get()->GetDlcState(
      GetDlcName(type),
      base::BindOnce(&AccessibilityDlcInstaller::MaybeInstallHelper,
                     GetWeakPtr(), type));
}

void AccessibilityDlcInstaller::MaybeInstallHelper(
    DlcType type,
    const std::string& error,
    const dlcservice::DlcState& dlc_state) {
  pending_dlc_request_ = false;
  if (error != dlcservice::kErrorNone) {
    OnError(error);
    return;
  }

  switch (dlc_state.state()) {
    case dlcservice::DlcState_State_INSTALLING:
      OnError(GetDlcInstallingErrorMessage(type));
      return;
    case dlcservice::DlcState_State_INSTALLED:
      installed_dlcs_.insert(type);
      CHECK(!on_installed_.is_null());
      std::move(on_installed_).Run(true, dlc_state.root_path());
      return;
    default:
      break;
  }

  // Install DLC.
  pending_dlc_request_ = true;
  dlcservice::InstallRequest install_request;
  install_request.set_id(GetDlcName(type));
  DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&AccessibilityDlcInstaller::OnInstalled, GetWeakPtr(),
                     type),
      base::BindRepeating(&AccessibilityDlcInstaller::OnProgress,
                          GetWeakPtr()));
}

void AccessibilityDlcInstaller::OnInstalled(
    DlcType type,
    const DlcserviceClient::InstallResult& install_result) {
  pending_dlc_request_ = false;
  if (type == DlcType::kPumpkin) {
    base::UmaHistogramBoolean(kPumpkinInstallationMetricName,
                              install_result.error == dlcservice::kErrorNone);
  }

  if (install_result.error != dlcservice::kErrorNone) {
    OnError(install_result.error);
    return;
  }

  installed_dlcs_.insert(type);
  CHECK(!on_installed_.is_null());
  std::move(on_installed_).Run(true, install_result.root_path);
}

void AccessibilityDlcInstaller::OnProgress(double progress) {
  on_progress_.Run(progress);
}

void AccessibilityDlcInstaller::OnError(const std::string& error) {
  CHECK(!on_error_.is_null());
  std::move(on_error_).Run(error);
}

std::string AccessibilityDlcInstaller::GetDlcName(DlcType type) {
  switch (type) {
    case DlcType::kPumpkin:
      return "pumpkin";
  }
}

std::string AccessibilityDlcInstaller::GetDlcInstallingErrorMessage(
    DlcType type) {
  switch (type) {
    case DlcType::kPumpkin:
      return "Pumpkin already installing.";
  }
}

std::string AccessibilityDlcInstaller::GetPendingDlcRequestErrorMessage(
    DlcType type) {
  switch (type) {
    case DlcType::kPumpkin:
      return "Cannot install Pumpkin, DLC request in progress.";
  }
}

bool AccessibilityDlcInstaller::IsPumpkinInstalled() const {
  return base::Contains(installed_dlcs_, DlcType::kPumpkin);
}

base::WeakPtr<AccessibilityDlcInstaller>
AccessibilityDlcInstaller::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
