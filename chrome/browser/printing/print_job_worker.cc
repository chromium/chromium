// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/print_job_constants.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

#if defined(OS_WIN)
#include "printing/printed_page_win.h"
#endif

using content::BrowserThread;

namespace printing {

namespace {

class PrintingContextDelegate : public PrintingContext::Delegate {
 public:
  PrintingContextDelegate(int render_process_id, int render_frame_id);
  ~PrintingContextDelegate() override;

  gfx::NativeView GetParentView() override;
  std::string GetAppLocale() override;

  // Not exposed to PrintingContext::Delegate because of dependency issues.
  content::WebContents* GetWebContents();

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 private:
  const int render_process_id_;
  const int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextDelegate);
};

PrintingContextDelegate::PrintingContextDelegate(int render_process_id,
                                                 int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

PrintingContextDelegate::~PrintingContextDelegate() {
}

gfx::NativeView PrintingContextDelegate::GetParentView() {
  content::WebContents* wc = GetWebContents();
  return wc ? wc->GetNativeView() : nullptr;
}

content::WebContents* PrintingContextDelegate::GetWebContents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  return rfh ? content::WebContents::FromRenderFrameHost(rfh) : nullptr;
}

std::string PrintingContextDelegate::GetAppLocale() {
  return g_browser_process->GetApplicationLocale();
}

void NotificationCallback(PrintJob* print_job,
                          JobEventDetails::Type detail_type,
                          int job_id,
                          PrintedDocument* document) {
  auto details =
      base::MakeRefCounted<JobEventDetails>(detail_type, job_id, document);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_EVENT,
      content::Source<PrintJob>(print_job),
      content::Details<JobEventDetails>(details.get()));
}

// Helper function to ensure |query| is valid until at least |callback| returns.
void WorkerHoldRefCallback(scoped_refptr<PrinterQuery> query,
                           base::OnceClosure callback) {
  std::move(callback).Run();
}

void PostOnQueryThread(scoped_refptr<PrinterQuery> query,
                       PrintingContext::PrintSettingsCallback callback,
                       PrintingContext::Result result) {
  query->PostTask(FROM_HERE,
                  base::BindOnce(&WorkerHoldRefCallback, query,
                                 base::BindOnce(std::move(callback), result)));
}

#if defined(OS_WIN)
void PageNotificationCallback(PrintJob* print_job,
                              JobEventDetails::Type detail_type,
                              int job_id,
                              PrintedDocument* document,
                              PrintedPage* page) {
  auto details = base::MakeRefCounted<JobEventDetails>(detail_type, job_id,
                                                       document, page);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_EVENT,
      content::Source<PrintJob>(print_job),
      content::Details<JobEventDetails>(details.get()));
}
#endif

}  // namespace

PrintJobWorker::PrintJobWorker(int render_process_id,
                               int render_frame_id,
                               PrinterQuery* query)
    : printing_context_delegate_(
          std::make_unique<PrintingContextDelegate>(render_process_id,
                                                    render_frame_id)),
      printing_context_(
          PrintingContext::Create(printing_context_delegate_.get())),
      query_(query),
      thread_("Printing_Worker"),
      weak_factory_(this) {
  // The object is created in the IO thread.
  DCHECK(query_->RunsTasksInCurrentSequence());
}

PrintJobWorker::~PrintJobWorker() {
  // The object is normally deleted by PrintJob in the UI thread, but when the
  // user cancels printing or in the case of print preview, the worker is
  // destroyed with the PrinterQuery, which is on the I/O thread.
  if (query_) {
    DCHECK(!print_job_);
    DCHECK(query_->RunsTasksInCurrentSequence());
  } else {
    DCHECK(print_job_);
    DCHECK(print_job_->RunsTasksInCurrentSequence());
  }
  Stop();
}

void PrintJobWorker::SetPrintJob(PrintJob* print_job) {
  DCHECK_EQ(page_number_, PageNumber::npos());
  print_job_ = print_job;

  // Release the Printer Query reference. It is no longer needed.
  query_ = nullptr;
}

