// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ash/crosapi/parent_access_ash.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_dialog.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace crosapi {

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
  std::vector<uint8_t> favicon_bitmap;
  gfx::PNGCodec::FastEncodeBGRASkBitmap(*favicon.bitmap(), false,
                                        &favicon_bitmap);

  parent_access_ui::mojom::ParentAccessParamsPtr params =
      parent_access_ui::mojom::ParentAccessParams::New(
          parent_access_ui::mojom::ParentAccessParams::FlowType::kWebsiteAccess,
          parent_access_ui::mojom::FlowTypeParams::NewWebApprovalsParams(
              parent_access_ui::mojom::WebApprovalsParams::New(
                  url, child_display_name, favicon_bitmap)));

  chromeos::ParentAccessDialogProvider provider;

  chromeos::ParentAccessDialogProvider::ShowError show_dialog_result =
      provider.Show(
          std::move(params),
          base::BindOnce(
              [](std::unique_ptr<chromeos::ParentAccessDialog::Result> result)
                  -> void {
                // TODO(b/200587178): Handle ParentAccessDialogCallback.
              }));

  crosapi::mojom::ParentAccessResultPtr result =
      crosapi::mojom::ParentAccessResult::New();

  // This result indicates basic errors that can occur synchronously.
  // TODO(b/246671931) Other async results will be dealt with in the
  // ParentAccessDialogCallback when it is ready.
  switch (show_dialog_result) {
    case chromeos::ParentAccessDialogProvider::ShowError::kDialogAlreadyVisible:
      result->status = crosapi::mojom::ParentAccessResult::Status::kError;
      result->error_type =
          crosapi::mojom::ParentAccessResult::ErrorType::kAlreadyVisible;
      break;
    case chromeos::ParentAccessDialogProvider::ShowError::kNotAChildUser:
      result->status = crosapi::mojom::ParentAccessResult::Status::kError;
      result->error_type =
          crosapi::mojom::ParentAccessResult::ErrorType::kNotAChildUser;
      break;
    case chromeos::ParentAccessDialogProvider::ShowError::kNone:
      result->status = crosapi::mojom::ParentAccessResult::Status::kUnknown;
      break;
  }
  // TODO(b/246671931): Wait until we get the result form the dialog before
  // running the crosapi callback.
  std::move(callback).Run(std::move(result));
}

}  // namespace crosapi
