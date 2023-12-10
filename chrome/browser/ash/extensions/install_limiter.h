// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_H_

#include <stdint.h>

#include <set>

#include "base/containers/queue.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace extensions {

// InstallLimiter defers big app installs after all small app installs and then
// runs big app installs one by one. This improves first-time login experience.
// See http://crbug.com/166296
class InstallLimiter : public KeyedService {
 public:
  static InstallLimiter* Get(Profile* profile);

  // Install should be deferred if the size is larger than 1MB and the app is
  // not the screensaver in demo mode (which requires instant installation to
  // avoid visual delay).
  static bool ShouldDeferInstall(int64_t app_size, const std::string& app_id);

  InstallLimiter();

  InstallLimiter(const InstallLimiter&) = delete;
  InstallLimiter& operator=(const InstallLimiter&) = delete;

  ~InstallLimiter() override;

  void DisableForTest();

  void Add(const scoped_refptr<CrxInstaller>& installer,
           const CRXFileInfo& file_info);

  // Triggers installation of deferred installations if all file sizes for
  // added installations have been determined.
  void OnAllExternalProvidersReady();

 private:
  // DeferredInstall holds info to run a CrxInstaller later.
  struct DeferredInstall {
    DeferredInstall(const scoped_refptr<CrxInstaller>& installer,
                    const CRXFileInfo& file_info);
    DeferredInstall(const DeferredInstall& other);
    ~DeferredInstall();

    const scoped_refptr<CrxInstaller> installer;
    const CRXFileInfo file_info;
  };

  using DeferredInstallList = base::queue<DeferredInstall>;

  // Adds install info with size. If |size| is greater than a certain threshold,
  // it stores the install info into |deferred_installs_| to run it later.
  // Otherwise, it just runs the installer.
  void AddWithSize(const scoped_refptr<CrxInstaller>& installer,
                   const CRXFileInfo& file_info,
                   int64_t size);

  // Checks and runs deferred big app installs when appropriate.
  void CheckAndRunDeferrredInstalls();

  // Starts install using passed-in info and observes |installer|'s done
  // notification.
  void RunInstall(const scoped_refptr<CrxInstaller>& installer,
                  const CRXFileInfo& file_info);

  // Called when CrxInstaller::InstallCrx() finishes.
  void OnInstallerDone(const std::optional<CrxInstallError>& error);

  // Checks that OnAllExternalProvidersReady() has been called and all file
  // sizes for added installations are determined. If this method returns true,
  // we can directly continue installing all remaining extensions, since there
  // will be no more added installations coming.
  bool AllInstallsQueuedWithFileSize() const;

  DeferredInstallList deferred_installs_;
  // Counts how many installs are currently running.
  uint32_t num_running_installs_ = 0;

  // A timer to wait before running deferred big app install.
  base::OneShotTimer wait_timer_;

  bool disabled_for_test_;

  bool all_external_providers_ready_ = false;
  int num_installs_waiting_for_file_size_ = 0;

  base::WeakPtrFactory<InstallLimiter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_INSTALL_LIMITER_H_
