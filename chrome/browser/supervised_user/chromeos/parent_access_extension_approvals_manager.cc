// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/install_prompt_permissions.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

ParentAccessExtensionApprovalsManager::ParentAccessExtensionApprovalsManager()
    : ParentAccessExtensionApprovalsManager(
          std::make_unique<ash::ParentAccessDialogProvider>()) {}

ParentAccessExtensionApprovalsManager::ParentAccessExtensionApprovalsManager(
    std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider)
    : dialog_provider_(std::move(dialog_provider)) {}

ParentAccessExtensionApprovalsManager::
    ~ParentAccessExtensionApprovalsManager() = default;

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

  using parent_access_ui::mojom::ExtensionApprovalsParams;
  using parent_access_ui::mojom::ExtensionApprovalsParamsPtr;
  using parent_access_ui::mojom::ExtensionPermission;
  using parent_access_ui::mojom::ExtensionPermissionPtr;
  using parent_access_ui::mojom::FlowTypeParams;
  using parent_access_ui::mojom::ParentAccessParams;
  using parent_access_ui::mojom::ParentAccessParamsPtr;

  std::vector<ExtensionPermissionPtr> extension_permissions;
  extension_permissions.reserve(permissions_count);
  for (size_t i = 0; i < permissions_count; ++i) {
    ExtensionPermissionPtr permission = ExtensionPermission::New(
        prompt_permissions.permissions[i], prompt_permissions.details[i]);
    extension_permissions.push_back(std::move(permission));
  }

  // Convert icon to a bitmap representation.
  std::optional<std::vector<uint8_t>> icon_bitmap =
      gfx::PNGCodec::FastEncodeBGRASkBitmap(*icon.bitmap(), false);
  ExtensionApprovalsParamsPtr extension_params = ExtensionApprovalsParams::New(
      base::UTF8ToUTF16(extension.name()),
      icon_bitmap.value_or(std::vector<uint8_t>()),
      base::UTF8ToUTF16(supervised_user::GetAccountGivenName(*profile)),
      std::move(extension_permissions));

  // Assemble the parameters for a extension approval request.
  ParentAccessParamsPtr params = ParentAccessParams::New(
      ParentAccessParams::FlowType::kExtensionAccess,
      FlowTypeParams::NewExtensionApprovalsParams(std::move(extension_params)),
      extension_install_mode == ExtensionInstallMode::kInstallationDenied);

  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));
  auto show_error = dialog_provider_->Show(
      std::move(params),
      base::BindOnce(
          &ParentAccessExtensionApprovalsManager::OnParentAccessDialogClosed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback1)));
  if (show_error != ash::ParentAccessDialogProvider::ShowError::kNone) {
    std::move(callback2).Run(SupervisedExtensionApprovalResult::kFailed);
  }
}

void ParentAccessExtensionApprovalsManager::OnParentAccessDialogClosed(
    SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback,
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  switch (result->status) {
    case ash::ParentAccessDialog::Result::Status::kApproved:
      std::move(callback).Run(SupervisedExtensionApprovalResult::kApproved);
      return;

    case ash::ParentAccessDialog::Result::Status::kDeclined:
    case ash::ParentAccessDialog::Result::Status::kCanceled:
      std::move(callback).Run(SupervisedExtensionApprovalResult::kCanceled);
      return;

    case ash::ParentAccessDialog::Result::Status::kError:
      std::move(callback).Run(SupervisedExtensionApprovalResult::kFailed);
      return;

    case ash::ParentAccessDialog::Result::Status::kDisabled:
      SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::EnablementState::
              kFailedToEnable);
      std::move(callback).Run(SupervisedExtensionApprovalResult::kBlocked);
      return;
  }
}

}  // namespace extensions
