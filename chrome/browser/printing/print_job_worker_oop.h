// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "OOP printing must be enabled"
#endif

namespace printing {

class PrintedDocument;

// Worker thread code. It manages the PrintingContext and offloads all system
// driver interactions to the Print Backend service.  This is the object that
// generates most NOTIFY_PRINT_JOB_EVENT notifications, but they are generated
// through a NotificationTask task to be executed from the right thread, the UI
// thread.  PrintJob always outlives its worker instance.
class PrintJobWorkerOop : public PrintJobWorker {
 public:
  explicit PrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id);
  PrintJobWorkerOop(const PrintJobWorkerOop&) = delete;
  PrintJobWorkerOop& operator=(const PrintJobWorkerOop&) = delete;
  ~PrintJobWorkerOop() override;

  // `PrintJobWorker` overrides.
  void StartPrinting(PrintedDocument* new_document) override;

 protected:
  // For testing.
  PrintJobWorkerOop(content::GlobalRenderFrameHostId rfh_id,
                    bool simulate_spooling_memory_errors);

  // Local callback wrappers for Print Backend Service mojom call.  Virtual to
  // support testing.
  virtual void OnDidUseDefaultSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#if BUILDFLAG(IS_WIN)
  virtual void OnDidAskUserForSettings(
      SettingsCallback callback,
      mojom::PrintSettingsResultPtr print_settings);
#endif
  virtual void OnDidStartPrinting(mojom::ResultCode result);
#if BUILDFLAG(IS_WIN)
  virtual void OnDidRenderPrintedPage(uint32_t page_index,
                                      mojom::ResultCode result);
#endif
  virtual void OnDidRenderPrintedDocument(mojom::ResultCode result);
  virtual void OnDidDocumentDone(int job_id, mojom::ResultCode result);
  virtual void OnDidCancel(scoped_refptr<PrintJob> job);

  // `PrintJobWorker` overrides.
#if BUILDFLAG(IS_WIN)
  bool SpoolPage(PrintedPage* page) override;
#endif
  bool SpoolDocument() override;
  void OnDocumentDone() override;
  void UseDefaultSettings(SettingsCallback callback) override;
  void GetSettingsWithUI(uint32_t document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback) override;
  void SetSettings(base::Value::Dict new_settings,
                   SettingsCallback callback) override;
  void OnFailure() override;

 private:
  // Support to unregister this worker as a printing client.  Applicable any
  // time a print job finishes, is canceled, or needs to be restarted.
  void UnregisterServiceManagerClient();

  // Helper function for restarting a print job after error.
  bool TryRestartPrinting();

  // Initiate failure handling, including notification to the user.
  void NotifyFailure(mojom::ResultCode result);

  // Local callback wrapper for Print Backend Service mojom call.
  void OnDidUpdatePrintSettings(const std::string& device_name,
                                SettingsCallback callback,
                                mojom::PrintSettingsResultPtr print_settings);

  // Mojo support to send messages from UI thread.
  void SendUseDefaultSettings(SettingsCallback callback);
#if BUILDFLAG(IS_WIN)
  void SendAskUserForSettings(uint32_t document_page_count,
                              bool has_selection,
                              bool is_scripted,
                              SettingsCallback callback);
#endif
  void SendStartPrinting(const std::string& device_name,
                         const std::u16string& document_name);
#if BUILDFLAG(IS_WIN)
  void SendRenderPrintedPage(
      const PrintedPage* page,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page_data);
#endif  // BUILDFLAG(IS_WIN)
  void SendRenderPrintedDocument(
      mojom::MetafileDataType data_type,
      base::ReadOnlySharedMemoryRegion serialized_data);
  void SendDocumentDone();
  void SendCancel(scoped_refptr<PrintJob> job);

  // Used to test spooling memory error handling.
  bool simulate_spooling_memory_errors_ = false;

  // Client ID with the print backend service manager for this print job.
  // Used only from UI thread.
  absl::optional<uint32_t> service_manager_client_id_;

  // The device name used when printing via a service.  Used only from the UI
  // thread.
  std::string device_name_;

  // The processed name of the document being printed.  Used only from the UI
  // thread.
  std::u16string document_name_;

  // The printed document. Only has read-only access.  This reference separate
  // from the one already in the base class provides a guarantee that the
  // `PrintedDocument` will persist until OOP processing completes, even if
  // the `PrintJob` should drop its reference as part of failure/cancel
  // processing.  Named differently than base (even though both are private)
  // to avoid any potential confusion between them.
  scoped_refptr<PrintedDocument> document_oop_;

  // The type of target to print to.  Used only from the UI thread.
  mojom::PrintTargetType print_target_type_ =
      mojom::PrintTargetType::kDirectToDevice;

#if BUILDFLAG(IS_WIN)
  // Number of pages that have completed printing.
  uint32_t pages_printed_count_ = 0;
#endif

  // Tracks if a restart for printing has already been attempted.
  bool print_retried_ = false;

  // Tracks if the service has already been requested to cancel printing the
  // document
  bool print_cancel_requested_ = false;

  // Weak pointers have flags that get bound to the thread where they are
  // checked, so it is necessary to use different factories when getting a
  // weak pointer to send to the worker task runner vs. to the UI thread.
  base::WeakPtrFactory<PrintJobWorkerOop> worker_weak_factory_{this};
  base::WeakPtrFactory<PrintJobWorkerOop> ui_weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
