// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_base.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_params.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/buffer.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_features.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/module_database.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/printing/print_job_utils_lacros.h"
#endif

namespace printing {

namespace {

using PrintSettingsCallback =
    base::OnceCallback<void(std::unique_ptr<PrinterQuery>)>;

void OnDidGetDefaultPrintSettings(
    scoped_refptr<PrintQueriesQueue> queue,
    bool want_pdf_settings,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::GetDefaultPrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::PrintParamsPtr params = mojom::PrintParams::New();
  if (printer_query &&
      printer_query->last_status() == mojom::ResultCode::kSuccess) {
    RenderParamsFromPrintSettings(printer_query->settings(), params.get());
    params->document_cookie = printer_query->cookie();
  }

  if (!want_pdf_settings && !PrintMsgPrintParamsIsValid(*params)) {
    ShowPrintErrorDialogForInvalidPrinterError();
  }

  std::move(callback).Run(std::move(params));

  // If printing was enabled.
  if (printer_query) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue->QueuePrinterQuery(std::move(printer_query));
    }
  }
}

mojom::PrintPagesParamsPtr CreateEmptyPrintPagesParamsPtr() {
  auto params = mojom::PrintPagesParams::New();
  params->params = mojom::PrintParams::New();
  return params;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#if BUILDFLAG(IS_WIN)
void NotifySystemDialogCancelled(base::WeakPtr<PrintViewManagerBase> manager) {
  if (manager)
    manager->SystemDialogCancelled();
}
#endif  // BUILDFLAG(IS_WIN)

void OnDidUpdatePrintSettings(
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::UpdatePrintSettingsCallback callback,
    base::WeakPtr<PrintViewManagerBase> manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printer_query);
  mojom::PrintPagesParamsPtr params = CreateEmptyPrintPagesParamsPtr();
  if (printer_query->last_status() == mojom::ResultCode::kSuccess) {
    RenderParamsFromPrintSettings(printer_query->settings(),
                                  params->params.get());
    params->params->document_cookie = printer_query->cookie();
    params->pages = printer_query->settings().ranges();
  }
  bool canceled = printer_query->last_status() == mojom::ResultCode::kCanceled;
#if BUILDFLAG(IS_WIN)
  if (canceled)
    NotifySystemDialogCancelled(std::move(manager));
#endif

  std::move(callback).Run(std::move(params), canceled);

  if (printer_query->cookie() && printer_query->settings().dpi()) {
    queue->QueuePrinterQuery(std::move(printer_query));
  }
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void OnDidScriptedPrint(
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::PrintPagesParamsPtr params = CreateEmptyPrintPagesParamsPtr();
  if (printer_query->last_status() == mojom::ResultCode::kSuccess &&
      printer_query->settings().dpi()) {
    RenderParamsFromPrintSettings(printer_query->settings(),
                                  params->params.get());
    params->params->document_cookie = printer_query->cookie();
    params->pages = printer_query->settings().ranges();
  }
  bool has_valid_cookie = params->params->document_cookie;
  bool has_dpi = !params->params->dpi.IsEmpty();
  std::move(callback).Run(std::move(params));

  if (has_dpi && has_valid_cookie) {
    queue->QueuePrinterQuery(std::move(printer_query));
  }
}

}  // namespace

PrintViewManagerBase::PrintViewManagerBase(content::WebContents* web_contents)
    : PrintManager(web_contents),
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  printing_enabled_.Init(prefs::kPrintingEnabled, profile->GetPrefs());
}

PrintViewManagerBase::~PrintViewManagerBase() {
  ReleasePrinterQuery();
  DisconnectFromCurrentPrintJob();
}

bool PrintViewManagerBase::PrintNow(content::RenderFrameHost* rfh) {
  // Remember the ID for `rfh`, to enable checking that the `RenderFrameHost`
  // is still valid after a possible inner message loop runs in
  // `DisconnectFromCurrentPrintJob()`.
  content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this)
    return false;

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  // Don't print if `rfh` is no longer live.
  if (!content::RenderFrameHost::FromID(rfh_id) || !rfh->IsRenderFrameLive())
    return false;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get()) {
    // Register this worker so that the service persists as long as the user
    // keeps the system print dialog UI displayed.
    if (!RegisterSystemPrintClient())
      return false;
  }
#endif

  SetPrintingRFH(rfh);
  CompletePrintNow(rfh);
  return true;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::PrintForPrintPreview(
    base::Value::Dict job_settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    content::RenderFrameHost* rfh,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get() &&
      job_settings.FindBool(kSettingShowSystemDialog).value_or(false)) {
    if (!RegisterSystemPrintClient()) {
      // Platform unable to support system print dialog at this time, treat
      // this as a cancel.
      std::move(callback).Run(
          base::Value("Concurrent system print not allowed"));
      return;
    }
  }
