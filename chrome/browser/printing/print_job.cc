// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_job.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_job_manager.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/browser/printing/printer_query.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printed_document.h"

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/printing/pdf_to_emf_converter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "printing/backend/win_helper.h"
#include "printing/page_number.h"
#include "printing/pdf_render_settings.h"
#include "printing/printed_page_win.h"
#include "printing/printing_features.h"
#include "url/gurl.h"
#endif


namespace printing {

namespace {

// Helper function to ensure `job` is valid until at least `callback` returns.
void HoldRefCallback(scoped_refptr<PrintJob> job, base::OnceClosure callback) {
  std::move(callback).Run();
}

#if BUILDFLAG(IS_WIN)
// Those must be kept in sync with the values defined in policy_templates.json.
enum class PrintPostScriptMode {
  // Do normal PostScript generation. Text is always rendered with Type 3 fonts.
  // Default value when policy not set.
  kDefault = 0,
  // Text is rendered with Type 42 fonts if possible.
  kType42 = 1,
  kMaxValue = kType42,
};

// Those must be kept in sync with the values defined in policy_templates.json.
enum class PrintRasterizationMode {
  // Do full page rasterization if necessary. Default value when policy not set.
  kFull = 0,
  // Avoid rasterization if possible.
  kFast = 1,
  kMaxValue = kFast,
};

bool PrintWithPostScriptType42Fonts(PrefService* prefs) {
  // Managed preference takes precedence over user preference and field trials.
  if (prefs && prefs->IsManagedPreference(prefs::kPrintPostScriptMode)) {
    int value = prefs->GetInteger(prefs::kPrintPostScriptMode);
    return value == static_cast<int>(PrintPostScriptMode::kType42);
  }

  return base::FeatureList::IsEnabled(
      features::kPrintWithPostScriptType42Fonts);
}

bool PrintWithReducedRasterization(PrefService* prefs) {
  // Managed preference takes precedence over user preference and field trials.
  if (prefs && prefs->IsManagedPreference(prefs::kPrintRasterizationMode)) {
    int value = prefs->GetInteger(prefs::kPrintRasterizationMode);
    return value == static_cast<int>(PrintRasterizationMode::kFast);
  }

  return base::FeatureList::IsEnabled(features::kPrintWithReducedRasterization);
}

PrefService* GetPrefsForWebContents(content::WebContents* web_contents) {
  // TODO(thestig): Figure out why crbug.com/1083911 occurred, which is likely
  // because `web_contents` was null. As a result, this section has many more
  // pointer checks to avoid crashing.
  content::BrowserContext* context =
      web_contents ? web_contents->GetBrowserContext() : nullptr;
  return context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
}

content::WebContents* GetWebContents(content::GlobalRenderFrameHostId rfh_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  return rfh ? content::WebContents::FromRenderFrameHost(rfh) : nullptr;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

PrintJob::PrintJob(PrintJobManager* print_job_manager)
    : print_job_manager_(print_job_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(print_job_manager_);
}

PrintJob::PrintJob() = default;

PrintJob::~PrintJob() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // The job should be finished (or at least canceled) when it is destroyed.
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!worker_ || !worker_->IsRunning());

  for (auto& observer : observers_) {
    observer.OnDestruction();
  }
}

void PrintJob::Initialize(std::unique_ptr<PrinterQuery> query,
                          const std::u16string& name,
                          uint32_t page_count) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!worker_);
  DCHECK(!is_job_pending_);
  DCHECK(!is_canceling_);
  DCHECK(!document_);
  worker_ = query->TransferContextToNewWorker(this);
  worker_->Start();
  rfh_id_ = query->rfh_id();
  std::unique_ptr<PrintSettings> settings = query->ExtractSettings();

#if BUILDFLAG(IS_WIN)
  pdf_page_mapping_ = PageNumber::GetPages(settings->ranges(), page_count);
  PrefService* prefs = GetPrefsForWebContents(GetWebContents(rfh_id_));
  if (prefs && prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
    use_skia_ = prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled);
  }
