// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_basic.h"

#include "components/printing/common/print.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chromeos {

PrintViewManagerCrosBasic::PrintViewManagerCrosBasic(
    content::WebContents* web_contents)
    : PrintViewManagerCrosBase(web_contents),
      content::WebContentsUserData<PrintViewManagerCrosBasic>(*web_contents) {}

// static
void PrintViewManagerCrosBasic::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<::printing::mojom::PrintManagerHost>
        receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  CHECK(web_contents);

  // An instance of this class is guaranteed to be available if binding a
  // PrintManagerHost.
  auto* print_manager =
      PrintViewManagerCrosBasic::FromWebContents(web_contents);
  CHECK(print_manager);

  print_manager->BindReceiver(std::move(receiver), rfh);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerCrosBasic);
}  // namespace chromeos
