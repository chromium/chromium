// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_dlc_installer.h"

#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"

namespace arc {

namespace {
// Global pointer to ArcDlcInstaller instance. The lifetime of this
// object is managed by ArcSessionManager.
ArcDlcInstaller* g_instance = nullptr;
}  // namespace

// Public functions.

// Static function.
ArcDlcInstaller* ArcDlcInstaller::Get() {
  return g_instance;
}

void ArcDlcInstaller::RequestEnable() {
  is_dlc_enabled_ = true;
  if (state_ == InstallerState::kUninstalled)
    Install();
}

void ArcDlcInstaller::RequestDisable() {
  is_dlc_enabled_ = false;
  if (state_ == InstallerState::kInstalled)
    Uninstall();
}

void ArcDlcInstaller::WaitForStableState(base::OnceClosure callback) {
  if (callback)
    callback_list_.emplace_back(std::move(callback));

  if (state_ == InstallerState::kUninstalled ||
      state_ == InstallerState::kInstalled)
    InvokeCallbacks();
}

// Private functions.

ArcDlcInstaller::ArcDlcInstaller() {
  DCHECK(!g_instance);
  g_instance = this;
}

ArcDlcInstaller::~ArcDlcInstaller() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void ArcDlcInstaller::Install() {
  // Cannot start ARC DLC installation if state_ is not kUninstalled.
  DCHECK(state_ == InstallerState::kUninstalled);

  state_ = InstallerState::kInstalling;
  VLOG(2) << "Installing ARC DLC: " << kHoudiniRvcDlc;
  dlcservice::InstallRequest install_request;
  install_request.set_id(kHoudiniRvcDlc);
  ash::DlcserviceClient::Get()->Install(
      install_request,
      base::BindOnce(&ArcDlcInstaller::OnDlcInstalled,
                     weak_ptr_factory_.GetWeakPtr(), kHoudiniRvcDlc),
      base::DoNothing());
}

void ArcDlcInstaller::OnDlcInstalled(
    std::string_view dlc,
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error == dlcservice::kErrorNone) {
    VLOG(1) << dlc << " is installed successfully.";
  } else if (install_result.error == dlcservice::kErrorInvalidDlc) {
    LOG(ERROR) << dlc << " fails to install. This ARC DLC is invalid.";
  } else if (install_result.error == dlcservice::kErrorNeedReboot) {
    LOG(ERROR) << dlc
               << " fails to install. Device has pending update "
                  "and needs a reboot first.";
  } else if (install_result.error == dlcservice::kErrorAllocation) {
    LOG(ERROR) << dlc << " fails to install. Device needs to free up space.";
  } else if (install_result.error == dlcservice::kErrorNoImageFound) {
    LOG(ERROR) << dlc
               << " fails to install. Omaha cannot provide an image, "
                  "device may need to be updated.";
  } else if (install_result.error == dlcservice::kErrorInternal) {
    LOG(ERROR) << dlc << " fails to install. Internal error in dlcservice.";
  } else if (install_result.error == dlcservice::kErrorBusy) {
    LOG(ERROR) << dlc << " fails to install. Dlcservice is busy.";
  } else {
    LOG(ERROR) << dlc << " fails to install. Received an error: "
               << install_result.error;
  }

  if (install_result.error == dlcservice::kErrorNone) {
    state_ = InstallerState::kInstalled;
  } else {
    state_ = InstallerState::kUninstalled;
    LOG(ERROR) << dlc << " will be installed again on next boot.";
  }

  if (!is_dlc_enabled_ && state_ == InstallerState::kInstalled) {
    Uninstall();
  } else {
    InvokeCallbacks();
  }
}

void ArcDlcInstaller::Uninstall() {
  // Cannot start ARC DLC uninstallation if state_ is not kInstalled.
  DCHECK(state_ == InstallerState::kInstalled);

  state_ = InstallerState::kUninstalling;
  VLOG(2) << "Uninstalling ARC DLC: " << kHoudiniRvcDlc;
  ash::DlcserviceClient::Get()->Uninstall(
      kHoudiniRvcDlc,
      base::BindOnce(&ArcDlcInstaller::OnDlcUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), kHoudiniRvcDlc));
}

void ArcDlcInstaller::OnDlcUninstalled(std::string_view dlc,
                                       std::string_view err) {
  if (err == dlcservice::kErrorNone) {
    VLOG(1) << dlc << " is uninstalled successfully.";
    state_ = InstallerState::kUninstalled;
    if (is_dlc_enabled_) {
      Install();
    } else {
      InvokeCallbacks();
    }
  } else {
    LOG(ERROR) << "Failed to uninstall ARC DLC " << dlc << ": " << err;
    // Keeps state as kInstalled because ARC DLCs cannot be uninstalled.
    state_ = InstallerState::kInstalled;
    InvokeCallbacks();
  }
}

void ArcDlcInstaller::InvokeCallbacks() {
  if (callback_list_.empty())
    return;

  std::vector<base::OnceClosure> callback_list;
  callback_list.swap(callback_list_);
  for (auto& callback : callback_list)
    std::move(callback).Run();
}

}  // namespace arc
