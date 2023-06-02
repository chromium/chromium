// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/install_gate.h"

class Profile;

namespace extensions {
// Delays an extension update if the old version is not idle.
class UpdateInstallGate : public InstallGate {
 public:
  explicit UpdateInstallGate(Profile* profile);

  UpdateInstallGate(const UpdateInstallGate&) = delete;
  UpdateInstallGate& operator=(const UpdateInstallGate&) = delete;

  // InstallGate:
  Action ShouldDelay(const Extension* extension,
                     bool install_immediately) override;

 private:
  // Not owned.
  const raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATE_INSTALL_GATE_H_
