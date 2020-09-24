// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_message_handler.h"

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "printing/mojom/print.mojom.h"
#include "printing/nup_parameters.h"
#include "printing/page_setup.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

using content::BrowserThread;
using content::WebContents;

namespace printing {

namespace {

// TODO(https://crbug.com/1008939): Remove this once all preview UI messages
// are moved to print_preview_ui.cc.
void StopWorker(int document_cookie) {
  if (document_cookie <= 0)
    return;
  scoped_refptr<PrintQueriesQueue> queue =
      g_browser_process->print_job_manager()->queue();
  std::unique_ptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
  if (printer_query) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PrinterQuery::StopWorker, std::move(printer_query)));
  }
}

bool ShouldUseCompositor(PrintPreviewUI* print_preview_ui) {
  return IsOopifEnabled() && print_preview_ui->source_is_modifiable();
}

bool IsValidPageNumber(uint32_t page_number, uint32_t page_count) {
  return page_number < page_count;
}

}  // namespace

PrintPreviewMessageHandler::PrintPreviewMessageHandler(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK(web_contents);
}

PrintPreviewMessageHandler::~PrintPreviewMessageHandler() {}

WebContents* PrintPreviewMessageHandler::GetPrintPreviewDialog() {
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  if (!dialog_controller)
    return nullptr;
  return dialog_controller->GetPrintPreviewForContents(web_contents());
}

PrintPreviewUI* PrintPreviewMessageHandler::GetPrintPreviewUI(
    int preview_ui_id) {
  WebContents* dialog = GetPrintPreviewDialog();
  if (!dialog || !dialog->GetWebUI())
    return nullptr;
  PrintPreviewUI* preview_ui =
      static_cast<PrintPreviewUI*>(dialog->GetWebUI()->GetController());
  base::Optional<int32_t> id = preview_ui->GetIDForPrintPreviewUI();
  return (id && *id == preview_ui_id) ? preview_ui : nullptr;
}

void PrintPreviewMessageHandler::OnRequestPrintPreview(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_RequestPrintPreview_Params& params) {
  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(web_contents());
  if (print_view_manager->RejectPrintPreviewRequestIfRestricted(
          render_frame_host)) {
    return;
  }
  if (params.webnode_only) {
    print_view_manager->PrintPreviewForWebNode(render_frame_host);
  }
  PrintPreviewDialogController::PrintPreview(web_contents());
  PrintPreviewUI::SetInitialParams(GetPrintPreviewDialog(), params);
}

void PrintPreviewMessageHandler::OnDidStartPreview(
    const mojom::DidStartPreviewParams& params,
    const mojom::PreviewIds& ids) {
  if (params.page_count == 0 || params.page_count > kMaxPageCount ||
      params.pages_to_render.empty()) {
    NOTREACHED();
    return;
  }

  for (uint32_t page_number : params.pages_to_render) {
    if (!IsValidPageNumber(page_number, params.page_count)) {
      NOTREACHED();
      return;
    }
  }

  if (!printing::NupParameters::IsSupported(params.pages_per_sheet)) {
    NOTREACHED();
    return;
  }

  if (params.page_size.IsEmpty()) {
    NOTREACHED();
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  print_preview_ui->OnDidStartPreview(params, ids.request_id);
}

void PrintPreviewMessageHandler::OnDidPrepareForDocumentToPdf(
    content::RenderFrameHost* render_frame_host,
    int document_cookie,
    const mojom::PreviewIds& ids) {
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  // Determine if document composition from individual pages with the print
  // compositor is the desired configuration. Issue a preparation call to the
  // PrintCompositeClient if that hasn't been done yet. Otherwise, return early.
  if (!ShouldUseCompositor(print_preview_ui))
    return;

  // For case of print preview, page metafile is used to composite into
  // the document PDF at same time.  Need to indicate that this scenario
  // is at play for the compositor.
  auto* client = PrintCompositeClient::FromWebContents(web_contents());
  DCHECK(client);
  if (client->GetIsDocumentConcurrentlyComposited(document_cookie))
    return;

  client->DoPrepareForDocumentToPdf(
      document_cookie, render_frame_host,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &PrintPreviewMessageHandler::OnPrepareForDocumentToPdfDone,
              weak_ptr_factory_.GetWeakPtr(), ids),
          mojom::PrintCompositor::Status::kCompositingFailure));
}

