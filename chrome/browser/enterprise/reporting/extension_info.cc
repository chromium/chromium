// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_info.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permissions_data.h"

using extensions::mojom::ManifestLocation;

namespace em = ::enterprise_management;

namespace enterprise_reporting {

namespace {

em::Extension_InstallType GetExtensionInstallType(
    ManifestLocation extension_location) {
  switch (extension_location) {
    case ManifestLocation::kInternal:
      return em::Extension_InstallType_TYPE_NORMAL;
    case ManifestLocation::kUnpacked:
    case ManifestLocation::kCommandLine:
      return em::Extension_InstallType_TYPE_DEVELOPMENT;
    case ManifestLocation::kExternalPref:
    case ManifestLocation::kExternalRegistry:
    case ManifestLocation::kExternalPrefDownload:
      return em::Extension_InstallType_TYPE_SIDELOAD;
    case ManifestLocation::kExternalPolicy:
    case ManifestLocation::kExternalPolicyDownload:
      return em::Extension_InstallType_TYPE_ADMIN;
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case ManifestLocation::kInvalidLocation:
    case ManifestLocation::kComponent:
    case ManifestLocation::kExternalComponent:
      return em::Extension_InstallType_TYPE_OTHER;
  }
}

void AddPermission(const extensions::Extension* extension,
                   em::Extension* extension_info) {
  auto add_permission = [extension_info](const std::string& permission) {
    extension_info->add_permissions(permission);
  };

  base::ranges::for_each(
      extensions::PermissionsParser::GetRequiredPermissions(extension)
          .GetAPIsAsStrings(),
      add_permission);

  base::ranges::for_each(
      extensions::PermissionsParser::GetOptionalPermissions(extension)
          .GetAPIsAsStrings(),
      add_permission);
  return;
}

void AddHostPermission(const extensions::Extension* extension,
                       em::Extension* extension_info) {
  auto add_permission = [extension_info](const URLPattern& url) {
    extension_info->add_host_permissions(url.GetAsString());
  };

  base::ranges::for_each(
      extensions::PermissionsParser::GetRequiredPermissions(extension)
          .explicit_hosts(),
      add_permission);

  base::ranges::for_each(
      extensions::PermissionsParser::GetOptionalPermissions(extension)
          .explicit_hosts(),
      add_permission);

  return;
}

void AddExtensions(const extensions::ExtensionSet& extensions,
                   em::ChromeUserProfileInfo* profile_info,
                   bool enabled) {
  for (const auto& extension : extensions) {
    // Skip the component extension.
    if (extensions::Manifest::IsComponentLocation(extension->location())) {
      continue;
    }

    auto* extension_info = profile_info->add_extensions();
    extension_info->set_id(extension->id());
    extension_info->set_version(extension->VersionString());
    extension_info->set_name(extension->name());
    extension_info->set_description(extension->description());
    extension_info->set_app_type(
        ConvertExtensionTypeToProto(extension->GetType()));
    extension_info->set_homepage_url(
        extensions::ManifestURL::GetHomepageURL(extension.get()).spec());
    extension_info->set_install_type(
        GetExtensionInstallType(extension->location()));
    extension_info->set_enabled(enabled);
    AddPermission(extension.get(), extension_info);
    AddHostPermission(extension.get(), extension_info);
    extension_info->set_from_webstore(extension->from_webstore());
    extension_info->set_manifest_version(extension->manifest_version());
  }
}

}  // namespace

em::Extension_ExtensionType ConvertExtensionTypeToProto(
    extensions::Manifest::Type extension_type) {
  switch (extension_type) {
    case extensions::Manifest::TYPE_UNKNOWN:
    case extensions::Manifest::TYPE_SHARED_MODULE:
      return em::Extension_ExtensionType_TYPE_UNKNOWN;
    case extensions::Manifest::TYPE_EXTENSION:
      return em::Extension_ExtensionType_TYPE_EXTENSION;
    case extensions::Manifest::TYPE_THEME:
      return em::Extension_ExtensionType_TYPE_THEME;
    case extensions::Manifest::TYPE_USER_SCRIPT:
      return em::Extension_ExtensionType_TYPE_USER_SCRIPT;
    case extensions::Manifest::TYPE_HOSTED_APP:
      return em::Extension_ExtensionType_TYPE_HOSTED_APP;
    case extensions::Manifest::TYPE_LEGACY_PACKAGED_APP:
      return em::Extension_ExtensionType_TYPE_LEGACY_PACKAGED_APP;
    case extensions::Manifest::TYPE_PLATFORM_APP:
      return em::Extension_ExtensionType_TYPE_PLATFORM_APP;
    case extensions::Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return em::Extension_ExtensionType_TYPE_LOGIN_SCREEN_EXTENSION;
    case extensions::Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      return em::Extension_ExtensionType_TYPE_CHROMEOS_SYSTEM_EXTENSION;
    case extensions::Manifest::NUM_LOAD_TYPES:
      NOTREACHED_IN_MIGRATION();
      return em::Extension_ExtensionType_TYPE_UNKNOWN;
  }
}

void AppendExtensionInfoIntoProfileReport(
    Profile* profile,
    em::ChromeUserProfileInfo* profile_info) {
  auto* registry = extensions::ExtensionRegistry::Get(profile);
  AddExtensions(registry->enabled_extensions(), profile_info, true);
  AddExtensions(registry->disabled_extensions(), profile_info, false);
  AddExtensions(registry->terminated_extensions(), profile_info, false);
}

}  // namespace enterprise_reporting
