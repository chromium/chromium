// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_extension_apps_utility.h"

#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace lacros_extension_apps_utility {
namespace {

// The delimiter separates the profile basename from the extension id.
constexpr char kDelimiter[] = "###";

}  // namespace

std::string MuxId(const Profile* profile,
                  const extensions::Extension* extension) {
  return profile->GetBaseName().value() + kDelimiter + extension->id();
}

bool DemuxId(const std::string& muxed_id,
             Profile** output_profile,
             const extensions::Extension** output_extension) {
  std::vector<std::string> splits = base::SplitStringUsingSubstr(
      muxed_id, kDelimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (splits.size() != 2)
    return false;
  std::string profile_basename = std::move(splits[0]);
  std::string extension_id = std::move(splits[1]);
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  Profile* matching_profile = nullptr;
  for (auto* profile : profiles) {
    if (profile->GetBaseName().value() == profile_basename) {
      matching_profile = profile;
      break;
    }
  }
  if (!matching_profile)
    return false;
  const extensions::Extension* extension =
      MaybeGetPackagedV2App(matching_profile, extension_id);
  if (!extension)
    return false;
  *output_profile = matching_profile;
  *output_extension = extension;
  return true;
}

const extensions::Extension* MaybeGetPackagedV2App(Profile* profile,
                                                   const std::string& app_id) {
  DCHECK(profile);
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile);
  DCHECK(registry);
  const extensions::Extension* extension =
      registry->GetInstalledExtension(app_id);
  if (!extension || !extension->is_platform_app())
    return nullptr;
  return extension;
}

}  // namespace lacros_extension_apps_utility
