// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/show_permissions_dialog_helper.h"

#include <memory>
#include <utility>

#include "apps/saved_files_service.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/file_system/saved_file_entry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

ShowPermissionsDialogHelper::ShowPermissionsDialogHelper(
    Profile* profile,
    base::OnceClosure on_complete)
    : profile_(profile), on_complete_(std::move(on_complete)) {}

ShowPermissionsDialogHelper::~ShowPermissionsDialogHelper() = default;

// static
void ShowPermissionsDialogHelper::Show(content::BrowserContext* browser_context,
                                       content::WebContents* web_contents,
                                       const Extension* extension,
                                       base::OnceClosure on_complete) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Show the new-style extensions dialog when it is available. It is currently
  // unavailable by default on Mac.
  if (CanPlatformShowAppInfoDialog()) {
    ShowAppInfoInNativeDialog(web_contents, profile, extension,
                              std::move(on_complete));
    return;  // All done.
  }

  // ShowPermissionsDialogHelper manages its own lifetime.
  ShowPermissionsDialogHelper* helper =
      new ShowPermissionsDialogHelper(profile, std::move(on_complete));
  helper->ShowPermissionsDialog(web_contents, extension);
}

void ShowPermissionsDialogHelper::ShowPermissionsDialog(
    content::WebContents* web_contents,
    const Extension* extension) {
  extension_id_ = extension->id();
  prompt_ = std::make_unique<ExtensionInstallPrompt>(web_contents);
  std::vector<base::FilePath> retained_file_paths;
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kFileSystem)) {
    std::vector<SavedFileEntry> retained_file_entries =
        apps::SavedFilesService::Get(profile_)->GetAllFileEntries(
            extension_id_);
    for (const SavedFileEntry& entry : retained_file_entries)
      retained_file_paths.push_back(entry.path);
  }
  std::vector<std::u16string> retained_device_messages;
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kUsb)) {
    retained_device_messages =
        DevicePermissionsManager::Get(profile_)
            ->GetPermissionMessageStrings(extension_id_);
  }

  // TODO(crbug.com/567839): We reuse the install dialog because it displays the
  // permissions wanted. However, we should be using a separate dialog since
  // this dialog is shown after the extension was already installed.
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::POST_INSTALL_PERMISSIONS_PROMPT));
  prompt->set_retained_files(retained_file_paths);
  prompt->set_retained_device_messages(retained_device_messages);
  // Unretained() is safe because this class manages its own lifetime and
  // deletes itself in OnInstallPromptDone().
  prompt_->ShowDialog(
      base::BindOnce(&ShowPermissionsDialogHelper::OnInstallPromptDone,
                     base::Unretained(this)),
      extension, nullptr, std::move(prompt),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void ShowPermissionsDialogHelper::OnInstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  // This dialog doesn't support the "withhold permissions" checkbox.
  DCHECK_NE(payload.result,
            ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS);

  if (payload.result == ExtensionInstallPrompt::Result::ACCEPTED) {
    // This is true when the user clicks "Revoke File Access."
    const Extension* extension =
        ExtensionRegistry::Get(profile_)
            ->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);

    if (extension)
      apps::SavedFilesService::Get(profile_)->ClearQueue(extension);
    apps::AppLoadService::Get(profile_)
        ->RestartApplicationIfRunning(extension_id_);
  }

  std::move(on_complete_).Run();
  delete this;
}

}  // namespace extensions
