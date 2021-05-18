// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_

#include <map>

#include "chrome/browser/ash/system_extensions/system_extension.h"

class SystemExtensionsInstallManager {
 public:
  SystemExtensionsInstallManager();
  SystemExtensionsInstallManager(const SystemExtensionsInstallManager&) =
      delete;
  SystemExtensionsInstallManager& operator=(
      const SystemExtensionsInstallManager&) = delete;
  ~SystemExtensionsInstallManager();

  // TODO(ortuno): Move these to a Registrar or Database.
  std::vector<SystemExtensionId> GetSystemExtensionIds();
  const SystemExtension* GetSystemExtensionById(const SystemExtensionId& id);

 private:
  void InstallFromCommandLineIfNecessary();

  // TODO(ortuno): Move this to a Registrar or Database.
  std::map<SystemExtensionId, SystemExtension> system_extensions_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
