// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"
#include "printing/printing_context_android.h"
#include "ui/android/window_android.h"

namespace {

uint32_t SaveDataToFd(base::ScopedFD fd,
                      uint32_t page_count,
                      scoped_refptr<base::RefCountedSharedMemoryMapping> data) {
  CHECK(fd.is_valid());
  bool did_write_successfully = base::WriteFileDescriptor(fd.get(), *data);
  return did_write_successfully ? page_count : 0;
}

}  // namespace
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
bool PrintViewManagerBasic::PrintNowAndroid(
    content::RenderFrameHost* rfh,
    std::unique_ptr<PrintSettings> settings,
    base::ScopedFD file_descriptor) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(settings);
  CHECK(file_descriptor.is_valid());

  bool print_selection_only = settings->selection_only();
  if (!StartPrintCommon(rfh, print_selection_only)) {
    return false;
  }

  settings_ = std::move(settings);
  fd_ = std::move(file_descriptor);
  set_cookie(PrintSettings::NewCookie());

  GetPrintRenderFrame(rfh)->PrintRequestedPages();

  for (auto& observer : GetTestObservers()) {
    observer.OnPrintNow(rfh);
  }
  return true;
}

bool PrintViewManagerBasic::InitiatePrint(content::RenderFrameHost* rfh) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  if (IsCrashed() || !rfh || !rfh->IsActive() || !rfh->IsRenderFrameLive()) {
    return false;
  }

  GetPrintRenderFrame(rfh)->InitiatePrintAndroid();
  return true;
}

void PrintViewManagerBasic::FinishPrint(content::RenderFrameHost* rfh) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  settings_.reset();
  fd_.reset();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this) {
    return;
  }
  // FinishPrint is a teardown operation. It must be delivered as long as the
  // frame is live, even if inactive (e.g. backgrounded or in BackForwardCache),
  // so the renderer does not permanently retain `print_in_progress_ = true`.
  if (IsCrashed() || !rfh || !rfh->IsRenderFrameLive()) {
    return;
  }

  GetPrintRenderFrame(rfh)->FinishPrintAndroid();
}

void PrintViewManagerBasic::SetupScriptedPrintAndroid(
    SetupScriptedPrintAndroidCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  content::RenderFrameHost& rfh = CurrentTargetFrame();
  CHECK(rfh.IsRenderFrameLive(), base::NotFatalUntil::M162);
  if (!rfh.IsActive() || !web_contents() ||
      !web_contents()->GetTopLevelNativeWindow()) {
    FinishPrint(&rfh);
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
  settings_.reset();
  fd_.reset();
  pdf_writing_done_callback().Run(page_count);
}

void PrintViewManagerBasic::PrintingFailed(int32_t cookie,
                                           mojom::PrintFailureReason reason) {
  // `settings_` is only set for jobs started via PrintNowAndroid(). Delegate
  // before IsCurrentJob(), which requires `settings_` to be non-null.
  if (!settings_) {
    PrintViewManagerBase::PrintingFailed(cookie, reason);
    return;
  }
  if (!IsCurrentTargetFramePrintingRFH() || !IsCurrentJob(cookie)) {
    return;
  }
  PdfWritingDone(0);
}

void PrintViewManagerBasic::DidGetPrintedPagesCount(int32_t cookie,
                                                    uint32_t number_pages) {
  if (!settings_) {
    PrintViewManagerBase::DidGetPrintedPagesCount(cookie, number_pages);
    return;
  }
  // `number_pages_` decides how much of the document is written into `fd_`, so
  // only the frame that owns this job may set it, and only for the current
  // document. PrintManager::DidGetPrintedPagesCount() DCHECKs the arguments it
  // is given, so they are validated here rather than there.
  if (!IsCurrentTargetFramePrintingRFH() || !IsCurrentJob(cookie) ||
      number_pages == 0) {
    return;
  }
  PrintManager::DidGetPrintedPagesCount(cookie, number_pages);
}

void PrintViewManagerBasic::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (settings_) {
    // Only the frame this Android print job was started for may consume the
    // pre-configured settings; otherwise another frame in the same
    // WebContents could take the job over and write its own document into
    // `fd_`.
    if (!IsCurrentTargetFramePrintingRFH()) {
      std::move(callback).Run(nullptr);
      return;
    }

    // Same guards as PrintViewManagerBase::GetDefaultPrintSettings(): the
    // policy may have been turned off after the job was started, and only
    // active RFHs may drive UI.
    if (!GetPrintingEnabledBooleanPref() || !CurrentTargetFrame().IsActive()) {
      PdfWritingDone(0);
      std::move(callback).Run(nullptr);
      return;
    }

    auto params = mojom::PrintParams::New();
    RenderParamsFromPrintSettings(*settings_, params.get());
    params->document_cookie = cookie();
    if (!PrintMsgPrintParamsIsValid(*params)) {
      PdfWritingDone(0);
      std::move(callback).Run(nullptr);
      return;
    }
    std::move(callback).Run(std::move(params));
    return;
  }
  PrintViewManagerBase::GetDefaultPrintSettings(std::move(callback));
}

