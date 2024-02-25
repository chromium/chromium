// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/cleanup/extension_cleanup_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/app_constants/constants.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

// Extensions that should not be attempted to be uninstalled and reinstalled.
const char* const kExemptExtensions[] = {
    app_constants::kChromeAppId,
    app_constants::kLacrosAppId,
};

namespace chromeos {

ExtensionCleanupHandler::ExtensionCleanupHandler() = default;

ExtensionCleanupHandler::~ExtensionCleanupHandler() = default;

void ExtensionCleanupHandler::Cleanup(CleanupHandlerCallback callback) {
  DCHECK(callback_.is_null());

  profile_ = ProfileManager::GetActiveUserProfile();
  if (!profile_) {
    std::move(callback).Run("There is no active user");
    return;
  }

  extension_service_ =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  callback_ = std::move(callback);
  errors_.clear();
  extensions_to_be_uninstalled_.clear();

  // UninstallExtensions() will trigger the cleanup process by uninstalling all
  // extensions and calling ReinstallExtension() after all extensions are
  // signaled as uninstalled.
  UninstallExtensions();
}

void ExtensionCleanupHandler::UninstallExtensions() {
  std::unordered_set<std::string> policy_exempt_extensions =
      GetCleanupExemptExtensions();

  auto* extension_registry = extensions::ExtensionRegistry::Get(profile_);
  const extensions::ExtensionSet all_installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();

  for (const auto& extension : all_installed_extensions) {
    std::string extension_id = extension->id();
    // Skip the apps/extensions not meant to be uninstalled and reinstalled. The
    // browsing history will be removed by BrowsingDataCleanupHandler and open
    // windows will be closed by OpenWindowsCleanupHandler.
    if (base::Contains(kExemptExtensions, extension_id))
      continue;

    // Skip extensions exempt by policy.
    if (base::Contains(policy_exempt_extensions, extension_id))
      continue;

    extensions_to_be_uninstalled_.insert(extension_id);
  }

  // Exit cleanup handler in case no extensions need to be uninstalled.
  if (extensions_to_be_uninstalled_.empty()) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  for (auto it = extensions_to_be_uninstalled_.begin();
       it != extensions_to_be_uninstalled_.end();) {
    std::u16string error;
    extension_service_->UninstallExtension(
        *it, extensions::UninstallReason::UNINSTALL_REASON_REINSTALL, &error,
        base::BindOnce(&ExtensionCleanupHandler::OnUninstallDataDeleterFinished,
                       base::Unretained(this), *it));

    if (!error.empty()) {
      errors_.push_back(*it + ": " + base::UTF16ToUTF8(error));
      it = extensions_to_be_uninstalled_.erase(it);
    } else {
      ++it;
    }
  }

  // |extensions_to_be_uninstalled_| can be empty if all extensions attempted to
  // be uninstalled have reported errors. We run the callback here because
  // |OnUninstallDataDeleterFinished()| will not be called in case of an error.
  if (extensions_to_be_uninstalled_.empty()) {
    std::string errors = base::JoinString(errors_, "\n");
    std::move(callback_).Run(errors);
  }
}

void ExtensionCleanupHandler::OnUninstallDataDeleterFinished(
    const std::string& extension_id) {
  extensions_to_be_uninstalled_.erase(extension_id);
  if (!extensions_to_be_uninstalled_.empty())
    return;

  if (!errors_.empty()) {
    std::string errors = base::JoinString(errors_, "\n");
    std::move(callback_).Run(errors);
    return;
  }

  ReinstallExtensions();
}

void ExtensionCleanupHandler::ReinstallExtensions() {
  // Reinstall force-installed extensions and external component extensions.
  extension_service_->ReinstallProviderExtensions();

  // Reinstall component extensions.
  extension_service_->component_loader()->AddDefaultComponentExtensions(
      /*skip_session_components=*/false);

  std::move(callback_).Run(std::nullopt);
}

std::unordered_set<std::string>
ExtensionCleanupHandler::GetCleanupExemptExtensions() {
  std::unordered_set<std::string> exempt_extensions;
  const base::Value::List& exempt_list = profile_->GetPrefs()->GetList(
      prefs::kRestrictedManagedGuestSessionExtensionCleanupExemptList);

  for (const base::Value& value : exempt_list) {
    exempt_extensions.insert(value.GetString());
  }
  return exempt_extensions;
}

}  // namespace chromeos
