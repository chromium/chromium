// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_
#define CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_

#include <memory>
#include <set>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace base {
class Time;
}  // namespace base

namespace ash {

// This class encapsulates all logic involving the installation of accessibility
// DLCs. It communicates install success, progress, and error using callbacks.
class AccessibilityDlcInstaller {
  using InstalledCallback =
      base::OnceCallback<void(const bool success,
                              const std::string& root_path)>;
  using ProgressCallback = base::RepeatingCallback<void(double progress)>;
  using ErrorCallback = base::OnceCallback<void(std::string_view error)>;

 public:
  enum class DlcType { kFaceGazeAssets, kPumpkin };

  class Callbacks {
   public:
    Callbacks(InstalledCallback on_installed,
              ProgressCallback on_progress,
              ErrorCallback on_error);
    ~Callbacks();
    Callbacks(const Callbacks&) = delete;
    Callbacks& operator=(const Callbacks&) = delete;

    void RunOnInstalled(const bool success, std::string root_path);
    void RunOnProgress(double progress);
    void RunOnError(std::string_view error);

   private:
    // A callback that is run when a DLC is installed.
    InstalledCallback on_installed_;
    // A callback that is run when DLC download progress is updated.
    ProgressCallback on_progress_;
    // A callback that is run when a DLC download encounters an error.
    ErrorCallback on_error_;
  };

  AccessibilityDlcInstaller();
  ~AccessibilityDlcInstaller();
  AccessibilityDlcInstaller(const AccessibilityDlcInstaller&) = delete;
  AccessibilityDlcInstaller& operator=(const AccessibilityDlcInstaller&) =
      delete;

  // Installs a DLC if it isn't already downloaded.
  void MaybeInstall(DlcType type,
                    InstalledCallback on_installed,
                    ProgressCallback on_progress,
                    ErrorCallback on_error);

  bool IsFaceGazeAssetsInstalled() const;
  bool IsPumpkinInstalled() const;

 private:
  // A helper function that is run once we've grabbed the state of a DLC from
  // the DLC service.
  void MaybeInstallHelper(DlcType type,
                          std::string_view error,
                          const dlcservice::DlcState& dlc_state);
  void OnInstalled(DlcType type,
                   const base::Time start_time,
                   const DlcserviceClient::InstallResult& install_result);
  void OnProgress(DlcType type, double progress);

  Callbacks* GetCallbacks(DlcType type);

  std::string GetDlcName(DlcType type);
  std::string GetDlcInstallingErrorMessage(DlcType type);
  std::string GetPendingDlcRequestErrorMessage(DlcType type);

  base::WeakPtr<AccessibilityDlcInstaller> GetWeakPtr();

  std::set<DlcType> installed_dlcs_;
  base::flat_map<DlcType, std::unique_ptr<Callbacks>> callbacks_;
  // Requests to DlcserviceClient are async. This is true if we've made a
  // request and are still waiting for a response.
  base::flat_map<DlcType, bool> pending_requests_;

  base::WeakPtrFactory<AccessibilityDlcInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCESSIBILITY_ACCESSIBILITY_DLC_INSTALLER_H_