#endif
  PrintSettingsCallback settings_callback =
      base::BindOnce(&PrintViewManagerBase::OnPrintSettingsDone,
                     weak_ptr_factory_.GetWeakPtr(), print_data,
                     job_settings.FindInt(kSettingPreviewPageCount).value(),
                     std::move(callback));
  std::unique_ptr<printing::PrinterQuery> printer_query =
      queue_->CreatePrinterQuery(rfh->GetGlobalId());
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->SetSettings(
      std::move(job_settings),
      base::BindOnce(std::move(settings_callback), std::move(printer_query)));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::PrintToPdf(
    content::RenderFrameHost* rfh,
    const std::string& page_ranges,
    mojom::PrintPagesParamsPtr print_pages_params,
    print_to_pdf::PdfPrintJob::PrintToPdfCallback callback) {
  print_to_pdf::PdfPrintJob::StartJob(
      web_contents(), rfh, GetPrintRenderFrame(rfh), page_ranges,
      std::move(print_pages_params), std::move(callback));
}

void PrintViewManagerBase::PrintDocument(
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& offsets) {
#if BUILDFLAG(IS_WIN)
  const bool source_is_pdf =
      !print_job_->document()->settings().is_modifiable();
  if (!printing::features::ShouldPrintUsingXps(source_is_pdf)) {
    // Print using GDI, which first requires conversion to EMF.
    print_job_->StartConversionToNativeFormat(print_data, page_size,
                                              content_area, offsets);
    return;
  }
#endif

  std::unique_ptr<MetafileSkia> metafile = std::make_unique<MetafileSkia>();
  CHECK(metafile->InitFromData(*print_data));

  // Update the rendered document. It will send notifications to the listener.
  PrintedDocument* document = print_job_->document();
  document->SetDocument(std::move(metafile));
  ShouldQuitFromInnerMessageLoop();
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::OnPrintSettingsDone(
    scoped_refptr<base::RefCountedMemory> print_data,
    uint32_t page_count,
    PrinterHandler::PrintCallback callback,
    std::unique_ptr<printing::PrinterQuery> printer_query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printer_query);

  // Check if the job was cancelled.  With out-of-process printing, this could
  // happen if we detect that another system print dialog is already being
  // displayed.  Otherwise this should only happen on Windows when the system
  // dialog is cancelled.
  if (printer_query->last_status() == mojom::ResultCode::kCanceled) {
    queue_->QueuePrinterQuery(std::move(printer_query));
#if BUILDFLAG(IS_WIN)
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintViewManagerBase::SystemDialogCancelled,
                                  weak_ptr_factory_.GetWeakPtr()));
#endif
    std::move(callback).Run(base::Value());
    return;
  }

  if (!printer_query->cookie() || !printer_query->settings().dpi()) {
    std::move(callback).Run(base::Value("Update settings failed"));
    return;
  }

  // Post task so that the query has time to reset the callback before calling
  // DidGetPrintedPagesCount().
  int cookie = printer_query->cookie();
  queue_->QueuePrinterQuery(std::move(printer_query));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintViewManagerBase::StartLocalPrintJob,
                                weak_ptr_factory_.GetWeakPtr(), print_data,
                                page_count, cookie, std::move(callback)));
}

void PrintViewManagerBase::StartLocalPrintJob(
    scoped_refptr<base::RefCountedMemory> print_data,
    uint32_t page_count,
    int cookie,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  set_cookie(cookie);
  DidGetPrintedPagesCount(cookie, page_count);

  if (!PrintJobHasDocument(cookie)) {
    std::move(callback).Run(base::Value("Failed to print"));
    return;
  }

#if BUILDFLAG(IS_WIN)
  print_job_->ResetPageMapping();
#endif

  const printing::PrintSettings& settings = print_job_->settings();
  gfx::Size page_size = settings.page_setup_device_units().physical_size();
  gfx::Rect content_area =
      gfx::Rect(0, 0, page_size.width(), page_size.height());

  PrintDocument(print_data, page_size, content_area,
                settings.page_setup_device_units().printable_area().origin());
  std::move(callback).Run(base::Value());
}

void PrintViewManagerBase::UpdatePrintSettingsReply(
    mojom::PrintManagerHost::UpdatePrintSettingsCallback callback,
    mojom::PrintPagesParamsPtr params,
    bool canceled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  set_cookie(params->params->document_cookie);
  std::move(callback).Run(std::move(params), canceled);
}

