// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MOCK_CRX_INSTALLER_H_
#define CHROME_BROWSER_EXTENSIONS_MOCK_CRX_INSTALLER_H_

#include "chrome/browser/extensions/crx_installer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

// A mock around CrxInstaller to track extension installations.
class MockCrxInstaller : public CrxInstaller {
 public:
  explicit MockCrxInstaller(ExtensionService* frontend);

  MOCK_METHOD(void, InstallCrxFile, (const CRXFileInfo& info), (override));

  MOCK_METHOD(void,
              AddInstallerCallback,
              (InstallerResultCallback callback),
              (override));

 protected:
  ~MockCrxInstaller() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MOCK_CRX_INSTALLER_H_
