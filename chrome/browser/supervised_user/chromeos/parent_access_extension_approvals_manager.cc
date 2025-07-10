// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/chromeos/chromeos_utils.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

ParentAccessExtensionApprovalsManager::ParentAccessExtensionApprovalsManager()
    : ParentAccessExtensionApprovalsManager(nullptr) {}

ParentAccessExtensionApprovalsManager::ParentAccessExtensionApprovalsManager(
    std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider) {
  if (dialog_provider) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->parent_access_ash()
        ->SetDialogProviderForTest(std::move(dialog_provider));  // IN-TEST
    dialog_provider_injected_ = true;
  }
}

ParentAccessExtensionApprovalsManager::
    ~ParentAccessExtensionApprovalsManager() {
  if (dialog_provider_injected_) {
    crosapi::CrosapiManager::Get()
        ->crosapi_ash()
        ->parent_access_ash()
        ->SetDialogProviderForTest(nullptr);  // IN-TEST
  }
}

void ParentAccessExtensionApprovalsManager::ShowParentAccessDialog(
    const Extension& extension,
    content::BrowserContext* context,
    const gfx::ImageSkia& icon,
    ExtensionInstallMode extension_install_mode,
    SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);

  // Load permission strings.
  InstallPromptPermissions prompt_permissions;
  std::unique_ptr<const PermissionSet> permissions_to_display =
      util::GetInstallPromptPermissionSetForExtension(&extension, profile);
  prompt_permissions.LoadFromPermissionSet(permissions_to_display.get(),
                                           extension.GetType());
  const size_t permissions_count = prompt_permissions.permissions.size();
  CHECK_EQ(permissions_count, prompt_permissions.details.size());

  std::vector<crosapi::mojom::ExtensionPermissionPtr> extension_permissions;
  extension_permissions.reserve(permissions_count);
  for (size_t i = 0; i < permissions_count; ++i) {
    crosapi::mojom::ExtensionPermissionPtr permission =
        crosapi::mojom::ExtensionPermission::New(
            prompt_permissions.permissions[i], prompt_permissions.details[i]);
    extension_permissions.push_back(std::move(permission));
  }

  crosapi::mojom::ParentAccess* parent_access =
      supervised_user::GetParentAccessApi();
  CHECK(parent_access);
  parent_access->GetExtensionParentApproval(
      base::UTF8ToUTF16(extension.name()),
      base::UTF8ToUTF16(supervised_user::GetAccountGivenName(*profile)), icon,
      std::move(extension_permissions),
      /* requests_disabled= */ extension_install_mode ==
          ExtensionInstallMode::kInstallationDenied,
      base::BindOnce(
          &ParentAccessExtensionApprovalsManager::OnParentAccessDialogClosed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ParentAccessExtensionApprovalsManager::OnParentAccessDialogClosed(
    SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback,
    crosapi::mojom::ParentAccessResultPtr result) {
  switch (result->which()) {
    case crosapi::mojom::ParentAccessResult::Tag::kApproved:
      std::move(callback).Run(
          SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kApproved);
      break;

    case crosapi::mojom::ParentAccessResult::Tag::kDeclined:
    case crosapi::mojom::ParentAccessResult::Tag::kCanceled:
      std::move(callback).Run(
          SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kCanceled);
      break;

    case crosapi::mojom::ParentAccessResult::Tag::kError:
      std::move(callback).Run(
          SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kFailed);
      break;
    case crosapi::mojom::ParentAccessResult::Tag::kDisabled:
      SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::EnablementState::
              kFailedToEnable);
      std::move(callback).Run(
          SupervisedUserExtensionsDelegate::ExtensionApprovalResult::kBlocked);
      break;
  }
}

}  // namespace extensions