#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::GetDefaultPrintSettingsReply(
    GetDefaultPrintSettingsCallback callback,
    mojom::PrintParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get() &&
      !params->document_cookie) {
    // The attempt to use the default settings failed.  There should be no
    // subsequent call to get settings from the user that would normally be
    // shared as part of this client registration.  Immediately notify the
    // service manager that this client is no longer needed.
    UnregisterSystemPrintClient();
  }
#endif
  set_cookie(params->document_cookie);
  std::move(callback).Run(std::move(params));
}

void PrintViewManagerBase::ScriptedPrintReply(
    ScriptedPrintCallback callback,
    int process_id,
    mojom::PrintPagesParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get()) {
#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
    if (params->params->document_cookie) {
      // Want the same PrintBackend service as the query so that we use the
      // same device context.
      DCHECK(query_with_ui_client_id_.has_value());
      print_document_client_id_ =
          PrintBackendServiceManager::GetInstance()
              .RegisterPrintDocumentClientReusingClientRemote(
                  *query_with_ui_client_id_);
    }
#endif
    // Finished getting all settings (defaults and from user), no further need
    // to be registered as a system print client.
    UnregisterSystemPrintClient();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  if (!content::RenderProcessHost::FromID(process_id)) {
    // Early return if the renderer is not alive.
    return;
  }

  set_cookie(params->params->document_cookie);
  std::move(callback).Run(std::move(params));
}

void PrintViewManagerBase::NavigationStopped() {
  // Cancel the current job, wait for the worker to finish.
  TerminatePrintJob(true);
}

std::u16string PrintViewManagerBase::RenderSourceName() {
  std::u16string name(web_contents()->GetTitle());
  if (name.empty())
    name = l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE);
  return name;
}

void PrintViewManagerBase::DidGetPrintedPagesCount(int32_t cookie,
                                                   uint32_t number_pages) {
  PrintManager::DidGetPrintedPagesCount(cookie, number_pages);
  OpportunisticallyCreatePrintJob(cookie);
}

#if BUILDFLAG(ENABLE_TAGGED_PDF)
void PrintViewManagerBase::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  auto* client = PrintCompositeClient::FromWebContents(web_contents());
  if (client)
    client->SetAccessibilityTree(cookie, accessibility_tree);
}
#endif

bool PrintViewManagerBase::PrintJobHasDocument(int cookie) {
  if (!OpportunisticallyCreatePrintJob(cookie))
    return false;

  // These checks may fail since we are completely asynchronous. Old spurious
  // messages can be received if one of the processes is overloaded.
  PrintedDocument* document = print_job_->document();
  return document && document->cookie() == cookie;
}

bool PrintViewManagerBase::OnComposePdfDoneImpl(
    int document_cookie,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    return false;
  }

  if (!print_job_ || !print_job_->document() ||
      print_job_->document()->cookie() != document_cookie) {
    return false;
  }

  DCHECK(region.IsValid());
  DCHECK(LooksLikePdf(region.Map().GetMemoryAsSpan<char>()));
  scoped_refptr<base::RefCountedSharedMemoryMapping> data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region);
  if (!data)
    return false;

  PrintDocument(data, page_size, content_area, physical_offsets);
  return true;
}

void PrintViewManagerBase::OnComposePdfDone(
    int document_cookie,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    DidPrintDocumentCallback callback,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool success =
      OnComposePdfDoneImpl(document_cookie, page_size, content_area,
                           physical_offsets, status, std::move(region));
  OnDidPrintDocument(std::move(callback), success);
}

void PrintViewManagerBase::OnDidPrintDocument(DidPrintDocumentCallback callback,
                                              bool succeeded) {
  std::move(callback).Run(succeeded);
  for (auto& observer : observers_)
    observer.OnDidPrintDocument();
}

void PrintViewManagerBase::DidPrintDocument(
    mojom::DidPrintDocumentParamsPtr params,
    DidPrintDocumentCallback callback) {
  if (!PrintJobHasDocument(params->document_cookie)) {
    OnDidPrintDocument(std::move(callback), /*succeeded=*/false);
    return;
  }

  const mojom::DidPrintContentParams& content = *params->content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
    web_contents()->Stop();
    OnDidPrintDocument(std::move(callback), /*succeeded=*/false);
    return;
  }

  if (IsOopifEnabled() && print_job_->document()->settings().is_modifiable()) {
    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    client->DoCompositeDocumentToPdf(
        params->document_cookie, GetCurrentTargetFrame(), content,
        base::BindOnce(&PrintViewManagerBase::OnComposePdfDone,
                       weak_ptr_factory_.GetWeakPtr(), params->document_cookie,
                       params->page_size, params->content_area,
                       params->physical_offsets, std::move(callback)));
    return;
  }
  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
    web_contents()->Stop();
    OnDidPrintDocument(std::move(callback), /*succeeded=*/false);
    return;
  }

  PrintDocument(data, params->page_size, params->content_area,
                params->physical_offsets);
  OnDidPrintDocument(std::move(callback), /*succeeded=*/true);
}

