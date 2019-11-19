// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_private/extension_install_status.h"

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

ExtensionInstallStatus GetWebstoreExtensionInstallStatus(
    const ExtensionId& extension_id,
    Profile* profile) {
  DCHECK(crx_file::id_util::IdIsValid(extension_id));

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (registry->enabled_extensions().Contains(extension_id))
    return kEnabled;

  if (registry->terminated_extensions().Contains(extension_id))
    return kTerminated;

  if (registry->blacklisted_extensions().Contains(extension_id))
    return kBlacklisted;

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(profile);
  ExtensionManagement::InstallationMode mode =
      extension_management->GetInstallationMode(
          extension_id, extension_urls::GetDefaultWebstoreUpdateUrl().spec());

  // If an installed extension is disabled due to policy, returns
  // kBlockedByPolicy, kCanRequest or kRequestPending instead of kDisabled.
  // By doing so, user can still request an installed and policy blocked
  // extension.
  if (mode == ExtensionManagement::INSTALLATION_FORCED ||
      mode == ExtensionManagement::INSTALLATION_RECOMMENDED ||
      mode == ExtensionManagement::INSTALLATION_ALLOWED) {
    if (registry->disabled_extensions().Contains(extension_id))
      return kDisabled;
    return kInstallable;
  }

  // The ability to request extension installs is not available if the extension
  // request policy is disabled
  if (!profile->GetPrefs()->GetBoolean(prefs::kCloudExtensionRequestEnabled))
    return kBlockedByPolicy;

  if (extension_management->IsInstallationExplicitlyBlocked(extension_id))
    return kBlockedByPolicy;

  const auto pending_list =
      profile->GetPrefs()->GetList(prefs::kCloudExtensionRequestIds)->GetList();
  if (std::any_of(pending_list.begin(), pending_list.end(),
                  [&extension_id](const base::Value& pending_id) {
                    return pending_id.GetString() == extension_id;
                  })) {
    return kRequestPending;
  }

  return kCanRequest;
}

}  // namespace extensions
