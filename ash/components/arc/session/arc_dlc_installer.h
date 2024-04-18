// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_ARC_DLC_INSTALLER_H_
#define ASH_COMPONENTS_ARC_SESSION_ARC_DLC_INSTALLER_H_

#include <string_view>

#include "base/barrier_closure.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace arc {

// Name of the Houdini (Android R) DLC.
// Defined in this file so that unit test can also access it.
constexpr char kHoudiniRvcDlc[] = "houdini-rvc-dlc";

// ArcDlcInstaller is responsible for installing and uninstalling ARC DLCs
// by using the ash::DlcserviceClient. The caller can also wait
// for the installation to complete by supplying a callback function.
//
// Semantics:
// 1. ARC DLCs installation/uninstallation can only be initiated by
// external callers using RequestEnable() and RequestDisable().
// RequestEnable(): Will trigger ARC DLCs installation immediately
// if the current state is kUninstalled.
// RequestDisable(): Will trigger ARC DLCs uninstallation immediately
// if the current state is kInstalled.
// 2. When the current state is kInstalling or kUninstalling in (1),
// it will not trigger any additional actions in parallel. Instead, it
// will just flip the is_dlc_enabled_ flag and wait for the end state
// to be reached (kInstalled/kUninstalled) first before determining the
// next action. This handles the edge case when RequestEnable() and
// RequestDisable() are being called repeatedly.
// 3. After reaching the end state (kInstalled/kUninstalled),
// is_dlc_enabled_ will be checked again to determine whether ARC DLCs
// needs to be installed/uninstalled again.
// 4. After reaching the end state (kInstalled/kUninstalled),
// InvokeCallbacks() will be called to unblock any waiters. Waiters are
// always called in the sequence they are added.
class ArcDlcInstaller {
 public:
  ArcDlcInstaller();
  ArcDlcInstaller(const ArcDlcInstaller&) = delete;
  ArcDlcInstaller& operator=(const ArcDlcInstaller&) = delete;
  ~ArcDlcInstaller();

  // Retrieves the global pointer to ArcDlcInstaller instance.
  static ArcDlcInstaller* Get();
  // Enables ARC DLCs. Will trigger ARC DLCs installation if current state is
  // kUninstalled.
  void RequestEnable();
  // Disables ARC DLCs. Will trigger ARC DLCs uninstallation if current state is
  // kInstalled.
  void RequestDisable();
  // Allows the caller to wait for the installation to complete by
  // supplying a callback function.
  void WaitForStableState(base::OnceClosure callback);

  enum class InstallerState {
    // Currently installing ARC DLCs.
    kInstalling,
    // ARC DLC installation has completed.
    kInstalled,
    // Currently uninstalling ARC DLCs.
    kUninstalling,
    // ARC DLC uninstallation has completed.
    kUninstalled,
  };

  // Getters and setters for unit testing.
  InstallerState GetStateForTesting() { return state_; }
  void SetStateForTesting(InstallerState s) { state_ = s; }
  bool GetIsDlcEnabledForTesting() { return is_dlc_enabled_; }

 private:
  // Installs all the ARC DLCs defined in this class. Currently
  // kHoudiniRvcDlc only.
  void Install();

  // Callback function when all the ARC DLC has completed
  // installation.
  void OnDlcInstalled(
      std::string_view dlc,
      const ash::DlcserviceClient::InstallResult& install_result);

  // Uninstalls all the installed ARC DLCs.
  void Uninstall();

  // Callback function when all the ARC DLC has completed
  // uninstallation.
  void OnDlcUninstalled(std::string_view dlc, std::string_view err);

  // Invokes and clears the list of callbacks in callback_list_.
  void InvokeCallbacks();

  // Indicates if ARC DLCs are enabled.
  bool is_dlc_enabled_ = false;

  // Stores the current installation / uninstallation state.
  InstallerState state_ = InstallerState::kUninstalled;

  // Stores the list of callbacks passed to WaitForStableState().
  std::vector<base::OnceClosure> callback_list_;

  base::WeakPtrFactory<ArcDlcInstaller> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_ARC_DLC_INSTALLER_H_