void PrintViewManagerBase::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!printing_enabled_.GetValue()) {
    GetDefaultPrintSettingsReply(std::move(callback),
                                 mojom::PrintParams::New());
    return;
  }
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get() &&
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
      !snapshotting_for_content_analysis_ &&
#endif
      !query_with_ui_client_id_.has_value()) {
    // Renderer process has requested settings outside of the expected setup.
    GetDefaultPrintSettingsReply(std::move(callback),
                                 mojom::PrintParams::New());
    return;
  }
#endif

  content::RenderFrameHost* render_frame_host = GetCurrentTargetFrame();
  content::RenderProcessHost* render_process_host =
      render_frame_host->GetProcess();
  auto callback_wrapper =
      base::BindOnce(&PrintViewManagerBase::GetDefaultPrintSettingsReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  std::unique_ptr<PrinterQuery> printer_query = queue_->PopPrinterQuery(0);
  if (!printer_query) {
    printer_query =
        queue_->CreatePrinterQuery(render_frame_host->GetGlobalId());
  }

  // Sometimes it is desired to get the PDF settings as opposed to the settings
  // of the default system print driver.
#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  bool want_pdf_settings = snapshotting_for_content_analysis_;
#else
  bool want_pdf_settings = false;
#endif

  // Loads default settings. This is asynchronous, only the mojo message sender
  // will hang until the settings are retrieved.
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->GetDefaultSettings(
      base::BindOnce(&OnDidGetDefaultPrintSettings, queue_, want_pdf_settings,
                     std::move(printer_query), std::move(callback_wrapper)),
      !render_process_host->IsPdf(), want_pdf_settings);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::UpdatePrintSettings(
    int32_t cookie,
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!printing_enabled_.GetValue()) {
    UpdatePrintSettingsReply(std::move(callback),
                             CreateEmptyPrintPagesParamsPtr(), false);
    return;
  }

  if (!job_settings.FindInt(kSettingPrinterType)) {
    UpdatePrintSettingsReply(std::move(callback),
                             CreateEmptyPrintPagesParamsPtr(), false);
    return;
  }

  content::BrowserContext* context =
      web_contents() ? web_contents()->GetBrowserContext() : nullptr;
  PrefService* prefs =
      context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
  if (prefs && prefs->HasPrefPath(prefs::kPrintRasterizePdfDpi)) {
    int value = prefs->GetInteger(prefs::kPrintRasterizePdfDpi);
    if (value > 0)
      job_settings.Set(kSettingRasterizePdfDpi, value);
  }

  auto callback_wrapper =
      base::BindOnce(&PrintViewManagerBase::UpdatePrintSettingsReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  std::unique_ptr<PrinterQuery> printer_query = queue_->PopPrinterQuery(cookie);
  if (!printer_query) {
    printer_query =
        queue_->CreatePrinterQuery(content::GlobalRenderFrameHostId());
  }
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->SetSettings(
      std::move(job_settings),
      base::BindOnce(&OnDidUpdatePrintSettings, queue_,
                     std::move(printer_query), std::move(callback_wrapper),
                     weak_ptr_factory_.GetWeakPtr()));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::IsPrintingEnabled(
    IsPrintingEnabledCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(printing_enabled_.GetValue());
}

void PrintViewManagerBase::ScriptedPrint(mojom::ScriptedPrintParamsPtr params,
                                         ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderFrameHost* render_frame_host = GetCurrentTargetFrame();
  content::RenderProcessHost* render_process_host =
      render_frame_host->GetProcess();
  if (params->is_scripted && render_frame_host->IsNestedWithinFencedFrame()) {
    // The renderer should have checked and disallowed the request for fenced
    // frames in ChromeClient. Ignore the request and mark it as bad if it
    // didn't happen for some reason.
    bad_message::ReceivedBadMessage(
        render_process_host, bad_message::PVMB_SCRIPTED_PRINT_FENCED_FRAME);
    std::move(callback).Run(CreateEmptyPrintPagesParamsPtr());
    return;
  }
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get() &&
      !query_with_ui_client_id_.has_value()) {
    // Renderer process has requested settings outside of the expected setup.
    std::move(callback).Run(CreateEmptyPrintPagesParamsPtr());
    return;
  }
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  enterprise_connectors::ContentAnalysisDelegate::Data scanning_data;
  if (base::FeatureList::IsEnabled(features::kEnablePrintContentAnalysis) &&
      enterprise_connectors::ContentAnalysisDelegate::IsEnabled(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
          web_contents()->GetOutermostWebContents()->GetLastCommittedURL(),
          &scanning_data, enterprise_connectors::AnalysisConnector::PRINT)) {
    auto scanning_done_callback = base::BindOnce(
        &PrintViewManagerBase::CompleteScriptedPrintAfterContentAnalysis,
        weak_ptr_factory_.GetWeakPtr(), std::move(params), std::move(callback));
    set_snapshotting_for_content_analysis();
    GetPrintRenderFrame(render_frame_host)
        ->SnapshotForContentAnalysis(base::BindOnce(
            &PrintViewManagerBase::OnGotSnapshotCallback,
            weak_ptr_factory_.GetWeakPtr(), std::move(scanning_done_callback),
            std::move(scanning_data), render_frame_host->GetGlobalId()));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

  CompleteScriptedPrint(render_frame_host, std::move(params),
                        std::move(callback));
}

void PrintViewManagerBase::PrintingFailed(int32_t cookie,
                                          mojom::PrintFailureReason reason) {
  // Note: Not redundant with cookie checks in the same method in other parts of
  // the class hierarchy.
  if (!IsValidCookie(cookie))
    return;

  PrintManager::PrintingFailed(cookie, reason);

  // `PrintingFailed()` can occur because asynchronous compositing results
  // don't complete until after a print job has already failed and been
  // destroyed.  In such cases the error notification to the user will
  // have already been displayed, and a second message should not be
  // shown.
  if (print_job_ && print_job_->document() &&
      print_job_->document()->cookie() == cookie) {
    ShowPrintErrorDialogForGenericError();
  }

  ReleasePrinterQuery();
}

void PrintViewManagerBase::AddObserver(Observer& observer) {
  observers_.AddObserver(&observer);
}

void PrintViewManagerBase::RemoveObserver(Observer& observer) {
  observers_.RemoveObserver(&observer);
}

void PrintViewManagerBase::RenderFrameHostStateChanged(
    content::RenderFrameHost* render_frame_host,
    content::RenderFrameHost::LifecycleState /*old_state*/,
    content::RenderFrameHost::LifecycleState new_state) {
  if (new_state == content::RenderFrameHost::LifecycleState::kActive &&
      render_frame_host->GetProcess()->IsPdf() &&
      !render_frame_host->GetMainFrame()->GetParentOrOuterDocument()) {
    GetPrintRenderFrame(render_frame_host)->ConnectToPdfRenderer();
  }
}

void PrintViewManagerBase::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  PrintManager::RenderFrameDeleted(render_frame_host);

  // Terminates or cancels the print job if one was pending.
  if (render_frame_host != printing_rfh_)
    return;

  printing_rfh_ = nullptr;

  PrintManager::PrintingRenderFrameDeleted();
  ReleasePrinterQuery();

  if (!print_job_)
    return;

  scoped_refptr<PrintedDocument> document(print_job_->document());
  if (document) {
    // If IsComplete() returns false, the document isn't completely rendered.
    // Since our renderer is gone, there's nothing to do, cancel it. Otherwise,
    // the print job may finish without problem.
    TerminatePrintJob(!document->IsComplete());
  }
}

#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::SystemDialogCancelled() {
  // System dialog was cancelled. Clean up the print job and notify the
  // BackgroundPrintingManager.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReleasePrinterQuery();
  TerminatePrintJob(true);
}
#endif

