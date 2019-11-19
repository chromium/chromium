// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "printing/printed_document.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "chrome/browser/printing/pdf_to_emf_converter.h"
#include "chrome/common/chrome_features.h"
#include "printing/pdf_render_settings.h"
#include "printing/printed_page_win.h"
#endif

using base::TimeDelta;

namespace printing {

// Helper function to ensure |job| is valid until at least |callback| returns.
void HoldRefCallback(scoped_refptr<PrintJob> job, base::OnceClosure callback) {
  std::move(callback).Run();
}

PrintJob::PrintJob() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrintJob::~PrintJob() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!worker_ || !worker_->IsRunning());
}

void PrintJob::Initialize(std::unique_ptr<PrinterQuery> query,
                          const base::string16& name,
                          int page_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!worker_);
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!document_);
  worker_ = query->DetachWorker();
  worker_->SetPrintJob(this);
  std::unique_ptr<PrintSettings> settings = query->ExtractSettings();

#if defined(OS_WIN)
  pdf_page_mapping_ = PageRange::GetPages(settings->ranges());
  if (pdf_page_mapping_.empty()) {
    for (int i = 0; i < page_count; i++)
      pdf_page_mapping_.push_back(i);
  }
#endif

  auto new_doc = base::MakeRefCounted<PrintedDocument>(std::move(settings),
                                                       name, query->cookie());
  new_doc->set_page_count(page_count);
  UpdatePrintedDocument(new_doc);

  // Don't forget to register to our own messages.
  registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                 content::Source<PrintJob>(this));
}

#if defined(OS_WIN)
// static
std::vector<int> PrintJob::GetFullPageMapping(const std::vector<int>& pages,
                                              int total_page_count) {
  std::vector<int> mapping(total_page_count, -1);
  for (int page_number : pages) {
    // Make sure the page is in range.
    if (page_number >= 0 && page_number < total_page_count)
      mapping[page_number] = page_number;
  }
  return mapping;
}

void PrintJob::StartConversionToNativeFormat(
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (PrintedDocument::HasDebugDumpPath())
    document()->DebugDumpData(print_data.get(), FILE_PATH_LITERAL(".pdf"));

  const PrintSettings& settings = document()->settings();
  if (settings.printer_is_textonly()) {
    StartPdfToTextConversion(print_data, page_size);
  } else if (settings.printer_is_ps2() || settings.printer_is_ps3()) {
    StartPdfToPostScriptConversion(print_data, content_area, physical_offsets,
                                   settings.printer_is_ps2());
  } else {
    StartPdfToEmfConversion(print_data, page_size, content_area);
  }

  // Indicate that the PDF is fully rendered and we no longer need the renderer
  // and web contents, so the print job does not need to be cancelled if they
  // die. This is needed on Windows because the PrintedDocument will not be
  // considered complete until PDF conversion finishes.
  document()->SetConvertingPdf();
}

void PrintJob::ResetPageMapping() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pdf_page_mapping_ =
      GetFullPageMapping(pdf_page_mapping_, document_->page_count());
}
#endif  // defined(OS_WIN)

void PrintJob::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);

  OnNotifyPrintJobEvent(*content::Details<JobEventDetails>(details).ptr());
}

void PrintJob::StartPrinting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!worker_->IsRunning() || is_job_pending_) {
    NOTREACHED();
    return;
  }

  // Real work is done in PrintJobWorker::StartPrinting().
  worker_->PostTask(
      FROM_HERE, base::BindOnce(&HoldRefCallback, base::WrapRefCounted(this),
                                base::BindOnce(&PrintJobWorker::StartPrinting,
                                               base::Unretained(worker_.get()),
                                               base::RetainedRef(document_))));
  // Set the flag right now.
  is_job_pending_ = true;

  // Tell everyone!
  auto details = base::MakeRefCounted<JobEventDetails>(JobEventDetails::NEW_DOC,
                                                       0, document_.get());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_EVENT,
      content::Source<PrintJob>(this),
      content::Details<JobEventDetails>(details.get()));
}

