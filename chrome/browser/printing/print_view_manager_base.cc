// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_base.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
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
#include "components/prefs/pref_service.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/printing/browser/print_manager_utils.h"
#include "components/printing/common/print_messages.h"
#include "components/services/pdf_compositor/public/cpp/pdf_service_mojo_types.h"
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
#include "printing/print_settings.h"
#include "printing/printed_document.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_error_dialog.h"
#endif

using base::TimeDelta;
using content::BrowserThread;

namespace printing {

namespace {

using PrintSettingsCallback =
    base::OnceCallback<void(scoped_refptr<PrinterQuery>)>;

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
void CreateQueryWithSettings(
    std::unique_ptr<base::DictionaryValue> job_settings,
    int render_process_id,
    int render_frame_id,
    scoped_refptr<PrintQueriesQueue> queue,
    PrintSettingsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_refptr<printing::PrinterQuery> printer_query =
      queue->CreatePrinterQuery(render_process_id, render_frame_id);
  printer_query->SetSettings(
      std::move(job_settings),
      base::BindOnce(std::move(callback), printer_query));
}

void OnPrintSettingsDoneWrapper(PrintSettingsCallback settings_callback,
                                scoped_refptr<PrinterQuery> query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(std::move(settings_callback), query));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

}  // namespace

PrintViewManagerBase::PrintViewManagerBase(content::WebContents* web_contents)
    : PrintManager(web_contents),
      printing_rfh_(nullptr),
      printing_succeeded_(false),
      queue_(g_browser_process->print_job_manager()->queue()),
      weak_ptr_factory_(this) {
  DCHECK(queue_);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  printing_enabled_.Init(
      prefs::kPrintingEnabled, profile->GetPrefs(),
      base::Bind(&PrintViewManagerBase::UpdatePrintingEnabled,
                 weak_ptr_factory_.GetWeakPtr()));
}

PrintViewManagerBase::~PrintViewManagerBase() {
  ReleasePrinterQuery();
  DisconnectFromCurrentPrintJob();
}

bool PrintViewManagerBase::PrintNow(content::RenderFrameHost* rfh) {
  DisconnectFromCurrentPrintJob();

  SetPrintingRFH(rfh);
  int32_t id = rfh->GetRoutingID();
  return PrintNowInternal(rfh, std::make_unique<PrintMsg_PrintPages>(id));
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::PrintForPrintPreview(
    std::unique_ptr<base::DictionaryValue> job_settings,
    const scoped_refptr<base::RefCountedMemory>& print_data,
    content::RenderFrameHost* rfh,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int page_count;
  job_settings->GetInteger(kSettingPreviewPageCount, &page_count);
  PrintSettingsCallback settings_callback =
      base::BindOnce(&PrintViewManagerBase::OnPrintSettingsDone,
                     weak_ptr_factory_.GetWeakPtr(), print_data, page_count,
                     std::move(callback));
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(CreateQueryWithSettings, std::move(job_settings),
                     rfh->GetProcess()->GetID(), rfh->GetRoutingID(), queue_,
                     base::BindOnce(OnPrintSettingsDoneWrapper,
                                    std::move(settings_callback))));
}
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

void PrintViewManagerBase::PrintDocument(
    const scoped_refptr<base::RefCountedMemory>& print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& offsets) {
#if defined(OS_WIN)
  print_job_->StartConversionToNativeFormat(print_data, page_size, content_area,
                                            offsets);
#else
  std::unique_ptr<MetafileSkia> metafile = std::make_unique<MetafileSkia>();
  CHECK(metafile->InitFromData(print_data->front(), print_data->size()));

  // Update the rendered document. It will send notifications to the listener.
  PrintedDocument* document = print_job_->document();
  document->SetDocument(std::move(metafile), page_size, content_area);
  ShouldQuitFromInnerMessageLoop();
#endif
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
void PrintViewManagerBase::OnPrintSettingsDone(
    const scoped_refptr<base::RefCountedMemory>& print_data,
    int page_count,
    PrinterHandler::PrintCallback callback,
    scoped_refptr<printing::PrinterQuery> printer_query) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check if the job was cancelled. This should only happen on Windows when
  // the system dialog is cancelled.
  if (printer_query &&
      printer_query->last_status() == PrintingContext::CANCEL) {
    queue_->QueuePrinterQuery(printer_query.get());
#if defined(OS_WIN)
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&PrintViewManagerBase::SystemDialogCancelled,
                       weak_ptr_factory_.GetWeakPtr()));
#endif
    std::move(callback).Run(base::Value());
    return;
  }

  if (!printer_query || !printer_query->cookie() ||
      !printer_query->settings().dpi()) {
    if (printer_query)
      printer_query->StopWorker();
    std::move(callback).Run(base::Value("Update settings failed"));
    return;
  }

  // Post task so that the query has time to reset the callback before calling
  // OnDidGetPrintedPagesCount.
  queue_->QueuePrinterQuery(printer_query.get());
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&PrintViewManagerBase::StartLocalPrintJob,
                     weak_ptr_factory_.GetWeakPtr(), print_data, page_count,
                     printer_query, std::move(callback)));
}

