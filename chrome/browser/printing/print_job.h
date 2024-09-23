// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "content/public/browser/global_routing_id.h"
#include "printing/print_settings.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <string>

#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
class GURL;
#endif

namespace base {
class Location;
class RefCountedMemory;
}

namespace printing {

class MetafilePlayer;
class PrintJobManager;
class PrintJobWorker;
class PrintedDocument;
#if BUILDFLAG(IS_WIN)
class PrintedPage;
#endif
class PrinterQuery;
class PrintSettings;

// Manages the print work for a specific document. Talks to the printer through
// PrintingContext through PrintJobWorker. Hides access to PrintingContext in a
// worker thread so the caller never blocks. PrintJob will send notifications on
// any state change. While printing, the PrintJobManager instance keeps a
// reference to the job to be sure it is kept alive. All the code in this class
// runs in the UI thread. All virtual functions are virtual only so that
// TestPrintJob can override them in tests.
class PrintJob : public base::RefCountedThreadSafe<PrintJob> {
 public:
  // An observer interface implemented by classes which are interested
  // in `PrintJob` events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDocDone(int job_id, PrintedDocument* document) {}
    virtual void OnJobDone() {}
    virtual void OnCanceling() {}
    virtual void OnFailed() {}
    virtual void OnDestruction() {}
  };

#if BUILDFLAG(IS_CHROMEOS)
  // An enumeration of components where print jobs can come from. The order of
  // these enums must match that of
  // chrome/browser/ash/printing/history/print_job_info.proto.
  using Source = crosapi::mojom::PrintJob::Source;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Create a empty PrintJob. When initializing with this constructor,
  // post-constructor initialization must be done with Initialize().
  // If PrintJob is created on Chrome OS, call SetSource() to set which
  // component initiated this print job.
  // `print_job_manager` must outlive this object.
  explicit PrintJob(PrintJobManager* print_job_manager);

  PrintJob(const PrintJob&) = delete;
  PrintJob& operator=(const PrintJob&) = delete;

  // Grabs the ownership of the PrintJobWorker from a PrinterQuery along with
  // the print settings. Sets the expected page count of the print job based on
  // the settings.
  virtual void Initialize(std::unique_ptr<PrinterQuery> query,
                          const std::u16string& name,
                          uint32_t page_count);

#if BUILDFLAG(IS_WIN)
  void StartConversionToNativeFormat(
      scoped_refptr<base::RefCountedMemory> print_data,
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets,
      const GURL& url);

  // Overwrites the PDF page mapping to fill in values of -1 for all indices
  // that are not selected. This is needed when the user opens the system
  // dialog from the link in Print Preview on Windows and then sets a selection
  // of pages, because all PDF pages will be converted, but only the user's
  // selected pages should be sent to the printer. See https://crbug.com/823876.
  void ResetPageMapping();

  // Called when `page` is done printing.
  void OnPageDone(PrintedPage* page);
#endif

  // Called when the document is done printing.
  virtual void OnDocDone(int job_id, PrintedDocument* document);

  // Called if the document fails to print.
  virtual void OnFailed();

  // Starts the actual printing. Signals the worker that it should begin to
  // spool as soon as data is available.
  virtual void StartPrinting();

  // Asks for the worker thread to finish its queued tasks and disconnects the
  // delegate object. The PrintJobManager will remove its reference.
  // WARNING: This may have the side-effect of destroying the object if the
  // caller doesn't have a handle to the object. Use PrintJob::is_stopped() to
  // check whether the worker thread has actually stopped.
  virtual void Stop();

  // Cancels printing job and stops the worker thread. Takes effect immediately.
  // The caller must have a reference to the PrintJob before calling Cancel(),
  // since Cancel() calls Stop(). See WARNING above for Stop().
  virtual void Cancel();

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  // Cleanup a printing job after content analysis denies printing.  Performs
  // any extra cleanup for this particular case that can't be safely done from
  // within Cancel().
  void CleanupAfterContentAnalysisDenial();
#endif

  // Synchronously wait for the job to finish. It is mainly useful when the
  // process is about to be shut down and we're waiting for the spooler to eat
  // our data.
  virtual bool FlushJob(base::TimeDelta timeout);

