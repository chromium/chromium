// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/functional/bind.h"
#include "printing/printing_context_android.h"
#endif

namespace printing {

PrintViewManagerBasic::PrintViewManagerBasic(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents),
      content::WebContentsUserData<PrintViewManagerBasic>(*web_contents) {
#if BUILDFLAG(IS_ANDROID)
  set_pdf_writing_done_callback(
      base::BindRepeating(&PrintingContextAndroid::PdfWritingDone));
#endif
}

PrintViewManagerBasic::~PrintViewManagerBasic() {
#if BUILDFLAG(IS_ANDROID)
  // Must do this call here and not let ~PrintViewManagerBase do it as
  // TerminatePrintJob() calls PdfWritingDone() and if that is done from
  // ~PrintViewManagerBase then a pure virtual call is done.
  DisconnectFromCurrentPrintJob();
#endif
}

// static
void PrintViewManagerBasic::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = PrintViewManagerBasic::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

#if BUILDFLAG(IS_ANDROID)
void PrintViewManagerBasic::PdfWritingDone(int page_count) {
  pdf_writing_done_callback().Run(page_count);
}
#endif

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerBasic);

}  // namespace printing
