// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_number.h"
#include "printing/printing_context.h"

namespace content {
class WebContents;
struct GlobalRenderFrameHostId;
}

namespace printing {

class PrintJob;
class PrintedDocument;
class PrintedPage;

// Worker thread code. It manages the PrintingContext, which can be blocking
// and/or run a message loop. This object calls back into the PrintJob in order
// to update the print job status. The callbacks all happen on the UI thread.
// PrintJob always outlives its worker instance.
class PrintJobWorker {
 public:
  using SettingsCallback =
      base::OnceCallback<void(std::unique_ptr<PrintSettings>,
                              mojom::ResultCode)>;

  explicit PrintJobWorker(content::GlobalRenderFrameHostId rfh_id);

  PrintJobWorker(const PrintJobWorker&) = delete;
  PrintJobWorker& operator=(const PrintJobWorker&) = delete;

  virtual ~PrintJobWorker();

  void SetPrintJob(PrintJob* print_job);

  /* The following functions may only be called before calling SetPrintJob(). */

  // Initializes the print settings for PDF. Must be called on the UI thread.
  std::unique_ptr<PrintSettings> GetPdfSettings();

  // Initializes the default print settings. Must be called on the UI thread.
  void GetDefaultSettings(SettingsCallback callback);

  // Initializes the print settings. Must be called on the UI thread. A Print...
  // dialog box will be shown to ask the user their preference. `is_scripted`
  // should be true for calls coming straight from window.print().
  void GetSettingsFromUser(uint32_t document_page_count,
                           bool has_selection,
                           mojom::MarginType margin_type,
                           bool is_scripted,
                           SettingsCallback callback);

  // Set the new print settings from a dictionary value. Must be called on the
  // UI thread.
  virtual void SetSettings(base::Value::Dict new_settings,
                           SettingsCallback callback);

#if BUILDFLAG(IS_CHROMEOS)
  // Set the new print settings from a POD type. Must be called on the UI
  // thread.
  void SetSettingsFromPOD(std::unique_ptr<printing::PrintSettings> new_settings,
                          SettingsCallback callback);
#endif

  /* The following functions may only be called after calling SetPrintJob(). */

  // Starts the printing loop. Every pages are printed as soon as the data is
  // available. Makes sure the new_document is the right one.
  virtual void StartPrinting(PrintedDocument* new_document);

  // Updates the printed document.
  void OnDocumentChanged(PrintedDocument* new_document);

  // Dequeues waiting pages. Called when PrintJob receives a
  // NOTIFY_PRINTED_DOCUMENT_UPDATED notification. It's time to look again if
  // the next page can be printed.
  void OnNewPage();

  /* The following functions may be called before or after SetPrintJob(). */

  // Cancels the job.
  void Cancel();

  // Returns true if the thread has been started, and not yet stopped.
  bool IsRunning() const;

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

  // Signals the thread to exit in the near future.
  void StopSoon();

  // Signals the thread to exit and returns once the thread has exited.
  void Stop();

  // Starts the thread.
  bool Start();

  // Returns the WebContents this work corresponds to.
  content::WebContents* GetWebContents();

 protected:
  // Sanity check that it is okay to proceed with starting a print job.
  bool StartPrintingSanityCheck(const PrintedDocument* new_document) const;

  // Get the document name to be used when initiating printing.
  std::u16string GetDocumentName(const PrintedDocument* new_document) const;

#if BUILDFLAG(IS_WIN)
  // Renders a page in the printer.  Returns false if any errors occur.
  // This is applicable when using the Windows GDI print API.
  virtual bool SpoolPage(PrintedPage* page);
#endif

  // Renders the document to the printer.  Returns false if any errors occur.
  virtual bool SpoolDocument();

  // Internal state verification that spooling of the document is complete.
  void CheckDocumentSpoolingComplete();

  // Closes the job since spooling is done.
  virtual void OnDocumentDone();

  // Helper function for document done processing.
  void FinishDocumentDone(int job_id);

  // Reports settings back to `callback`.
  void GetSettingsDone(SettingsCallback callback, mojom::ResultCode result);

  // Discards the current document, the current page and cancels the printing
  // context.
  virtual void OnFailure();

  // Asks the user for print settings. Must be called on the UI thread.
  // Required on Mac and Linux. Windows can display UI from non-main threads,
  // but sticks with this for consistency.
  virtual void GetSettingsWithUI(uint32_t document_page_count,
                                 bool has_selection,
                                 bool is_scripted,
                                 SettingsCallback callback);

  // Use the default settings. When using GTK+ or Mac, this can still end up
  // displaying a dialog. So this needs to happen from the UI thread on these
  // systems.
  virtual void UseDefaultSettings(SettingsCallback callback);

  PrintingContext* printing_context() { return printing_context_.get(); }
  PrintJob* print_job() { return print_job_; }
  const PageNumber& page_number() { return page_number_; }
  base::SequencedTaskRunner* task_runner() { return task_runner_.get(); }

 private:
  // Posts a task to call OnNewPage(). Used to wait for pages/document to be
  // available.
  void PostWaitForPage();

#if BUILDFLAG(IS_WIN)
  // Windows print GDI-specific handling for OnNewPage().
  bool OnNewPageHelperGdi();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
  // Called on the UI thread to update the print settings.
  void UpdatePrintSettingsFromPOD(
      std::unique_ptr<printing::PrintSettings> new_settings,
      SettingsCallback callback);
#endif

  // Printing context delegate.
  const std::unique_ptr<PrintingContext::Delegate> printing_context_delegate_;

  // Information about the printer setting.
  const std::unique_ptr<PrintingContext> printing_context_;

  // The printed document. Only has read-only access.
  scoped_refptr<PrintedDocument> document_;

  // The print job owning this worker thread. It is guaranteed to outlive this
  // object and should be set with SetPrintJob().
  raw_ptr<PrintJob> print_job_ = nullptr;

  // Current page number to print.
  PageNumber page_number_;

  // Thread to run worker tasks.
  base::Thread thread_;

  // Thread-safe pointer to task runner of the `thread_`.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to generate a WeakPtr for callbacks.
  base::WeakPtrFactory<PrintJobWorker> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_
