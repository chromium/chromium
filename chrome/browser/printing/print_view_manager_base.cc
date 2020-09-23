// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_base.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/printer_query.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_messages.h"
#include "components/services/print_compositor/public/cpp/print_service_mojo_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/buffer.h"
#include "printing/buildflags/buildflags.h"
#include "printing/metafile_skia.h"
#include "printing/mojom/print.mojom.h"
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "printing/printing_features.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_error_dialog.h"
#endif

namespace printing {

namespace {

using PrintSettingsCallback =
    base::OnceCallback<void(std::unique_ptr<PrinterQuery>)>;

void ShowWarningMessageBox(const base::string16& message) {
  // Runs always on the UI thread.
  static bool is_dialog_shown = false;
  if (is_dialog_shown)
    return;
  // Block opening dialog from nested task.
  base::AutoReset<bool> auto_reset(&is_dialog_shown, true);

  chrome::ShowWarningMessageBox(nullptr, base::string16(), message);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void OnPrintSettingsDoneWrapper(PrintSettingsCallback settings_callback,
                                std::unique_ptr<PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(settings_callback), std::move(query)));
}

void CreateQueryWithSettings(base::Value job_settings,
                             int render_process_id,
                             int render_frame_id,
                             scoped_refptr<PrintQueriesQueue> queue,
                             PrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  PrintSettingsCallback callback_wrapper =
      base::BindOnce(OnPrintSettingsDoneWrapper, std::move(callback));
  std::unique_ptr<printing::PrinterQuery> printer_query =
      queue->CreatePrinterQuery(render_process_id, render_frame_id);
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->SetSettings(
      std::move(job_settings),
      base::BindOnce(std::move(callback_wrapper), std::move(printer_query)));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Runs |callback| with |params| to reply to
// mojom::PrintManagerHost::GetDefaultPrintSettings.
void GetDefaultPrintSettingsReply(
    mojom::PrintManagerHost::GetDefaultPrintSettingsCallback callback,
    mojom::PrintParamsPtr params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(callback).Run(std::move(params));
}

void GetDefaultPrintSettingsReplyOnIO(
    scoped_refptr<PrintQueriesQueue> queue,
    std::unique_ptr<PrinterQuery> printer_query,
    mojom::PrintManagerHost::GetDefaultPrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  mojom::PrintParamsPtr params = mojom::PrintParams::New();
  if (printer_query && printer_query->last_status() == PrintingContext::OK) {
    RenderParamsFromPrintSettings(printer_query->settings(), params.get());
    params->document_cookie = printer_query->cookie();
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&GetDefaultPrintSettingsReply,
                                std::move(callback), std::move(params)));

  // If printing was enabled.
  if (printer_query) {
    // If user hasn't cancelled.
    if (printer_query->cookie() && printer_query->settings().dpi()) {
      queue->QueuePrinterQuery(std::move(printer_query));
    } else {
      printer_query->StopWorker();
    }
  }
}

void GetDefaultPrintSettingsOnIO(
    mojom::PrintManagerHost::GetDefaultPrintSettingsCallback callback,
    scoped_refptr<PrintQueriesQueue> queue,
    int process_id,
    int routing_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::unique_ptr<PrinterQuery> printer_query = queue->PopPrinterQuery(0);
  if (!printer_query)
    printer_query = queue->CreatePrinterQuery(process_id, routing_id);

  // Loads default settings. This is asynchronous, only the mojo message sender
  // will hang until the settings are retrieved.
  auto* printer_query_ptr = printer_query.get();
  printer_query_ptr->GetSettings(
      PrinterQuery::GetSettingsAskParam::DEFAULTS, 0, false,
      printing::mojom::MarginType::kDefaultMargins, false, false,
      base::BindOnce(&GetDefaultPrintSettingsReplyOnIO, queue,
                     std::move(printer_query), std::move(callback)));
}

}  // namespace

