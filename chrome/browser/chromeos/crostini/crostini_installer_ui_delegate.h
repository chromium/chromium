// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_

#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_types.mojom-forward.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"

namespace crostini {

class CrostiniInstallerUIDelegate {
 public:
  // The size of the download for the VM image.
  // TODO(timloh): This is just a placeholder.
  static constexpr int64_t kDownloadSizeInBytes = 300 * 1024 * 1024;
  // The minimum feasible size for a VM disk image.
  static constexpr int64_t kMinimumDiskSize =
      1ll * 1024 * 1024 * 1024;  // 1 GiB
  // Minimum amount of free disk space to install crostini successfully.
  static constexpr int64_t kMinimumFreeDiskSpace =
      crostini::CrostiniInstallerUIDelegate::kDownloadSizeInBytes +
      kMinimumDiskSize;

  // |progress_fraction| ranges from 0.0 to 1.0.
  using ProgressCallback =
      base::RepeatingCallback<void(crostini::mojom::InstallerState state,
                                   double progress_fraction)>;
  using ResultCallback =
      base::OnceCallback<void(crostini::mojom::InstallerError error)>;

  // Start the installation. |progress_callback| will be called multiple times
  // until |result_callback| is called. The crostini terminal will be launched
  // when the installation succeeds.
  virtual void Install(CrostiniManager::RestartOptions options,
                       ProgressCallback progress_callback,
                       ResultCallback result_callback) = 0;

  // Cancel the ongoing installation. |callback| will be called when it
  // finishes. The callbacks passed to |Install()| will not be called anymore.
  // A closing UI should call this if installation has started but hasn't
  // finished.
  virtual void Cancel(base::OnceClosure callback) = 0;
  // UI should call this if the user cancels without starting installation so
  // metrics can be recorded.
  virtual void CancelBeforeStart() = 0;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
