// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/download/public/background_service/download_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

class Profile;

namespace bruschetta {
class SimpleURLLoaderDownload;

class BruschettaInstallerImpl : public BruschettaInstaller {
 public:
  // Public for a free function in the .cc file, not actually part of the public
  // interface.
  struct Fds;
  BruschettaInstallerImpl(Profile* profile, base::OnceClosure close_callback);

  BruschettaInstallerImpl(const BruschettaInstallerImpl&) = delete;
  BruschettaInstallerImpl& operator=(const BruschettaInstallerImpl&) = delete;
  ~BruschettaInstallerImpl() override;

  void Cancel() override;
  void Install(std::string vm_name, std::string config_id) override;

  const base::Uuid& GetDownloadGuid() const override;

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
      guest_os::GuestOsDlcInstallation::Result install_result);
  void InstallFirmwareDlc();
  void OnFirmwareDlcInstalled(
      guest_os::GuestOsDlcInstallation::Result install_result);
  // TODO(b/270656010): Pick the winner between the two strategies. Loser gets
  // deleted, winner gets renamed back to "DownloadBootDisk" and etc.
  void DownloadBootDiskDownloadService();
  void OnBootDiskDownloadedDownloadService(
      const download::CompletionInfo& completion_info);
  void DownloadPflashDownloadService();
  void OnPflashDownloadedDownloadService(
      const download::CompletionInfo& completion_info);
  void DownloadBootDiskURLLoader();
  void OnBootDiskDownloadedURLLoader(base::FilePath path, std::string hash);
  void DownloadPflashURLLoader();
  void OnPflashDownloadedURLLoader(base::FilePath path, std::string hash);
  void OpenFds();
  void OnOpenFds(std::unique_ptr<Fds> fds);
  void CreateVmDisk();
  void OnCreateVmDisk(
      absl::optional<vm_tools::concierge::CreateDiskImageResponse> result);
  void InstallPflash();
  void OnInstallPflash(
      absl::optional<vm_tools::concierge::InstallPflashResponse> result);
  void StartVm();
  void OnStartVm(RunningVmPolicy launch_policy,
                 absl::optional<vm_tools::concierge::StartVmResponse> result);
  void LaunchTerminal();

  void NotifyObserver(State state);
  void Error(BruschettaInstallResult error);

  bool install_running_ = false;

  std::string vm_name_;
  std::string config_id_;
  base::Value::Dict config_;

  base::Uuid download_guid_;
  DownloadCallback download_callback_;

  base::FilePath boot_disk_path_;
  base::FilePath pflash_path_;
  std::string disk_path_;
  std::unique_ptr<Fds> fds_;

  std::unique_ptr<guest_os::GuestOsDlcInstallation> in_progress_dlc_;

  const raw_ptr<Profile> profile_;

  // The downloaded files get deleted once these go out of scope.
  std::unique_ptr<SimpleURLLoaderDownload> boot_disk_download_;
  std::unique_ptr<SimpleURLLoaderDownload> pflash_download_;

  base::OnceClosure close_closure_;

  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<BruschettaInstallerImpl> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
