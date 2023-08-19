// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_install_manager.h"

#include <string>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_install_error.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_url_handlers.h"

namespace extensions {

namespace {

// Histogram values for logging events related to externally installed
// extensions.
enum ExternalExtensionEvent {
  EXTERNAL_EXTENSION_INSTALLED = 0,
  EXTERNAL_EXTENSION_IGNORED,
  EXTERNAL_EXTENSION_REENABLED,
  EXTERNAL_EXTENSION_UNINSTALLED,
  EXTERNAL_EXTENSION_BUCKET_BOUNDARY,
};

// Prompt the user this many times before considering an extension acknowledged.
const int kMaxExtensionAcknowledgePromptCount = 3;

}  // namespace

ExternalInstallManager::ExternalInstallManager(
    content::BrowserContext* browser_context,
    bool is_first_run)
    : browser_context_(browser_context),
      is_first_run_(is_first_run),
      extension_prefs_(ExtensionPrefs::Get(browser_context_)),
      currently_visible_install_alert_(nullptr) {
  DCHECK(browser_context_);
  extension_registry_observation_.Observe(
      ExtensionRegistry::Get(browser_context_));
  // Populate the set of unacknowledged external extensions now. We can't just
  // rely on IsUnacknowledgedExternalExtension() for cases like
  // OnExtensionLoaded(), since we need to examine the disable reasons, which
  // can be removed throughout the session.
  for (const auto& extension :
       ExtensionRegistry::Get(browser_context)->disabled_extensions()) {
    if (IsUnacknowledgedExternalExtension(*extension))
      unacknowledged_ids_.insert(extension->id());
  }
}

ExternalInstallManager::~ExternalInstallManager() {
  // Shutdown should have been called.
  DCHECK(errors_.empty());
}

void ExternalInstallManager::Shutdown() {
  // Delete all errors when the profile is shutting down, before associated
  // services are deleted.
  errors_.clear();
}

bool ExternalInstallManager::IsPromptingEnabled() {
  return FeatureSwitch::prompt_for_external_extensions()->IsEnabled();
}

void ExternalInstallManager::AddExternalInstallError(const Extension* extension,
                                                     bool is_new_profile) {
  // Error already exists or has been previously shown.
  if (base::Contains(errors_, extension->id()) ||
      shown_ids_.count(extension->id()) > 0)
    return;

  ExtensionManagement* extension_management =
      ExtensionManagementFactory::GetForBrowserContext(browser_context_);
  ExternalInstallError::AlertType alert_type =
      (extension_management->UpdatesFromWebstore(*extension) && !is_new_profile)
          ? ExternalInstallError::BUBBLE_ALERT
          : ExternalInstallError::MENU_ALERT;

  std::unique_ptr<ExternalInstallError> error(new ExternalInstallError(
      browser_context_, extension->id(), alert_type, this));
  shown_ids_.insert(extension->id());
  errors_.insert(std::make_pair(extension->id(), std::move(error)));
}

void ExternalInstallManager::RemoveExternalInstallError(
    const std::string& extension_id) {
  auto iter = errors_.find(extension_id);
  if (iter != errors_.end()) {
    // The |extension_id| may be owned by the ExternalInstallError, which is
    // deleted subsequently. To avoid any UAFs, make a safe copy of
    // |extension_id| now.
    ExtensionId extension_id_copy = extension_id;

    if (iter->second.get() == currently_visible_install_alert_)
      currently_visible_install_alert_ = nullptr;
    errors_.erase(iter);
    // No need to erase the ID from |unacknowledged_ids_|; it's already in
    // |shown_ids_|.
    UpdateExternalExtensionAlert();
  }
}

void ExternalInstallManager::UpdateExternalExtensionAlert() {
  // If the feature is not enabled do nothing.
  if (!IsPromptingEnabled())
    return;

  // Look for any extensions that were disabled because of being unacknowledged
  // external extensions.
  const ExtensionSet& disabled_extensions =
      ExtensionRegistry::Get(browser_context_)->disabled_extensions();
  const ExtensionSet& blocked_extensions =
      ExtensionRegistry::Get(browser_context_)->blocked_extensions();

  // The list of ids can be mutated during this loop, so make a copy.
  const std::set<ExtensionId> ids_copy = unacknowledged_ids_;
  for (const auto& id : ids_copy) {
    if (base::Contains(errors_, id) || shown_ids_.count(id) > 0)
      continue;

    // Ignore the blocked and disabled extensions. They will be put into
    // disabled list once unblocked.
    if (blocked_extensions.GetByID(id))
      continue;

    const Extension* extension = disabled_extensions.GetByID(id);
    CHECK(extension);

    // Warn the user about the suspicious extension.
    if (extension_prefs_->IncrementAcknowledgePromptCount(id) >
        kMaxExtensionAcknowledgePromptCount) {
      // Stop prompting for this extension and record metrics.
      extension_prefs_->AcknowledgeExternalExtension(id);
      unacknowledged_ids_.erase(id);
      continue;
    }

    if (is_first_run_)
      extension_prefs_->SetExternalInstallFirstRun(id);

    // |first_run| is true if the extension was installed during a first run
    // (even if it's post-first run now).
    AddExternalInstallError(extension,
                            extension_prefs_->IsExternalInstallFirstRun(id));
  }
}

void ExternalInstallManager::AcknowledgeExternalExtension(
    const std::string& id) {
  unacknowledged_ids_.erase(id);
  extension_prefs_->AcknowledgeExternalExtension(id);
  UpdateExternalExtensionAlert();
}

void ExternalInstallManager::DidChangeInstallAlertVisibility(
    ExternalInstallError* external_install_error,
    bool visible) {
  if (visible) {
    currently_visible_install_alert_ = external_install_error;
  } else if (!visible &&
             currently_visible_install_alert_ == external_install_error) {
    currently_visible_install_alert_ = nullptr;
  }
}

std::vector<ExternalInstallError*>
ExternalInstallManager::GetErrorsForTesting() {
  std::vector<ExternalInstallError*> errors;
  for (auto const& error : errors_)
    errors.push_back(error.second.get());
  return errors;
}

void ExternalInstallManager::ClearShownIdsForTesting() {
  shown_ids_.clear();
}

void ExternalInstallManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!unacknowledged_ids_.count(extension->id()))
    return;

  // We treat loading as acknowledgement (since the user consciously chose to
  // re-enable the extension).
  AcknowledgeExternalExtension(extension->id());

  // If we had an error for this extension, remove it.
  RemoveExternalInstallError(extension->id());
}