void PrintViewManagerBase::OnDocDone(int job_id, PrintedDocument* document) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  NotifyAshJobCreated(*print_job_, job_id, *document);
#endif
#if BUILDFLAG(IS_ANDROID)
  DCHECK_LE(number_pages(), kMaxPageCount);
  PdfWritingDone(base::checked_cast<int>(number_pages()));
#endif
}

void PrintViewManagerBase::OnJobDone() {
  // Printing is done, we don't need it anymore.
  // print_job_->is_job_pending() may still be true, depending on the order
  // of object registration.
  printing_succeeded_ = true;
  ReleasePrintJob();
}

void PrintViewManagerBase::OnCanceling() {
  canceling_job_ = true;
}

void PrintViewManagerBase::OnFailed() {
  if (!canceling_job_)
    ShowPrintErrorDialogForGenericError();

  TerminatePrintJob(true);
}

bool PrintViewManagerBase::RenderAllMissingPagesNow() {
  if (!print_job_ || !print_job_->is_job_pending())
    return false;

  // Is the document already complete?
  if (print_job_->document() && print_job_->document()->IsComplete()) {
    printing_succeeded_ = true;
    return true;
  }

  // We can't print if there is no renderer.
  if (!web_contents() ||
      !web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    return false;
  }

  // WebContents is either dying or a second consecutive request to print
  // happened before the first had time to finish. We need to render all the
  // pages in an hurry if a print_job_ is still pending. No need to wait for it
  // to actually spool the pages, only to have the renderer generate them. Run
  // a message loop until we get our signal that the print job is satisfied.
  // `quit_inner_loop_` will be called as soon as
  // print_job_->document()->IsComplete() is true in DidPrintDocument(). The
  // check is done in ShouldQuitFromInnerMessageLoop().
  // BLOCKS until all the pages are received. (Need to enable recursive task)
  // WARNING: Do not do any work after RunInnerMessageLoop() returns, as `this`
  // may have gone away.
  if (!RunInnerMessageLoop()) {
    // This function is always called from DisconnectFromCurrentPrintJob() so we
    // know that the job will be stopped/canceled in any case.
    return false;
  }
  return true;
}

