// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

crosapi::mojom::ParentAccessResultPtr DialogResultToParentAccessResult(
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  crosapi::mojom::ParentAccessResultPtr parent_access_result;

  switch (result->status) {
    case ash::ParentAccessDialog::Result::Status::kApproved:
      parent_access_result = mojom::ParentAccessResult::NewApproved(
          crosapi::mojom::ParentAccessApprovedResult::New(
              result->parent_access_token,
              result->parent_access_token_expire_timestamp));
      break;

    case ash::ParentAccessDialog::Result::Status::kDeclined:
      parent_access_result = mojom::ParentAccessResult::NewDeclined(
          crosapi::mojom::ParentAccessDeclinedResult::New());
      break;

    case ash::ParentAccessDialog::Result::Status::kCanceled:
      parent_access_result = mojom::ParentAccessResult::NewCanceled(
          crosapi::mojom::ParentAccessCanceledResult::New());
      break;

    case ash::ParentAccessDialog::Result::Status::kError:
      parent_access_result = mojom::ParentAccessResult::NewError(
          crosapi::mojom::ParentAccessErrorResult::New(
              crosapi::mojom::ParentAccessErrorResult::Type::kUnknown));
      break;
    case ash::ParentAccessDialog::Result::Status::kDisabled:
      parent_access_result = mojom::ParentAccessResult::NewDisabled(
          crosapi::mojom::ParentAccessDisabledResult::New());
      break;
  }

  return parent_access_result;
}

crosapi::mojom::ParentAccessResultPtr ShowErrorToParentAccessResultError(
    ash::ParentAccessDialogProvider::ShowError show_dialog_result) {
  crosapi::mojom::ParentAccessResultPtr parent_access_result;

  // This result indicates basic errors that can occur synchronously.
  switch (show_dialog_result) {
    case ash::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible:
      parent_access_result = mojom::ParentAccessResult::NewError(
          crosapi::mojom::ParentAccessErrorResult::New(
              crosapi::mojom::ParentAccessErrorResult::Type::kAlreadyVisible));
      break;
    case ash::ParentAccessDialogProvider::ShowError::kNotAChildUser:
      parent_access_result = mojom::ParentAccessResult::NewError(
          crosapi::mojom::ParentAccessErrorResult::New(
              crosapi::mojom::ParentAccessErrorResult::Type::kNotAChildUser));
      break;
    case ash::ParentAccessDialogProvider::ShowError::kNone:
      // The dialog is showing successfully. Wait until we get the result from
      // the dialog before running the crosapi callback. This will be handled
      // in OnParentAccessDialogClosed.
      break;
  }
  return parent_access_result;
}

ParentAccessAsh::ParentAccessAsh() = default;

ParentAccessAsh::~ParentAccessAsh() = default;

void ParentAccessAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ParentAccess> receiver) {
  receivers_.Add(this, std::move(receiver));
}

// crosapi::mojom::ParentAccess:
void ParentAccessAsh::GetWebsiteParentApproval(
    const GURL& url,
    const std::u16string& child_display_name,
    const gfx::ImageSkia& favicon,
    GetWebsiteParentApprovalCallback callback) {
  // Encode the favicon as a PNG bitmap.
  std::vector<uint8_t> favicon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*favicon.bitmap(), false,
                                        &favicon_bitmap);

  // Assemble the parameters for a website access request.
  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New(
                  url, child_display_name, favicon_bitmap)),
          /* is_disabled= */ false);
  ShowParentAccessDialog(std::move(params), std::move(callback));
}

void ParentAccessAsh::GetExtensionParentApproval(
    const std::u16string& extension_name,
    const std::u16string& child_display_name,
    const gfx::ImageSkia& icon,
    const std::vector<crosapi::mojom::ExtensionPermissionPtr> permissions,
    bool requests_disabled,
    GetExtensionParentApprovalCallback callback) {
  using parent_access_ui::mojom::ExtensionApprovalsParams;
  using parent_access_ui::mojom::ExtensionApprovalsParamsPtr;
  using parent_access_ui::mojom::ExtensionPermission;
  using parent_access_ui::mojom::ExtensionPermissionPtr;
  using parent_access_ui::mojom::FlowTypeParams;
  using parent_access_ui::mojom::ParentAccessParams;
  using parent_access_ui::mojom::ParentAccessParamsPtr;

  std::vector<parent_access_ui::mojom::ExtensionPermissionPtr>
      extension_permissions;
  extension_permissions.reserve(permissions.size());
  for (size_t i = 0; i < permissions.size(); ++i) {
    ExtensionPermissionPtr permission = ExtensionPermission::New(
        permissions[i]->permission, permissions[i]->details);
    extension_permissions.push_back(std::move(permission));
  }

  // Convert icon to a bitmap representation.
  std::vector<uint8_t> icon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*icon.bitmap(), false, &icon_bitmap);
  ExtensionApprovalsParamsPtr extension_params = ExtensionApprovalsParams::New(
      extension_name, icon_bitmap, child_display_name,
      std::move(extension_permissions));

  // Assemble the parameters for a extension approval request.
  ParentAccessParamsPtr params = ParentAccessParams::New(
      ParentAccessParams::FlowType::kExtensionAccess,
      FlowTypeParams::NewExtensionApprovalsParams(std::move(extension_params)),
      requests_disabled);
  ShowParentAccessDialog(std::move(params), std::move(callback));
}

ash::ParentAccessDialogProvider* ParentAccessAsh::GetDialogProvider() {
  if (!dialog_provider_) {
    dialog_provider_ = std::make_unique<ash::ParentAccessDialogProvider>();
  }

  return dialog_provider_.get();
}

void ParentAccessAsh::ShowParentAccessDialog(
    parent_access_ui::mojom::ParentAccessParamsPtr params,
    ParentAccessAsh::ParentAccessCallback callback) {
  ash::ParentAccessDialogProvider::ShowError show_error =
      GetDialogProvider()->Show(
          std::move(params),
          base::BindOnce(&ParentAccessAsh::OnParentAccessDialogClosed,
                         base::Unretained(this)));

  crosapi::mojom::ParentAccessResultPtr show_error_result =
      ShowErrorToParentAccessResultError(show_error);

  if (show_error_result) {
    // This result indicates basic errors that can occur synchronously.
    std::move(callback).Run(std::move(show_error_result));
    return;
  } else {
    // No show-error, so save the callback so we can use it to respond when the
    // dialog completes.
    DCHECK(!callback_);
    callback_ = std::move(callback);
  }
}

ash::ParentAccessDialogProvider* ParentAccessAsh::SetDialogProviderForTest(
    std::unique_ptr<ash::ParentAccessDialogProvider> provider) {
  dialog_provider_ = std::move(provider);
  return dialog_provider_.get();
}

void ParentAccessAsh::OnParentAccessDialogClosed(
    std::unique_ptr<ash::ParentAccessDialog::Result> result) {
  if (callback_) {
    std::move(callback_).Run(
        DialogResultToParentAccessResult(std::move(result)));
  }
}

}  // namespace crosapi