void PrintJob::Stop() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (quit_closure_) {
    // In case we're running a nested run loop to wait for a job to finish,
    // and we finished before the timeout, quit the nested loop right away.
    std::move(quit_closure_).Run();
  }

  // Be sure to live long enough.
  scoped_refptr<PrintJob> handle(this);

  if (worker_->IsRunning()) {
    ControlledWorkerShutdown();
  } else {
    // Flush the cached document.
    is_job_pending_ = false;
    ClearPrintedDocument();
  }
}

void PrintJob::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_canceling_)
    return;

  is_canceling_ = true;

  if (worker_ && worker_->IsRunning()) {
    // Call this right now so it renders the context invalid. Do not use
    // InvokeLater since it would take too much time.
    worker_->Cancel();
  }
  // Make sure a Cancel() is broadcast.
  auto details = base::MakeRefCounted<JobEventDetails>(JobEventDetails::FAILED,
                                                       0, nullptr);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_EVENT,
      content::Source<PrintJob>(this),
      content::Details<JobEventDetails>(details.get()));
  Stop();
  is_canceling_ = false;
}

bool PrintJob::FlushJob(base::TimeDelta timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure the object outlive this message loop.
  scoped_refptr<PrintJob> handle(this);

  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = loop.QuitClosure();
  base::PostDelayedTask(FROM_HERE, {content::BrowserThread::UI},
                        loop.QuitClosure(), timeout);

  loop.Run();

  return true;
}

bool PrintJob::is_job_pending() const {
  return is_job_pending_;
}

PrintedDocument* PrintJob::document() const {
  return document_.get();
}

const PrintSettings& PrintJob::settings() const {
  return document()->settings();
}

#if defined(OS_CHROMEOS)
void PrintJob::SetSource(PrintJob::Source source,
                         const std::string& source_id) {
  source_ = source;
  source_id_ = source_id;
}

PrintJob::Source PrintJob::source() const {
  return source_;
}

const std::string& PrintJob::source_id() const {
  return source_id_;
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
class PrintJob::PdfConversionState {
 public:
  PdfConversionState(const gfx::Size& page_size, const gfx::Rect& content_area)
      : page_count_(0),
        current_page_(0),
        pages_in_progress_(0),
        page_size_(page_size),
        content_area_(content_area) {}

  void Start(scoped_refptr<base::RefCountedMemory> data,
             const PdfRenderSettings& conversion_settings,
             PdfConverter::StartCallback start_callback) {
    converter_ = PdfConverter::StartPdfConverter(data, conversion_settings,
                                                 std::move(start_callback));
  }

  void GetMorePages(PdfConverter::GetPageCallback get_page_callback) {
    const int kMaxNumberOfTempFilesPerDocument = 3;
    while (pages_in_progress_ < kMaxNumberOfTempFilesPerDocument &&
           current_page_ < page_count_) {
      ++pages_in_progress_;
      converter_->GetPage(current_page_++, get_page_callback);
    }
  }

  void OnPageProcessed(PdfConverter::GetPageCallback get_page_callback) {
    --pages_in_progress_;
    GetMorePages(get_page_callback);
    // Release converter if we don't need this any more.
    if (!pages_in_progress_ && current_page_ >= page_count_)
      converter_.reset();
  }

  void set_page_count(int page_count) { page_count_ = page_count; }
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& content_area() const { return content_area_; }

 private:
  int page_count_;
  int current_page_;
  int pages_in_progress_;
  gfx::Size page_size_;
  gfx::Rect content_area_;
  std::unique_ptr<PdfConverter> converter_;
};

void PrintJob::StartPdfToEmfConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size,
    const gfx::Rect& content_area) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ =
      std::make_unique<PdfConversionState>(page_size, content_area);

  // TODO(thestig): Figure out why rendering text with GDI results in random
  // missing characters for some users. https://crbug.com/658606
  // Update : The missing letters seem to have been caused by the same
  // problem as https://crbug.com/659604 which was resolved. GDI printing
  // seems to work with the fix for this bug applied.
  const PrintSettings& settings = document()->settings();
  bool print_text_with_gdi =
      settings.print_text_with_gdi() && !settings.printer_is_xps() &&
      base::FeatureList::IsEnabled(features::kGdiTextPrinting);
  PdfRenderSettings render_settings(
      content_area, gfx::Point(0, 0), settings.dpi_size(),
      /*autorotate=*/true, settings.color() == COLOR,
      print_text_with_gdi ? PdfRenderSettings::Mode::GDI_TEXT
                          : PdfRenderSettings::Mode::NORMAL);
  pdf_conversion_state_->Start(
      bytes, render_settings,
      base::BindOnce(&PrintJob::OnPdfConversionStarted, this));
}

