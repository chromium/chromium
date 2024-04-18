// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DLC_HELPER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DLC_HELPER_H_

#include <ostream>
#include <string_view>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace guest_os {

// This object represents an in-progress DLC installation.
//
// Deleting this object cancels the installation.
//
// Once the installation is completed, deleting the object has no effect.
class GuestOsDlcInstallation {
 public:
  enum class Error {
    Cancelled,
    // Errors actionable by the user
    Offline,
    NeedUpdate,
    NeedReboot,
    DiskFull,
    // Transient errors, can be retried automatically
    Busy,
    Internal,
    // Other errors, if you see this in practice something has gone very wrong.
    Invalid,
    UnknownFailure,
  };

  using Result = base::expected<base::FilePath, Error>;

  // A callback for reporting progress in the range [0,1].
  using ProgressCallback = base::RepeatingCallback<void(double)>;

  // Installs the |dlc_id| DLC with some added conveniences. Deleting this
  // object will cancel the installation if it is in-progress. Invokes
  // |completion_callback| when the installation finishes (or is cancelled)
  // either with the path on-disk to the DLC's root if it succeeds, or with an
  // error if it fails.
  //
  // During installation |progress_callback| will be invoked repeatedly with a
  // value in [0,1] to indicate the installation's progress.
  GuestOsDlcInstallation(std::string dlc_id,
                         base::OnceCallback<void(Result)> completion_callback,
                         ProgressCallback progress_callback);

  // This object can not be moved or copied.
  GuestOsDlcInstallation(const GuestOsDlcInstallation&) = delete;
  GuestOsDlcInstallation& operator=(const GuestOsDlcInstallation&) = delete;

  ~GuestOsDlcInstallation();

  // If you intend to uninstall immediately after canceling, prefer this
  // method. Normally you can just delete the object to cancel the installation,
  // but dlcservice may still try to mount it in the background. Using this
  // cancel ensures dlcservice won't be busy with the current installation.
  void CancelGracefully();

 private:
  void CheckState();

  void OnGetDlcStateCompleted(std::string_view err,
                              const dlcservice::DlcState& dlc_state);

  void StartInstall();

  void OnDlcInstallCompleted(
      const ash::DlcserviceClient::InstallResult& install_result);

  std::string dlc_id_;
  int retries_remaining_;
  base::OnceCallback<void(Result)> completion_callback_;
  ProgressCallback progress_callback_;
  bool gracefully_cancelled_ = false;

  base::WeakPtrFactory<GuestOsDlcInstallation> weak_factory_{this};
};

}  // namespace guest_os

std::ostream& operator<<(std::ostream& stream,
                         guest_os::GuestOsDlcInstallation::Error err);

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_DLC_HELPER_H_