#endif

  auto new_doc = base::MakeRefCounted<PrintedDocument>(std::move(settings),
                                                       name, query->cookie());
  new_doc->set_page_count(page_count);
  UpdatePrintedDocument(new_doc);
}

#if BUILDFLAG(IS_WIN)
// static
std::vector<uint32_t> PrintJob::GetFullPageMapping(
    const std::vector<uint32_t>& pages,
    uint32_t total_page_count) {
  std::vector<uint32_t> mapping(total_page_count, kInvalidPageIndex);
  for (uint32_t page_index : pages) {
    // Make sure the page is in range.
    if (page_index < total_page_count) {
      mapping[page_index] = page_index;
    }
  }
  return mapping;
}

void PrintJob::StartConversionToNativeFormat(
    scoped_refptr<base::RefCountedMemory> print_data,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const gfx::Point& physical_offsets,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (PrintedDocument::HasDebugDumpPath())
    document()->DebugDumpData(print_data.get(), FILE_PATH_LITERAL(".pdf"));

  const PrintSettings& settings = document()->settings();
  if (settings.printer_language_is_textonly()) {
    StartPdfToTextConversion(print_data, page_size, url);
  } else if (settings.printer_language_is_ps2() ||
             settings.printer_language_is_ps3()) {
    StartPdfToPostScriptConversion(print_data, content_area, physical_offsets,
                                   settings.printer_language_is_ps2(), url);
  } else {
    StartPdfToEmfConversion(print_data, page_size, content_area, url);
  }

  // Indicate that the PDF is fully rendered and we no longer need the renderer
  // and web contents, so the print job does not need to be cancelled if they
  // die. This is needed on Windows because the `PrintedDocument` will not be
  // considered complete until PDF conversion finishes.
  document()->SetConvertingPdf();
}

void PrintJob::ResetPageMapping() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pdf_page_mapping_ =
      GetFullPageMapping(pdf_page_mapping_, document_->page_count());
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJob::StartPrinting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!worker_->IsRunning() || is_job_pending_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

#if BUILDFLAG(IS_WIN)
  // Do not collect duration metric if the print job will need to invoke a
  // "Save Print Output As" dialog that waits on a user to select a filename.
  const std::string printer_name =
      base::UTF16ToUTF8(document_->settings().device_name());
  const bool capture_printing_time =
      !DoesDriverDisplayFileDialogForPrinting(printer_name);
#else
  constexpr bool capture_printing_time = true;
#endif
  if (capture_printing_time) {
    printing_start_time_ = base::TimeTicks::Now();
  }

  // Real work is done in `PrintJobWorker::StartPrinting()`.
  worker_->PostTask(
      FROM_HERE, base::BindOnce(&HoldRefCallback, base::WrapRefCounted(this),
                                base::BindOnce(&PrintJobWorker::StartPrinting,
                                               base::Unretained(worker_.get()),
                                               base::RetainedRef(document_))));
  // Set the flag right now.
  is_job_pending_ = true;

  print_job_manager_->OnStarted(this);
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

  for (auto& observer : observers_) {
    observer.OnCanceling();
  }

  if (worker_ && worker_->IsRunning()) {
    // Call this right now so it renders the context invalid. Do not use
    // InvokeLater since it would take too much time.
    worker_->Cancel();
  }
  OnFailed();
  is_canceling_ = false;
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
void PrintJob::CleanupAfterContentAnalysisDenial() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  worker_->CleanupAfterContentAnalysisDenial();
}
#endif

bool PrintJob::FlushJob(base::TimeDelta timeout) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure the object outlive this message loop.
  scoped_refptr<PrintJob> handle(this);

  base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = loop.QuitClosure();
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), timeout);

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

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
class PrintJob::PdfConversionState {
 public:
  PdfConversionState(const gfx::Size& page_size,
                     const gfx::Rect& content_area,
                     const std::optional<bool>& use_skia,
                     const GURL& url)
      : page_size_(page_size),
        content_area_(content_area),
        use_skia_(use_skia),
        url_(url) {}

