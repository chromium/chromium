// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_

#include <memory>

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class SystemExtensionsInstallManager;

// Name of the directory, under the user profile directory, where System
// Extensions are installed.
extern const char kSystemExtensionsProfileDirectory[];

// Manages the installation, storage, and execution of System Extensions.
class SystemExtensionsProvider : public KeyedService {
 public:
  // May return nullptr if there is no provider associated with this profile.
  static SystemExtensionsProvider* Get(Profile* profile);
  static bool IsEnabled();

  SystemExtensionsProvider();
  SystemExtensionsProvider(const SystemExtensionsProvider&) = delete;
  SystemExtensionsProvider& operator=(const SystemExtensionsProvider&) = delete;
  ~SystemExtensionsProvider() override;

  SystemExtensionsInstallManager& install_manager() {
    return *install_manager_;
  }

 private:
  std::unique_ptr<SystemExtensionsInstallManager> install_manager_;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_
