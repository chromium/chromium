// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "stdint.h"

namespace chromeos {

PrintViewManagerCros::PrintViewManagerCros(content::WebContents* web_contents)
    : PrintViewManagerCrosBase(web_contents),
      content::WebContentsUserData<PrintViewManagerCros>(*web_contents) {}

// TODO(jimmyxgong): Implement stubs.
void PrintViewManagerCros::DidShowPrintDialog() {}

void PrintViewManagerCros::SetupScriptedPrintPreview(
    SetupScriptedPrintPreviewCallback callback) {}

void PrintViewManagerCros::ShowScriptedPrintPreview(bool source_is_modifiable) {
}

void PrintViewManagerCros::RequestPrintPreview(
    ::printing::mojom::RequestPrintPreviewParamsPtr params) {}

void PrintViewManagerCros::CheckForCancel(int32_t preview_ui_id,
                                          int32_t request_id,
                                          CheckForCancelCallback callback) {}

bool PrintViewManagerCros::PrintNow(content::RenderFrameHost* rfh) {
  return false;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerCros);

}  // namespace chromeos
