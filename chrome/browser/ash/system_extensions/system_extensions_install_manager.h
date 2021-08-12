// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_sandboxed_unpacker.h"

class SystemExtensionsInstallManager {
 public:
  SystemExtensionsInstallManager();
  SystemExtensionsInstallManager(const SystemExtensionsInstallManager&) =
      delete;
  SystemExtensionsInstallManager& operator=(
      const SystemExtensionsInstallManager&) = delete;
  ~SystemExtensionsInstallManager();

  const base::OneShotEvent& on_command_line_install_finished() {
    return on_command_line_install_finished_;
  }

  // TODO(ortuno): Move these to a Registrar or Database.
  std::vector<SystemExtensionId> GetSystemExtensionIds();
  const SystemExtension* GetSystemExtensionById(const SystemExtensionId& id);

 private:
  void InstallFromCommandLineIfNecessary();
  void OnGetSystemExtensionFromDir(
      SystemExtensionsSandboxedUnpacker::Status status,
      std::unique_ptr<SystemExtension> system_extension);

  base::OneShotEvent on_command_line_install_finished_;

  SystemExtensionsSandboxedUnpacker sandboxed_unpacker_;

  // TODO(ortuno): Move this to a Registrar or Database.
  std::map<SystemExtensionId, std::unique_ptr<SystemExtension>>
      system_extensions_;

  base::WeakPtrFactory<SystemExtensionsInstallManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_INSTALL_MANAGER_H_
