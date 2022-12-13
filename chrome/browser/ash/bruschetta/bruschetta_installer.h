// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_

#include "base/check_is_test.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/download/public/background_service/download_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace bruschetta {

class BruschettaInstaller {
 public:
  BruschettaInstaller(Profile* profile, base::OnceClosure close_callback);

  BruschettaInstaller(const BruschettaInstaller&) = delete;
  BruschettaInstaller& operator=(const BruschettaInstaller&) = delete;
  ~BruschettaInstaller();

  void Cancel();
  void Install(std::string vm_name, std::string config_id);

  const base::GUID& GetDownloadGuid() const;

  void DownloadStarted(const std::string& guid,
                       download::DownloadParams::StartResult result);
  void DownloadFailed();
  void DownloadSucceeded(const download::CompletionInfo& completion_info);

  enum class State {
    kInstallStarted,
    kDlcInstall,
    kFirmwareDownload,
    kFirmwareMount,
    kBootDiskDownload,
    kBootDiskMount,
    kOpenFiles,
    kCreateVmDisk,
    kStartVm,
    kLaunchTerminal,
  };

  class TestingObserver {
   public:
    virtual void StateChanged(State state) = 0;
    virtual void Error() = 0;
  };

  void set_observer_for_testing(TestingObserver* observer) {
    CHECK_IS_TEST();
    observer_ = observer;
  }

 private:
  using DownloadCallback =
      base::OnceCallback<void(const download::CompletionInfo&)>;

  bool MaybeClose();

  void StartDownload(GURL url, DownloadCallback callback);

  void InstallToolsDlc();
  void OnToolsDlcInstalled(
      const chromeos::DlcserviceClient::InstallResult& install_result);
  void DownloadFirmware();
  void OnFirmwareDownloaded(const download::CompletionInfo& completion_info);
  void MountFirmware(const base::FilePath& path);
  void OnFirmwareMounted(ash::MountError error_code,
                         const ash::MountPoint& mount_info);
  void DownloadBootDisk();
  void OnBootDiskDownloaded(const download::CompletionInfo& completion_info);
  void MountBootDisk(const base::FilePath& path);
  void OnBootDiskMounted(ash::MountError error_code,
                         const ash::MountPoint& mount_info);
  void OpenFds();
  void OnOpenFds(absl::optional<std::pair<base::ScopedFD, base::ScopedFD>> fds);
  void CreateVmDisk();
  void OnCreateVmDisk(
      absl::optional<vm_tools::concierge::CreateDiskImageResponse> result);
  void StartVm();
  void OnStartVm(absl::optional<vm_tools::concierge::StartVmResponse> result);
  void LaunchTerminal();

  void NotifyObserver(State state);
  void NotifyObserverError();

  bool install_running_ = false;

  std::string vm_name_;
  std::string config_id_;
  base::Value::Dict config_;

  base::GUID download_guid_;
  DownloadCallback download_callback_;

  std::string firmware_mount_path_;
  std::string boot_disk_mount_path_;
  base::ScopedFD firmware_fd_;
  base::ScopedFD boot_disk_fd_;
  std::string disk_path_;

  const base::raw_ptr<Profile> profile_;

  base::OnceClosure close_closure_;

  base::raw_ptr<TestingObserver> observer_ = nullptr;

  base::WeakPtrFactory<BruschettaInstaller> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_H_
