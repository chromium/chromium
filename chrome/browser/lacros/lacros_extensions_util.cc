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

std::string MuxId(const Profile* profile,
                  const extensions::Extension* extension) {
  return apps::MuxId(profile, extension->id());
}

bool DemuxId(const std::string& muxed_id,
             Profile** output_profile,
             const extensions::Extension** output_extension) {
  std::vector<std::string> splits = apps::DemuxId(muxed_id);
  if (splits.size() != 2)
    return false;
  std::string profile_basename = std::move(splits[0]);
  std::string extension_id = std::move(splits[1]);
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  Profile* matching_profile = nullptr;
  for (auto* profile : profiles) {
    bool is_sentinel_and_main_profile =
        profile_basename == "" && profile->IsMainProfile();
    if (is_sentinel_and_main_profile ||
        (profile->GetBaseName().value() == profile_basename)) {
      matching_profile = profile;
      break;
    }
  }
  if (!matching_profile)
    return false;
  const extensions::Extension* extension =
      MaybeGetExtension(matching_profile, extension_id);
  if (!extension)
    return false;
  *output_profile = matching_profile;
  *output_extension = extension;
  return true;
}

bool DemuxPlatformAppId(const std::string& muxed_id,
                        Profile** output_profile,
                        const extensions::Extension** output_extension) {
  Profile* profile = nullptr;
  const extensions::Extension* extension = nullptr;
  if (!DemuxId(muxed_id, &profile, &extension) || !extension->is_platform_app())
    return false;
  *output_profile = profile;
  *output_extension = extension;
  return true;
}

}  // namespace lacros_extensions_util