  void Start(scoped_refptr<base::RefCountedMemory> data,
             const PdfRenderSettings& conversion_settings,
             PdfConverter::StartCallback start_callback) {
    converter_ = PdfConverter::StartPdfConverter(
        data, conversion_settings, use_skia_, url_, std::move(start_callback));
  }

  void GetMorePages(PdfConverter::GetPageCallback get_page_callback) {
    const int kMaxNumberOfTempFilesPerDocument = 3;
    while (pages_in_progress_ < kMaxNumberOfTempFilesPerDocument &&
           current_page_index_ < page_count_) {
      ++pages_in_progress_;
      converter_->GetPage(current_page_index_++, get_page_callback);
    }
  }

  void OnPageProcessed(PdfConverter::GetPageCallback get_page_callback) {
    --pages_in_progress_;
    GetMorePages(get_page_callback);
    // Release converter if we don't need this any more.
    if (!pages_in_progress_ && current_page_index_ >= page_count_)
      converter_.reset();
  }

  void set_page_count(uint32_t page_count) { page_count_ = page_count; }
  const gfx::Size& page_size() const { return page_size_; }
  const gfx::Rect& content_area() const { return content_area_; }

 private:
  uint32_t page_count_ = 0;
  uint32_t current_page_index_ = 0;
  int pages_in_progress_ = 0;
  const gfx::Size page_size_;
  const gfx::Rect content_area_;
  const std::optional<bool> use_skia_;
  const GURL url_;
  std::unique_ptr<PdfConverter> converter_;
};

void PrintJob::StartPdfToEmfConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size,
    const gfx::Rect& content_area,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ = std::make_unique<PdfConversionState>(
      page_size, content_area, use_skia_, url);

  const PrintSettings& settings = document()->settings();

  PrefService* prefs = GetPrefsForWebContents(GetWebContents(rfh_id_));
  bool print_with_reduced_rasterization = PrintWithReducedRasterization(prefs);

  using RenderMode = PdfRenderSettings::Mode;
  RenderMode mode = print_with_reduced_rasterization
                        ? RenderMode::EMF_WITH_REDUCED_RASTERIZATION
                        : RenderMode::NORMAL;

  PdfRenderSettings render_settings(
      content_area, gfx::Point(0, 0), settings.dpi_size(),
      /*autorotate=*/true, settings.color() == mojom::ColorModel::kColor, mode);
  pdf_conversion_state_->Start(
      bytes, render_settings,
      base::BindOnce(&PrintJob::OnPdfConversionStarted, this));
}

void PrintJob::OnPdfConversionStarted(uint32_t page_count) {
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

void PrintJob::OnPdfPageConverted(uint32_t page_index,
                                  float scale_factor,
                                  std::unique_ptr<MetafilePlayer> metafile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(pdf_conversion_state_);
  if (!document_ || !metafile || page_index == kInvalidPageIndex ||
      page_index >= pdf_page_mapping_.size()) {
    // Be sure to live long enough.
    scoped_refptr<PrintJob> handle(this);
    pdf_conversion_state_.reset();
    Cancel();
    return;
  }

  // Add the page to the document if it is one of the pages requested by the
  // user. If it is not, ignore it.
  if (pdf_page_mapping_[page_index] != kInvalidPageIndex) {
    // Update the rendered document. It will send notifications to the listener.
    document_->SetPage(pdf_page_mapping_[page_index], std::move(metafile),
                       scale_factor, pdf_conversion_state_->page_size(),
                       pdf_conversion_state_->content_area());
  }

  pdf_conversion_state_->GetMorePages(
      base::BindRepeating(&PrintJob::OnPdfPageConverted, this));
}

void PrintJob::StartPdfToTextConversion(
    scoped_refptr<base::RefCountedMemory> bytes,
    const gfx::Size& page_size,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ = std::make_unique<PdfConversionState>(
      gfx::Size(), gfx::Rect(), use_skia_, url);
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
    bool ps_level2,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!pdf_conversion_state_);
  pdf_conversion_state_ = std::make_unique<PdfConversionState>(
      gfx::Size(), gfx::Rect(), use_skia_, url);
  const PrintSettings& settings = document()->settings();

  PdfRenderSettings::Mode mode;
  if (ps_level2) {
    mode = PdfRenderSettings::Mode::POSTSCRIPT_LEVEL2;
  } else {
    PrefService* prefs = GetPrefsForWebContents(GetWebContents(rfh_id_));
    mode = PrintWithPostScriptType42Fonts(prefs)
               ? PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3_WITH_TYPE42_FONTS
               : PdfRenderSettings::Mode::POSTSCRIPT_LEVEL3;
  }
  PdfRenderSettings render_settings(
      content_area, physical_offsets, settings.dpi_size(),
      /*autorotate=*/true, settings.color() == mojom::ColorModel::kColor, mode);
  pdf_conversion_state_->Start(
      bytes, render_settings,
      base::BindOnce(&PrintJob::OnPdfConversionStarted, this));
}
#endif  // BUILDFLAG(IS_WIN)

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

