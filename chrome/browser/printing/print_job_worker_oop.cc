// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker_oop.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
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
#include "content/public/browser/web_contents.h"
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

mojom::PrintTargetType DeterminePrintTargetType(
    const base::Value::Dict& job_settings) {
#if BUILDFLAG(IS_MAC)
  if (job_settings.contains(kSettingOpenPDFInPreview))
    return mojom::PrintTargetType::kExternalPreview;
#endif
  if (job_settings.FindBool(kSettingShowSystemDialog).value_or(false))
    return mojom::PrintTargetType::kSystemDialog;
  return mojom::PrintTargetType::kDirectToDevice;
}

}  // namespace

PrintJobWorkerOop::PrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id)
    : PrintJobWorker(rfh_id) {}

PrintJobWorkerOop::PrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id,
                                     bool simulate_spooling_memory_errors)
    : PrintJobWorker(rfh_id),
      simulate_spooling_memory_errors_(simulate_spooling_memory_errors) {}

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

void PrintJobWorkerOop::OnDidUseDefaultSettings(
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    PRINTER_LOG(ERROR) << "Error trying to use default settings via service: "
                       << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  } else {
    VLOG(1) << "Use default settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }

  GetSettingsDone(std::move(callback), result);
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::OnDidAskUserForSettings(
    SettingsCallback callback,
    mojom::PrintSettingsResultPtr print_settings) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::ResultCode result;
  if (print_settings->is_result_code()) {
    result = print_settings->get_result_code();
    DCHECK_NE(result, mojom::ResultCode::kSuccess);
    if (result != mojom::ResultCode::kCanceled) {
      PRINTER_LOG(ERROR) << "Error getting settings from user via service: "
                         << result;
    }

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.  Blocked on crbug.com/1243873 for Windows.
  } else {
    VLOG(1) << "Ask user for settings from service complete";
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }

  GetSettingsDone(std::move(callback), result);
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJobWorkerOop::OnDidStartPrinting(mojom::ResultCode result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != mojom::ResultCode::kSuccess) {
    PRINTER_LOG(ERROR) << "Error initiating printing via service for document "
                       << document_oop_->cookie() << ": " << result;
    if (result != mojom::ResultCode::kAccessDenied || !TryRestartPrinting())
      NotifyFailure(result);
    return;
  }
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
    task_runner()->PostTask(FROM_HERE,
                            base::BindOnce(&PrintJobWorkerOop::OnFailure,
                                           worker_weak_factory_.GetWeakPtr()));
    return;
  }
  VLOG(1) << "Rendered printed page via service for document "
          << document_oop_->cookie() << " page " << page_index;

  // Signal everyone that the page is printed.
  print_job()->PostTask(FROM_HERE,
                        base::BindOnce(&PrintJob::OnPageDone, print_job(),
                                       base::RetainedRef(page)));

  ++pages_printed_count_;
  if (pages_printed_count_ == document_oop_->page_count()) {
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
  DCHECK_EQ(pages_printed_count_, document_oop_->page_count());
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
  FinishDocumentDone(job_id);

  // Also done with private document reference.
  document_oop_ = nullptr;
}

void PrintJobWorkerOop::OnDidCancel(scoped_refptr<PrintJob> job) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "Cancel completed for printing via service for document "
           << document_oop_->cookie();

  UnregisterServiceManagerClient();

  // Done with private document reference.
  document_oop_ = nullptr;
}

#if BUILDFLAG(IS_WIN)
bool PrintJobWorkerOop::SpoolPage(PrintedPage* page) {
  DCHECK(task_runner()->RunsTasksInCurrentSequence());
  DCHECK_NE(page_number(), PageNumber::npos());

#if !defined(NDEBUG)
  DCHECK(document_oop_->IsPageInList(*page));
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

  const MetafilePlayer* metafile = document_oop_->GetMetafile();
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

void PrintJobWorkerOop::UseDefaultSettings(SettingsCallback callback) {
  SendUseDefaultSettings(std::move(callback));
}

void PrintJobWorkerOop::GetSettingsWithUI(uint32_t document_page_count,
                                          bool has_selection,
                                          bool is_scripted,
                                          SettingsCallback callback) {
#if BUILDFLAG(IS_WIN)
  SendAskUserForSettings(document_page_count, has_selection, is_scripted,
                         std::move(callback));
#else
  // Invoke the browser version of getting settings with the system UI:
  //   - macOS:  It is impossible to invoke a system dialog UI from a service
  //       utility and have that dialog be application modal for a window that
  //       was launched by the browser process.
  //   - Linux:  TODO(crbug.com/809738)  Determine if Linux Wayland can be made
  //       to have a system dialog be modal against an application window in the
  //       browser process.
  //   - Other platforms don't have a system print UI or do not use OOP
  //     printing, so this does not matter.
  PrintJobWorker::GetSettingsWithUI(document_page_count, has_selection,
                                    is_scripted, std::move(callback));
#endif
}

void PrintJobWorkerOop::SetSettings(base::Value::Dict new_settings,
                                    SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Do not take a const reference, as `new_settings` will be modified below.
  std::string device_name = *new_settings.FindString(kSettingDeviceName);

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = DeterminePrintTargetType(new_settings);

  VLOG(1) << "Updating print settings via service for " << device_name;
  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UpdatePrintSettings(
      device_name, std::move(new_settings),
      base::BindOnce(&PrintJobWorkerOop::OnDidUpdatePrintSettings,
                     ui_weak_factory_.GetWeakPtr(), device_name,
                     std::move(callback)));
}

void PrintJobWorkerOop::OnFailure() {
  // Retain a reference to the PrintJob to ensure it doesn't get deleted before
  // the `OnDidCancel()` callback occurs.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorkerOop::SendCancel,
                                ui_weak_factory_.GetWeakPtr(),
                                base::WrapRefCounted(print_job())));
  PrintJobWorker::OnFailure();
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
    PRINTER_LOG(ERROR) << "Error updating print settings via service for `"
                       << device_name << "`: " << result;

    // TODO(crbug.com/809738)  Fill in support for handling of access-denied
    // result code.
  } else {
    VLOG(1) << "Update print settings via service complete for " << device_name;
    result = mojom::ResultCode::kSuccess;
    printing_context()->ApplyPrintSettings(print_settings->get_settings());
  }
  GetSettingsDone(std::move(callback), result);
}

