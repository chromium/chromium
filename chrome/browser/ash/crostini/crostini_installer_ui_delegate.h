// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-forward.h"

namespace crostini {

class CrostiniInstallerUIDelegate {
 public:
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

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_INSTALLER_UI_DELEGATE_H_