#if BUILDFLAG(IS_WIN)
void PrintJob::OnPageDone(PrintedPage* page) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (pdf_conversion_state_) {
    pdf_conversion_state_->OnPageProcessed(
        base::BindRepeating(&PrintJob::OnPdfPageConverted, this));
  }
  document_->RemovePage(page);
}
#endif  // BUILDFLAG(IS_WIN)

void PrintJob::OnFailed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Stop();

  print_job_manager_->OnFailed(this);

  for (auto& observer : observers_) {
    observer.OnFailed();
  }
}

void PrintJob::OnDocDone(int job_id, PrintedDocument* document) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (printing_start_time_.has_value()) {
    base::UmaHistogramMediumTimes(
        "Printing.PrintDuration.LocalPrinter.Success",
        base::TimeTicks::Now() - printing_start_time_.value());
    printing_start_time_.reset();
  }

  print_job_manager_->OnDocDone(this, document, job_id);

  for (auto& observer : observers_) {
    observer.OnDocDone(job_id, document);
  }

  // This will call `Stop()` and broadcast a `JOB_DONE` message.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PrintJob::OnDocumentDone, this));
}

void PrintJob::OnDocumentDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Be sure to live long enough. The instance could be destroyed by the
  // `JOB_DONE` broadcast.
  scoped_refptr<PrintJob> handle(this);

  // Stop the worker thread.
  Stop();

  print_job_manager_->OnJobDone(this);

  for (auto& observer : observers_) {
    observer.OnJobDone();
  }
}

void PrintJob::ControlledWorkerShutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The deadlock this code works around is specific to window messaging on
  // Windows, so we aren't likely to need it on any other platforms.
#if BUILDFLAG(IS_WIN)
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
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE, base::BindOnce(&PrintJob::ControlledWorkerShutdown, this),
        base::Milliseconds(100));
    return;
  }
#endif

  // Now make sure the thread object is cleaned up. Do this on a worker
  // thread because it may block.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrintJobWorker::Stop, base::Unretained(worker_.get())),
      base::BindOnce(&PrintJob::HoldUntilStopIsCalled, this));

  is_job_pending_ = false;
  ClearPrintedDocument();
}

bool PrintJob::PostTask(const base::Location& from_here,
                        base::OnceClosure task) {
  return content::GetUIThreadTaskRunner({})->PostTask(from_here,
                                                      std::move(task));
}

void PrintJob::HoldUntilStopIsCalled() {
}

void PrintJob::set_job_pending_for_testing(bool pending) {
  is_job_pending_ = pending;
}

void PrintJob::AddObserver(Observer& observer) {
  observers_.AddObserver(&observer);
}

void PrintJob::RemoveObserver(Observer& observer) {
  observers_.RemoveObserver(&observer);
}

}  // namespace printing
