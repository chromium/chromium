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

namespace content {
class RenderProcessHost;
}

namespace ash {

// Manages the installation, storage, and execution of System Extensions.
class SystemExtensionsProvider : public KeyedService {
 public:
  // May return nullptr if there is no provider associated with this profile.
  static SystemExtensionsProvider* Get(Profile* profile);
  static bool IsEnabled();

  // TODO(crbug.com/1272371): Remove when APIs can be accessed in a less hacky
  // way.
  // If true, System Extension APIs will be bound on all service workers. This
  // is being added temporarily for development. Use in conjunction with e.g
  // --enable-blink-features=BlinkExtensionChromeOS,
  //                         BlinkExtensionChromeOSWindowManagement
  // to use regular service workers to test your System Extension APIs.
  static bool IsDebugMode();

  explicit SystemExtensionsProvider(Profile* profile);
  SystemExtensionsProvider(const SystemExtensionsProvider&) = delete;
  SystemExtensionsProvider& operator=(const SystemExtensionsProvider&) = delete;
  ~SystemExtensionsProvider() override;

  SystemExtensionsInstallManager& install_manager() {
    return *install_manager_;
  }

  // Called when a service worker will be started to enable blink runtime
  // features based on system extension type.
  void WillStartServiceWorker(const GURL& script_url,
                              content::RenderProcessHost* render_process_host);

 private:
  std::unique_ptr<SystemExtensionsInstallManager> install_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_
