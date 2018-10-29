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
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/pdf_nup_converter_client.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "printing/nup_parameters.h"
#include "printing/page_setup.h"
#include "printing/print_job_constants.h"
#include "printing/print_settings.h"

using content::BrowserThread;
using content::WebContents;

namespace printing {

namespace {

void StopWorker(int document_cookie) {
  if (document_cookie <= 0)
    return;
  scoped_refptr<PrintQueriesQueue> queue =
      g_browser_process->print_job_manager()->queue();
  scoped_refptr<PrinterQuery> printer_query =
      queue->PopPrinterQuery(document_cookie);
  if (printer_query) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&PrinterQuery::StopWorker, printer_query));
  }
}

bool ShouldUseCompositor(PrintPreviewUI* print_preview_ui) {
  return IsOopifEnabled() && print_preview_ui->source_is_modifiable();
}

bool IsValidPageNumber(int page_number, int page_count) {
  return page_number >= 0 && page_number < page_count;
}

}  // namespace

PrintPreviewMessageHandler::PrintPreviewMessageHandler(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents), weak_ptr_factory_(this) {
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
  return preview_ui->GetIDForPrintPreviewUI() == preview_ui_id ? preview_ui
                                                               : nullptr;
}

void PrintPreviewMessageHandler::OnRequestPrintPreview(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_RequestPrintPreview_Params& params) {
  if (params.webnode_only) {
    PrintViewManager::FromWebContents(web_contents())->PrintPreviewForWebNode(
        render_frame_host);
  }
  PrintPreviewDialogController::PrintPreview(web_contents());
  PrintPreviewUI::SetInitialParams(GetPrintPreviewDialog(), params);
}

void PrintPreviewMessageHandler::OnDidStartPreview(
    const PrintHostMsg_DidStartPreview_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  if (params.page_count <= 0 || params.pages_to_render.empty()) {
    NOTREACHED();
    return;
  }

  for (int page_number : params.pages_to_render) {
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

void PrintPreviewMessageHandler::OnDidPreviewPage(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPreviewPage_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  int page_number = params.page_number;
  const PrintHostMsg_DidPrintContent_Params& content = params.content;
  if (page_number < FIRST_PAGE_INDEX || !content.metafile_data_region.IsValid())
    return;

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
        params.document_cookie, render_frame_host, page_number, content,
        base::BindOnce(&PrintPreviewMessageHandler::OnCompositePdfPageDone,
                       weak_ptr_factory_.GetWeakPtr(), page_number,
                       params.document_cookie, ids));
  } else {
    NotifyUIPreviewPageReady(
        print_preview_ui, page_number, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
            content.metafile_data_region));
  }
}

void PrintPreviewMessageHandler::OnMetafileReadyForPrinting(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPreviewDocument_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  // Always try to stop the worker.
  StopWorker(params.document_cookie);

  const PrintHostMsg_DidPrintContent_Params& content = params.content;
  if (!content.metafile_data_region.IsValid())
    return;

  if (params.expected_pages_count <= 0) {
    NOTREACHED();
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  if (ShouldUseCompositor(print_preview_ui)) {
    // Don't bother compositing if this request has been cancelled already.
    if (PrintPreviewUI::ShouldCancelRequest(ids))
      return;

    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    DCHECK(client);

    client->DoCompositeDocumentToPdf(
        params.document_cookie, render_frame_host, content,
        base::BindOnce(&PrintPreviewMessageHandler::OnCompositePdfDocumentDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       params.expected_pages_count, params.document_cookie,
                       ids));
  } else {
    NotifyUIPreviewDocumentReady(
        print_preview_ui, params.expected_pages_count, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
            content.metafile_data_region));
  }
}

void PrintPreviewMessageHandler::OnPrintPreviewFailed(
    int document_cookie,
    const PrintHostMsg_PreviewIds& ids) {
  StopWorker(document_cookie);

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;
  print_preview_ui->OnPrintPreviewFailed(ids.request_id);
}

void PrintPreviewMessageHandler::OnDidGetDefaultPageLayout(
    const PageSizeMargins& page_layout_in_points,
    const gfx::Rect& printable_area_in_points,
    bool has_custom_page_size_style,
    const PrintHostMsg_PreviewIds& ids) {
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  print_preview_ui->OnDidGetDefaultPageLayout(
      page_layout_in_points, printable_area_in_points,
      has_custom_page_size_style, ids.request_id);
}

void PrintPreviewMessageHandler::OnPrintPreviewCancelled(
    int document_cookie,
    const PrintHostMsg_PreviewIds& ids) {
  // Always need to stop the worker.
  StopWorker(document_cookie);

  // Notify UI
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;
  print_preview_ui->OnPrintPreviewCancelled(ids.request_id);
}