void PrintViewManagerBase::StartLocalPrintJob(
    const scoped_refptr<base::RefCountedMemory>& print_data,
    int page_count,
    scoped_refptr<printing::PrinterQuery> printer_query,
    PrinterHandler::PrintCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnDidGetPrintedPagesCount(printer_query->cookie(), page_count);

  if (!PrintJobHasDocument(printer_query->cookie())) {
    std::move(callback).Run(base::Value("Failed to print"));
    return;
  }

#if defined(OS_WIN)
  print_job_->ResetPageMapping();
#endif

  const printing::PrintSettings& settings = printer_query->settings();
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

void PrintViewManagerBase::OnDidGetPrintedPagesCount(int cookie,
                                                     int number_pages) {
  PrintManager::OnDidGetPrintedPagesCount(cookie, number_pages);
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

void PrintViewManagerBase::OnComposePdfDone(
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    mojom::PdfCompositor::Status status,
    base::ReadOnlySharedMemoryRegion region) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != mojom::PdfCompositor::Status::SUCCESS) {
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
}

void PrintViewManagerBase::OnDidPrintDocument(
    content::RenderFrameHost* render_frame_host,
    const PrintHostMsg_DidPrintDocument_Params& params) {
  if (!PrintJobHasDocument(params.document_cookie))
    return;

  const PrintHostMsg_DidPrintContent_Params& content = params.content;
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
                       params.content_area, params.physical_offsets));
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
}

void PrintViewManagerBase::OnPrintingFailed(int cookie) {
  PrintManager::OnPrintingFailed(cookie);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  ShowPrintErrorDialog();
#endif

  ReleasePrinterQuery();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_RELEASED,
      content::Source<content::WebContents>(web_contents()),
      content::NotificationService::NoDetails());
}

void PrintViewManagerBase::OnShowInvalidPrinterSettingsError() {
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

bool PrintViewManagerBase::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(PrintViewManagerBase, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(PrintHostMsg_DidPrintDocument, OnDidPrintDocument)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  handled = true;
  IPC_BEGIN_MESSAGE_MAP(PrintViewManagerBase, message)
    IPC_MESSAGE_HANDLER(PrintHostMsg_ShowInvalidPrinterSettingsError,
                        OnShowInvalidPrinterSettingsError)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || PrintManager::OnMessageReceived(message, render_frame_host);
}

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
    case JobEventDetails::NEW_DOC:
#if defined(OS_WIN)
    case JobEventDetails::PAGE_DONE:
#endif
    case JobEventDetails::DOC_DONE: {
      // Don't care about the actual printing process.
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

bool PrintViewManagerBase::CreateNewPrintJob(PrinterQuery* query) {
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
  print_job_->Initialize(query, RenderSourceName(), number_pages_);
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

  if (rfh) {
    auto msg = std::make_unique<PrintMsg_PrintingDone>(rfh->GetRoutingID(),
                                                       printing_succeeded_);
    rfh->Send(msg.release());
  }

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
  static const int kPrinterSettingsTimeout = 60000;
  base::OneShotTimer quit_timer;
  base::RunLoop run_loop;
  quit_timer.Start(FROM_HERE,
                   TimeDelta::FromMilliseconds(kPrinterSettingsTimeout),
                   run_loop.QuitWhenIdleClosure());

  quit_inner_loop_ = run_loop.QuitClosure();

  // Need to enable recursive task.
  {
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;
    run_loop.Run();
  }

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
  scoped_refptr<PrinterQuery> queued_query = queue_->PopPrinterQuery(cookie);
  if (!queued_query) {
    NOTREACHED();
    return false;
  }

  if (!CreateNewPrintJob(queued_query.get())) {
    // Don't kill anything.
    return false;
  }

  // Settings are already loaded. Go ahead. This will set
  // print_job_->is_job_pending() to true.
  print_job_->StartPrinting();
  return true;
}

bool PrintViewManagerBase::PrintNowInternal(
    content::RenderFrameHost* rfh,
    std::unique_ptr<IPC::Message> message) {
  // Don't print / print preview interstitials or crashed tabs.
  if (web_contents()->ShowingInterstitialPage() || web_contents()->IsCrashed())
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

  scoped_refptr<PrinterQuery> printer_query;
  printer_query = queue_->PopPrinterQuery(cookie);
  if (!printer_query)
    return;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&PrinterQuery::StopWorker, printer_query));
}

void PrintViewManagerBase::SendPrintingEnabled(bool enabled,
                                               content::RenderFrameHost* rfh) {
  rfh->Send(new PrintMsg_SetPrintingEnabled(rfh->GetRoutingID(), enabled));
}

}  // namespace printing