void PrintViewManagerBasic::ScriptedPrint(mojom::ScriptedPrintParamsPtr params,
                                          ScriptedPrintCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (settings_) {
    if (!IsCurrentTargetFramePrintingRFH() || !IsCurrentJob(params->cookie)) {
      std::move(callback).Run(nullptr);
      return;
    }

    if (!CurrentTargetFrame().IsActive()) {
      PdfWritingDone(0);
      std::move(callback).Run(nullptr);
      return;
    }

    auto print_pages_params = mojom::PrintPagesParams::New();
    print_pages_params->params = mojom::PrintParams::New();
    RenderParamsFromPrintSettings(*settings_, print_pages_params->params.get());
    print_pages_params->params->document_cookie = cookie();
    print_pages_params->pages = settings_->ranges();

    if (!PrintMsgPrintParamsIsValid(*print_pages_params->params)) {
      PdfWritingDone(0);
      std::move(callback).Run(nullptr);
      return;
    }

    std::move(callback).Run(std::move(print_pages_params));
    return;
  }

  PrintViewManagerBase::ScriptedPrint(std::move(params), std::move(callback));
}

void PrintViewManagerBasic::DidPrintDocument(
    mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  if (!settings_) {
    PrintViewManagerBase::DidPrintDocument(std::move(params),
                                           std::move(callback));
    return;
  }

  // Validate the sender before touching any job state. A frame that does not
  // own this job must neither consume `fd_` nor report a write failure for it,
  // and the same holds for a document that is not the one being printed.
  if (!IsCurrentTargetFramePrintingRFH() ||
      !IsCurrentJob(params->document_cookie)) {
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  if (!fd_.is_valid()) {
    // The descriptor was already consumed or released (e.g. FinishPrint() ran
    // first). Fail explicitly instead of falling back to the PrintJob-based
    // desktop path.
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  base::ScopedFD print_fd = std::move(fd_);

  const mojom::DidPrintContentParams& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  // Snapshot the page count: compositing is asynchronous, and the renderer can
  // send another DidGetPrintedPagesCount() while it runs.
  const uint32_t page_count = number_pages();
  if (page_count > kMaxPageCount) {
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  if (IsOopifEnabled() && settings_->is_modifiable()) {
    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    client->CompositeDocument(
        params->document_cookie, CurrentTargetFrame(), content,
        /*is_pdf=*/false, ui::AXTreeUpdate(),
        mojom::GenerateDocumentOutline::kNone,
        base::BindOnce(&PrintViewManagerBasic::OnCompositeDocumentDoneAndroid,
                       weak_ptr_factory_.GetWeakPtr(), params->document_cookie,
                       std::move(print_fd), page_count, std::move(callback)));
    return;
  }

  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  WriteDocumentToFd(params->document_cookie, std::move(print_fd), page_count,
                    std::move(data), std::move(callback));
}

bool PrintViewManagerBasic::IsCurrentJob(int32_t document_cookie) const {
  return settings_ && IsValidCookie(document_cookie);
}

void PrintViewManagerBasic::OnCompositeDocumentDoneAndroid(
    int32_t document_cookie,
    base::ScopedFD print_fd,
    uint32_t page_count,
    DidPrintDocumentCallback callback,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!IsCurrentJob(document_cookie)) {
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  if (status != mojom::PrintCompositor::Status::kSuccess || !region.IsValid()) {
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  auto data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region);
  if (!data) {
    PdfWritingDone(0);
    OnDidPrintDocumentDone(std::move(callback), /*succeeded=*/false);
    return;
  }

  WriteDocumentToFd(document_cookie, std::move(print_fd), page_count,
                    std::move(data), std::move(callback));
}

void PrintViewManagerBasic::WriteDocumentToFd(
    int32_t document_cookie,
    base::ScopedFD print_fd,
    uint32_t page_count,
    scoped_refptr<base::RefCountedSharedMemoryMapping> data,
    DidPrintDocumentCallback callback) {
  base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&SaveDataToFd, std::move(print_fd), page_count,
                         std::move(data)),
          base::BindOnce(&PrintViewManagerBasic::OnDidPrintDocumentWritingDone,
                         weak_ptr_factory_.GetWeakPtr(), document_cookie,
                         std::move(callback)));
}

void PrintViewManagerBasic::OnDidPrintDocumentWritingDone(
    int32_t document_cookie,
    DidPrintDocumentCallback did_print_document_cb,
    uint32_t page_count) {
  CHECK_LE(page_count, kMaxPageCount);
  if (IsCurrentJob(document_cookie)) {
    PdfWritingDone(base::checked_cast<int>(page_count));
  }
  OnDidPrintDocumentDone(std::move(did_print_document_cb), page_count > 0);
}

void PrintViewManagerBasic::OnDidPrintDocumentDone(
    DidPrintDocumentCallback callback,
    bool succeeded) {
  std::move(callback).Run(succeeded);
  for (auto& observer : GetTestObservers()) {
    observer.OnDidPrintDocument();
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerBasic);

}  // namespace printing
