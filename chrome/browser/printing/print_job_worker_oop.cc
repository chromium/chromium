// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker_oop.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/metafile.h"
#include "printing/printed_document.h"
#include "printing/printing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "printing/printed_page_win.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

mojom::PrintTargetType DeterminePrintTargetType(
    const base::Value& job_settings) {
#if BUILDFLAG(IS_MAC)
  if (job_settings.FindKey(kSettingOpenPDFInPreview))
    return mojom::PrintTargetType::kExternalPreview;
#endif
  if (job_settings.FindBoolKey(kSettingShowSystemDialog).value_or(false))
    return mojom::PrintTargetType::kSystemDialog;
  return mojom::PrintTargetType::kDirectToDevice;
}

}  // namespace

PrintJobWorkerOop::PrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id)
    : PrintJobWorker(rfh_id) {}

PrintJobWorkerOop::~PrintJobWorkerOop() {
  DCHECK(!service_manager_client_id_.has_value());
}

void PrintJobWorkerOop::StartPrinting(PrintedDocument* new_document) {
  if (!StartPrintingSanityCheck(new_document))
    return;

  // Do browser-side context setup.
  std::u16string document_name = GetDocumentName(new_document);
  mojom::ResultCode result = printing_context()->NewDocument(document_name);
  if (result != mojom::ResultCode::kSuccess) {
    OnFailure();
    return;
  }

  std::string device_name =
      base::UTF16ToUTF8(document()->settings().device_name());
  VLOG(1) << "Start printing document " << document()->cookie() << " to "
          << device_name;

  // `PrintBackendServiceManager` interactions must happen on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::SendStartPrinting,
                                ui_weak_factory_.GetWeakPtr(), device_name,
                                document_name));
}

void PrintJobWorkerOop::OnDidStartPrinting(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Error initiating printing via service for document "
                       << document()->cookie() << ": " << result;
    if (result != mojom::ResultCode::kAccessDenied || !TryRestartPrinting())
      NotifyFailure(result);
    return;
  }
  VLOG(1) << "Printing initiated with service for document "
          << document()->cookie();
#if BUILDFLAG(IS_WIN)
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJobWorker::OnNewPage,
                                         worker_weak_factory_.GetWeakPtr()));
#else
  // TODO(crbug.com/809738)  Still need more support for printing pipeline in
  // the service (need `RenderPrintedDocument()` support).
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                         worker_weak_factory_.GetWeakPtr()));
#endif
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::OnDidRenderPrintedPage(uint32_t page_index,
                                               mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR)
        << "Error rendering printed page via service for document "
        << document()->cookie() << ": " << result;
    NotifyFailure(result);
    return;
  }
  scoped_refptr<PrintedPage> page = document()->GetPage(page_index);
  if (!page) {
    DLOG(ERROR) << "Unable to get page " << page_index << " for document "
                << document()->cookie();
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                           worker_weak_factory_.GetWeakPtr()));
    return;
  }
  VLOG(1) << "Rendered printed page with service for document "
          << document()->cookie() << " page " << page_index;

  // Signal everyone that the page is printed.
  print_job()->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJob::OnPageDone, base::RetainedRef(print_job()),
                     base::RetainedRef(page)));

  ++pages_printed_count_;
  if (pages_printed_count_ == document()->page_count()) {
    // The last page has printed, can proceed to document done processing.
    VLOG(1) << "All pages printed for document";
    SendDocumentDone();
  }
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::OnDidDocumentDone(int job_id,
                                          mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_WIN)
  DCHECK_EQ(pages_printed_count_, document()->page_count());
#endif
  if (result != mojom::ResultCode::kSuccess) {
    VLOG(1) << "Error completing printing via service for document "
            << document()->cookie() << ": " << result;
    NotifyFailure(result);
    return;
  }
  VLOG(1) << "Printing completed with service for document "
          << document()->cookie();
  UnregisterServiceManagerClient();
  FinishDocumentDone(job_id);
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::SpoolPage(PrintedPage* page) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK_NE(page_number(), PageNumber::npos());

#if !defined(NDEBUG)
  DCHECK(document()->IsPageInList(*page));
