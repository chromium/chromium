// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros.h"

#include "base/unguessable_token.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "stdint.h"

namespace chromeos {

PrintViewManagerCros::PrintViewManagerCros(content::WebContents* web_contents)
    : PrintViewManagerCrosBase(web_contents),
      content::WebContentsUserData<PrintViewManagerCros>(*web_contents) {}

// static
void PrintViewManagerCros::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<::printing::mojom::PrintManagerHost>
        receiver,
    content::RenderFrameHost* rfh) {
  CHECK(rfh);

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  CHECK(web_contents);

  // An instance of PrintViewManagerCros is guaranteed to be available if
  // binding a PrintManagerHost.
  auto* print_manager = PrintViewManagerCros::FromWebContents(web_contents);
  CHECK(print_manager);

  print_manager->BindReceiver(std::move(receiver), rfh);
}

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

void PrintViewManagerCros::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host_ == render_frame_host) {
    PrintPreviewDone();
  }
  // TODO(jimmyxgong): Attempt to remove a print job if active.
}

bool PrintViewManagerCros::PrintPreviewNow(content::RenderFrameHost* rfh,
                                           bool has_selection) {
  CHECK(rfh);

  // Don't print / print preview crashed tabs.
  // If the render frame is not live, it's possible we'll never get the
  // `RenderFrameDeleted` observer call and may lead to a UaF
  // render_frame_host_.
  if (IsCrashed() || !rfh->IsRenderFrameLive()) {
    return false;
  }

  // TODO(b/339311234): Handle passing a print renderer remote from android.
  GetPrintRenderFrame(rfh)->InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
      mojo::NullAssociatedRemote(),
#endif
      has_selection);
  DCHECK(!render_frame_host_);
  render_frame_host_ = rfh;
  return true;
}

void PrintViewManagerCros::PrintPreviewDone() {
  render_frame_host_ = nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerCros);

}  // namespace chromeos