void PrintPreviewMessageHandler::OnDidPreviewPage(
    content::RenderFrameHost* render_frame_host,
    const mojom::DidPreviewPageParams& params,
    const mojom::PreviewIds& ids) {
  uint32_t page_number = params.page_number;
  const mojom::DidPrintContentParams& content = *params.content;
  if (page_number == kInvalidPageIndex ||
      !content.metafile_data_region.IsValid()) {
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  if (!print_preview_ui->OnPendingPreviewPage(page_number)) {
    NOTREACHED();
    return;
  }

  if (ShouldUseCompositor(print_preview_ui)) {
    // Don't bother compositing if this request has been cancelled already.
    if (PrintPreviewUI::ShouldCancelRequest(ids))
      return;

    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    DCHECK(client);

    // Use utility process to convert skia metafile to pdf.
    client->DoCompositePageToPdf(
        params.document_cookie, render_frame_host, content,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(&PrintPreviewMessageHandler::OnCompositePdfPageDone,
                           weak_ptr_factory_.GetWeakPtr(), page_number,
                           params.document_cookie, ids),
            mojom::PrintCompositor::Status::kCompositingFailure,
            base::ReadOnlySharedMemoryRegion()));
  } else {
    NotifyUIPreviewPageReady(
        print_preview_ui, page_number, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
            content.metafile_data_region));
  }
}

void PrintPreviewMessageHandler::OnMetafileReadyForPrinting(
    content::RenderFrameHost* render_frame_host,
    const mojom::DidPreviewDocumentParams& params,
    const mojom::PreviewIds& ids) {
  // Always try to stop the worker.
  StopWorker(params.document_cookie);

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  const bool composite_document_using_individual_pages =
      ShouldUseCompositor(print_preview_ui);
  const base::ReadOnlySharedMemoryRegion& metafile =
      params.content->metafile_data_region;

  // When the Print Compositor is active, the print document is composed from
  // the individual pages, so |metafile| should be invalid.
  // When it is inactive, the print document is composed from |metafile|.
  // So if this comparison succeeds, that means the renderer sent bad data.
  if (composite_document_using_individual_pages == metafile.IsValid())
    return;

  if (params.expected_pages_count == 0) {
    NOTREACHED();
    return;
  }

  if (composite_document_using_individual_pages) {
    // Don't bother compositing if this request has been cancelled already.
    if (PrintPreviewUI::ShouldCancelRequest(ids))
      return;

    auto callback = base::BindOnce(
        &PrintPreviewMessageHandler::OnCompositeToPdfDone,
        weak_ptr_factory_.GetWeakPtr(), params.document_cookie, ids);

    // Page metafile is used to composite into the document at same time.
    // Need to provide particulars of how many pages are required before
    // document will be completed.
    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    client->DoCompleteDocumentToPdf(
        params.document_cookie, params.expected_pages_count,
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            std::move(callback),
            mojom::PrintCompositor::Status::kCompositingFailure,
            base::ReadOnlySharedMemoryRegion()));
  } else {
    NotifyUIPreviewDocumentReady(
        print_preview_ui, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(metafile));
  }
}

void PrintPreviewMessageHandler::OnDidGetDefaultPageLayout(
    const mojom::PageSizeMargins& page_layout_in_points,
    const gfx::Rect& printable_area_in_points,
    bool has_custom_page_size_style,
    const mojom::PreviewIds& ids) {
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  print_preview_ui->OnDidGetDefaultPageLayout(
      page_layout_in_points, printable_area_in_points,
      has_custom_page_size_style, ids.request_id);
}

void PrintPreviewMessageHandler::NotifyUIPreviewPageReady(
    PrintPreviewUI* print_preview_ui,
    uint32_t page_number,
    const mojom::PreviewIds& ids,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!data_bytes || !data_bytes->size())
    return;

  // Don't bother notifying the UI if this request has been cancelled already.
  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  print_preview_ui->OnDidPreviewPage(page_number, std::move(data_bytes),
                                     ids.request_id);
}

void PrintPreviewMessageHandler::NotifyUIPreviewDocumentReady(
    PrintPreviewUI* print_preview_ui,
    const mojom::PreviewIds& ids,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!data_bytes || !data_bytes->size())
    return;

  // Don't bother notifying the UI if this request has been cancelled already.
  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  print_preview_ui->OnPreviewDataIsAvailable(std::move(data_bytes),
                                             ids.request_id);
}

