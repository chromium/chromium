// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FAKE_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_FAKE_CRX_INSTALLER_H_

#include "chrome/browser/extensions/crx_installer.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

// A fake CrxInstaller.
//
// Has InstallCrxFile as a NOOP, letting test code
// decide when to call RunInstallerCallbacks to fake installation
// completion.
class FakeCrxInstaller : public CrxInstaller {
 public:
  explicit FakeCrxInstaller(content::BrowserContext* context);

  void InstallCrxFile(const CRXFileInfo& info) override;

  using CrxInstaller::RunInstallerCallbacks;

 protected:
  ~FakeCrxInstaller() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FAKE_CRX_INSTALLER_H_
