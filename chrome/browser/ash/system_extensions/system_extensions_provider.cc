// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"

namespace ash {

// TODO:(https://crbug.com/1192426): Change this to system extension scheme when
// it's ready.
const char* kSystemExtensionScheme = content::kChromeUIUntrustedScheme;

// static
SystemExtensionsProvider& SystemExtensionsProvider::Get(Profile* profile) {
  DCHECK(ash::IsSystemExtensionsEnabled(profile));
  return *SystemExtensionsProviderFactory::GetForProfileIfExists(profile);
}

// static
bool SystemExtensionsProvider::IsDebugMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kSystemExtensionsDebug);
}

SystemExtensionsProvider::SystemExtensionsProvider(Profile* profile) {
  install_manager_ = std::make_unique<SystemExtensionsInstallManager>(profile);
}

void SystemExtensionsProvider::
    UpdateEnabledBlinkRuntimeFeaturesInIsolatedWorker(
        const GURL& script_url,
        std::vector<std::string>& out_forced_enabled_runtime_features) {
  if (!script_url.SchemeIs(kSystemExtensionScheme))
    return;

  auto* system_extension =
      install_manager_->GetSystemExtensionByURL(script_url);
  if (!system_extension)
    return;

  // TODO(https://crbug.com/1272371): Change the following to query system
  // extension feature list.
  out_forced_enabled_runtime_features.push_back("BlinkExtensionChromeOS");
  if (system_extension->type == SystemExtensionType::kEcho) {
    out_forced_enabled_runtime_features.push_back(
        "BlinkExtensionChromeOSWindowManagement");
  }
}

SystemExtensionsProvider::~SystemExtensionsProvider() = default;

}  // namespace ash
