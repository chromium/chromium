// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_
#define CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/crostini/baguette_download.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

class PrefService;
class Profile;

// TODO(crbug.com/377377749): add downloader which grabs image file from GS
// bucket based on VERSION-PIN
inline constexpr char kBaguettePath[] =
    "/home/chronos/user/MyFiles/Downloads/baguette.img.zst";

namespace crostini {

// This class is responsible for managing (un)instatllation of Baguette - the
// containerless Crostini VM.
class BaguetteInstaller {
 public:
  BaguetteInstaller(Profile* profile, PrefService& local_state);
  ~BaguetteInstaller();

  BaguetteInstaller(const BaguetteInstaller&) = delete;
  BaguetteInstaller& operator=(const BaguetteInstaller&) = delete;

  enum class InstallResult {
    // The install succeeded.
    Success,
    // The install failed due to an error downloading.
    DownloadError,
    // The install failed due to a bad checksum of downloaded image.
    ChecksumError,
    // The install failed for an unspecified reason.
    Failure,
    // The install failed because it needed to download an image and the device
    // is offline.
    Offline,
    // The device must be updated before termina can be installed.
    NeedUpdate,
    // The install request was cancelled.
    Cancelled,
  };
  using UninstallResult = int;
  using BaguetteInstallerCallback =
      base::OnceCallback<void(InstallResult result,
                              std::optional<base::ScopedFD> fd)>;

  void Install(BaguetteInstallerCallback callback);
  void Uninstall(base::OnceCallback<void(bool)> callback);

 private:
  void GetBaguetteImageUrl(BaguetteInstallerCallback callback);
  void OnListVmDisks(
      BaguetteInstallerCallback callback,
      std::optional<vm_tools::concierge::ListVmDisksResponse> response);
  void OnInstallDlc(BaguetteInstallerCallback callback,
                    guest_os::GuestOsDlcInstallation::Result result);
  void OnConciergeAvailable(BaguetteInstallerCallback callback,
                            bool service_is_available);
  void DownloadBaguetteImage(
      BaguetteInstallerCallback callback,
      std::optional<vm_tools::concierge::GetBaguetteImageUrlResponse> response);
  void OnDiskImageDownloaded(BaguetteInstallerCallback callback,
                             std::string expected_hash,
                             base::FilePath path,
                             std::string hash);
  void OnOpenFd(BaguetteInstallerCallback callback, base::ScopedFD image);
  void RemoveDlc(base::OnceCallback<void(bool)> callback);

  std::vector<std::unique_ptr<guest_os::GuestOsDlcInstallation>> installations_;

  // Downloaded file gets deleted once the downloader object goes out of scope.
  std::unique_ptr<BaguetteDownload> image_download_;
  base::RepeatingCallback<std::unique_ptr<BaguetteDownload>(void)>
      download_factory_;
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<BaguetteInstaller> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_BAGUETTE_INSTALLER_H_