void PrintViewManagerBase::ShouldQuitFromInnerMessageLoop() {
  // Look at the reason.
  DCHECK(print_job_->document());
  if (print_job_->document() && print_job_->document()->IsComplete() &&
      quit_inner_loop_) {
    // We are in a message loop created by RenderAllMissingPagesNow. Quit from
    // it.
    std::move(quit_inner_loop_).Run();
  }
}

bool PrintViewManagerBase::CreateNewPrintJob(
    std::unique_ptr<PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!quit_inner_loop_);
  DCHECK(query);

  // Disconnect the current `print_job_`.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this)
    return false;

  // We can't print if there is no renderer.
  if (!web_contents()->GetPrimaryMainFrame()->IsRenderFrameLive()) {
    return false;
  }

  DCHECK(!print_job_);
  print_job_ =
      base::MakeRefCounted<PrintJob>(g_browser_process->print_job_manager());
  print_job_->Initialize(std::move(query), RenderSourceName(), number_pages());
#if BUILDFLAG(IS_CHROMEOS)
  print_job_->SetSource(web_contents()->GetBrowserContext()->IsOffTheRecord()
                            ? PrintJob::Source::kPrintPreviewIncognito
                            : PrintJob::Source::kPrintPreview,
                        /*source_id=*/"");
#endif
  print_job_->AddObserver(*this);

  printing_succeeded_ = false;
  return true;
}

void PrintViewManagerBase::DisconnectFromCurrentPrintJob() {
  // Make sure all the necessary rendered page are done. Don't bother with the
  // return value.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  bool result = RenderAllMissingPagesNow();
  if (!weak_this)
    return;

  // Verify that assertion.
  if (print_job_ && print_job_->document() &&
      !print_job_->document()->IsComplete()) {
    DCHECK(!result);
    // That failed.
    TerminatePrintJob(true);
  } else {
    // DO NOT wait for the job to finish.
    ReleasePrintJob();
  }
}

void PrintViewManagerBase::TerminatePrintJob(bool cancel) {
  if (!print_job_)
    return;

  if (cancel) {
    canceling_job_ = true;

    // We don't need the metafile data anymore because the printing is canceled.
    print_job_->Cancel();
    quit_inner_loop_.Reset();
#if BUILDFLAG(IS_ANDROID)
    PdfWritingDone(0);
#endif
  } else {
    DCHECK(!quit_inner_loop_);
    DCHECK(!print_job_->document() || print_job_->document()->IsComplete());

    // WebContents is either dying or navigating elsewhere. We need to render
    // all the pages in an hurry if a print job is still pending. This does the
    // trick since it runs a blocking message loop:
    print_job_->Stop();
  }
  ReleasePrintJob();
}

void PrintViewManagerBase::ReleasePrintJob() {
  content::RenderFrameHost* rfh = printing_rfh_;
  printing_rfh_ = nullptr;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (printing::features::kEnableOopPrintDriversJobPrint.Get()) {
    // Ensure that any residual registration of printing client is released.
    // This might be necessary in some abnormal cases, such as the associated
    // render process having terminated.
    UnregisterSystemPrintClient();
  }
#endif

  if (!print_job_)
    return;

  if (rfh) {
    // printing_rfh_ should only ever point to a RenderFrameHost with a live
    // RenderFrame.
    DCHECK(rfh->IsRenderFrameLive());
    GetPrintRenderFrame(rfh)->PrintingDone(printing_succeeded_);
  }

  print_job_->RemoveObserver(*this);

  // Don't close the worker thread.
  print_job_ = nullptr;
}