void PrintJobWorker::GetSettings(bool ask_user_for_settings,
                                 int document_page_count,
                                 bool has_selection,
                                 MarginType margin_type,
                                 bool is_scripted,
                                 bool is_modifiable) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(page_number_, PageNumber::npos());

  // This function is only called by the PrinterQuery.
  DCHECK(query_);

  // Recursive task processing is needed for the dialog in case it needs to be
  // destroyed by a task.
  // TODO(thestig): This code is wrong. SetNestableTasksAllowed(true) is needed
  // on the thread where the PrintDlgEx is called, and definitely both calls
  // should happen on the same thread. See http://crbug.com/73466
  // MessageLoopCurrent::Get()->SetNestableTasksAllowed(true);
  printing_context_->set_margin_type(margin_type);
  printing_context_->set_is_modifiable(is_modifiable);

  // When we delegate to a destination, we don't ask the user for settings.
  // TODO(mad): Ask the destination for settings.
  if (ask_user_for_settings) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &WorkerHoldRefCallback, base::WrapRefCounted(query_),
            base::BindOnce(&PrintJobWorker::GetSettingsWithUI,
                           base::Unretained(this), document_page_count,
                           has_selection, is_scripted)));
  } else {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&WorkerHoldRefCallback, base::WrapRefCounted(query_),
                       base::BindOnce(&PrintJobWorker::UseDefaultSettings,
                                      base::Unretained(this))));
  }
}

void PrintJobWorker::SetSettings(
    std::unique_ptr<base::DictionaryValue> new_settings) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(query_);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &WorkerHoldRefCallback, base::WrapRefCounted(query_),
          base::BindOnce(&PrintJobWorker::UpdatePrintSettings,
                         base::Unretained(this), std::move(new_settings))));
}

#if defined(OS_CHROMEOS)
void PrintJobWorker::SetSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(query_);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &WorkerHoldRefCallback, base::WrapRefCounted(query_),
          base::BindOnce(&PrintJobWorker::UpdatePrintSettingsFromPOD,
                         base::Unretained(this), std::move(new_settings))));
}
#endif

void PrintJobWorker::UpdatePrintSettings(
    std::unique_ptr<base::DictionaryValue> new_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrintingContext::Result result =
      printing_context_->UpdatePrintSettings(*new_settings);
  GetSettingsDone(result);
}

#if defined(OS_CHROMEOS)
void PrintJobWorker::UpdatePrintSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrintingContext::Result result =
      printing_context_->UpdatePrintSettingsFromPOD(std::move(new_settings));
  GetSettingsDone(result);
}
#endif

void PrintJobWorker::GetSettingsDone(PrintingContext::Result result) {
  // Most PrintingContext functions may start a message loop and process
  // message recursively, so disable recursive task processing.
  // TODO(thestig): See above comment. SetNestableTasksAllowed(false) needs to
  // be called on the same thread as the previous call.  See
  // http://crbug.com/73466
  // MessageLoopCurrent::Get()->SetNestableTasksAllowed(false);

  // We can't use OnFailure() here since query_ does not support notifications.

  DCHECK(query_);
  query_->PostTask(FROM_HERE,
                   base::BindOnce(&PrinterQuery::GetSettingsDone,
                                  base::WrapRefCounted(query_),
                                  printing_context_->settings(), result));
}

void PrintJobWorker::GetSettingsWithUI(
    int document_page_count,
    bool has_selection,
    bool is_scripted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PrintingContextDelegate* printing_context_delegate =
      static_cast<PrintingContextDelegate*>(printing_context_delegate_.get());
  content::WebContents* web_contents =
      printing_context_delegate->GetWebContents();

#if defined(OS_ANDROID)
  if (is_scripted) {
    TabAndroid* tab =
        web_contents ? TabAndroid::FromWebContents(web_contents) : nullptr;

    // Regardless of whether the following call fails or not, the javascript
    // call will return since startPendingPrint will make it return immediately
    // in case of error.
    if (tab) {
      tab->SetPendingPrint(printing_context_delegate->render_process_id(),
                           printing_context_delegate->render_frame_id());
    }
  }
#endif

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreenForCurrentTab())
    web_contents->ExitFullscreen(true);

  // weak_factory_ creates pointers valid only on query_ thread.
  printing_context_->AskUserForSettings(
      document_page_count, has_selection, is_scripted,
      base::BindOnce(&PostOnQueryThread, base::WrapRefCounted(query_),
                     base::BindOnce(&PrintJobWorker::GetSettingsDone,
                                    weak_factory_.GetWeakPtr())));
}

void PrintJobWorker::UseDefaultSettings() {
  PrintingContext::Result result = printing_context_->UseDefaultSettings();
  GetSettingsDone(result);
}

void PrintJobWorker::StartPrinting(PrintedDocument* new_document) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (page_number_ != PageNumber::npos()) {
    NOTREACHED();
    return;
  }

  if (!document_) {
    NOTREACHED();
    return;
  }

  if (document_.get() != new_document) {
    NOTREACHED();
    return;
  }

  base::string16 document_name = SimplifyDocumentTitle(document_->name());
  if (document_name.empty()) {
    document_name = SimplifyDocumentTitle(
        l10n_util::GetStringUTF16(IDS_DEFAULT_PRINT_DOCUMENT_TITLE));
  }
  PrintingContext::Result result =
      printing_context_->NewDocument(document_name);
  if (result != PrintingContext::OK) {
    OnFailure();
    return;
  }

  // This will start a loop to wait for the page data.
  OnNewPage();
  // Don't touch this anymore since the instance could be destroyed. It happens
  // if all the pages are printed a one sweep and the client doesn't have a
  // handle to us anymore. There's a timing issue involved between the worker
  // thread and the UI thread. Take no chance.
}