void PrintPreviewMessageHandler::OnCompositePdfPageDone(
    uint32_t page_number,
    int document_cookie,
    const mojom::PreviewIds& ids,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewFailed(ids.request_id);
    return;
  }

  if (!print_preview_ui)
    return;

  int pages_per_sheet = print_preview_ui->pages_per_sheet();
  if (pages_per_sheet == 1) {
    NotifyUIPreviewPageReady(
        print_preview_ui, page_number, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
  } else {
    print_preview_ui->AddPdfPageForNupConversion(std::move(region));
    uint32_t current_page_index =
        print_preview_ui->GetPageToNupConvertIndex(page_number);
    if (current_page_index == kInvalidPageIndex) {
      return;
    }

    if (((current_page_index + 1) % pages_per_sheet) == 0 ||
        print_preview_ui->LastPageComposited(page_number)) {
      uint32_t new_page_number =
          base::checked_cast<uint32_t>(current_page_index / pages_per_sheet);
      DCHECK_NE(new_page_number, kInvalidPageIndex);
      std::vector<base::ReadOnlySharedMemoryRegion> pdf_page_regions =
          print_preview_ui->TakePagesForNupConvert();

      gfx::Rect printable_area = PageSetup::GetSymmetricalPrintableArea(
          print_preview_ui->page_size(), print_preview_ui->printable_area());
      if (printable_area.IsEmpty())
        return;

      auto* client = PdfNupConverterClient::FromWebContents(web_contents());
      DCHECK(client);
      client->DoNupPdfConvert(
          document_cookie, pages_per_sheet, print_preview_ui->page_size(),
          printable_area, std::move(pdf_page_regions),
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&PrintPreviewMessageHandler::OnNupPdfConvertDone,
                             weak_ptr_factory_.GetWeakPtr(), new_page_number,
                             ids),
              mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
              base::ReadOnlySharedMemoryRegion()));
    }
  }
}

void PrintPreviewMessageHandler::OnNupPdfConvertDone(
    uint32_t page_number,
    const mojom::PreviewIds& ids,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf page conversion failed with error " << status;
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewFailed(ids.request_id);
    return;
  }

  if (!print_preview_ui)
    return;

  NotifyUIPreviewPageReady(
      print_preview_ui, page_number, ids,
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
}

void PrintPreviewMessageHandler::OnCompositeToPdfDone(
    int document_cookie,
    const mojom::PreviewIds& ids,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Completion of document to pdf failed with error " << status;
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewFailed(ids.request_id);
    return;
  }
  if (!print_preview_ui)
    return;

  int pages_per_sheet = print_preview_ui->pages_per_sheet();
  if (pages_per_sheet == 1) {
    NotifyUIPreviewDocumentReady(
        print_preview_ui, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
  } else {
    auto* client = PdfNupConverterClient::FromWebContents(web_contents());
    DCHECK(client);

    gfx::Rect printable_area = PageSetup::GetSymmetricalPrintableArea(
        print_preview_ui->page_size(), print_preview_ui->printable_area());
    if (printable_area.IsEmpty())
      return;

    client->DoNupPdfDocumentConvert(
        document_cookie, pages_per_sheet, print_preview_ui->page_size(),
        printable_area, std::move(region),
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(
            base::BindOnce(
                &PrintPreviewMessageHandler::OnNupPdfDocumentConvertDone,
                weak_ptr_factory_.GetWeakPtr(), ids),
            mojom::PdfNupConverter::Status::CONVERSION_FAILURE,
            base::ReadOnlySharedMemoryRegion()));
  }
}

void PrintPreviewMessageHandler::OnPrepareForDocumentToPdfDone(
    const mojom::PreviewIds& ids,
    mojom::PrintCompositor::Status status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  if (status != mojom::PrintCompositor::Status::kSuccess) {
    PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewFailed(ids.request_id);
  }
}

void PrintPreviewMessageHandler::OnNupPdfDocumentConvertDone(
    const mojom::PreviewIds& ids,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf document convert failed with error " << status;
    if (print_preview_ui)
      print_preview_ui->OnPrintPreviewFailed(ids.request_id);
    return;
  }

  if (!print_preview_ui)
    return;

  NotifyUIPreviewDocumentReady(
      print_preview_ui, ids,
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
}

bool PrintPreviewMessageHandler::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PrintPreviewMessageHandler, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                        OnRequestPrintPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrepareDocumentForPreview,
                        OnDidPrepareForDocumentToPdf)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPreviewPage, OnDidPreviewPage)
    IPC_MESSAGE_HANDLER(PrintHostMsg_MetafileReadyForPrinting,
                        OnMetafileReadyForPrinting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintPreviewMessageHandler, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidStartPreview, OnDidStartPreview)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDefaultPageLayout,
                        OnDidGetDefaultPageLayout)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintPreviewMessageHandler)

}  // namespace printing