PrintViewManagerBase::PrintViewManagerBase(content::WebContents* web_contents)
    : PrintManager(web_contents),
      queue_(g_browser_process->print_job_manager()->queue()) {
  DCHECK(queue_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  printing_enabled_.Init(
      prefs::kPrintingEnabled, profile->GetPrefs(),
      base::BindRepeating(&PrintViewManagerBase::UpdatePrintingEnabled,
                          weak_ptr_factory_.GetWeakPtr()));
}

PrintViewManagerBase::~PrintViewManagerBase() {
  ReleasePrinterQuery();
  DisconnectFromCurrentPrintJob();
}

bool PrintViewManagerBase::PrintNow(content::RenderFrameHost* rfh) {
  DisconnectFromCurrentPrintJob();

  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;

  SetPrintingRFH(rfh);
  GetPrintRenderFrame(rfh)->PrintRequestedPages();
  return true;
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::PrintForPrintPreview(
    base::Value job_settings,
    scoped_refptr<base::RefCountedMemory> print_data,
    content::RenderFrameHost* rfh,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PrintSettingsCallback settings_callback =
      base::BindOnce(&PrintViewManagerBase::OnPrintSettingsDone,
                     weak_ptr_factory_.GetWeakPtr(), print_data,
                     job_settings.FindIntKey(kSettingPreviewPageCount).value(),
                     std::move(callback));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(CreateQueryWithSettings, std::move(job_settings),
                     rfh->GetProcess()->GetID(), rfh->GetRoutingID(), queue_,
                     std::move(settings_callback)));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::PrintDocument(
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& offsets) {
#if defined(OS_WIN)
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

  // Check if the job was cancelled. This should only happen on Windows when
  // the system dialog is cancelled.
  if (printer_query->last_status() == PrintingContext::CANCEL) {
    queue_->QueuePrinterQuery(std::move(printer_query));
#if defined(OS_WIN)
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintViewManagerBase::SystemDialogCancelled,
                                  weak_ptr_factory_.GetWeakPtr()));
#endif
    std::move(callback).Run(base::Value());
    return;
  }

  if (!printer_query->cookie() || !printer_query->settings().dpi()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PrinterQuery::StopWorker, std::move(printer_query)));
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

  DidGetPrintedPagesCount(cookie, page_count);

  if (!PrintJobHasDocument(cookie)) {
    std::move(callback).Run(base::Value("Failed to print"));
    return;
  }

#if defined(OS_WIN)
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

void PrintViewManagerBase::UpdatePrintingEnabled() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The Unretained() is safe because ForEachFrame() is synchronous.
  web_contents()->ForEachFrame(base::BindRepeating(
      &PrintViewManagerBase::SendPrintingEnabled, base::Unretained(this),
      printing_enabled_.GetValue()));
}

void PrintViewManagerBase::NavigationStopped() {
  // Cancel the current job, wait for the worker to finish.
  TerminatePrintJob(true);
}

base::string16 PrintViewManagerBase::RenderSourceName() {
  base::string16 name(web_contents()->GetTitle());
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
  PrintCompositeClient::FromWebContents(web_contents())
      ->SetAccessibilityTree(cookie, accessibility_tree);
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

void PrintViewManagerBase::OnComposePdfDone(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    std::unique_ptr<DelayedFrameDispatchHelper> helper,
    mojom::PrintCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PrintCompositor::Status::kSuccess) {
    DLOG(ERROR) << "Compositing pdf failed with error " << status;
    return;
  }

  if (!print_job_->document())
    return;

  scoped_refptr<base::RefCountedSharedMemoryMapping> data =
      base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(region);
  if (!data)
    return;

  PrintDocument(data, page_size, content_area, physical_offsets);
  helper->SendCompleted();
}

void PrintViewManagerBase::OnDidPrintDocument(
    content::RenderFrameHost* render_frame_host,
    const mojom::DidPrintDocumentParams& params,
    std::unique_ptr<DelayedFrameDispatchHelper> helper) {
  if (!PrintJobHasDocument(params.document_cookie))
    return;

  const mojom::DidPrintContentParams& content = *params.content;
  if (!content.metafile_data_region.IsValid()) {
    NOTREACHED() << "invalid memory handle";
    web_contents()->Stop();
    return;
  }

  auto* client = PrintCompositeClient::FromWebContents(web_contents());
  if (IsOopifEnabled() && print_job_->document()->settings().is_modifiable()) {
    client->DoCompositeDocumentToPdf(
        params.document_cookie, render_frame_host, content,
        base::BindOnce(&PrintViewManagerBase::OnComposePdfDone,
                       weak_ptr_factory_.GetWeakPtr(), params.page_size,
                       params.content_area, params.physical_offsets,
                       std::move(helper)));
    return;
  }
  auto data = base::RefCountedSharedMemoryMapping::CreateFromWholeRegion(
      content.metafile_data_region);
  if (!data) {
    NOTREACHED() << "couldn't map";
    web_contents()->Stop();
    return;
  }

  PrintDocument(data, params.page_size, params.content_area,
                params.physical_offsets);
  helper->SendCompleted();
}

