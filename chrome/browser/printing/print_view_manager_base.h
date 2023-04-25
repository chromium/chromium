// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "components/prefs/pref_member.h"
#include "components/printing/browser/print_manager.h"
#include "components/printing/browser/print_to_pdf/pdf_print_job.h"
#include "components/printing/common/print.mojom-forward.h"
#include "components/services/print_compositor/public/mojom/print_compositor.mojom.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "chrome/browser/printing/print_backend_service_manager.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

#if BUILDFLAG(ENABLE_TAGGED_PDF)
#include "ui/accessibility/ax_tree_update_forward.h"
#endif

namespace base {
class RefCountedMemory;
}

namespace printing {

class PrintQueriesQueue;
class PrinterQuery;

// Base class for managing the print commands for a WebContents.
class PrintViewManagerBase : public PrintManager, public PrintJob::Observer {
 public:
  // An observer interface implemented by classes which are interested
  // in `PrintViewManagerBase` events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPrintNow(const content::RenderFrameHost* rfh) {}

    virtual void OnScriptedPrint() {}

    // This method is never called unless `ENABLE_PRINT_PREVIEW`.
    virtual void OnPrintPreview(const content::RenderFrameHost* rfh) {}

    // This method is never called unless `ENABLE_OOP_PRINTING`.
    virtual void OnRegisterSystemPrintClient(bool succeeded) {}

    virtual void OnDidPrintDocument() {}
  };

  PrintViewManagerBase(const PrintViewManagerBase&) = delete;
  PrintViewManagerBase& operator=(const PrintViewManagerBase&) = delete;

  ~PrintViewManagerBase() override;

  // Prints the current document immediately. Since the rendering is
  // asynchronous, the actual printing will not be completed on the return of
  // this function. Returns false if printing is impossible at the moment.
  virtual bool PrintNow(content::RenderFrameHost* rfh);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Prints the document in `print_data` with settings specified in
  // `job_settings`. Runs `callback` with an error string on failure and with an
  // empty string if the print job is started successfully. `rfh` is the render
  // frame host for the preview initiator contents respectively.
  void PrintForPrintPreview(base::Value::Dict job_settings,
                            scoped_refptr<base::RefCountedMemory> print_data,
                            content::RenderFrameHost* rfh,
                            PrinterHandler::PrintCallback callback);
#endif

  // Prints the current document pages specified by `page_ranges` with
  // parameters, specified by `print_pages_params` into a PDF document,
  // returned with `callback`. If `page_ranges` is empty, the entire
  // document is printed.
  void PrintToPdf(content::RenderFrameHost* rfh,
                  const std::string& page_ranges,
                  mojom::PrintPagesParamsPtr print_pages_params,
                  print_to_pdf::PdfPrintJob::PrintToPdfCallback callback);

// Notifies the print view manager that the system dialog has been cancelled
// after being opened from Print Preview.
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void SystemDialogCancelled();
#endif

  std::u16string RenderSourceName();

  content::RenderFrameHost* GetPrintingRFHForTesting() const {
    return printing_rfh_;
  }

