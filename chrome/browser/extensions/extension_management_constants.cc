// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/extension_management_constants.h"

namespace extensions {
namespace schema_constants {

// Some values below are used by the policy component to filter out policy
// values. They must be synced with
// components/policy/core/common/policy_loader_common.cc

const char kWildcard[] = "*";

const char kInstallationMode[] = "installation_mode";
const char kAllowed[] = "allowed";
const char kBlocked[] = "blocked";
const char kForceInstalled[] = "force_installed";
const char kNormalInstalled[] = "normal_installed";
const char kRemoved[] = "removed";

const char kBlockedPermissions[] = "blocked_permissions";
const char kAllowedPermissions[] = "allowed_permissions";

const char kPolicyBlockedHosts[] = "runtime_blocked_hosts";
const char kPolicyAllowedHosts[] = "runtime_allowed_hosts";
const size_t kMaxItemsURLPatternSet = 100;

const char kUpdateUrl[] = "update_url";
const char kOverrideUpdateUrl[] = "override_update_url";
const char kInstallSources[] = "install_sources";
const char kAllowedTypes[] = "allowed_types";

const char kMinimumVersionRequired[] = "minimum_version_required";

const char kUpdateUrlPrefix[] = "update_url:";

const char kBlockedInstallMessage[] = "blocked_install_message";

const char kToolbarPin[] = "toolbar_pin";
const char kForcePinned[] = "force_pinned";
const char kDefaultUnpinned[] = "default_unpinned";

const char kFileUrlNavigationAllowed[] = "file_url_navigation_allowed";

const AllowedTypesMapEntry kAllowedTypesMap[] = {
    {"extension", Manifest::TYPE_EXTENSION},
    {"theme", Manifest::TYPE_THEME},
    {"user_script", Manifest::TYPE_USER_SCRIPT},
    {"hosted_app", Manifest::TYPE_HOSTED_APP},
    {"legacy_packaged_app", Manifest::TYPE_LEGACY_PACKAGED_APP},
    {"platform_app", Manifest::TYPE_PLATFORM_APP},
    {"chromeos_system_extension", Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION},
    // TODO(binjin): Add shared_module type here and update
    // ExtensionAllowedTypes policy.
};

const size_t kAllowedTypesMapSize = std::size(kAllowedTypesMap);

Manifest::Type GetManifestType(const std::string& name) {
  for (size_t index = 0; index < kAllowedTypesMapSize; ++index) {
    if (kAllowedTypesMap[index].name == name)
      return kAllowedTypesMap[index].manifest_type;
  }
  return Manifest::TYPE_UNKNOWN;
}

}  // namespace schema_constants
}  // namespace extensions
