// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_

#include "base/macros.h"
#include "chrome/browser/extensions/install_gate.h"

namespace extensions {
class Extension;
class ExtensionRegistry;
}

class Profile;

namespace ash {

class KioskAppUpdateInstallGate : public extensions::InstallGate {
 public:
  explicit KioskAppUpdateInstallGate(Profile* profile);
  ~KioskAppUpdateInstallGate() override;

  // InstallGate:
  Action ShouldDelay(const extensions::Extension* extension,
                     bool install_immediately) override;

 private:
  Profile* const profile_;
  extensions::ExtensionRegistry* const registry_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppUpdateInstallGate);
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_
