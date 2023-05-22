// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/parent_access_extension_approvals_manager.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_prompt_permissions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_set.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"

namespace {
extensions::TestExtensionApprovalsManagerObserver* test_observer = nullptr;
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
    ExtensionInstallMode extension_install_mode,
    SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback) {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);

  // Load permission strings.
  InstallPromptPermissions prompt_permissions;
  std::unique_ptr<const PermissionSet> permissions_to_display =
      util::GetInstallPromptPermissionSetForExtension(
          &extension, profile,
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
                  base::UTF8ToUTF16(
                      supervised_user::GetAccountGivenName(*profile)),
                  std::move(permissions))),
          /* is_disabled= */ extension_install_mode ==
              ExtensionInstallMode::kInstallationDenied);

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
    if (test_observer) {
      test_observer->OnTestParentAccessDialogCreated();
    }
  }
}

ash::ParentAccessDialogProvider*
ParentAccessExtensionApprovalsManager::SetDialogProviderForTest(
    std::unique_ptr<ash::ParentAccessDialogProvider> provider) {
  dialog_provider_ = std::move(provider);
  return dialog_provider_.get();
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
      SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
          SupervisedUserExtensionsMetricsRecorder::EnablementState::
              kFailedToEnable);
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

TestExtensionApprovalsManagerObserver::TestExtensionApprovalsManagerObserver(
    TestExtensionApprovalsManagerObserver* observer) {
  test_observer = observer;
}

TestExtensionApprovalsManagerObserver::
    ~TestExtensionApprovalsManagerObserver() {
  test_observer = nullptr;
}
}  // namespace extensions
