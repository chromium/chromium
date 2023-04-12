// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace {

std::u16string GetActiveUserFirstName() {
  // TODO(b/250924204): Support fetching active user name in LaCrOS.
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  const user_manager::User* user = manager->GetActiveUser();
  return user->GetGivenName();
}

}  // namespace

namespace extensions {

ParentAccessExtensionApprovalsManager::ParentAccessExtensionApprovalsManager() =
    default;

ParentAccessExtensionApprovalsManager::
    ~ParentAccessExtensionApprovalsManager() = default;

void ParentAccessExtensionApprovalsManager::ShowParentAccessDialog(
    const Extension& extension,
    content::BrowserContext* context,
    const gfx::ImageSkia& icon,
    SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback) {
  // Load permission strings.
  InstallPromptPermissions prompt_permissions;
  std::unique_ptr<const PermissionSet> permissions_to_display =
      util::GetInstallPromptPermissionSetForExtension(
          &extension, Profile::FromBrowserContext(context),
          // Matches behavior of regular extension install prompt because this
          // prompt is never used for delegated permissions, which is the only
          // time optional permissions are shown.
          false /* include_optional_permissions */
      );
  prompt_permissions.LoadFromPermissionSet(permissions_to_display.get(),
                                           extension.GetType());
  parent_access_ui::mojom::ExtensionPermissionsPtr permissions =
      parent_access_ui::mojom::ExtensionPermissions::New(
          prompt_permissions.permissions, prompt_permissions.details);

  // Convert icon to a bitmap representation.
  std::vector<uint8_t> icon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*icon.bitmap(), false, &icon_bitmap);

  // Assemble the parameters for a extension approval request.
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::
              kExtensionAccess,
          parent_access_ui::mojom::FlowTypeParams::NewExtensionApprovalsParams(
              parent_access_ui::mojom::ExtensionApprovalsParams::New(
                  base::UTF8ToUTF16(extension.name()), icon_bitmap,
                  GetActiveUserFirstName(), std::move(permissions))),
          /* is_disabled= */ !CanInstallExtensions(context));

  ash::ParentAccessDialogProvider::ShowError show_error =
      GetParentAccessDialogProvider()->Show(
          std::move(params),
          base::BindOnce(&ParentAccessExtensionApprovalsManager::
                             OnParentAccessDialogClosed,
                         base::Unretained(this)));

  if (show_error != ash::ParentAccessDialogProvider::ShowError::kNone) {
    std::move(callback).Run(
        SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed);
  } else {
    done_callback_ = std::move(callback);
  }
}

ash::ParentAccessDialogProvider*
ParentAccessExtensionApprovalsManager::SetDialogProviderForTest(
    std::unique_ptr<ash::ParentAccessDialogProvider> provider) {
  dialog_provider_ = std::move(provider);
  return dialog_provider_.get();
}

bool ParentAccessExtensionApprovalsManager::CanInstallExtensions(
    content::BrowserContext* context) const {
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForBrowserContext(context);
  return supervised_user_service->CanInstallExtensions();
}

void ParentAccessExtensionApprovalsManager::OnParentAccessDialogClosed(
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  switch (result->status) {
    case ash::ParentAccessDialog::Result::Status::kApproved:
      std::move(done_callback_)
          .Run(SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
                   kApproved);
      break;

    case ash::ParentAccessDialog::Result::Status::kDeclined:
    case ash::ParentAccessDialog::Result::Status::kCanceled:
      std::move(done_callback_)
          .Run(SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
                   kCanceled);
      break;

    case ash::ParentAccessDialog::Result::Status::kError:
      std::move(done_callback_)
          .Run(SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
                   kFailed);
      break;
    case ash::ParentAccessDialog::Result::Status::kDisabled:
      std::move(done_callback_)
          .Run(SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
                   kBlocked);
      break;
  }
}

ash::ParentAccessDialogProvider*
ParentAccessExtensionApprovalsManager::GetParentAccessDialogProvider() {
  if (!dialog_provider_) {
    dialog_provider_ = std::make_unique<ash::ParentAccessDialogProvider>();
  }
  return dialog_provider_.get();
}

}  // namespace extensions