bool PrintViewManagerBase::RunInnerMessageLoop() {
  // This value may actually be too low:
  //
  // - If we're looping because of printer settings initialization, the premise
  // here is that some poor users have their print server away on a VPN over a
  // slow connection. In this situation, the simple fact of opening the printer
  // can be dead slow. On the other side, we don't want to die infinitely for a
  // real network error. Give the printer 60 seconds to comply.
  //
  // - If we're looping because of renderer page generation, the renderer could
  // be CPU bound, the page overly complex/large or the system just
  // memory-bound.
  static constexpr base::TimeDelta kPrinterSettingsTimeout = base::Seconds(60);
  base::OneShotTimer quit_timer;
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  quit_timer.Start(FROM_HERE, kPrinterSettingsTimeout,
                   run_loop.QuitWhenIdleClosure());

  quit_inner_loop_ = run_loop.QuitClosure();

  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  run_loop.Run();
  if (!weak_this)
    return false;

  // If the inner-loop quit closure is still set then we timed out.
  bool success = !quit_inner_loop_;
  quit_inner_loop_.Reset();

  return success;
}

bool PrintViewManagerBase::OpportunisticallyCreatePrintJob(int cookie) {
  if (print_job_)
    return true;

  if (!cookie) {
    // Out of sync. It may happen since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return false;
  }

  // The job was initiated by a script. Time to get the corresponding worker
  // thread.
  std::unique_ptr<PrinterQuery> queued_query = queue_->PopPrinterQuery(cookie);
  if (!queued_query) {
    // Out of sync.  It may happen since we are completely asynchronous, when
    // an error occurs during the first setup of a print job.
    return false;
  }

  if (!CreateNewPrintJob(std::move(queued_query))) {
    // Don't kill anything.
    return false;
  }

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  // Don't start printing if the print job was created only for snapshotting.
  if (snapshotting_for_content_analysis_)
    return true;
#endif

#if BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  if (print_document_client_id_) {
    // Ensure that the print job knows it is already registered as a client.
    print_job_->SetPrintDocumentClient(*print_document_client_id_);
    print_document_client_id_.reset();
  }
#endif

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

bool PrintViewManagerBase::IsCrashed() {
  return web_contents()->IsCrashed();
}

void PrintViewManagerBase::SetPrintingRFH(content::RenderFrameHost* rfh) {
  // Do not allow any print operation during prerendering.
  if (rfh->GetLifecycleState() ==
      content::RenderFrameHost::LifecycleState::kPrerendering) {
    // If we come here during prerendering, it's because either:
    // 1) Renderer did something unexpected (indicates a compromised renderer),
    // or 2) Some plumbing in the browser side is wrong (wrong code).
    // mojo::ReportBadMessage() below will let the renderer crash for 1), or
    // will hit DCHECK for 2).
    mojo::ReportBadMessage(
        "The print's message shouldn't reach here during prerendering.");
    return;
  }
  DCHECK(!printing_rfh_);
  // Protect against future unsafety, since printing_rfh_ is cleared by
  // RenderFrameDeleted(), which will not be called if the render frame is not
  // live.
  CHECK(rfh->IsRenderFrameLive());
  printing_rfh_ = rfh;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
bool PrintViewManagerBase::RegisterSystemPrintClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printing::features::kEnableOopPrintDriversJobPrint.Get());
  DCHECK(!query_with_ui_client_id_.has_value());
  query_with_ui_client_id_ =
      PrintBackendServiceManager::GetInstance().RegisterQueryWithUiClient();
  bool registered = query_with_ui_client_id_.has_value();
  if (!registered) {
    PRINTER_LOG(DEBUG) << "Unable to initiate a concurrent system print dialog";
  }
  for (auto& observer : GetObservers()) {
    observer.OnRegisterSystemPrintClient(registered);
  }
  return registered;
}

void PrintViewManagerBase::UnregisterSystemPrintClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printing::features::kEnableOopPrintDriversJobPrint.Get());
  if (!query_with_ui_client_id_.has_value()) {
    return;
  }

  PrintBackendServiceManager::GetInstance().UnregisterClient(
      *query_with_ui_client_id_);
  query_with_ui_client_id_.reset();
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

void PrintViewManagerBase::ReleasePrinterQuery() {
  int current_cookie = cookie();
  if (!current_cookie)
    return;

  set_cookie(0);

  PrintJobManager* print_job_manager = g_browser_process->print_job_manager();
  // May be NULL in tests.
  if (!print_job_manager)
    return;

  std::unique_ptr<PrinterQuery> printer_query =
      queue_->PopPrinterQuery(current_cookie);
  if (!printer_query)
    return;
}

