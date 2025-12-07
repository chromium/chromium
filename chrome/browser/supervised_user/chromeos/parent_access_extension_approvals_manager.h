// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {

class Extension;

class ParentAccessExtensionApprovalsManager {
 public:
  ParentAccessExtensionApprovalsManager();

  // Injects `dialog_provider` for testing.
  explicit ParentAccessExtensionApprovalsManager(
      std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider);

  ParentAccessExtensionApprovalsManager(
      const ParentAccessExtensionApprovalsManager&) = delete;
  ParentAccessExtensionApprovalsManager& operator=(
      const ParentAccessExtensionApprovalsManager&) = delete;
  ~ParentAccessExtensionApprovalsManager();

  // Models the user's permission to install extension,
  enum ExtensionInstallMode {
    kInstallationPermitted = 0,
    kInstallationDenied = 1
  };

  // Opens the ParentAccessDialog.
  void ShowParentAccessDialog(
      const Extension& extension,
      content::BrowserContext* context,
      const gfx::ImageSkia& icon,
      ExtensionInstallMode extension_install_mode,
      SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback);

 private:
  void OnParentAccessDialogClosed(
      SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback,
      std::unique_ptr<ash::ParentAccessDialog::Result> result);

  std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider_;

  base::WeakPtrFactory<ParentAccessExtensionApprovalsManager> weak_ptr_factory_{
      this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_
