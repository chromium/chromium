// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_base.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
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
#include "chrome/browser/printing/print_compositor_util.h"
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
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_params.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/buffer.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#include "printing/print_settings_conversion.h"
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/oop_features.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/printing/xps_features.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/module_database.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/printing/local_printer_utils_chromeos.h"
#endif

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/data_protection/print_utils.h"
#endif

namespace printing {

namespace {

void OnDidGetDefaultPrintSettings(
    scoped_refptr<PrintQueriesQueue> queue,
    bool want_pdf_settings,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::GetDefaultPrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (printer_query->last_status() != mojom::ResultCode::kSuccess) {
    if (!want_pdf_settings) {
      ShowPrintErrorDialogForInvalidPrinterError();
    }
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::PrintParamsPtr params = mojom::PrintParams::New();
  RenderParamsFromPrintSettings(printer_query->settings(), params.get());
  params->document_cookie = printer_query->cookie();

  if (!PrintMsgPrintParamsIsValid(*params)) {
    if (!want_pdf_settings) {
      ShowPrintErrorDialogForInvalidPrinterError();
    }
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(std::move(params));
  queue->QueuePrinterQuery(std::move(printer_query));
}

void OnDidScriptedPrint(
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::ScriptedPrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (printer_query->last_status() != mojom::ResultCode::kSuccess ||
      !printer_query->settings().dpi()) {
    // Notify user of the error, unless it was explicitly canceled.
    if (printer_query->last_status() != mojom::ResultCode::kCanceled) {
      ShowPrintErrorDialogForGenericError();
    }
    std::move(callback).Run(nullptr);
    return;
  }

  auto params = mojom::PrintPagesParams::New();
  params->params = mojom::PrintParams::New();
  RenderParamsFromPrintSettings(printer_query->settings(),
                                params->params.get());
  params->params->document_cookie = printer_query->cookie();
  if (!PrintMsgPrintParamsIsValid(*params->params)) {
    std::move(callback).Run(nullptr);
    return;
  }

  params->pages = printer_query->settings().ranges();
  std::move(callback).Run(std::move(params));
  queue->QueuePrinterQuery(std::move(printer_query));
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
std::string PrintMsgPrintParamsErrorDetails(const mojom::PrintParams& params) {
  std::vector<std::string_view> details;

  if (params.content_size.IsEmpty()) {
    details.push_back("content size is empty");
  }
  if (params.page_size.IsEmpty()) {
    details.push_back("page size is empty");
  }
  if (params.printable_area.IsEmpty()) {
    details.push_back("printable area is empty");
  }
  if (!params.document_cookie) {
    details.push_back("invalid document cookie");
  }
  if (params.dpi.width() <= kMinDpi || params.dpi.height() <= kMinDpi) {
    details.push_back("invalid DPI dimensions");
  }
  if (params.margin_top < 0 || params.margin_left < 0) {
    details.push_back("invalid margins");
  }

  return base::JoinString(details, "; ");
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace

BASE_FEATURE(kCheckPrintRfhIsActive,
             "CheckPrintRfhIsActive",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
// TODO(crbug.com/41419019):  Remove `DisableThirdPartyBlocking()` once OOP
// printing is always enabled for Windows.
// static
void PrintViewManagerBase::DisableThirdPartyBlocking() {
#if BUILDFLAG(ENABLE_OOP_PRINTING) && BUILDFLAG(ENABLE_OOP_BASIC_PRINT_DIALOG)
  const bool loads_print_drivers_in_browser_process = !ShouldPrintJobOop();
#else
  constexpr bool loads_print_drivers_in_browser_process = true;
#endif
  if (loads_print_drivers_in_browser_process) {
    ModuleDatabase::DisableThirdPartyBlocking();
  }
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

bool PrintViewManagerBase::PrintNow(content::RenderFrameHost* rfh) {
  if (!StartPrintCommon(rfh)) {
    return false;
  }

  GetPrintRenderFrame(rfh)->PrintRequestedPages();

  for (auto& observer : GetTestObservers()) {
    observer.OnPrintNow(rfh);
  }
  return true;
}

void PrintViewManagerBase::PrintNodeUnderContextMenu(
    content::RenderFrameHost* rfh) {
  if (!StartPrintCommon(rfh)) {
    return;
  }

  GetPrintRenderFrame(rfh)->PrintNodeUnderContextMenu();

  for (auto& observer : GetTestObservers()) {
    observer.OnPrintNow(rfh);
  }
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::PrintForPrintPreview(
    base::Value::Dict job_settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    content::RenderFrameHost* rfh,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING) || BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  bool show_system_dialog =
      job_settings.FindBool(kSettingShowSystemDialog).value_or(false);
#endif

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (show_system_dialog && ShouldPrintJobOop()) {
    if (!RegisterSystemPrintClient()) {
      // Platform unable to support system print dialog at this time, treat
      // this as a cancel.
      std::move(callback).Run(
          base::Value("Concurrent system print not allowed"));
      return;
    }
  }
#endif

  std::unique_ptr<printing::PrinterQuery> printer_query =
      queue()->CreatePrinterQuery(rfh->GetGlobalId());
  auto* printer_query_ptr = printer_query.get();
  const int page_count = job_settings.FindInt(kSettingPreviewPageCount).value();

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (query_with_ui_client_id().has_value()) {
    printer_query->SetClientId(query_with_ui_client_id().value());
  }
#endif
  printer_query_ptr->SetSettings(
      std::move(job_settings),
      base::BindOnce(&PrintViewManagerBase::OnPrintSettingsDone,
                     weak_ptr_factory_.GetWeakPtr(), print_data, page_count,
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
                     show_system_dialog,
#endif
                     std::move(callback), std::move(printer_query)));
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
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  if (content_analysis_before_printing_document_) {
    std::move(content_analysis_before_printing_document_)
        .Run(print_data, page_size, content_area, offsets);
    return;
  }
#endif

#if BUILDFLAG(IS_WIN)
  const bool source_is_pdf =
      !print_job_->document()->settings().is_modifiable();
  if (!ShouldPrintUsingXps(source_is_pdf)) {
    // Print using GDI, which first requires conversion to EMF.
    print_job_->StartConversionToNativeFormat(
        print_data, page_size, content_area, offsets,
        web_contents()->GetLastCommittedURL());
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
#if BUILDFLAG(IS_WIN)
void PrintViewManagerBase::OnDidUpdatePrintableArea(
    std::unique_ptr<PrinterQuery> printer_query,
    base::Value::Dict job_settings,
    std::unique_ptr<PrintSettings> print_settings,
    UpdatePrintSettingsCallback callback,
    bool success) {
  if (!success) {
    PRINTER_LOG(ERROR) << "Unable to update printable area for "
                       << base::UTF16ToUTF8(print_settings->device_name())
                       << " (paper vendor id "
                       << print_settings->requested_media().vendor_id << ")";
    std::move(callback).Run(nullptr);
    return;
  }
  PRINTER_LOG(EVENT) << "Paper printable area updated for vendor id "
                     << print_settings->requested_media().vendor_id;
  CompleteUpdatePrintSettings(std::move(job_settings),
                              std::move(print_settings), std::move(callback));
}
#endif

void PrintViewManagerBase::CompleteUpdatePrintSettings(
    base::Value::Dict job_settings,
    std::unique_ptr<PrintSettings> print_settings,
    UpdatePrintSettingsCallback callback) {
  mojom::PrintPagesParamsPtr settings = mojom::PrintPagesParams::New();
  settings->pages = GetPageRangesFromJobSettings(job_settings);
  settings->params = mojom::PrintParams::New();
  RenderParamsFromPrintSettings(*print_settings, settings->params.get());
  settings->params->document_cookie = PrintSettings::NewCookie();
  if (!PrintMsgPrintParamsIsValid(*settings->params)) {
    mojom::PrinterType printer_type = static_cast<mojom::PrinterType>(
        *job_settings.FindInt(kSettingPrinterType));
    PRINTER_LOG(ERROR) << "Printer settings invalid for "
                       << base::UTF16ToUTF8(print_settings->device_name())
                       << " (destination type " << printer_type << "): "
                       << PrintMsgPrintParamsErrorDetails(*settings->params);
    std::move(callback).Run(nullptr);
    return;
  }

  set_cookie(settings->params->document_cookie);
  std::move(callback).Run(std::move(settings));
}

void PrintViewManagerBase::OnPrintSettingsDone(
    scoped_refptr<base::RefCountedMemory> print_data,
    uint32_t page_count,
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    bool show_system_dialog,
#endif
    PrinterHandler::PrintCallback callback,
    std::unique_ptr<printing::PrinterQuery> printer_query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(printer_query);

  // Check if the job was cancelled.  With out-of-process printing, this could
  // happen if we detect that another system print dialog is already being
  // displayed.  Otherwise this should only happen on Windows when the system
  // dialog is cancelled.
  if (printer_query->last_status() == mojom::ResultCode::kCanceled) {
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (ShouldPrintJobOop()) {
      UnregisterSystemPrintClient();
    }
#endif
#if BUILDFLAG(IS_WIN)
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintViewManagerBase::SystemDialogCancelled,
                                  weak_ptr_factory_.GetWeakPtr()));
#endif
    std::move(callback).Run(base::Value());
    return;
  }

  if (!printer_query->cookie() || !printer_query->settings().dpi()) {
    PRINTER_LOG(ERROR) << "Unable to update print settings";
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (ShouldPrintJobOop()) {
      UnregisterSystemPrintClient();
    }
#endif
    ShowPrintErrorDialogForGenericError();
    std::move(callback).Run(base::Value("Update settings failed"));
    return;
  }

  // Post task so that the query has time to reset the callback before calling
  // DidGetPrintedPagesCount().
  int cookie = printer_query->cookie();
  queue()->QueuePrinterQuery(std::move(printer_query));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrintViewManagerBase::StartLocalPrintJob,
                     weak_ptr_factory_.GetWeakPtr(), print_data, page_count,
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
                     show_system_dialog,
#endif
                     cookie, std::move(callback)));
}

void PrintViewManagerBase::StartLocalPrintJob(
    scoped_refptr<base::RefCountedMemory> print_data,
    uint32_t page_count,
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    bool show_system_dialog,
#endif
    int cookie,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Populating `content_analysis_before_printing_document_` if needed should be
  // done first in this function's workflow, this way other code can check if
  // content analysis is going to happen and delay starting `print_job_` to
  // avoid needlessly prompting the user.
  using enterprise_data_protection::PrintScanningContext;
  auto context = show_system_dialog
                     ? PrintScanningContext::kSystemPrintBeforePrintDocument
                     : PrintScanningContext::kNormalPrintBeforePrintDocument;

  std::optional<enterprise_connectors::ContentAnalysisDelegate::Data>
      scanning_data = enterprise_data_protection::GetPrintAnalysisData(
          web_contents(), context);

  if (scanning_data) {
    set_content_analysis_before_printing_document(base::BindOnce(
        &PrintViewManagerBase::ContentAnalysisBeforePrintingDocument,
        weak_ptr_factory_.GetWeakPtr(), std::move(*scanning_data)));
  }
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

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
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::GetDefaultPrintSettingsReply(
    GetDefaultPrintSettingsCallback callback,
    mojom::PrintParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop() && !params) {
    // The attempt to use the default settings failed.  There should be no
    // subsequent call to get settings from the user that would normally be
    // shared as part of this client registration.  Immediately notify the
    // service manager that this client is no longer needed.
    UnregisterSystemPrintClient();
  }
#endif
  if (params) {
    set_cookie(params->document_cookie);
    std::move(callback).Run(std::move(params));
  } else {
    set_cookie(PrintSettings::NewInvalidCookie());
    std::move(callback).Run(nullptr);
  }
}

void PrintViewManagerBase::ScriptedPrintReply(
    ScriptedPrintCallback callback,
    int process_id,
    mojom::PrintPagesParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    // Finished getting all settings (defaults and from user), no further need
    // to be registered as a system print client.
    UnregisterSystemPrintClient();
  }
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)
  if (!content::RenderProcessHost::FromID(process_id)) {
    // Early return if the renderer is not alive.
    return;
  }

  if (params) {
    set_cookie(params->params->document_cookie);
    std::move(callback).Run(std::move(params));
  } else {
    set_cookie(PrintSettings::NewInvalidCookie());
    std::move(callback).Run(nullptr);
  }
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
  DCHECK(LooksLikePdf(region.Map().GetMemoryAsSpan<const uint8_t>()));
  scoped_refptr<base::RefCountedSharedMemoryMapping> data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region);
  if (!data)
    return false;

  PrintDocument(data, page_size, content_area, physical_offsets);
  return true;
}

void PrintViewManagerBase::OnComposeDocumentDone(
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
  for (auto& observer : GetTestObservers()) {
    observer.OnDidPrintDocument();
  }
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
    NOTREACHED_IN_MIGRATION() << "invalid memory handle";
    web_contents()->Stop();
    OnDidPrintDocument(std::move(callback), /*succeeded=*/false);
    return;
  }

  if (IsOopifEnabled() && print_job_->document()->settings().is_modifiable()) {
    auto* client = PrintCompositeClient::FromWebContents(web_contents());
    client->CompositeDocument(
        params->document_cookie, GetCurrentTargetFrame(), content,
        ui::AXTreeUpdate(), mojom::GenerateDocumentOutline::kNone,
        GetCompositorDocumentType(),
        base::BindOnce(&PrintViewManagerBase::OnComposeDocumentDone,
                       weak_ptr_factory_.GetWeakPtr(), params->document_cookie,
                       params->page_size, params->content_area,
                       params->physical_offsets, std::move(callback)));
    return;
  }
  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED_IN_MIGRATION() << "couldn't map";
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
  if (!GetPrintingEnabledBooleanPref()) {
    GetDefaultPrintSettingsReply(std::move(callback), nullptr);
    return;
  }

  content::RenderFrameHost* render_frame_host = GetCurrentTargetFrame();
  if (base::FeatureList::IsEnabled(kCheckPrintRfhIsActive) &&
      !render_frame_host->IsActive()) {
    // Only active RFHs should show UI elements.
    GetDefaultPrintSettingsReply(std::move(callback), nullptr);
    return;
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop() &&
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
      !analyzing_content_ &&
#endif
      !query_with_ui_client_id().has_value()) {
    // Script initiated print, this is first signal of start of printing.
    RegisterSystemPrintClient();
  }
#endif

  content::RenderProcessHost* render_process_host =
      render_frame_host->GetProcess();
  auto callback_wrapper =
      base::BindOnce(&PrintViewManagerBase::GetDefaultPrintSettingsReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  std::unique_ptr<PrinterQuery> printer_query =
      queue()->PopPrinterQuery(PrintSettings::NewInvalidCookie());
  if (!printer_query) {
    printer_query =
        queue()->CreatePrinterQuery(render_frame_host->GetGlobalId());
#if BUILDFLAG(ENABLE_OOP_PRINTING)
    if (query_with_ui_client_id().has_value()) {
      printer_query->SetClientId(query_with_ui_client_id().value());
    }
#endif
  }

  // Sometimes it is desired to get the PDF settings as opposed to the settings
  // of the default system print driver.
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  bool want_pdf_settings = analyzing_content_;
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
    base::Value::Dict job_settings,
    UpdatePrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!GetPrintingEnabledBooleanPref()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::optional<int> printer_type_value =
      job_settings.FindInt(kSettingPrinterType);
  if (!printer_type_value) {
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::PrinterType printer_type =
      static_cast<mojom::PrinterType>(*printer_type_value);
  if (printer_type != mojom::PrinterType::kExtension &&
      printer_type != mojom::PrinterType::kPdf &&
      printer_type != mojom::PrinterType::kLocal) {
    std::move(callback).Run(nullptr);
    return;
  }

  // `job_settings` does not yet contain the rasterized PDF dpi, so if the user
  // has the print preference set, fetch it for use in
  // `PrintSettingsFromJobSettings()`.
  content::BrowserContext* context =
      web_contents() ? web_contents()->GetBrowserContext() : nullptr;
  PrefService* prefs =
      context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
  if (prefs && prefs->HasPrefPath(prefs::kPrintRasterizePdfDpi)) {
    int value = prefs->GetInteger(prefs::kPrintRasterizePdfDpi);
    if (value > 0)
      job_settings.Set(kSettingRasterizePdfDpi, value);
  }

  std::unique_ptr<PrintSettings> print_settings =
      PrintSettingsFromJobSettings(job_settings);
  if (!print_settings) {
    std::move(callback).Run(nullptr);
    return;
  }

  bool open_in_external_preview =
      job_settings.contains(kSettingOpenPDFInPreview);
  if (!open_in_external_preview &&
      (printer_type == mojom::PrinterType::kPdf ||
       printer_type == mojom::PrinterType::kExtension)) {
    if (print_settings->page_setup_device_units().printable_area().IsEmpty()) {
      PrinterQuery::ApplyDefaultPrintableAreaToVirtualPrinterPrintSettings(
          *print_settings);
    }
  }

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40260379):  Remove this if the printable areas can be made
  // fully available from `PrintBackend::GetPrinterSemanticCapsAndDefaults()`
  // for in-browser queries.
  if (printer_type == mojom::PrinterType::kLocal) {
    // Without a document cookie to find a previous query, must generate a
    // fresh printer query each time, even if the paper size didn't change.
    std::unique_ptr<PrinterQuery> printer_query =
        queue()->CreatePrinterQuery(GetCurrentTargetFrame()->GetGlobalId());

    auto* printer_query_ptr = printer_query.get();
    auto* print_settings_ptr = print_settings.get();
    printer_query_ptr->UpdatePrintableArea(
        print_settings_ptr,
        base::BindOnce(&PrintViewManagerBase::OnDidUpdatePrintableArea,
                       weak_ptr_factory_.GetWeakPtr(), std::move(printer_query),
                       std::move(job_settings), std::move(print_settings),
                       std::move(callback)));
    return;
  }
#endif

  CompleteUpdatePrintSettings(std::move(job_settings),
                              std::move(print_settings), std::move(callback));
}

void PrintViewManagerBase::SetAccessibilityTree(
    int32_t cookie,
    const ui::AXTreeUpdate& accessibility_tree) {
  auto* client = PrintCompositeClient::FromWebContents(web_contents());
  if (client) {
    client->SetAccessibilityTree(cookie, accessibility_tree);
  }
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::IsPrintingEnabled(
    IsPrintingEnabledCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(GetPrintingEnabledBooleanPref());
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
    std::move(callback).Run(nullptr);
    return;
  }
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop() && !query_with_ui_client_id().has_value()) {
    // Renderer process has requested settings outside of the expected setup.
    std::move(callback).Run(nullptr);
    return;
  }
#endif
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  std::optional<enterprise_connectors::ContentAnalysisDelegate::Data>
      scanning_data = enterprise_data_protection::GetPrintAnalysisData(
          web_contents(), enterprise_data_protection::PrintScanningContext::
                              kBeforeSystemDialog);
  if (scanning_data) {
    set_content_analysis_before_printing_document(base::BindOnce(
        &PrintViewManagerBase::ContentAnalysisBeforePrintingDocument,
        weak_ptr_factory_.GetWeakPtr(), std::move(*scanning_data)));
  }
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

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

void PrintViewManagerBase::AddTestObserver(TestObserver& observer) {
  test_observers_.AddObserver(&observer);
}

void PrintViewManagerBase::RemoveTestObserver(TestObserver& observer) {
  test_observers_.RemoveObserver(&observer);
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

  for (auto& observer : GetTestObservers()) {
    observer.OnRenderFrameDeleted();
  }

  printing_rfh_ = nullptr;

  PrintManager::PrintingRenderFrameDeleted();
  ReleasePrinterQuery();
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    UnregisterSystemPrintClient();
  }
#endif

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

bool PrintViewManagerBase::GetPrintingEnabledBooleanPref() const {
  return printing_enabled_.GetValue();
}

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

scoped_refptr<PrintJob> PrintViewManagerBase::CreatePrintJob(
    PrintJobManager* print_job_manager) {
  return base::MakeRefCounted<PrintJob>(print_job_manager);
}

bool PrintViewManagerBase::SetupNewPrintJob(
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
  print_job_ = CreatePrintJob(g_browser_process->print_job_manager());
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
  if (ShouldPrintJobOop()) {
    // Ensure that any residual registration of printing client is released.
    // This might be necessary in some abnormal cases, such as the associated
    // render process having terminated.
#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    if (!analyzing_content_) {
      UnregisterSystemPrintClient();
    }
#else
    UnregisterSystemPrintClient();
#endif
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
  std::unique_ptr<PrinterQuery> queued_query = queue()->PopPrinterQuery(cookie);
  if (!queued_query) {
    // Out of sync.  It may happen since we are completely asynchronous, when
    // an error occurs during the first setup of a print job.
    return false;
  }

  if (!SetupNewPrintJob(std::move(queued_query))) {
    // Don't kill anything.
    return false;
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Don't start printing if enterprise checks are being performed to check if
  // printing is allowed, or if content analysis is going to take place right
  // before starting `print_job_`.
  if (analyzing_content_ || content_analysis_before_printing_document_) {
    return true;
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

bool PrintViewManagerBase::StartPrintCommon(content::RenderFrameHost* rfh) {
  // Remember the ID for `rfh`, to enable checking that the `RenderFrameHost`
  // is still valid after a possible inner message loop runs in
  // `DisconnectFromCurrentPrintJob()`.
  content::GlobalRenderFrameHostId rfh_id = rfh->GetGlobalId();
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  DisconnectFromCurrentPrintJob();
  if (!weak_this) {
    return false;
  }

  // Don't print / print preview crashed tabs.
  if (IsCrashed()) {
    return false;
  }

  // Don't print if `rfh` is no longer live.
  if (!content::RenderFrameHost::FromID(rfh_id) || !rfh->IsRenderFrameLive()) {
    return false;
  }

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  if (ShouldPrintJobOop()) {
    // Register this worker so that the service persists as long as the user
    // keeps the system print dialog UI displayed.
    if (!RegisterSystemPrintClient()) {
      return false;
    }
  }
#endif

  SetPrintingRFH(rfh);
  return true;
}

#if BUILDFLAG(ENABLE_OOP_PRINTING)
bool PrintViewManagerBase::RegisterSystemPrintClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());
  DCHECK(!query_with_ui_client_id().has_value());
  query_with_ui_client_id_ =
      PrintBackendServiceManager::GetInstance().RegisterQueryWithUiClient();
  bool registered = query_with_ui_client_id().has_value();
  if (!registered) {
    PRINTER_LOG(DEBUG) << "Unable to initiate a concurrent system print dialog";
  }
  for (auto& observer : GetTestObservers()) {
    observer.OnRegisterSystemPrintClient(registered);
  }
  return registered;
}

void PrintViewManagerBase::UnregisterSystemPrintClient() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(ShouldPrintJobOop());
  if (!query_with_ui_client_id().has_value()) {
    return;
  }

  PrintBackendServiceManager::GetInstance().UnregisterClient(
      query_with_ui_client_id().value());
  query_with_ui_client_id_.reset();
}
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

void PrintViewManagerBase::ReleasePrinterQuery() {
  int current_cookie = cookie();
  if (!current_cookie)
    return;

  set_cookie(PrintSettings::NewInvalidCookie());

  PrintJobManager* print_job_manager = g_browser_process->print_job_manager();
  // May be NULL in tests.
  if (!print_job_manager)
    return;

  // Let `printer_query` go out of scope to release it.
  std::unique_ptr<PrinterQuery> printer_query =
      queue()->PopPrinterQuery(current_cookie);
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
  DisableThirdPartyBlocking();
#endif

  std::unique_ptr<PrinterQuery> printer_query =
      queue()->PopPrinterQuery(params->cookie);
  if (!printer_query)
    printer_query = queue()->CreatePrinterQuery(rfh->GetGlobalId());

  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->GetSettingsFromUser(
      params->expected_pages_count, params->has_selection, params->margin_type,
      params->is_scripted, !render_process_host->IsPdf(),
      base::BindOnce(&OnDidScriptedPrint, queue_, std::move(printer_query),
                     std::move(callback_wrapper)));
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintViewManagerBase::CompletePrintDocumentAfterContentAnalysis(
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& offsets,
    bool allowed) {
  if (!allowed || IsCrashed()) {
    ReleasePrinterQuery();
    print_job_->CleanupAfterContentAnalysisDenial();
    TerminatePrintJob(/*cancel=*/true);
    return;
  }
  print_job_->StartPrinting();
  PrintDocument(print_data, page_size, content_area, offsets);
}

void PrintViewManagerBase::ContentAnalysisBeforePrintingDocument(
    enterprise_connectors::ContentAnalysisDelegate::Data scanning_data,
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& offsets) {
  scanning_data.printer_name =
      base::UTF16ToUTF8(print_job_->document()->settings().device_name());

  auto on_verdict = base::BindOnce(
      &PrintViewManagerBase::CompletePrintDocumentAfterContentAnalysis,
      weak_ptr_factory_.GetWeakPtr(), print_data, page_size, content_area,
      offsets);

  enterprise_data_protection::PrintIfAllowedByPolicy(
      print_data, web_contents()->GetOutermostWebContents(),
      std::move(scanning_data), std::move(on_verdict));
}

void PrintViewManagerBase::set_analyzing_content(bool analyzing) {
  PRINTER_LOG(EVENT) << (analyzing ? "Starting" : "Completed")
                     << " content analysis";
  analyzing_content_ = analyzing;
}

void PrintViewManagerBase::set_content_analysis_before_printing_document(
    PrintDocumentCallback callback) {
  content_analysis_before_printing_document_ = std::move(callback);
}
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace printing