void PrintJob::OnPdfConversionStarted(int page_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (page_count <= 0) {
    // Be sure to live long enough.
    scoped_refptr<PrintJob> handle(this);
    pdf_conversion_state_.reset();
    Cancel();
    return;
  }
  pdf_conversion_state_->set_page_count(page_count);
  pdf_conversion_state_->GetMorePages(
      base::BindRepeating(&PrintJob::OnPdfPageConverted, this));
}

void PrintJob::OnPdfPageConverted(int page_number,
                                  float scale_factor,
                                  std::unique_ptr<MetafilePlayer> metafile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(pdf_conversion_state_);
  if (!document_ || !metafile || page_number < 0 ||
      static_cast<size_t>(page_number) >= pdf_page_mapping_.size()) {
    // Be sure to live long enough.
    scoped_refptr<PrintJob> handle(this);
    pdf_conversion_state_.reset();
    Cancel();
    return;
  }

  // Add the page to the document if it is one of the pages requested by the
  // user. If it is not, ignore it.
  if (pdf_page_mapping_[page_number] != -1) {
    // Update the rendered document. It will send notifications to the listener.
    document_->SetPage(pdf_page_mapping_[page_number], std::move(metafile),
                       scale_factor, pdf_conversion_state_->page_size(),
                       pdf_conversion_state_->content_area());
  }

  pdf_conversion_state_->GetMorePages(
      base::BindRepeating(&PrintJob::OnPdfPageConverted, this));
}

void PrintJob::StartPdfToTextConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ =
      std::make_unique<PdfConversionState>(gfx::Size(), gfx::Rect());
  gfx::Rect page_area = gfx::Rect(0, 0, page_size.width(), page_size.height());
  const PrintSettings& settings = document()->settings();
  PdfRenderSettings render_settings(
      page_area, gfx::Point(0, 0), settings.dpi_size(),
      /*autorotate=*/true,
      /*use_color=*/true, PdfRenderSettings::Mode::TEXTONLY);
  pdf_conversion_state_->Start(
      bytes, render_settings,
      base::BindOnce(&PrintJob::OnPdfConversionStarted, this));
}

void PrintJob::StartPdfToPostScriptConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    bool ps_level2) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ = std::make_unique<PdfConversionState>(
      gfx::Size(), gfx::Rect());
  const PrintSettings& settings = document()->settings();
  PdfRenderSettings render_settings(
      content_area, physical_offsets, settings.dpi_size(),
      /*autorotate=*/true, settings.color() == COLOR,
      ps_level2 ? PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2
                : PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3);
  pdf_conversion_state_->Start(
      bytes, render_settings,
      base::BindOnce(&PrintJob::OnPdfConversionStarted, this));
}
#endif  // defined(OS_WIN)

void PrintJob::UpdatePrintedDocument(
    scoped_refptr<PrintedDocument> new_document) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(new_document);

  document_ = new_document;
  if (worker_)
    SyncPrintedDocumentToWorker();
}

void PrintJob::ClearPrintedDocument() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!document_)
    return;

  document_ = nullptr;
  if (worker_)
    SyncPrintedDocumentToWorker();
}

void PrintJob::SyncPrintedDocumentToWorker() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(worker_);
  DCHECK(!is_job_pending_);
  worker_->PostTask(
      FROM_HERE,
      base::BindOnce(&HoldRefCallback, base::WrapRefCounted(this),
                     base::BindOnce(&PrintJobWorker::OnDocumentChanged,
                                    base::Unretained(worker_.get()),
                                    base::RetainedRef(document_))));
}

