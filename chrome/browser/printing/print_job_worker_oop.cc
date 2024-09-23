// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker_oop.h"

#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile.h"
#include "printing/printed_document.h"

#if BUILDFLAG(IS_WIN)
#include "printing/printed_page_win.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

// Enumeration of printing events when submitting a job to a print driver.
// This must stay in sync with the corresponding histogram in `histograms.xml`.
// These values are persisted to logs.  Entries should not be renumbered and
// numeric values should never be reused.
enum class PrintOopResult {
  kSuccessful = 0,
  kCanceled = 1,
  kAccessDenied = 2,
  kFailed = 3,
  kMaxValue = kFailed,
};

constexpr char kPrintOopPrintResultHistogramName[] = "Printing.Oop.PrintResult";

}  // namespace

PrintJobWorkerOop::PrintJobWorkerOop(
    std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
    std::unique_ptr<PrintingContext> printing_context,
    std::optional<PrintBackendServiceManager::ClientId> client_id,
    std::optional<PrintBackendServiceManager::ContextId> context_id,
    PrintJob* print_job,
    bool print_from_system_dialog)
    : PrintJobWorkerOop(std::move(printing_context_delegate),
                        std::move(printing_context),
                        client_id,
                        context_id,
                        print_job,
                        print_from_system_dialog,
                        /*simulate_spooling_memory_errors=*/false) {}

PrintJobWorkerOop::PrintJobWorkerOop(
    std::unique_ptr<PrintingContext::Delegate> printing_context_delegate,
    std::unique_ptr<PrintingContext> printing_context,
    std::optional<PrintBackendServiceManager::ClientId> client_id,
    std::optional<PrintBackendServiceManager::ContextId> context_id,
    PrintJob* print_job,
    bool print_from_system_dialog,
    bool simulate_spooling_memory_errors)
    : PrintJobWorker(std::move(printing_context_delegate),
                     std::move(printing_context),
                     print_job),
      simulate_spooling_memory_errors_(simulate_spooling_memory_errors),
      service_manager_client_id_(client_id),
      printing_context_id_(context_id),
      print_from_system_dialog_(print_from_system_dialog) {}

PrintJobWorkerOop::~PrintJobWorkerOop() {
  DCHECK(!service_manager_client_id_.has_value());
}

void PrintJobWorkerOop::StartPrinting(PrintedDocument* new_document) {
  if (!StartPrintingSanityCheck(new_document))
    return;

  // Do browser-side context setup.
  std::u16string document_name = GetDocumentName(new_document);
  bool success = SetupDocument(document_name);
  DCHECK(success);

  // Keep another reference to the document just for OOP.  This reference
  // ensures the document object is retained even if the job cancels out and
  // the reference to it from `PrintJobWorker` is dropped.  This guarantees
  // that it can still be used in the various asynchronous callbacks.
  document_oop_ = new_document;

  std::string device_name =
      base::UTF16ToUTF8(document_oop_->settings().device_name());
  VLOG(1) << "Start printing document " << document_oop_->cookie() << " to "
          << device_name;

  // `PrintBackendServiceManager` interactions must happen on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::SendStartPrinting,
                                ui_weak_factory_.GetWeakPtr(), device_name,
                                document_name));
}

