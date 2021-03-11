// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job_worker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/grit/generated_resources.h"
#include "components/crash/core/common/crash_keys.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "printing/backend/print_backend.h"
#include "printing/print_job_constants.h"
#include "printing/printed_document.h"
#include "printing/printing_utils.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_printer.h"
#include "printing/printing_context_android.h"
#endif

#if defined(OS_WIN)
#include "base/threading/thread_restrictions.h"
#include "printing/printed_page_win.h"
#include "printing/printing_features.h"
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

PrintJobWorker::PrintJobWorker(int render_process_id, int render_frame_id)
    : printing_context_delegate_(
          std::make_unique<PrintingContextDelegate>(render_process_id,
                                                    render_frame_id)),
      printing_context_(
          PrintingContext::Create(printing_context_delegate_.get())),
      thread_("Printing_Worker") {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

PrintJobWorker::~PrintJobWorker() {
  // The object is normally deleted by PrintJob in the UI thread, but when the
  // user cancels printing or in the case of print preview, the worker is
  // destroyed with the PrinterQuery, which is on the I/O thread.
  if (print_job_) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }
  Stop();
}

void PrintJobWorker::SetPrintJob(PrintJob* print_job) {
  DCHECK_EQ(page_number_, PageNumber::npos());
  print_job_ = print_job;
}

void PrintJobWorker::GetSettings(bool ask_user_for_settings,
                                 uint32_t document_page_count,
                                 bool has_selection,
                                 mojom::MarginType margin_type,
                                 bool is_scripted,
                                 bool is_modifiable,
                                 SettingsCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(page_number_, PageNumber::npos());

  printing_context_->set_margin_type(margin_type);
  printing_context_->set_is_modifiable(is_modifiable);

  // When we delegate to a destination, we don't ask the user for settings.
  // TODO(mad): Ask the destination for settings.
  if (ask_user_for_settings) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PrintJobWorker::GetSettingsWithUI,
                       base::Unretained(this), document_page_count,
                       has_selection, is_scripted, std::move(callback)));
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PrintJobWorker::UseDefaultSettings,
                                  base::Unretained(this), std::move(callback)));
  }
}

void PrintJobWorker::SetSettings(base::Value new_settings,
                                 SettingsCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorker::UpdatePrintSettings,
                                base::Unretained(this), std::move(new_settings),
                                std::move(callback)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintJobWorker::SetSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings,
    SettingsCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJobWorker::UpdatePrintSettingsFromPOD,
                                base::Unretained(this), std::move(new_settings),
                                std::move(callback)));
}
#endif

