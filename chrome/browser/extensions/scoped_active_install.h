// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SCOPED_ACTIVE_INSTALL_H_
#define CHROME_BROWSER_EXTENSIONS_SCOPED_ACTIVE_INSTALL_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"

namespace extensions {

struct ActiveInstallData;

// Registers and deregisters an active extension install with InstallTracker.
class ScopedActiveInstall : public InstallObserver {
 public:
  // This constructor registers an active install with InstallTracker.
  ScopedActiveInstall(InstallTracker* tracker,
                      const ActiveInstallData& install_data);

  // This constructor does not register an active install. The extension install
  // is still deregistered upon destruction.
  ScopedActiveInstall(InstallTracker* tracker, const std::string& extension_id);

  ~ScopedActiveInstall() override;

  // Ensures that the active install is not deregistered upon destruction. This
  // may be necessary if the extension install outlives the lifetime of this
  // instance.
  void CancelDeregister();

 private:
  void Init();

  // InstallObserver implementation.
  void OnShutdown() override;

  InstallTracker* tracker_;
  ScopedObserver<InstallTracker, InstallObserver> tracker_observer_{this};
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ScopedActiveInstall);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SCOPED_ACTIVE_INSTALL_H_