void PrintJobWorkerOop::Cancel() {
  PrintJobWorker::Cancel();
  PrintJobWorkerOop::OnCancel();
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintJobWorkerOop::CleanupAfterContentAnalysisDenial() {
  PrintJobWorker::CleanupAfterContentAnalysisDenial();
  UnregisterServiceManagerClient();
}
#endif

void PrintJobWorkerOop::OnDidStartPrinting(mojom::ResultCode result,
                                           int job_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Error initiating printing via service for document "
                       << document_oop_->cookie() << ": " << result;
    if (result != mojom::ResultCode::kAccessDenied || !TryRestartPrinting())
      NotifyFailure(result);
    return;
  }

  // Retain the job ID that was set in the service.
  printing_context()->SetJobId(job_id);

  VLOG(1) << "Printing initiated with service for document "
          << document_oop_->cookie();
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJobWorker::OnNewPage,
                                         worker_weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::OnDidRenderPrintedPage(uint32_t page_index,
                                               mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    // Once an error happens during rendering, there could be multiple calls
    // to here as the queue of sent pages all return back with error.
    PRINTER_LOG(ERROR)
        << "Error rendering printed page via service for document "
        << document_oop_->cookie() << ": " << result;
    NotifyFailure(result);
    return;
  }
  scoped_refptr<PrintedPage> page = document_oop_->GetPage(page_index);
  if (!page) {
    PRINTER_LOG(ERROR) << "Unable to get page " << page_index
                       << " via service for document "
                       << document_oop_->cookie();
    NotifyFailure(mojom::ResultCode::kFailed);
    return;
  }
  VLOG(1) << "Rendered printed page via service for document "
          << document_oop_->cookie() << " page " << page_index;

  // Signal everyone that the page is printed.
  print_job()->PostTask(FROM_HERE,
                        base::BindOnce(&PrintJob::OnPageDone, print_job(),
                                       base::RetainedRef(page)));

  ++pages_printed_count_;
  if (pages_printed_count_ == document_oop_->expected_page_count()) {
    // The last page has printed, can proceed to document done processing.
    VLOG(1) << "All pages printed for document";
    SendDocumentDone();
  }
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::OnDidRenderPrintedDocument(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR)
        << "Error rendering printed document via service for document "
        << document_oop_->cookie() << ": " << result;
    NotifyFailure(result);
    return;
  }
  VLOG(1) << "Rendered printed document via service for document "
          << document_oop_->cookie();
  SendDocumentDone();
}

void PrintJobWorkerOop::OnDidDocumentDone(int job_id,
                                          mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  DCHECK_EQ(pages_printed_count_, document_oop_->expected_page_count());
#endif
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Error completing printing via service for document "
                       << document_oop_->cookie() << ": " << result;
    NotifyFailure(result);
    return;
  }
  PRINTER_LOG(EVENT) << "Printing completed via service for document "
                     << document_oop_->cookie();
  UnregisterServiceManagerClient();
  base::UmaHistogramEnumeration(kPrintOopPrintResultHistogramName,
                                PrintOopResult::kSuccessful);
  task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::FinishDocumentDone,
                                worker_weak_factory_.GetWeakPtr(), job_id));

  // Also done with private document reference.
  document_oop_ = nullptr;
}

void PrintJobWorkerOop::OnDidCancel(scoped_refptr<PrintJob> job,
                                    mojom::ResultCode cancel_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Cancel completed for printing via service for document "
           << document_oop_->cookie();

  UnregisterServiceManagerClient();
  printing_context_id_.reset();

  if (cancel_reason == mojom::ResultCode::kCanceled) {
    print_job()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJob::Cancel, std::move(job)));
  } else {
    PostTask(FROM_HERE, base::BindOnce(
                            [](base::WeakPtr<PrintJobWorkerOop> self,
                               scoped_refptr<PrintJob> job) {
                              self->PrintJobWorker::OnFailure();
                            },
                            worker_weak_factory_.GetWeakPtr(), std::move(job)));
  }
  // Done with private document reference.
  document_oop_ = nullptr;
}

#if BUILDFLAG(IS_WIN)
bool PrintJobWorkerOop::SpoolPage(PrintedPage* page) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK_NE(page_number(), PageNumber::npos());

#if !defined(NDEBUG)
  DCHECK(document()->IsPageInList(*page));
#endif

  const MetafilePlayer* metafile = page->metafile();
  DCHECK(metafile);
  base::MappedReadOnlyRegion region_mapping =
      metafile->GetDataAsSharedMemoryRegion();
  if (simulate_spooling_memory_errors_ || !region_mapping.IsValid()) {
    PRINTER_LOG(ERROR)
        << "Spooling page via service failed due to shared memory error.";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintJobWorkerOop::NotifyFailure,
                                  ui_weak_factory_.GetWeakPtr(),
                                  mojom::ResultCode::kFailed));
    return false;
  }

  VLOG(1) << "Spooling page " << page_number() << " to print via service";
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorkerOop::SendRenderPrintedPage,
                     ui_weak_factory_.GetWeakPtr(), base::RetainedRef(page),
                     metafile->GetDataType(),
                     std::move(region_mapping.region)));
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

