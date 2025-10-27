// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include <set>
#include <string>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"
#include "components/policy/core/common/features.h"

namespace policy {

ExtensionInstallPolicyService::ExtensionInstallPolicyService(Profile* profile)
    : profile_(profile) {
      CHECK(base::FeatureList::IsEnabled(features::kEnableExtensionInstallPolicyFetching));
}

ExtensionInstallPolicyService::~ExtensionInstallPolicyService() = default;

void ExtensionInstallPolicyService::CanInstallExtension(
    const std::string& extension_id,
    base::OnceCallback<void(bool)> callback) {
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    std::move(callback).Run(true);
    return;
  }

  std::move(callback).Run(true);
}

void ExtensionInstallPolicyService::GetDisallowedExtensions(
    const std::vector<std::string>& extension_ids_and_versions,
    base::OnceCallback<void(std::set<std::string> /*disallowed_extension*/)>
        callback) {
  if (extension_ids_and_versions.empty()) {
    std::move(callback).Run({});
    return;
  }
}

}  // namespace policy