  // mojom::PrintManagerHost:
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override;
  void DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override;
#if BUILDFLAG(ENABLE_TAGGED_PDF)
  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override;
#endif
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void UpdatePrintSettings(base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override;
#endif
  void IsPrintingEnabled(IsPrintingEnabledCallback callback) override;
  void ScriptedPrint(mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override;
  void PrintingFailed(int32_t cookie,
                      mojom::PrintFailureReason reason) override;

  // Adds and removes observers for `PrintViewManagerBase` events. The order in
  // which notifications are sent to observers is undefined. Observers must be
  // sure to remove the observer before they go away.
  void AddObserver(Observer& observer);
  void RemoveObserver(Observer& observer);

 protected:
  explicit PrintViewManagerBase(content::WebContents* web_contents);

  // Helper method for checking whether the WebContents is crashed.
  bool IsCrashed();

  void SetPrintingRFH(content::RenderFrameHost* rfh);

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Register with the `PrintBackendServiceManager` as a client for queries
  // which will require a UI (the system print dialog).  Some platforms have
  // limitations on having multiple clients of this type; this function returns
  // `false` if such a registration fails because of this restriction.  In
  // that case no further attempts to make the queries should be made.
  bool RegisterSystemPrintClient();

  // Unregister with the `PrintBackendServiceManager` if a client for queries
  // which require a UI.  This function can be called even if there is no
  // current registration.
  void UnregisterSystemPrintClient();
#endif

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  // Creates a new empty print job. It has no settings loaded. If there is
  // currently a print job, safely disconnect from it. Returns false if it is
  // impossible to safely disconnect from the current print job or it is
  // impossible to create a new print job.
  virtual bool CreateNewPrintJob(std::unique_ptr<PrinterQuery> query);

  // Makes sure the current print_job_ has all its data before continuing, and
  // disconnect from it.
  // WARNING: `this` may not be alive after DisconnectFromCurrentPrintJob()
  // returns.
  void DisconnectFromCurrentPrintJob();

  // PrintJob::Observer overrides:
  void OnDocDone(int job_id, PrintedDocument* document) override;
  void OnJobDone() override;
  void OnCanceling() override;
  void OnFailed() override;

  base::ObserverList<Observer>& GetObservers() { return observers_; }

  // Prints the document by posting on the IO thread. This should only be called
  // by `ScriptedPrint()` and `CompleteScriptedPrintAfterContentAnalysis()`.
  // This method is virtual for testing purposes.
  virtual void CompleteScriptedPrint(content::RenderFrameHost* rfh,
                                     mojom::ScriptedPrintParamsPtr params,
                                     ScriptedPrintCallback callback);

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  // Helper method for scanning a page by sending requests and launching the
  // scanning dialog as required. This helper is shared between system print
  // scans and print preview scans. This method is virtual for testing purposes.
  virtual void OnGotSnapshotCallback(
      base::OnceCallback<void(bool should_proceed)> callback,
      enterprise_connectors::ContentAnalysisDelegate::Data data,
      content::GlobalRenderFrameHostId rfh_id,
      mojom::DidPrintDocumentParamsPtr params);

  // Helper method called after the snapshotted page has been composited into a
  // scannable PDF document. This method is virtual for testing purposes.
  virtual void OnCompositedForContentAnalysis(
      base::OnceCallback<void(bool should_proceed)> callback,
      enterprise_connectors::ContentAnalysisDelegate::Data data,
      content::GlobalRenderFrameHostId rfh_id,
      mojom::PrintCompositor::Status status,
      base::ReadOnlySharedMemoryRegion page_region);

  // Helper method to set `snapshotting_for_content_analysis_` in child classes.
  void set_snapshotting_for_content_analysis();
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

  // Manages the low-level talk to the printer.
  scoped_refptr<PrintJob> print_job_;

 private:
  friend class TestPrintViewManager;

  // content::WebContentsObserver implementation.
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState /*old_state*/,
      content::RenderFrameHost::LifecycleState new_state) override;

  // Cancels the print job.
  void NavigationStopped() override;

  // Implementation without callbacks.
  bool OnComposePdfDoneImpl(int document_cookie,
                            const gfx::Size& page_size,
                            const gfx::Rect& content_area,
                            const gfx::Point& physical_offsets,
                            mojom::PrintCompositor::Status status,
                            base::ReadOnlySharedMemoryRegion region);

  // IPC message handlers for service.
  void OnComposePdfDone(int document_cookie,
                        const gfx::Size& page_size,
                        const gfx::Rect& content_area,
                        const gfx::Point& physical_offsets,
                        DidPrintDocumentCallback callback,
                        mojom::PrintCompositor::Status status,
                        base::ReadOnlySharedMemoryRegion region);

  // Helper for mojom::PrintManagerHost handling.
  void OnDidPrintDocument(PrintManager::DidPrintDocumentCallback callback,
                          bool succeeded);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // Helpers for UpdatePrintSettings().
#if BUILDFLAG(IS_WIN)
  void OnDidUpdatePrintableArea(std::unique_ptr<PrinterQuery> printer_query,
                                base::Value::Dict job_settings,
                                std::unique_ptr<PrintSettings> print_settings,
                                UpdatePrintSettingsCallback callback,
                                bool success);
#endif
  void CompleteUpdatePrintSettings(
      base::Value::Dict job_settings,
      std::unique_ptr<PrintSettings> print_settings,
      UpdatePrintSettingsCallback callback);

  // Helpers for PrintForPrintPreview();
  void OnPrintSettingsDone(scoped_refptr<base::RefCountedMemory> print_data,
                           uint32_t page_count,
                           PrinterHandler::PrintCallback callback,
                           std::unique_ptr<PrinterQuery> printer_query);

  void StartLocalPrintJob(scoped_refptr<base::RefCountedMemory> print_data,
                          uint32_t page_count,
                          int cookie,
                          PrinterHandler::PrintCallback callback);

  // Runs `callback` with `params` to reply to UpdatePrintSettings().
  void UpdatePrintSettingsReply(
      mojom::PrintManagerHost::UpdatePrintSettingsCallback callback,
      mojom::PrintPagesParamsPtr params,
      bool canceled);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  // Runs `callback` with `params` to reply to GetDefaultPrintSettings().
  void GetDefaultPrintSettingsReply(GetDefaultPrintSettingsCallback callback,
                                    mojom::PrintParamsPtr params);

  // Runs `callback` with `params` to reply to ScriptedPrint().
  void ScriptedPrintReply(ScriptedPrintCallback callback,
                          int process_id,
                          mojom::PrintPagesParamsPtr params);

  // Requests the RenderView to render all the missing pages for the print job.
  // No-op if no print job is pending. Returns true if at least one page has
  // been requested to the renderer.
  // WARNING: `this` may not be alive after RenderAllMissingPagesNow() returns.
  bool RenderAllMissingPagesNow();

  // Checks that synchronization is correct with `print_job_` based on `cookie`.
  bool PrintJobHasDocument(int cookie);

  // Starts printing the `document` in `print_job_` with the given `print_data`.
  // This method assumes PrintJobHasDocument() has been called, and `print_data`
  // contains valid data.
  void PrintDocument(scoped_refptr<base::RefCountedMemory> print_data,
                     const gfx::Size& page_size,
                     const gfx::Rect& content_area,
                     const gfx::Point& offsets);

  // Quits the current message loop if these conditions hold true: a document is
  // loaded and is complete and waiting_for_pages_to_be_rendered_ is true. This
  // function is called in DidPrintDocument(). The inner message loop was
  // created by RenderAllMissingPagesNow().
  void ShouldQuitFromInnerMessageLoop();

  // Terminates the print job. No-op if no print job has been created. If
  // `cancel` is true, cancel it instead of waiting for the job to finish. Will
  // call ReleasePrintJob().
  void TerminatePrintJob(bool cancel);

  // Releases print_job_. Correctly deregisters from notifications. No-op if
  // no print job has been created.
  void ReleasePrintJob();

  // Runs an inner message loop. It will set inside_inner_message_loop_ to true
  // while the blocking inner message loop is running. This is useful in cases
  // where the RenderView is about to be destroyed while a printing job isn't
  // finished.
  // WARNING: `this` may not be alive after RunInnerMessageLoop() returns.
  bool RunInnerMessageLoop();

  // In the case of Scripted Printing, where the renderer is controlling the
  // control flow, print_job_ is initialized whenever possible. No-op is
  // print_job_ is initialized.
  bool OpportunisticallyCreatePrintJob(int cookie);

  // Release the PrinterQuery associated with our `cookie_`.
  void ReleasePrinterQuery();

  // Prints the document by calling the `PrintRequestedPages()` renderer API and
  // notifies observers. This should only be called by `PrintNow()` or
  // `CompletePrintNowAfterContentAnalysis()`.
  void CompletePrintNow(content::RenderFrameHost* rfh);

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  // Helper for content analysis code that calls `CompletePrintNow()` if
  // `allowed` is true and printing is still possible.
  void CompletePrintNowAfterContentAnalysis(bool allowed);

  // Helper for content analysis code that calls `CompleteScriptedPrint()` if
  // `allowed` is true and printing is still possible.
  void CompleteScriptedPrintAfterContentAnalysis(
      mojom::ScriptedPrintParamsPtr params,
      ScriptedPrintCallback callback,
      bool allowed);
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

  // The current RFH that is printing with a system printing dialog.
  raw_ptr<content::RenderFrameHost> printing_rfh_ = nullptr;

  // Indication of success of the print job.
  bool printing_succeeded_ = false;

  // Indication that the job is getting canceled.
  bool canceling_job_ = false;

  // Set while running an inner message loop inside RenderAllMissingPagesNow().
  // This means we are _blocking_ until all the necessary pages have been
  // rendered or the print settings are being loaded.
  base::OnceClosure quit_inner_loop_;

  // Whether printing is enabled.
  BooleanPrefMember printing_enabled_;

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  // Client ID with the print backend service manager for system print dialog.
  absl::optional<PrintBackendServiceManager::ClientId> query_with_ui_client_id_;
#endif

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
  // Indicates that a snapshot of the page/document is currently being made.
  bool snapshotting_for_content_analysis_ = false;
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

  const scoped_refptr<PrintQueriesQueue> queue_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PrintViewManagerBase> weak_ptr_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_BASE_H_