void PrintPreviewMessageHandler::OnInvalidPrinterSettings(
    int document_cookie,
    const PrintHostMsg_PreviewIds& ids) {
  StopWorker(document_cookie);
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;
  print_preview_ui->OnInvalidPrinterSettings(ids.request_id);
}

void PrintPreviewMessageHandler::OnSetOptionsFromDocument(
    const PrintHostMsg_SetOptionsFromDocument_Params& params,
    const PrintHostMsg_PreviewIds& ids) {
  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;
  print_preview_ui->OnSetOptionsFromDocument(params, ids.request_id);
}

void PrintPreviewMessageHandler::NotifyUIPreviewPageReady(
    PrintPreviewUI* print_preview_ui,
    int page_number,
    const PrintHostMsg_PreviewIds& ids,
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
    int page_count,
    const PrintHostMsg_PreviewIds& ids,
    scoped_refptr<base::RefCountedMemory> data_bytes) {
  if (!data_bytes || !data_bytes->size())
    return;

  // Don't bother notifying the UI if this request has been cancelled already.
  if (PrintPreviewUI::ShouldCancelRequest(ids))
    return;

  print_preview_ui->OnPreviewDataIsAvailable(page_count, std::move(data_bytes),
                                             ids.request_id);
}

void PrintPreviewMessageHandler::OnCompositePdfPageDone(
    int page_number,
    int document_cookie,
    const PrintHostMsg_PreviewIds& ids,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != mojom::PdfCompositor::Status::SUCCESS) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  int pages_per_sheet = print_preview_ui->pages_per_sheet();
  if (pages_per_sheet == 1) {
    NotifyUIPreviewPageReady(
        print_preview_ui, page_number, ids,
        base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
  } else {
    print_preview_ui->AddPdfPageForNupConversion(std::move(region));
    int current_page_index =
        print_preview_ui->GetPageToNupConvertIndex(page_number);
    if (current_page_index == -1) {
      return;
    }

    if (((current_page_index + 1) % pages_per_sheet) == 0 ||
        print_preview_ui->LastPageComposited(page_number)) {
      int new_page_number = current_page_index / pages_per_sheet;
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
          base::BindOnce(&PrintPreviewMessageHandler::OnNupPdfConvertDone,
                         weak_ptr_factory_.GetWeakPtr(), new_page_number, ids));
    }
  }
}

void PrintPreviewMessageHandler::OnNupPdfConvertDone(
    int page_number,
    const PrintHostMsg_PreviewIds& ids,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf page conversion failed with error " << status;
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  NotifyUIPreviewPageReady(
      print_preview_ui, page_number, ids,
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region));
}

void PrintPreviewMessageHandler::OnCompositePdfDocumentDone(
    int page_count,
    int document_cookie,
    const PrintHostMsg_PreviewIds& ids,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (status != mojom::PdfCompositor::Status::SUCCESS) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  int pages_per_sheet = print_preview_ui->pages_per_sheet();
  if (pages_per_sheet == 1) {
    NotifyUIPreviewDocumentReady(
        print_preview_ui, page_count, ids,
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
        base::BindOnce(&PrintPreviewMessageHandler::OnNupPdfDocumentConvertDone,
                       weak_ptr_factory_.GetWeakPtr(),
                       (page_count + pages_per_sheet - 1) / pages_per_sheet,
                       ids));
  }
}

void PrintPreviewMessageHandler::OnNupPdfDocumentConvertDone(
    int page_count,
    const PrintHostMsg_PreviewIds& ids,
    mojom::PdfNupConverter::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PdfNupConverter::Status::SUCCESS) {
    DLOG(ERROR) << "Nup pdf document convert failed with error " << status;
    return;
  }

  PrintPreviewUI* print_preview_ui = GetPrintPreviewUI(ids.ui_id);
  if (!print_preview_ui)
    return;

  NotifyUIPreviewDocumentReady(
      print_preview_ui, page_count, ids,
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
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintPreviewFailed,
                        OnPrintPreviewFailed)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidGetDefaultPageLayout,
                        OnDidGetDefaultPageLayout)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintPreviewCancelled,
                        OnPrintPreviewCancelled)
    IPC_MESSAGE_HANDLER(PrintHostMsg_PrintPreviewInvalidPrinterSettings,
                        OnInvalidPrinterSettings)
    IPC_MESSAGE_HANDLER(PrintHostMsg_SetOptionsFromDocument,
                        OnSetOptionsFromDocument)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace printing