void PrintViewManagerBase::CompletePrintNow(content::RenderFrameHost* rfh) {
  GetPrintRenderFrame(rfh)->PrintRequestedPages();

  for (auto& observer : GetObservers())
    observer.OnPrintNow(rfh);
}

void PrintViewManagerBase::CompleteScriptedPrint(
    content::RenderFrameHost* rfh,
    mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* render_process_host = rfh->GetProcess();
  auto callback_wrapper = base::BindOnce(
      &PrintViewManagerBase::ScriptedPrintReply, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), render_process_host->GetID());
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  ModuleDatabase::GetInstance()->DisableThirdPartyBlocking();
#endif

  std::unique_ptr<PrinterQuery> printer_query =
      queue_->PopPrinterQuery(params->cookie);
  if (!printer_query)
    printer_query = queue_->CreatePrinterQuery(rfh->GetGlobalId());

  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->GetSettingsFromUser(
      params->expected_pages_count, params->has_selection, params->margin_type,
      params->is_scripted, !render_process_host->IsPdf(),
      base::BindOnce(&OnDidScriptedPrint, queue_, std::move(printer_query),
                     std::move(callback_wrapper)));
}

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
void PrintViewManagerBase::CompletePrintNowAfterContentAnalysis(bool allowed) {
  if (!allowed || !printing_rfh_ || IsCrashed() ||
      !printing_rfh_->IsRenderFrameLive()) {
    return;
  }

  CompletePrintNow(printing_rfh_);
}

void PrintViewManagerBase::CompleteScriptedPrintAfterContentAnalysis(
    mojom::ScriptedPrintParamsPtr params,
    ScriptedPrintCallback callback,
    bool allowed) {
  if (!allowed || !printing_rfh_ || IsCrashed() ||
      !printing_rfh_->IsRenderFrameLive()) {
    std::move(callback).Run(CreateEmptyPrintPagesParamsPtr());
    return;
  }
  CompleteScriptedPrint(printing_rfh_, std::move(params), std::move(callback));
}

void PrintViewManagerBase::OnGotSnapshotCallback(
    base::OnceCallback<void(bool should_proceed)> callback,
    enterprise_connectors::ContentAnalysisDelegate::Data data,
    content::GlobalRenderFrameHostId rfh_id,
    mojom::DidPrintDocumentParamsPtr params) {
  snapshotting_for_content_analysis_ = false;
  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  if (!params || !rfh || !PrintJobHasDocument(params->document_cookie) ||
      !params->content->metafile_data_region.IsValid()) {
    TerminatePrintJob(/*cancel=*/true);
    std::move(callback).Run(/*allowed=*/true);
    return;
  }

  if (IsOopifEnabled() && print_job_->document()->settings().is_modifiable()) {
    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    client->DoCompositeDocumentToPdf(
        params->document_cookie, rfh, *params->content,
        base::BindOnce(&PrintViewManagerBase::OnCompositedForContentAnalysis,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(data), rfh_id));
    return;
  }
  OnCompositedForContentAnalysis(
      std::move(callback), std::move(data), rfh_id,
      mojom::PrintCompositor::Status::kSuccess,
      std::move(params->content->metafile_data_region));
}

void PrintViewManagerBase::OnCompositedForContentAnalysis(
    base::OnceCallback<void(bool should_proceed)> callback,
    enterprise_connectors::ContentAnalysisDelegate::Data data,
    content::GlobalRenderFrameHostId rfh_id,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion page_region) {
  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  if (!rfh || status != mojom::PrintCompositor::Status::kSuccess ||
      !page_region.IsValid()) {
    TerminatePrintJob(/*cancel=*/true);
    std::move(callback).Run(true);
    return;
  }

  // Reset the print job and `rfh` so the snapshotting doesn't affect the actual
  // printing later.
  TerminatePrintJob(/*cancel=*/true);
  SetPrintingRFH(rfh);
  data.page = std::move(page_region);

  enterprise_connectors::ContentAnalysisDelegate::CreateForWebContents(
      web_contents()->GetOutermostWebContents(), std::move(data),
      base::BindOnce(
          [](base::OnceCallback<void(bool should_proceed)> callback,
             const enterprise_connectors::ContentAnalysisDelegate::Data& data,
             enterprise_connectors::ContentAnalysisDelegate::Result& result) {
            std::move(callback).Run(result.page_result);
          },
          std::move(callback)),
      safe_browsing::DeepScanAccessPoint::PRINT);
}

void PrintViewManagerBase::set_snapshotting_for_content_analysis() {
  snapshotting_for_content_analysis_ = true;
}

#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace printing