void PrintJobWorker::UpdatePrintSettings(base::Value new_settings,
                                         SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<crash_keys::ScopedPrinterInfo> crash_key;
  PrinterType type = static_cast<PrinterType>(
      new_settings.FindIntKey(kSettingPrinterType).value());
  if (type == PrinterType::kLocal) {
#if defined(OS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    scoped_refptr<PrintBackend> print_backend =
        PrintBackend::CreateInstance(g_browser_process->GetApplicationLocale());
    std::string printer_name = *new_settings.FindStringKey(kSettingDeviceName);
    crash_key = std::make_unique<crash_keys::ScopedPrinterInfo>(
        print_backend->GetPrinterDriverInfo(printer_name));

#if (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && defined(USE_CUPS)
    PrinterBasicInfo basic_info;
    if (print_backend->GetPrinterBasicInfo(printer_name, &basic_info)) {
      base::Value advanced_settings(base::Value::Type::DICTIONARY);
      for (const auto& pair : basic_info.options)
        advanced_settings.SetStringKey(pair.first, pair.second);

      new_settings.SetKey(kSettingAdvancedSettings,
                          std::move(advanced_settings));
    }
#endif  // (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) &&
        // defined(USE_CUPS)
  }

  PrintingContext::Result result;
  {
#if defined(OS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    result = printing_context_->UpdatePrintSettings(std::move(new_settings));
  }
  GetSettingsDone(std::move(callback), result);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PrintJobWorker::UpdatePrintSettingsFromPOD(
    std::unique_ptr<printing::PrintSettings> new_settings,
    SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrintingContext::Result result =
      printing_context_->UpdatePrintSettingsFromPOD(std::move(new_settings));
  GetSettingsDone(std::move(callback), result);
}
#endif

void PrintJobWorker::GetSettingsDone(SettingsCallback callback,
                                     PrintingContext::Result result) {
  std::move(callback).Run(printing_context_->TakeAndResetSettings(), result);
}

void PrintJobWorker::GetSettingsWithUI(uint32_t document_page_count,
                                       bool has_selection,
                                       bool is_scripted,
                                       SettingsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (document_page_count > kMaxPageCount) {
    GetSettingsDone(std::move(callback), PrintingContext::Result::FAILED);
    return;
  }

  content::WebContents* web_contents = GetWebContents();

#if defined(OS_ANDROID)
  if (is_scripted) {
    TabAndroid* tab =
        web_contents ? TabAndroid::FromWebContents(web_contents) : nullptr;

    // Regardless of whether the following call fails or not, the javascript
    // call will return since startPendingPrint will make it return immediately
    // in case of error.
    if (tab) {
      auto* printing_context_delegate = static_cast<PrintingContextDelegate*>(
          printing_context_delegate_.get());
      PrintingContextAndroid::SetPendingPrint(
          web_contents->GetTopLevelNativeWindow(),
          GetPrintableForTab(tab->GetJavaObject()),
          printing_context_delegate->render_process_id(),
          printing_context_delegate->render_frame_id());
    }
  }
#endif

  // Running a dialog causes an exit to webpage-initiated fullscreen.
  // http://crbug.com/728276
  if (web_contents && web_contents->IsFullscreen())
    web_contents->ExitFullscreen(true);

  printing_context_->AskUserForSettings(
      base::checked_cast<int>(document_page_count), has_selection, is_scripted,
      base::BindOnce(&PrintJobWorker::GetSettingsDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobWorker::UseDefaultSettings(SettingsCallback callback) {
  PrintingContext::Result result;
  {
#if defined(OS_WIN)
    // Blocking is needed here because Windows printer drivers are oftentimes
    // not thread-safe and have to be accessed on the UI thread.
    base::ScopedAllowBlocking allow_blocking;
#endif
    result = printing_context_->UseDefaultSettings();
  }
  GetSettingsDone(std::move(callback), result);
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

  std::u16string document_name = SimplifyDocumentTitle(document_->name());
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

  bool do_spool_job = true;
#if defined(OS_WIN)
  const bool source_is_pdf =
      !print_job_->document()->settings().is_modifiable();
  if (!printing::features::ShouldPrintUsingXps(source_is_pdf)) {
    // Using the Windows GDI print API.
    if (!OnNewPageHelperGdi())
      return;

    do_spool_job = false;
  }
#endif  // defined(OS_WIN)

  if (do_spool_job) {
    if (!document_->GetMetafile()) {
      PostWaitForPage();
      return;
    }
    SpoolJob();
  }

  OnDocumentDone();
  // Don't touch |this| anymore since the instance could be destroyed.
}

#if defined(OS_WIN)
bool PrintJobWorker::OnNewPageHelperGdi() {
  if (page_number_ == PageNumber::npos()) {
    // Find first page to print.
    int page_count = document_->page_count();
    if (!page_count) {
      // We still don't know how many pages the document contains.
      return false;
    }
    // We have enough information to initialize |page_number_|.
    page_number_.Init(document_->settings(), page_count);
  }

  while (true) {
    scoped_refptr<PrintedPage> page = document_->GetPage(page_number_.ToUint());
    if (!page) {
      PostWaitForPage();
      return false;
    }
    // The page is there, print it.
    SpoolPage(page.get());
    ++page_number_;
    if (page_number_ == PageNumber::npos())
      break;
  }
  return true;
}
#endif  // defined(OS_WIN)

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
      FROM_HERE,
      base::BindOnce(&PageNotificationCallback, base::RetainedRef(print_job_),
                     JobEventDetails::PAGE_DONE, printing_context_->job_id(),
                     base::RetainedRef(document_), base::RetainedRef(page)));
}
#endif  // defined(OS_WIN)

void PrintJobWorker::SpoolJob() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!document_->RenderPrintedDocument(printing_context_.get()))
    OnFailure();
}

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

content::WebContents* PrintJobWorker::GetWebContents() {
  PrintingContextDelegate* printing_context_delegate =
      static_cast<PrintingContextDelegate*>(printing_context_delegate_.get());
  return printing_context_delegate->GetWebContents();
}

}  // namespace printing