void PrintJobWorker::OnDocumentChanged(PrintedDocument* new_document) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (page_number_ != PageNumber::npos()) {
    NOTREACHED();
    return;
  }

  document_ = new_document;
}

void PrintJobWorker::PostWaitForPage() {
  // We need to wait for the page to be available.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PrintJobWorker::OnNewPage, weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(500));
}

void PrintJobWorker::OnNewPage() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!document_)
    return;

#if defined(OS_WIN)
  if (page_number_ == PageNumber::npos()) {
    // Find first page to print.
    int page_count = document_->page_count();
    if (!page_count) {
      // We still don't know how many pages the document contains.
      return;
    }
    // We have enough information to initialize |page_number_|.
    page_number_.Init(document_->settings(), page_count);
  }

  while (true) {
    scoped_refptr<PrintedPage> page = document_->GetPage(page_number_.ToInt());
    if (!page) {
      PostWaitForPage();
      return;
    }
    // The page is there, print it.
    SpoolPage(page.get());
    ++page_number_;
    if (page_number_ == PageNumber::npos())
      break;
  }
#else
  if (!document_->GetMetafile()) {
    PostWaitForPage();
    return;
  }
  SpoolJob();
#endif  // defined(OS_WIN)

  OnDocumentDone();
  // Don't touch |this| anymore since the instance could be destroyed.
}

void PrintJobWorker::Cancel() {
  // This is the only function that can be called from any thread.
  printing_context_->Cancel();
  // Cannot touch any member variable since we don't know in which thread
  // context we run.
}

bool PrintJobWorker::IsRunning() const {
  return thread_.IsRunning();
}

bool PrintJobWorker::PostTask(const base::Location& from_here,
                              base::OnceClosure task) {
  return task_runner_ && task_runner_->PostTask(from_here, std::move(task));
}

void PrintJobWorker::StopSoon() {
  thread_.StopSoon();
}

void PrintJobWorker::Stop() {
  thread_.Stop();
}

bool PrintJobWorker::Start() {
  bool result = thread_.Start();
  task_runner_ = thread_.task_runner();
  return result;
}

void PrintJobWorker::OnDocumentDone() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(page_number_, PageNumber::npos());
  DCHECK(document_);
  // PrintJob must own this, because only PrintJob can send notifications.
  DCHECK(print_job_);

  int job_id = printing_context_->job_id();
  if (printing_context_->DocumentDone() != PrintingContext::OK) {
    OnFailure();
    return;
  }

  print_job_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationCallback, base::RetainedRef(print_job_),
                     JobEventDetails::DOC_DONE, job_id,
                     base::RetainedRef(document_)));

  // Makes sure the variables are reinitialized.
  document_ = nullptr;
}

#if defined(OS_WIN)
void PrintJobWorker::SpoolPage(PrintedPage* page) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(page_number_, PageNumber::npos());

  // Preprocess.
  if (printing_context_->NewPage() != PrintingContext::OK) {
    OnFailure();
    return;
  }

  // Actual printing.
  document_->RenderPrintedPage(*page, printing_context_->context());

  // Postprocess.
  if (printing_context_->PageDone() != PrintingContext::OK) {
    OnFailure();
    return;
  }

  // Signal everyone that the page is printed.
  DCHECK(print_job_);
  print_job_->PostTask(
      FROM_HERE, base::BindRepeating(
                     &PageNotificationCallback, base::RetainedRef(print_job_),
                     JobEventDetails::PAGE_DONE, printing_context_->job_id(),
                     base::RetainedRef(document_), base::RetainedRef(page)));
}
#else
void PrintJobWorker::SpoolJob() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!document_->RenderPrintedDocument(printing_context_.get()))
    OnFailure();
}
#endif

void PrintJobWorker::OnFailure() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(print_job_);

  // We may loose our last reference by broadcasting the FAILED event.
  scoped_refptr<PrintJob> handle(print_job_);

  print_job_->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationCallback, base::RetainedRef(print_job_),
                     JobEventDetails::FAILED, 0, base::RetainedRef(document_)));
  Cancel();

  // Makes sure the variables are reinitialized.
  document_ = nullptr;
  page_number_ = PageNumber::npos();
}

}  // namespace printing
