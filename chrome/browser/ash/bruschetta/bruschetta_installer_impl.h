// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_

#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chromeos/ash/components/dbus/concierge/concierge_service.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "components/download/public/background_service/download_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace bruschetta {

class BruschettaInstallerImpl : public BruschettaInstaller {
 public:
  BruschettaInstallerImpl(Profile* profile, base::OnceClosure close_callback);

  BruschettaInstallerImpl(const BruschettaInstallerImpl&) = delete;
  BruschettaInstallerImpl& operator=(const BruschettaInstallerImpl&) = delete;
  ~BruschettaInstallerImpl() override;

  void Cancel() override;
  void Install(std::string vm_name, std::string config_id) override;

  const base::GUID& GetDownloadGuid() const override;

  void DownloadStarted(const std::string& guid,
                       download::DownloadParams::StartResult result) override;
  void DownloadFailed() override;
  void DownloadSucceeded(
      const download::CompletionInfo& completion_info) override;

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

 private:
  using DownloadCallback =
      base::OnceCallback<void(const download::CompletionInfo&)>;

  bool MaybeClose();

  void StartDownload(GURL url, DownloadCallback callback);

  void InstallToolsDlc();
  void OnToolsDlcInstalled(
      const ash::DlcserviceClient::InstallResult& install_result);
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

  base::raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<BruschettaInstallerImpl> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
