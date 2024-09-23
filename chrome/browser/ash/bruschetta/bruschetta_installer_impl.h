// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/bruschetta/bruschetta_download.h"
#include "chrome/browser/ash/bruschetta/bruschetta_installer.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/download/public/background_service/download_metadata.h"
#include "url/gurl.h"

class Profile;

namespace bruschetta {
class BruschettaDownload;

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

  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void SetDownloadFactoryForTesting(
      base::RepeatingCallback<std::unique_ptr<BruschettaDownload>(void)>
          callback) {
    download_factory_ = std::move(callback);
  }

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
  void DownloadBootDisk();
  void OnBootDiskDownloaded(base::FilePath path, std::string hash);
  void DownloadPflash();
  void OnPflashDownloaded(base::FilePath path, std::string hash);
  void OpenFds();
  void OnOpenFds(std::unique_ptr<Fds> fds);
  void EnsureConciergeAvailable();
  void OnConciergeAvailable(bool service_is_available);
  void CreateVmDisk();
  void OnCreateVmDisk(
      std::optional<vm_tools::concierge::CreateDiskImageResponse> result);
  void InstallPflash();
  void OnInstallPflash(
      std::optional<vm_tools::concierge::InstallPflashResponse> result);
  void ClearVek();
  void OnClearVek(const attestation::DeleteKeysReply& result);
  void StartVm();
  void OnStartVm(RunningVmPolicy launch_policy,
                 std::optional<vm_tools::concierge::StartVmResponse> result);
  void LaunchTerminal();

  void NotifyObserver(State state);
  void Error(BruschettaInstallResult error);

  bool install_running_ = false;

  std::string vm_name_;
  std::string config_id_;
  base::Value::Dict config_;

  base::FilePath boot_disk_path_;
  base::FilePath pflash_path_;
  std::string disk_path_;
  std::unique_ptr<Fds> fds_;

  std::unique_ptr<guest_os::GuestOsDlcInstallation> in_progress_dlc_;

  const raw_ptr<Profile> profile_;

  // The downloaded files get deleted once these go out of scope.
  std::unique_ptr<BruschettaDownload> boot_disk_download_;
  std::unique_ptr<BruschettaDownload> pflash_download_;
  base::RepeatingCallback<std::unique_ptr<BruschettaDownload>(void)>
      download_factory_ = base::BindRepeating([]() {
        std::unique_ptr<BruschettaDownload> d =
            std::make_unique<SimpleURLLoaderDownload>();
        return d;
      });

  base::OnceClosure close_closure_;

  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<BruschettaInstallerImpl> weak_ptr_factory_{this};
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_INSTALLER_IMPL_H_
