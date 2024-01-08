// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/install_gate.h"

namespace extensions {
class Extension;
class ExtensionRegistry;
}  // namespace extensions

class Profile;

namespace ash {

class KioskAppUpdateInstallGate : public extensions::InstallGate {
 public:
  explicit KioskAppUpdateInstallGate(Profile* profile);
  KioskAppUpdateInstallGate(const KioskAppUpdateInstallGate&) = delete;
  KioskAppUpdateInstallGate& operator=(const KioskAppUpdateInstallGate&) =
      delete;
  ~KioskAppUpdateInstallGate() override;

  // InstallGate:
  Action ShouldDelay(const extensions::Extension* extension,
                     bool install_immediately) override;

 private:
  const raw_ptr<Profile> profile_;
  const raw_ptr<extensions::ExtensionRegistry> registry_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_INSTALL_GATE_H_