bool PrintJobWorkerOop::SpoolDocument() {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());

  const MetafilePlayer* metafile = document()->GetMetafile();
  DCHECK(metafile);
  base::MappedReadOnlyRegion region_mapping =
      metafile->GetDataAsSharedMemoryRegion();
  if (simulate_spooling_memory_errors_ || !region_mapping.IsValid()) {
    PRINTER_LOG(ERROR)
        << "Spooling document via service failed due to shared memory error.";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintJobWorkerOop::NotifyFailure,
                                  ui_weak_factory_.GetWeakPtr(),
                                  mojom::ResultCode::kFailed));
    return false;
  }

  VLOG(1) << "Spooling job to print via service";
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorkerOop::SendRenderPrintedDocument,
                     ui_weak_factory_.GetWeakPtr(), metafile->GetDataType(),
                     std::move(region_mapping.region)));
  return true;
}

void PrintJobWorkerOop::OnDocumentDone() {
  // Can do browser-side checks related to completeness for sending, but must
  // wait to do OOP related work until OnDidDocumentDone() is received.
  CheckDocumentSpoolingComplete();

  // Since this call occurs due to all pages having been sent, do not just call
  // `SendDocumentDone()`.  That should happen as a result of callbacks from
  // PrintBackend service.
}

void PrintJobWorkerOop::FinishDocumentDone(int job_id) {
  // Helper function to get onto worker thread, since using the protected base
  // class method directly with `base::BindOnce()` calls is not allowed.
  PrintJobWorker::FinishDocumentDone(job_id);
}

void PrintJobWorkerOop::OnCancel() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::NotifyFailure,
                                ui_weak_factory_.GetWeakPtr(),
                                mojom::ResultCode::kCanceled));
}

void PrintJobWorkerOop::OnFailure() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::NotifyFailure,
                                ui_weak_factory_.GetWeakPtr(),
                                mojom::ResultCode::kFailed));
}

void PrintJobWorkerOop::UnregisterServiceManagerClient() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (service_manager_client_id_.has_value()) {
    PrintBackendServiceManager::GetInstance().UnregisterClient(
        service_manager_client_id_.value());
    service_manager_client_id_.reset();
  }
}

bool PrintJobWorkerOop::TryRestartPrinting() {
  // Safety precaution to avoid any chance of infinite loop for retrying.
  if (print_retried_)
    return false;
  print_retried_ = true;

  // Register that this printer requires elevated privileges.
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.SetPrinterDriverFoundToRequireElevatedPrivilege(device_name_);

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  if (print_from_system_dialog_) {
    // Cannot print from process where system dialog was displayed.  Another
    // print attempt where dialog is used again will be required.
    PRINTER_LOG(ERROR) << "Failure during system printing, unable to retry.";
    return false;
  }
#endif

  // Failure from access-denied means we no longer need the prior client ID.
  UnregisterServiceManagerClient();

  // Need to establish a new persistent printing context to use during
  // printing.  Then retry the operation, which should now happen at a higher
  // privilege level.
  SendEstablishPrintingContext();
  SendStartPrinting(device_name_, document_name_);
  return true;
}

void PrintJobWorkerOop::NotifyFailure(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If an error has occurred during rendering in middle of a multi-page job,
  // it could be possible for the `OnDidRenderPrintedPage()` callback of latter
  // pages to still go through error processing.  In such a case the document
  // might already have been canceled, so we should ensure to only send a
  // cancel request to the service if we haven't already done so.
  if (print_cancel_requested_) {
    return;
  }

  print_cancel_requested_ = true;

  PrintOopResult uma_result = PrintOopResult::kFailed;
  if (result == mojom::ResultCode::kAccessDenied) {
    // An attempt to restart could be undesirable if some pages were able to
    // be sent to the destination before the error occurred.  If we receive
    // an access-denied error in such cases then we just abort this print job
    // with an error notification to the user.  This is more clear to the user
    // what has occurred than if we transparently retry the job and succeed,
    // where the user could end up with too many printed pages and not know
    // why.
    // Register that this printer requires elevated privileges so that any
    // further attempts to print should succeed.
    PrintBackendServiceManager& service_mgr =
        PrintBackendServiceManager::GetInstance();
    service_mgr.SetPrinterDriverFoundToRequireElevatedPrivilege(device_name_);
    uma_result = PrintOopResult::kAccessDenied;
  } else if (result == mojom::ResultCode::kCanceled) {
    uma_result = PrintOopResult::kCanceled;
  }
  base::UmaHistogramEnumeration(kPrintOopPrintResultHistogramName, uma_result);

  if (!document_oop_) {
    // If no document has been started for printing then don't send cancel.
    return;
  }

  // Initiate rest of regular failure handling.
  SendCancel(base::BindOnce(&PrintJobWorkerOop::OnDidCancel,
                            ui_weak_factory_.GetWeakPtr(),
                            base::WrapRefCounted(print_job()), result));
}

