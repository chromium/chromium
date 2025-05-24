// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/contact_center_insights/contact_center_insights_extension_manager.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system.h"

using ::extensions::ComponentLoader;

namespace chromeos {

void ContactCenterInsightsExtensionManager::Delegate::InstallExtension(
    ComponentLoader* component_loader) {
  component_loader->Add(
      IDR_CONTACT_CENTER_INSIGHTS_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("chromeos/contact_center_insights")));
}

void ContactCenterInsightsExtensionManager::Delegate::UninstallExtension(
    ComponentLoader* component_loader) {
  component_loader->Remove(extension_misc::kContactCenterInsightsExtensionId);
}

bool ContactCenterInsightsExtensionManager::Delegate::IsProfileAffiliated(
    Profile* profile) const {
  return ::enterprise_util::IsProfileAffiliated(profile);
}

bool ContactCenterInsightsExtensionManager::Delegate::IsExtensionInstalled(
    ComponentLoader* component_loader) const {
  return component_loader->Exists(
      extension_misc::kContactCenterInsightsExtensionId);
}

ContactCenterInsightsExtensionManager::ContactCenterInsightsExtensionManager(
    ComponentLoader* component_loader,
    Profile* profile,
    std::unique_ptr<ContactCenterInsightsExtensionManager::Delegate> delegate)
    : component_loader_(component_loader),
      profile_(profile),
      delegate_(std::move(delegate)) {
  Init();
}

ContactCenterInsightsExtensionManager::
    ~ContactCenterInsightsExtensionManager() = default;

void ContactCenterInsightsExtensionManager::Init() {
  if (CanInstallExtension()) {
    delegate_->InstallExtension(component_loader_);
  } else {
    // Remove extension if installed because it does not meet necessary
    // pre-conditions.
    RemoveExtensionIfInstalled();
  }

  registrar_.Init(profile_->GetPrefs());
  if (delegate_->IsProfileAffiliated(profile_)) {
    // Setup registrar so it starts listening to relevant pref changes.
    registrar_.Add(::prefs::kInsightsExtensionEnabled,
                   base::BindRepeating(
                       &ContactCenterInsightsExtensionManager::OnPrefChanged,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool ContactCenterInsightsExtensionManager::CanInstallExtension() const {
  if (!delegate_->IsProfileAffiliated(profile_)) {
    return false;
  }

  const auto& pref_service = *profile_->GetPrefs();
  return pref_service.HasPrefPath(::prefs::kInsightsExtensionEnabled) &&
         pref_service.GetBoolean(::prefs::kInsightsExtensionEnabled);
}

void ContactCenterInsightsExtensionManager::OnPrefChanged() {
  if (CanInstallExtension()) {
    // This will be a no-op if the extension is already installed with the same
    // version, so it is okay to attempt an installation here.
    delegate_->InstallExtension(component_loader_);
    return;
  }

  // Remove/unload component extension if installed because it does
  // not meet necessary pre-conditions.
  RemoveExtensionIfInstalled();
}

void ContactCenterInsightsExtensionManager::RemoveExtensionIfInstalled() {
  if (delegate_->IsExtensionInstalled(component_loader_)) {
    delegate_->UninstallExtension(component_loader_);
  }
}

}  // namespace chromeos
