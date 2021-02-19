// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_allowlist.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "extensions/browser/allowlist_state.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_features.h"

namespace extensions {

ExtensionAllowlist::ExtensionAllowlist(Profile* profile,
                                       ExtensionPrefs* extension_prefs,
                                       ExtensionService* extension_service)
    : profile_(profile),
      extension_prefs_(extension_prefs),
      extension_service_(extension_service),
      registry_(ExtensionRegistry::Get(profile)) {}

void ExtensionAllowlist::Init() {
  SetAllowlistEnforcedField();

  // Register to Enhanced Safe Browsing setting changes for allowlist
  // enforcements.
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSafeBrowsingEnhanced,
      base::BindRepeating(&ExtensionAllowlist::OnSafeBrowsingEnhancedChanged,
                          base::Unretained(this)));

  if (is_allowlist_enforced_) {
    ActivateAllowlistEnforcement();
  } else {
    DeactivateAllowlistEnforcement();
  }
}

void ExtensionAllowlist::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value& attributes) {
  const base::Value* allowlist_value = attributes.FindKey("_esbAllowlist");

  if (!allowlist_value) {
    // TODO(jeffcyr): Add metric to track if there is an issue.

    // Ignore missing attribute. Omaha server should set the attribute to |true|
    // or |false|. This way the allowlist state won't flip if there is a server
    // bug where the attribute isn't sent. This will also leave external
    // extensions in the |ALLOWLIST_UNDEFINED| state.
    return;
  }

  bool allowlisted = allowlist_value->GetBool();

  // TODO(jeffcyr): Add an observer when allowlist state change, otherwise the
  // 'chrome://extensions' warning may not be refreshed.

  // Set the allowlist state even if there is no enforcement. This will allow
  // immediate enforcement when it is activated.
  extension_prefs_->SetExtensionAllowlistState(
      extension_id,
      allowlisted ? ALLOWLIST_ALLOWLISTED : ALLOWLIST_NOT_ALLOWLISTED);

  if (!is_allowlist_enforced_) {
    DCHECK(!extension_prefs_->HasDisableReason(
        extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED));
    return;
  }

  if (allowlisted) {
    extension_service_->RemoveDisableReasonAndMaybeEnable(
        extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED);
  } else {
    // TODO(jeffcyr): User re-enable is broken, if the user manually re-enabled
    // the extension, it is currently disabled again on the next update check or
    // restart. This will be fixed before launch.

    extension_service_->DisableExtension(
        extension_id, disable_reason::DISABLE_NOT_ALLOWLISTED);
  }
}

bool ExtensionAllowlist::ShouldDisplayWarning(
    const std::string& extension_id) const {
  return is_allowlist_enforced_ &&
         extension_prefs_->GetExtensionAllowlistState(extension_id) ==
             ALLOWLIST_NOT_ALLOWLISTED;
}

void ExtensionAllowlist::SetAllowlistEnforcedField() {
  is_allowlist_enforced_ =
      base::FeatureList::IsEnabled(
          extensions_features::kEnforceSafeBrowsingExtensionAllowlist) &&
      safe_browsing::IsEnhancedProtectionEnabled(*profile_->GetPrefs());
}

void ExtensionAllowlist::ActivateAllowlistEnforcement() {
  DCHECK(is_allowlist_enforced_);

  std::unique_ptr<ExtensionSet> all_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : *all_extensions) {
    if (extension_prefs_->GetExtensionAllowlistState(extension->id()) ==
        ALLOWLIST_NOT_ALLOWLISTED) {
      extension_service_->DisableExtension(
          extension->id(), disable_reason::DISABLE_NOT_ALLOWLISTED);
    }
  }
}

void ExtensionAllowlist::DeactivateAllowlistEnforcement() {
  DCHECK(!is_allowlist_enforced_);

  std::unique_ptr<ExtensionSet> all_extensions =
      registry_->GenerateInstalledExtensionsSet();
  for (const auto& extension : *all_extensions) {
    extension_service_->RemoveDisableReasonAndMaybeEnable(
        extension->id(), disable_reason::DISABLE_NOT_ALLOWLISTED);
  }
}

void ExtensionAllowlist::OnSafeBrowsingEnhancedChanged() {
  bool old_value = is_allowlist_enforced_;

  // Note that |is_allowlist_enforced_| could remain |false| even if the ESB
  // setting was turned on if the feature flag is disabled.
  SetAllowlistEnforcedField();

  if (old_value == is_allowlist_enforced_)
    return;

  if (is_allowlist_enforced_) {
    ActivateAllowlistEnforcement();
  } else {
    DeactivateAllowlistEnforcement();
  }
}

}  // namespace extensions