  // Returns true if the print job is pending, i.e. between a StartPrinting()
  // and the end of the spooling.
  bool is_job_pending() const;

  // Access the current printed document. Warning: may be NULL.
  PrintedDocument* document() const;

  // Access stored settings.
  const PrintSettings& settings() const;

#if BUILDFLAG(IS_CHROMEOS)
  // Sets the component which initiated the print job.
  void SetSource(Source source, const std::string& source_id);

  // Returns the source of print job.
  Source source() const;

  // Returns the ID of the source.
  const std::string& source_id() const;

  const base::ObserverList<Observer>& GetObserversForTesting() {
    return observers_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

  // Adds and removes observers for `PrintJob` events. The order in
  // which notifications are sent to observers is undefined. Observers must be
  // sure to remove the observer before they go away.
  void AddObserver(Observer& observer);
  void RemoveObserver(Observer& observer);

 protected:
  // Refcounted class.
  friend class base::RefCountedThreadSafe<PrintJob>;

  virtual ~PrintJob();

  // Constructs a PrintJob with a null PrintJobManager instance. Used only in
  // testing contexts.
  PrintJob();

  void set_job_pending_for_testing(bool pending);

  // Updates `document_` to a new instance. Protected so that tests can access
  // it.
  void UpdatePrintedDocument(scoped_refptr<PrintedDocument> new_document);

#if BUILDFLAG(IS_WIN)
  // Virtual to support testing.
  virtual void OnPdfPageConverted(uint32_t page_index,
                                  float scale_factor,
                                  std::unique_ptr<MetafilePlayer> metafile);
#endif

 private:
#if BUILDFLAG(IS_WIN)
  FRIEND_TEST_ALL_PREFIXES(PrintJobTest, PageRangeMapping);
#endif

  // Clears reference to `document_`.
  void ClearPrintedDocument();

  // Helper method for UpdatePrintedDocument() and ClearPrintedDocument() to
  // sync `document_` updates with `worker_`.
  void SyncPrintedDocumentToWorker();

  // Releases the worker thread by calling Stop(), then broadcasts a JOB_DONE
  // notification.
  void OnDocumentDone();

  // Terminates the worker thread in a very controlled way, to work around any
  // eventual deadlock.
  void ControlledWorkerShutdown();

  void HoldUntilStopIsCalled();

#if BUILDFLAG(IS_WIN)
  virtual void StartPdfToEmfConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const GURL& url);

  virtual void StartPdfToPostScriptConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets,
      bool ps_level2,
      const GURL& url);

  virtual void StartPdfToTextConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Size& page_size,
      const GURL& url);

  void OnPdfConversionStarted(uint32_t page_count);

  // Helper method to do the work for ResetPageMapping(). Split for unit tests.
  static std::vector<uint32_t> GetFullPageMapping(
      const std::vector<uint32_t>& pages,
      uint32_t total_page_count);
#endif  // BUILDFLAG(IS_WIN)

  base::ObserverList<Observer> observers_;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  std::unique_ptr<PrintJobWorker> worker_;

  content::GlobalRenderFrameHostId rfh_id_;

  // The global PrintJobManager. May be null in testing contexts
  // only. Otherwise guaranteed to outlive this object.
  raw_ptr<PrintJobManager> print_job_manager_ = nullptr;

  // The printed document.
  scoped_refptr<PrintedDocument> document_;

  // Time at start of printing.  Used for metrics.
  std::optional<base::TimeTicks> printing_start_time_;

  // Is the worker thread printing.
  bool is_job_pending_ = false;

  // Is Canceling? If so, try to not cause recursion if on FAILED notification,
  // the notified calls Cancel() again.
  bool is_canceling_ = false;

#if BUILDFLAG(IS_WIN)
  class PdfConversionState;
  std::unique_ptr<PdfConversionState> pdf_conversion_state_;
  std::vector<uint32_t> pdf_page_mapping_;
  std::optional<bool> use_skia_;
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
  // The component which initiated the print job.
  Source source_;

  // ID of the source.
  // This should be blank if the source is kPrintPreview or kArc.
  std::string source_id_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Holds the quit closure while running a nested RunLoop to flush tasks.
  base::OnceClosure quit_closure_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_H_
