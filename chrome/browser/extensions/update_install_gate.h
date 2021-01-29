// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_

#include "base/macros.h"
#include "chrome/browser/extensions/install_gate.h"

class Profile;

namespace extensions {
// Delays an extension update if the old version is not idle.
class UpdateInstallGate : public InstallGate {
 public:
  explicit UpdateInstallGate(Profile* profile);

  // InstallGate:
  Action ShouldDelay(const Extension* extension,
                     bool install_immediately) override;

 private:
  // Not owned.
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(UpdateInstallGate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_