void PrintJob::OnNotifyPrintJobEvent(const JobEventDetails& event_details) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  switch (event_details.type()) {
    case JobEventDetails::FAILED: {
      // No need to cancel since the worker already canceled itself.
      Stop();
      break;
    }
    case JobEventDetails::USER_INIT_DONE:
    case JobEventDetails::DEFAULT_INIT_DONE:
    case JobEventDetails::USER_INIT_CANCELED: {
      DCHECK_EQ(event_details.document(), document_.get());
      break;
    }
    case JobEventDetails::NEW_DOC:
    case JobEventDetails::JOB_DONE:
    case JobEventDetails::ALL_PAGES_REQUESTED: {
      // Don't care.
      break;
    }
    case JobEventDetails::DOC_DONE: {
      // This will call Stop() and broadcast a JOB_DONE message.
      base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                     base::BindOnce(&PrintJob::OnDocumentDone, this));
      break;
    }
#if defined(OS_WIN)
    case JobEventDetails::PAGE_DONE:
      if (pdf_conversion_state_) {
        pdf_conversion_state_->OnPageProcessed(
            base::BindRepeating(&PrintJob::OnPdfPageConverted, this));
      }
      break;
#endif  // defined(OS_WIN)
    default: {
      NOTREACHED();
      break;
    }
  }
}

void PrintJob::OnDocumentDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Be sure to live long enough. The instance could be destroyed by the
  // JOB_DONE broadcast.
  scoped_refptr<PrintJob> handle(this);

  // Stop the worker thread.
  Stop();

  auto details = base::MakeRefCounted<JobEventDetails>(
      JobEventDetails::JOB_DONE, 0, document_.get());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PRINT_JOB_EVENT,
      content::Source<PrintJob>(this),
      content::Details<JobEventDetails>(details.get()));
}

void PrintJob::ControlledWorkerShutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The deadlock this code works around is specific to window messaging on
  // Windows, so we aren't likely to need it on any other platforms.
#if defined(OS_WIN)
  // We could easily get into a deadlock case if worker_->Stop() is used; the
  // printer driver created a window as a child of the browser window. By
  // canceling the job, the printer driver initiated dialog box is destroyed,
  // which sends a blocking message to its parent window. If the browser window
  // thread is not processing messages, a deadlock occurs.
  //
  // This function ensures that the dialog box will be destroyed in a timely
  // manner by the mere fact that the thread will terminate. So the potential
  // deadlock is eliminated.
  worker_->StopSoon();

  // Delay shutdown until the worker terminates.  We want this code path
  // to wait on the thread to quit before continuing.
  if (worker_->IsRunning()) {
    base::PostDelayedTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&PrintJob::ControlledWorkerShutdown, this),
        base::TimeDelta::FromMilliseconds(100));
    return;
  }
#endif

  // Now make sure the thread object is cleaned up. Do this on a worker
  // thread because it may block.
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrintJobWorker::Stop, base::Unretained(worker_.get())),
      base::BindOnce(&PrintJob::HoldUntilStopIsCalled, this));

  is_job_pending_ = false;
  registrar_.RemoveAll();
  ClearPrintedDocument();
}

bool PrintJob::PostTask(const base::Location& from_here,
                        base::OnceClosure task) {
  return base::PostTask(from_here, {content::BrowserThread::UI},
                        std::move(task));
}

void PrintJob::HoldUntilStopIsCalled() {
}

void PrintJob::set_job_pending(bool pending) {
  is_job_pending_ = pending;
}

#if defined(OS_WIN)
JobEventDetails::JobEventDetails(Type type,
                                 int job_id,
                                 PrintedDocument* document,
                                 PrintedPage* page)
    : document_(document), page_(page), type_(type), job_id_(job_id) {}
#endif

JobEventDetails::JobEventDetails(Type type,
                                 int job_id,
                                 PrintedDocument* document)
    : document_(document), type_(type), job_id_(job_id) {}

JobEventDetails::~JobEventDetails() {
}

PrintedDocument* JobEventDetails::document() const { return document_.get(); }

#if defined(OS_WIN)
PrintedPage* JobEventDetails::page() const { return page_.get(); }
#endif
}  // namespace printing