#endif

  const MetafilePlayer* metafile = page->metafile();
  DCHECK(metafile);
  base::MappedReadOnlyRegion region_mapping =
      metafile->GetDataAsSharedMemoryRegion();
  if (!region_mapping.IsValid()) {
    OnFailure();
    return;
  }

  VLOG(1) << "Spooling page " << page_number() << " to print via service";
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorkerOop::SendRenderPrintedPage,
                     ui_weak_factory_.GetWeakPtr(), base::RetainedRef(page),
                     metafile->GetDataType(),
                     std::move(region_mapping.region)));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::OnDocumentDone() {
  // Can do browser-side checks related to completeness for sending, but must
  // wait to do OOP related work until OnDidDocumentDone() is received.
  CheckDocumentSpoolingComplete();

  // Since this call occurs due to all pages having been sent, do not just call
  // `SendDocumentDone()`.  That should happen as a result of callbacks from
  // PrintBackend service.
}

void PrintJobWorkerOop::UpdatePrintSettings(base::Value new_settings,
                                            SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't use as a const reference, since that reference into `new_settings`
  // isn't safe after TakeDictDeprecated() destroys the internal dictionary for
  // it.
  std::string device_name = *new_settings.FindStringKey(kSettingDeviceName);

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = DeterminePrintTargetType(new_settings);

  VLOG(1) << "Updating print settings via service for " << device_name;
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UpdatePrintSettings(
      device_name, std::move(new_settings).TakeDictDeprecated(),
      base::BindOnce(&PrintJobWorkerOop::OnDidUpdatePrintSettings,
                     ui_weak_factory_.GetWeakPtr(), device_name,
                     std::move(callback)));
}

void PrintJobWorkerOop::OnFailure() {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorkerOop::UnregisterServiceManagerClient,
                     ui_weak_factory_.GetWeakPtr()));
  PrintJobWorker::OnFailure();
}

void PrintJobWorkerOop::ShowErrorDialog() {
  ShowPrintErrorDialog();
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

  // Failure from access-denied means we no longer need the prior client ID.
  UnregisterServiceManagerClient();

  // Retry the operation, which should now happen at a higher privilege
  // level.
  SendStartPrinting(device_name_, document_name_);
  return true;
}

void PrintJobWorkerOop::NotifyFailure(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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
  }
  ShowErrorDialog();

  // Initiate rest of regular failure handling.
  task_runner()->PostTask(FROM_HERE,
                          base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                         worker_weak_factory_.GetWeakPtr()));
}

void PrintJobWorkerOop::OnDidUpdatePrintSettings(
    const std::string& device_name,
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    PRINTER_LOG(ERROR) << "Failure to update print settings for " << device_name
                       << " - error " << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.
  } else {
    VLOG(1) << "Update print settings from service complete for "
            << device_name;
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }
  GetSettingsDone(std::move(callback), result);
}

void PrintJobWorkerOop::SendStartPrinting(const std::string& device_name,
                                          const std::u16string& document_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  // The device name is needed repeatedly for each call to the service, cache
  // that for this print job.
  device_name_ = device_name;

  // Save the document name in case it is needed for retrying a job after
  // failure.
  document_name_ = document_name;

  const int32_t document_cookie = document()->cookie();
  VLOG(1) << "Starting printing via service for to `" << device_name_
          << "` for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Register this worker as a printing client.
  service_manager_client_id_ =
      service_mgr.RegisterPrintDocumentClient(device_name_);

  service_mgr.StartPrinting(
      device_name_, document_cookie, document_name_, print_target_type_,
      document()->settings(),
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
  const int32_t document_cookie = document()->cookie();
  VLOG(1) << "Sending page " << page_index << " of document " << document_cookie
          << " to `" << device_name_ << "` for printing";
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.RenderPrintedPage(
      device_name_, document_cookie, *page, page_data_type,
      std::move(serialized_page_data),
      base::BindOnce(&PrintJobWorkerOop::OnDidRenderPrintedPage,
                     ui_weak_factory_.GetWeakPtr(), page_index));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::SendDocumentDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int32_t document_cookie = document()->cookie();
  VLOG(1) << "Sending document done for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.DocumentDone(device_name_, document_cookie,
                           base::BindOnce(&PrintJobWorkerOop::OnDidDocumentDone,
                                          ui_weak_factory_.GetWeakPtr(),
                                          printing_context()->job_id()));
}

}  // namespace printing
