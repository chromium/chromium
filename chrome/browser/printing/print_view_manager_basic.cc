// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "printing/printing_context_android.h"
#include "ui/android/window_android.h"
#endif

namespace printing {

PrintViewManagerBasic::PrintViewManagerBasic(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents),
      content::WebContentsUserData<PrintViewManagerBasic>(*web_contents) {
#if BUILDFLAG(IS_ANDROID)
  // When printing completes, it ultimately triggers this callback.
  // The print dialog is modal, so user cannot move the tab to another window,
  // hence it is safe to assume the window is the same one as when printing
  // started.
  set_pdf_writing_done_callback(base::BindRepeating(
      [](PrintViewManagerBasic* manager, int page_count) {
        if (manager->web_contents()) {
          if (auto* window =
                  manager->web_contents()->GetTopLevelNativeWindow()) {
            PrintingContextAndroid::PdfWritingDone(page_count, window);
            return;
          }
        }
        LOG(ERROR)
            << "Could not notify PdfWritingDone: Native window not found";
      },
      // Safe to skip memory management because `this` (via
      // PrintViewManagerBase) owns the callback, guaranteeing it won't be
      // executed after this object is destroyed.
      base::Unretained(this)));
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
void PrintViewManagerBasic::SetupScriptedPrintAndroid(
    SetupScriptedPrintAndroidCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost& rfh = CurrentTargetFrame();
  DCHECK(rfh.IsRenderFrameLive());
  if (!rfh.IsActive()) {
    std::move(callback).Run();
    return;
  }

  // Start Printing Flow via PrinterQuery
  std::unique_ptr<PrinterQuery> printer_query =
      queue()->CreatePrinterQuery(rfh.GetGlobalId());
  auto* printer_query_ptr = printer_query.get();

  printer_query_ptr->GetSettingsFromUser(
      /*expected_page_count=*/0, /*has_selection=*/false,
      mojom::MarginType::kDefaultMargins, /*is_scripted=*/true,
      /*is_modifiable=*/true,
      base::BindOnce(&PrintViewManagerBasic::OnSetupScriptedPrintAndroidDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(printer_query)));
}

void PrintViewManagerBasic::OnSetupScriptedPrintAndroidDone(
    SetupScriptedPrintAndroidCallback callback,
    std::unique_ptr<PrinterQuery> printer_query) {
  std::move(callback).Run();
  // Note: `printer_query` is intentionally dropped here. On Android, the print
  // dialog manages the actual print request natively, so the `PrinterQuery`
  // does not need to be queued for the renderer's `ScriptedPrint` IPC.
}

void PrintViewManagerBasic::PdfWritingDone(int page_count) {
  pdf_writing_done_callback().Run(page_count);
}
#endif  // BUILDFLAG(IS_ANDROID)

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerBasic);

}  // namespace printing
