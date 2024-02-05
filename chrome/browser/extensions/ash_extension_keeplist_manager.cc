// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/ash_extension_keeplist_manager.h"

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/hosted_app_util.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_constants/constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

AshExtensionKeeplistManager::AshExtensionKeeplistManager(
    Profile* profile,
    ExtensionPrefs* extension_prefs,
    ExtensionService* extension_service)
    : extension_prefs_(extension_prefs),
      extension_service_(extension_service),
      registry_(ExtensionRegistry::Get(profile)),
      should_enforce_keeplist_(
          crosapi::browser_util::ShouldEnforceAshExtensionKeepList()) {
  if (should_enforce_keeplist_)
    registry_observation_.Observe(registry_);
}

AshExtensionKeeplistManager::~AshExtensionKeeplistManager() = default;

void AshExtensionKeeplistManager::Init() {
  if (should_enforce_keeplist_)
    ActivateKeeplistEnforcement();
  else
    DeactivateKeeplistEnforcement();
}

void AshExtensionKeeplistManager::ActivateKeeplistEnforcement() {
  DCHECK(should_enforce_keeplist_);

  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  for (const auto& extension : all_extensions) {
    if (ShouldDisable(extension.get()))
      Disable(extension->id());
  }
}

bool AshExtensionKeeplistManager::ShouldDisable(
    const Extension* extension) const {
  if (extension->is_extension() && !ExtensionRunsInOS(extension->id()))
    return true;

  if (extension->is_platform_app() &&
      crosapi::browser_util::IsLacrosChromeAppsEnabled() &&
      !ExtensionAppRunsInOS(extension->id())) {
    return true;
  }

  if (extension->is_hosted_app() &&
      extension->id() != app_constants::kChromeAppId &&
      crosapi::IsStandaloneBrowserHostedAppsEnabled()) {
    return true;
  }

  return false;
}

void AshExtensionKeeplistManager::Disable(const ExtensionId& extension_id) {
  DCHECK(should_enforce_keeplist_);

  extension_service_->DisableExtension(
      extension_id, disable_reason::DISABLE_NOT_ASH_KEEPLISTED);

  // An extension is not allowed to be disabled by user due to different reasons
  // (shared module, installed as a component extension or installed by policy,
  // etc.). We would log a message here to track the extensions that can't be
  // disabled and analyze to see if we have missed any extensions in the keep
  // list during the audit.
  if (registry_->enabled_extensions().Contains(extension_id)) {
    LOG(WARNING) << "Can not enforce disabling extension id:" << extension_id;
  }
}

void AshExtensionKeeplistManager::DeactivateKeeplistEnforcement() {
  DCHECK(!should_enforce_keeplist_);

  const ExtensionSet all_extensions =
      registry_->GenerateInstalledExtensionsSet();

  // Find all extensions disabled by keeplist enforcement, remove the disable
  // reason.
  for (const auto& extension : all_extensions) {
    if (extension_prefs_->HasDisableReason(
            extension->id(), disable_reason::DISABLE_NOT_ASH_KEEPLISTED)) {
      extension_service_->RemoveDisableReasonAndMaybeEnable(
          extension->id(), disable_reason::DISABLE_NOT_ASH_KEEPLISTED);
    }
  }
}

void AshExtensionKeeplistManager::OnExtensionReady(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!should_enforce_keeplist_)
    return;

  if (ShouldDisable(extension))
    Disable(extension->id());
}

}  // namespace extensions