void PrintViewManagerBase::GetDefaultPrintSettings(
    GetDefaultPrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!printing_enabled_.GetValue()) {
    auto params = mojom::PrintParams::New();
    GetDefaultPrintSettingsReply(std::move(callback), std::move(params));
    return;
  }

  content::RenderFrameHost* render_frame_host =
      print_manager_host_receivers_.GetCurrentTargetFrame();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&GetDefaultPrintSettingsOnIO, std::move(callback), queue_,
                     render_frame_host->GetProcess()->GetID(),
                     render_frame_host->GetRoutingID()));
}

void PrintViewManagerBase::PrintingFailed(int32_t cookie) {
  PrintManager::PrintingFailed(cookie);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  ShowPrintErrorDialog();
#endif

  ReleasePrinterQuery();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_RELEASED,
      content::Source<content::WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void PrintViewManagerBase::OnScriptedPrint(
    content::RenderFrameHost* render_frame_host,
    const mojom::ScriptedPrintParams& params,
    IPC::Message* reply_msg) {
  NOTREACHED() << "should be handled by printing:: PrintingMessageFilter";
}

void PrintViewManagerBase::ShowInvalidPrinterSettingsError() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ShowWarningMessageBox,
                                l10n_util::GetStringUTF16(
                                    IDS_PRINT_INVALID_PRINTER_SETTINGS)));
}

void PrintViewManagerBase::DidStartLoading() {
  UpdatePrintingEnabled();
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

#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::SystemDialogCancelled() {
  // System dialog was cancelled. Clean up the print job and notify the
  // BackgroundPrintingManager.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ReleasePrinterQuery();
  TerminatePrintJob(true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_RELEASED,
      content::Source<content::WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}
#endif

void PrintViewManagerBase::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);
  OnNotifyPrintJobEvent(*content::Details<JobEventDetails>(details).ptr());
}

void PrintViewManagerBase::OnNotifyPrintJobEvent(
    const JobEventDetails& event_details) {
  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      TerminatePrintJob(true);

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<content::WebContents>(web_contents()),
          content::NotificationService::NoDetails());
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      NOTREACHED();
      break;
    }
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      ShouldQuitFromInnerMessageLoop();
      break;
    }
#if defined(OS_WIN)
    case JobEventDetails::PAGE_DONE:
#endif
    case JobEventDetails::NEW_DOC: {
      // Don't care about the actual printing process.
      break;
    }
    case JobEventDetails::DOC_DONE: {
      // Don't care about the actual printing process, except on Android.
#if defined(OS_ANDROID)
      DCHECK_LE(number_pages_, kMaxPageCount);
      PdfWritingDone(base::checked_cast<int>(number_pages_));
#endif
      break;
    }
    case JobEventDetails::JOB_DONE: {
      // Printing is done, we don't need it anymore.
      // print_job_->is_job_pending() may still be true, depending on the order
      // of object registration.
      printing_succeeded_ = true;
      ReleasePrintJob();

      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PRINT_JOB_RELEASED,
          content::Source<content::WebContents>(web_contents()),
          content::NotificationService::NoDetails());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
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
      !web_contents()->GetRenderViewHost() ||
      !web_contents()->GetRenderViewHost()->IsRenderViewLive()) {
    return false;
  }

  // WebContents is either dying or a second consecutive request to print
  // happened before the first had time to finish. We need to render all the
  // pages in an hurry if a print_job_ is still pending. No need to wait for it
  // to actually spool the pages, only to have the renderer generate them. Run
  // a message loop until we get our signal that the print job is satisfied.
  // PrintJob will send a ALL_PAGES_REQUESTED after having received all the
  // pages it needs. |quit_inner_loop_| will be called as soon as
  // print_job_->document()->IsComplete() is true on either ALL_PAGES_REQUESTED
  // or in DidPrintDocument(). The check is done in
  // ShouldQuitFromInnerMessageLoop().
  // BLOCKS until all the pages are received. (Need to enable recursive task)
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

  // Disconnect the current |print_job_|.
  DisconnectFromCurrentPrintJob();

  // We can't print if there is no renderer.
  if (!web_contents()->GetRenderViewHost() ||
      !web_contents()->GetRenderViewHost()->IsRenderViewLive()) {
    return false;
  }

  DCHECK(!print_job_);
  print_job_ = base::MakeRefCounted<PrintJob>();
  print_job_->Initialize(std::move(query), RenderSourceName(), number_pages_);