void PrintJobWorkerOop::SendUseDefaultSettings(SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.UseDefaultSettings(
      /*printer_name=*/std::string(),
      base::BindOnce(&PrintJobWorkerOop::OnDidUseDefaultSettings,
                     ui_weak_factory_.GetWeakPtr(), std::move(callback)));
}

#if BUILDFLAG(IS_WIN)
void PrintJobWorkerOop::SendAskUserForSettings(uint32_t document_page_count,
                                               bool has_selection,
                                               bool is_scripted,
                                               SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(features::kEnableOopPrintDriversJobPrint.Get());

  if (document_page_count > kMaxPageCount) {
    GetSettingsDone(std::move(callback), mojom::ResultCode::kFailed);
    return;
  }

  // Save the print target type from the settings, since this will be needed
  // later when printing is started.
  print_target_type_ = mojom::PrintTargetType::kDirectToDevice;

  content::WebContents* web_contents = GetWebContents();

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreen())
    web_contents->ExitFullscreen(true);

  gfx::NativeView parent_view =
      web_contents ? web_contents->GetTopLevelNativeWindow() : nullptr;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();
  service_mgr.AskUserForSettings(
      /*printer_name=*/std::string(), parent_view, document_page_count,
      has_selection, is_scripted,
      base::BindOnce(&PrintJobWorkerOop::OnDidAskUserForSettings,
                     ui_weak_factory_.GetWeakPtr(), std::move(callback)));
}
#endif  // BUILDFLAG(IS_WIN)

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

  const int32_t document_cookie = document_oop_->cookie();
  PRINTER_LOG(DEBUG) << "Starting printing via service for to `" << device_name_
                     << "` for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Register this worker as a printing client.
  service_manager_client_id_ =
      service_mgr.RegisterPrintDocumentClient(device_name_);

  service_mgr.StartPrinting(
      device_name_, document_cookie, document_name_, print_target_type_,
      document_oop_->settings(),
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
      device_name_, document_cookie, document_oop_->page_count(), data_type,
      std::move(serialized_data),
      base::BindOnce(&PrintJobWorkerOop::OnDidRenderPrintedDocument,
                     ui_weak_factory_.GetWeakPtr()));
}

void PrintJobWorkerOop::SendDocumentDone() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int32_t document_cookie = document_oop_->cookie();
  VLOG(1) << "Sending document done for document " << document_cookie;

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  service_mgr.DocumentDone(device_name_, document_cookie,
                           base::BindOnce(&PrintJobWorkerOop::OnDidDocumentDone,
                                          ui_weak_factory_.GetWeakPtr(),
                                          printing_context()->job_id()));
}

void PrintJobWorkerOop::SendCancel(scoped_refptr<PrintJob> job) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If an error has occurred during rendering in middle of a multi-page job,
  // it could be possible for the `OnDidRenderPrintedPage()` callback of latter
  // pages to still go through error processing.  In such a case the document
  // might already have been canceled, so we should ensure to only send a
  // cancel request to the service if we haven't already done so.
  if (print_cancel_requested_)
    return;

  print_cancel_requested_ = true;
  VLOG(1) << "Sending cancel for document " << document_oop_->cookie();

  PrintBackendServiceManager& service_mgr =
      PrintBackendServiceManager::GetInstance();

  // Retain a reference to the PrintJob to ensure it doesn't get deleted before
  // the `OnDidCancel()` callback occurs.
  service_mgr.Cancel(
      device_name_, document_oop_->cookie(),
      base::BindOnce(&PrintJobWorkerOop::OnDidCancel,
                     ui_weak_factory_.GetWeakPtr(), std::move(job)));
}

}  // namespace printing