void PrintJobWorkerOop::SendEstablishPrintingContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Register this worker as a printing client, if registration wasn't already
  // performed earlier.
  if (!service_manager_client_id_.has_value()) {
    service_manager_client_id_ =
        service_mgr.RegisterPrintDocumentClient(device_name_);
  }

  printing_context_id_ = service_mgr.EstablishPrintingContext(
      *service_manager_client_id_, device_name_
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      ,
      /*parent_view=*/nullptr
#endif
  );
}

void PrintJobWorkerOop::SendStartPrinting(const std::string& device_name,
                                          const std::u16string& document_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());

  // The device name is needed repeatedly for each call to the service, cache
  // that for this print job.
  device_name_ = device_name;

  // Save the document name in case it is needed for retrying a job after
  // failure.
  document_name_ = document_name;

  const int32_t document_cookie = document_oop_->cookie();
  PRINTER_LOG(DEBUG) << "Starting printing via service for to `" << device_name_
                     << "` for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  if (!printing_context_id_.has_value()) {
    // Need to establish a persistent printing context to use during printing.
    SendEstablishPrintingContext();
  }

#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  std::optional<PrintSettings> settings;
  if (print_from_system_dialog_) {
    settings = document_oop_->settings();
  }
#endif
  service_mgr.StartPrinting(
      *service_manager_client_id_, device_name, *printing_context_id_,
      document_cookie, document_name_,
#if !BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
      settings,
#endif
      base::BindOnce(&PrintJobWorkerOop::OnDidStartPrinting,
                     ui_weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::SendRenderPrintedPage(
    const PrintedPage* page,
    mojom::MetafileDataType page_data_type,
    base::ReadOnlySharedMemoryRegion serialized_page_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Page numbers are 0-based for the printing context.
  const uint32_t page_index = page->page_number() - 1;
  const int32_t document_cookie = document_oop_->cookie();
  if (print_cancel_requested_) {
    VLOG(1) << "Dropping page " << page_index << " of document "
            << document_cookie << " to `" << device_name_
            << "` because job was canceled";
    return;
  }

  VLOG(1) << "Sending page " << page_index << " of document " << document_cookie
          << " to `" << device_name_ << "` for printing";
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.RenderPrintedPage(
      *service_manager_client_id_, device_name_, document_cookie, *page,
      page_data_type, std::move(serialized_page_data),
      base::BindOnce(&PrintJobWorkerOop::OnDidRenderPrintedPage,
                     ui_weak_factory_.GetWeakPtr(), page_index));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::SendRenderPrintedDocument(
    mojom::MetafileDataType data_type,
    base::ReadOnlySharedMemoryRegion serialized_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int32_t document_cookie = document_oop_->cookie();
  VLOG(1) << "Sending document " << document_cookie << " to `" << device_name_
          << "` for printing";
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.RenderPrintedDocument(
      *service_manager_client_id_, device_name_, document_cookie,
      document_oop_->page_count(), data_type, std::move(serialized_data),
      base::BindOnce(&PrintJobWorkerOop::OnDidRenderPrintedDocument,
                     ui_weak_factory_.GetWeakPtr()));
}

void PrintJobWorkerOop::SendDocumentDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int32_t document_cookie = document_oop_->cookie();
  VLOG(1) << "Sending document done for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.DocumentDone(*service_manager_client_id_, device_name_,
                           document_cookie,
                           base::BindOnce(&PrintJobWorkerOop::OnDidDocumentDone,
                                          ui_weak_factory_.GetWeakPtr(),
                                          printing_context()->job_id()));
}

void PrintJobWorkerOop::SendCancel(base::OnceClosure on_did_cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  VLOG(1) << "Sending cancel for document " << document_oop_->cookie();

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Retain a reference to the PrintJob to ensure it doesn't get deleted before
  // the `OnDidCancel()` callback occurs.
  service_mgr.Cancel(*service_manager_client_id_, device_name_,
                     document_oop_->cookie(),
                     std::move(on_did_cancel_callback));
}

}  // namespace printing
