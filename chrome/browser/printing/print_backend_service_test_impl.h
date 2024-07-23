// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/test_print_backend.h"

#if BUILDFLAG(IS_WIN)
#include "base/containers/queue.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/services/printing/public/mojom/printer_xml_parser.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "printing/mojom/print.mojom.h"
#endif

#if BUILDFLAG(IS_WIN)
namespace gfx {
class Rect;
class Size;
}  // namespace gfx
#endif

namespace printing {

#if BUILDFLAG(IS_WIN)
struct RenderPrintedPageData;
#endif

// `PrintBackendServiceTestImpl` uses a `TestPrintBackend` to enable testing
// of the `PrintBackendService` without relying upon the presence of real
// printer drivers.
class PrintBackendServiceTestImpl : public PrintBackendServiceImpl {
 public:
  // Launch the service in-process for testing using the provided backend.
  // `sandboxed` identifies if this service is potentially subject to
  // experiencing access-denied errors on some commands.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchForTesting(
      mojo::Remote<mojom::PrintBackendService>& remote,
      scoped_refptr<TestPrintBackend> backend,
      bool sandboxed);

#if BUILDFLAG(IS_WIN)
  // Launch the service in-process for testing using the provided backend.
  // `sandboxed` identifies if this service is potentially subject to
  // experiencing access-denied errors on some commands. Launches the service on
  // the thread associated with `service_task_runner`.
  static std::unique_ptr<PrintBackendServiceTestImpl>
  LaunchForTestingWithServiceThread(
      mojo::Remote<mojom::PrintBackendService>& remote,
      scoped_refptr<TestPrintBackend> backend,
      bool sandboxed,
      mojo::PendingRemote<mojom::PrinterXmlParser> xml_parser_remote,
      scoped_refptr<base::SingleThreadTaskRunner> service_task_runner);
#endif  // BUILDFLAG(IS_WIN)

  PrintBackendServiceTestImpl(const PrintBackendServiceTestImpl&) = delete;
  PrintBackendServiceTestImpl& operator=(const PrintBackendServiceTestImpl&) =
      delete;
  ~PrintBackendServiceTestImpl() override;

  // Override which needs special handling for using `test_print_backend_`.
  void Init(
#if BUILDFLAG(IS_WIN)
      const std::string& locale,
      mojo::PendingRemote<mojom::PrinterXmlParser> remote
#else
      const std::string& locale
#endif  // BUILDFLAG(IS_WIN)
      ) override;

  // Overrides to support testing service termination scenarios.
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
#endif
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
  void UpdatePrintSettings(
      uint32_t context_id,
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;
#if BUILDFLAG(IS_WIN)
  void RenderPrintedPage(
      int32_t document_cookie,
      uint32_t page_index,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page,
      const gfx::Size& page_size,
      const gfx::Rect& page_content_rect,
      float shrink_factor,
      mojom::PrintBackendService::RenderPrintedPageCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)

  // Tests which will have a leftover printing context established in the
  // service can use this to skip the destructor check that all contexts were
  // cleaned up.
  void SkipPersistentContextsCheckOnShutdown() {
    skip_dtor_persistent_contexts_check_ = true;
  }

  // Cause the service to terminate on the next interaction it receives.  Once
  // terminated no further Mojo calls will be possible since there will not be
  // a receiver to handle them.
  void SetTerminateReceiverOnNextInteraction() { terminate_receiver_ = true; }

#if BUILDFLAG(IS_WIN)
  // Set the page number for which rendering should be delayed until.  Pages
  // are held in queue until this page number is seen, after which the pages
  // are released in sequence for rendering.
  void set_rendering_delayed_until_page(uint32_t page_number) {
    rendering_delayed_until_page_number_ = page_number;
  }
#endif

 private:
  // Use LaunchForTesting() or LaunchForTestingWithServiceThread().
  PrintBackendServiceTestImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver,
      bool is_sandboxed,
      scoped_refptr<TestPrintBackend> backend);

  void OnDidGetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback,
      mojom::DefaultPrinterNameResultPtr printer_name);

  void TerminateConnection();

#if BUILDFLAG(IS_WIN)
  // Launches and returns a test Print Backend service run on a service thread.
  // This runs on the service thread.
  static std::unique_ptr<PrintBackendServiceTestImpl>
  CreateServiceOnServiceThread(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver,
      bool is_sandboxed,
      scoped_refptr<TestPrintBackend> backend,
      mojo::PendingRemote<mojom::PrinterXmlParser> xml_parser_remote);
#endif  // BUILDFLAG(IS_WIN)

  // When pretending to be sandboxed, have the possibility of getting access
  // denied errors.
  const bool is_sandboxed_;

  // Marker for skipping check for empty persistent contexts at destruction.
  bool skip_dtor_persistent_contexts_check_ = false;

  // Marker to signal service should terminate on next interaction.
  bool terminate_receiver_ = false;

#if BUILDFLAG(IS_WIN)
  // Marker to signal that rendering should be delayed until the page with this
  // index is reached.  This provides a mechanism for the print pipeline to get
  // multiple pages queued up.
  uint32_t rendering_delayed_until_page_number_ = 0;

  // The queue of pages whose rendering processing is being delayed.
  base::queue<std::unique_ptr<RenderPrintedPageData>> delayed_rendering_pages_;
#endif

  scoped_refptr<TestPrintBackend> test_print_backend_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