void ExternalInstallManager::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  ExtensionManagement* settings =
      ExtensionManagementFactory::GetForBrowserContext(browser_context_);
  bool is_recommended_by_policy = settings->GetInstallationMode(extension) ==
                                  ExtensionManagement::INSTALLATION_RECOMMENDED;
  // Certain extension locations are specific enough that we can
  // auto-acknowledge any extension that came from one of them.
  // Extensions recommended by policy can also be auto-acknowledged.
  if (Manifest::IsPolicyLocation(extension->location()) ||
      extension->location() == mojom::ManifestLocation::kExternalComponent ||
      is_recommended_by_policy) {
    AcknowledgeExternalExtension(extension->id());
    return;
  }

  if (!IsUnacknowledgedExternalExtension(*extension))
    return;

  unacknowledged_ids_.insert(extension->id());
  UpdateExternalExtensionAlert();
}

void ExternalInstallManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (base::Contains(errors_, extension->id()))
    RemoveExternalInstallError(extension->id());
  unacknowledged_ids_.erase(extension->id());
}

bool ExternalInstallManager::IsUnacknowledgedExternalExtension(
    const Extension& extension) const {
  if (!IsPromptingEnabled())
    return false;

  int disable_reasons = extension_prefs_->GetDisableReasons(extension.id());
  bool is_from_sideload_wipeout =
      (disable_reasons & disable_reason::DISABLE_SIDELOAD_WIPEOUT) != 0;
  // We don't consider extensions that weren't disabled for being external so
  // that we grandfather in extensions. External extensions are only disabled on
  // install with the "prompt for external extensions" feature enabled.
  bool is_disabled_external =
      (disable_reasons & disable_reason::DISABLE_EXTERNAL_EXTENSION) != 0;
  return is_disabled_external && !is_from_sideload_wipeout &&
         Manifest::IsExternalLocation(extension.location()) &&
         !extension_prefs_->IsExternalExtensionAcknowledged(extension.id());
}

}  // namespace extensions
