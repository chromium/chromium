// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/file_handlers/web_file_handlers_permission_handler.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/pref_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

namespace extensions {

namespace {

constexpr PrefMap kPrefShouldOpen{"web_file_handlers_should_open",
                                  PrefType::kBool,
                                  PrefScope::kExtensionSpecific};

// Get extension prefs for profile.
ExtensionPrefs* GetExtensionPrefs(Profile* profile) {
  return ExtensionPrefs::Get(profile);
}

// Get dictionary of extension prefs.
std::optional<bool> GetExtensionPrefsAsBoolean(
    ExtensionPrefs* extension_prefs,
    const ExtensionId& extension_id) {
  bool out_value = false;
  if (extension_prefs->ReadPrefAsBoolean(extension_id, kPrefShouldOpen,
                                         &out_value)) {
    return out_value;
  }
  return std::nullopt;
}

// Get extension pref result.
std::optional<bool> GetExtensionPrefsAsBoolean(const Extension& extension,
                                               Profile* profile) {
  return GetExtensionPrefsAsBoolean(GetExtensionPrefs(profile), extension.id());
}

}  // namespace

// Open file using Web File Handlers. Maybe show a file launch dialog first.
WebFileHandlersPermissionHandler::WebFileHandlersPermissionHandler(
    Profile* profile)
    : profile_(profile) {}

WebFileHandlersPermissionHandler::~WebFileHandlersPermissionHandler() = default;

void WebFileHandlersPermissionHandler::CallbackAfterDialog(
    const ExtensionId& extension_id,
    const std::vector<std::u16string>& file_types,
    CallbackType launch_callback,
    bool should_open,
    bool should_remember) {
  // Maybe remember the decision to open the file.
  if (should_remember) {
    auto* extension_prefs = GetExtensionPrefs(profile_);
    extension_prefs->SetBooleanPref(extension_id, kPrefShouldOpen, should_open);
  }

  // Maybe open the file.
  std::move(launch_callback).Run(should_open);
}

void WebFileHandlersPermissionHandler::Confirm(
    const Extension& extension,
    const std::vector<base::SafeBaseName>& base_names,
    CallbackType launch_callback) {
  CHECK(!base_names.empty());

  // Default installed extensions can skip the file launch dialog.
  // TODO(crbug.com/40269541): Remove the allowlist check after development.
  // Also, for development and manual testing purposes, the manifest_features
  // allowlist can also bypass the dialog.
  if (WebFileHandlers::CanBypassPermissionDialog(extension)) {
    std::move(launch_callback).Run(/*should_open=*/true);
    return;
  }

  auto apps_file_handlers = GetAppsFileHandlers(extension);
  const std::vector<std::u16string> file_types =
      web_app::TransformFileExtensionsForDisplay(
          apps::GetFileExtensionsFromFileHandlers(apps_file_handlers));

  // Get saved preferences that represents previous file opening decisions.
  const auto current_prefs = GetExtensionPrefsAsBoolean(extension, profile_);
  if (current_prefs.has_value()) {
    std::move(launch_callback).Run(current_prefs.value());
    return;
  }

  // Maybe open the file. Maybe remember the decision to open the file.
  auto callback_after_dialog =
      base::BindOnce(&WebFileHandlersPermissionHandler::CallbackAfterDialog,
                     weak_factory_.GetWeakPtr(), extension.id(), file_types,
                     std::move(launch_callback));

  // Present a contextual file launch dialog.
  file_handlers::ShowWebFileHandlersFileLaunchDialog(
      std::move(base_names), file_types, std::move(callback_after_dialog));
}

// static
const apps::FileHandlers WebFileHandlersPermissionHandler::GetAppsFileHandlers(
    const Extension& extension) {
  apps::FileHandlers web_file_handlers;
  auto* file_handlers = WebFileHandlers::GetFileHandlers(extension);

  if (!file_handlers) {
    return web_file_handlers;
  }

  for (const auto& web_file_handler : *file_handlers) {
    apps::FileHandler file_handler;
    file_handler.action = GURL(web_file_handler.file_handler.action);
    file_handler.display_name =
        base::UTF8ToUTF16(web_file_handler.file_handler.name);

    // Compute `accept`, which contains all mime types and file extensions.
    apps::FileHandler::Accept accept;
    for (const auto [mime_type, file_extension_list] :
         web_file_handler.file_handler.accept.additional_properties) {
      apps::FileHandler::AcceptEntry accept_entry;
      accept_entry.mime_type = mime_type;
      base::flat_set<std::string> file_extensions;
      for (const auto& file_extension : file_extension_list.GetList()) {
        file_extensions.insert(file_extension.GetString());
      }
      accept_entry.file_extensions = file_extensions;
      accept.emplace_back(accept_entry);
    }
    file_handler.accept = accept;

    // The default launch type is single client.
    file_handler.launch_type =
        web_file_handler.launch_type ==
                WebFileHandler::LaunchType::kSingleClient
            ? apps::FileHandler::LaunchType::kSingleClient
            : apps::FileHandler::LaunchType::kMultipleClients;

    web_file_handlers.emplace_back(file_handler);
  }

  return web_file_handlers;
}

}  // namespace extensions