#if defined(OS_CHROMEOS)
  PrintJob::Source source = PrintJob::Source::PRINT_PREVIEW;
  if (base::FeatureList::IsEnabled(
          chromeos::features::kPrintJobManagementApp) &&
      web_contents()->GetBrowserContext()->IsOffTheRecord()) {
    source = PrintJob::Source::PRINT_PREVIEW_INCOGNITO;
  }
  print_job_->SetSource(source, /*source_id=*/"");
#endif

  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::Source<PrintJob>(print_job_.get()));
  printing_succeeded_ = false;
  return true;
}

void PrintViewManagerBase::DisconnectFromCurrentPrintJob() {
  // Make sure all the necessary rendered page are done. Don't bother with the
  // return value.
  bool result = RenderAllMissingPagesNow();

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
    // We don't need the metafile data anymore because the printing is canceled.
    print_job_->Cancel();
    quit_inner_loop_.Reset();
#if defined(OS_ANDROID)
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

  if (!print_job_)
    return;

  if (rfh)
    GetPrintRenderFrame(rfh)->PrintingDone(printing_succeeded_);

  registrar_.Remove(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                    content::Source<PrintJob>(print_job_.get()));
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
  static constexpr base::TimeDelta kPrinterSettingsTimeout =
      base::TimeDelta::FromSeconds(60);
  base::OneShotTimer quit_timer;
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  quit_timer.Start(FROM_HERE, kPrinterSettingsTimeout,
                   run_loop.QuitWhenIdleClosure());

  quit_inner_loop_ = run_loop.QuitClosure();

  run_loop.Run();

  // If the inner-loop quit closure is still set then we timed out.
  bool success = !quit_inner_loop_;
  quit_inner_loop_.Reset();

  return success;
}

bool PrintViewManagerBase::OpportunisticallyCreatePrintJob(int cookie) {
  if (print_job_)
    return true;

  if (!cookie) {
    // Out of sync. It may happens since we are completely asynchronous. Old
    // spurious message can happen if one of the processes is overloaded.
    return false;
  }

  // The job was initiated by a script. Time to get the corresponding worker
  // thread.
  std::unique_ptr<PrinterQuery> queued_query = queue_->PopPrinterQuery(cookie);
  if (!queued_query) {
    NOTREACHED();
    return false;
  }

  if (!CreateNewPrintJob(std::move(queued_query))) {
    // Don't kill anything.
    return false;
  }

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

bool PrintViewManagerBase::IsCrashed() {
  return web_contents()->IsCrashed();
}

bool PrintViewManagerBase::PrintNowInternal(
    content::RenderFrameHost* rfh,
    std::unique_ptr<IPC::Message> message) {
  // Don't print / print preview crashed tabs.
  if (IsCrashed())
    return false;
  return rfh->Send(message.release());
}

void PrintViewManagerBase::SetPrintingRFH(content::RenderFrameHost* rfh) {
  DCHECK(!printing_rfh_);
  printing_rfh_ = rfh;
}

void PrintViewManagerBase::ReleasePrinterQuery() {
  if (!cookie_)
    return;

  int cookie = cookie_;
  cookie_ = 0;

  PrintJobManager* print_job_manager = g_browser_process->print_job_manager();
  // May be NULL in tests.
  if (!print_job_manager)
    return;

  std::unique_ptr<PrinterQuery> printer_query = queue_->PopPrinterQuery(cookie);
  if (!printer_query)
    return;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PrinterQuery::StopWorker, std::move(printer_query)));
}

void PrintViewManagerBase::SendPrintingEnabled(bool enabled,
                                               content::RenderFrameHost* rfh) {
  GetPrintRenderFrame(rfh)->SetPrintingEnabled(enabled);
}

}  // namespace printing
