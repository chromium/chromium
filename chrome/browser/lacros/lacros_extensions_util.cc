// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extensions_util.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build//build_config.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace lacros_extensions_util {

bool IsExtensionApp(const extensions::Extension* extension) {
  return extension->is_platform_app() ||
         (extension->is_hosted_app() && apps::ShouldHostedAppsRunInLacros());
}

const extensions::Extension* MaybeGetExtension(
    Profile* profile,
    const std::string& extension_id) {
  DCHECK(profile);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  return registry->GetInstalledExtension(extension_id);
}

const extensions::Extension* MaybeGetExtension(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  extensions::ExtensionRegistry* registry = extensions::ExtensionRegistry::Get(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  DCHECK(registry);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  return extensions.GetAppByURL(web_contents->GetVisibleURL());
}

bool GetProfileAndExtension(const std::string& extension_id,
                            Profile** output_profile,
                            const extensions::Extension** output_extension) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile);
  const extensions::Extension* extension =
      lacros_extensions_util::MaybeGetExtension(profile, extension_id);
  if (!extension) {
    return false;
  }
  *output_profile = profile;
  *output_extension = extension;
  return true;
}

}  // namespace lacros_extensions_util
