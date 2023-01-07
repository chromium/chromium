// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_test_impl.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

#if BUILDFLAG(IS_WIN)
struct RenderPrintedPageData {
  RenderPrintedPageData(
      int32_t document_cookie,
      uint32_t page_index,
      mojom::MetafileDataType page_data_type,
      base::ReadOnlySharedMemoryRegion serialized_page,
      const gfx::Size& page_size,
      const gfx::Rect& page_content_rect,
      float shrink_factor,
      mojom::PrintBackendService::RenderPrintedPageCallback callback)
      : document_cookie(document_cookie),
        page_index(page_index),
        page_data_type(page_data_type),
        serialized_page(std::move(serialized_page)),
        page_size(page_size),
        page_content_rect(page_content_rect),
        shrink_factor(shrink_factor),
        callback(std::move(callback)) {}
  RenderPrintedPageData(const RenderPrintedPageData&) = delete;
  RenderPrintedPageData& operator=(const RenderPrintedPageData&) = delete;
  ~RenderPrintedPageData() = default;

  int32_t document_cookie;
  uint32_t page_index;
  mojom::MetafileDataType page_data_type;
  base::ReadOnlySharedMemoryRegion serialized_page;
  gfx::Size page_size;
  gfx::Rect page_content_rect;
  float shrink_factor;
  mojom::PrintBackendService::RenderPrintedPageCallback callback;
};
#endif

PrintBackendServiceTestImpl::PrintBackendServiceTestImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver,
    scoped_refptr<TestPrintBackend> backend)
    : PrintBackendServiceImpl(std::move(receiver)),
      test_print_backend_(std::move(backend)) {}

PrintBackendServiceTestImpl::~PrintBackendServiceTestImpl() = default;

void PrintBackendServiceTestImpl::Init(const std::string& locale) {
  DCHECK(test_print_backend_);
  print_backend_ = test_print_backend_;
  PrintBackendServiceImpl::InitCommon(locale);
}

void PrintBackendServiceTestImpl::EnumeratePrinters(
    mojom::PrintBackendService::EnumeratePrintersCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::EnumeratePrinters(std::move(callback));
}

void PrintBackendServiceTestImpl::GetDefaultPrinterName(
    mojom::PrintBackendService::GetDefaultPrinterNameCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }
  PrintBackendServiceImpl::GetDefaultPrinterName(std::move(callback));
}

void PrintBackendServiceTestImpl::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
        callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::GetPrinterSemanticCapsAndDefaults(
      printer_name, std::move(callback));
}

void PrintBackendServiceTestImpl::FetchCapabilities(
    const std::string& printer_name,
    mojom::PrintBackendService::FetchCapabilitiesCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::FetchCapabilities(printer_name, std::move(callback));
}

void PrintBackendServiceTestImpl::UpdatePrintSettings(
    base::Value::Dict job_settings,
    mojom::PrintBackendService::UpdatePrintSettingsCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  PrintBackendServiceImpl::UpdatePrintSettings(std::move(job_settings),
                                               std::move(callback));
}

#if BUILDFLAG(IS_WIN)
void PrintBackendServiceTestImpl::RenderPrintedPage(
    int32_t document_cookie,
    uint32_t page_index,
    mojom::MetafileDataType page_data_type,
    base::ReadOnlySharedMemoryRegion serialized_page,
    const gfx::Size& page_size,
    const gfx::Rect& page_content_rect,
    float shrink_factor,
    mojom::PrintBackendService::RenderPrintedPageCallback callback) {
  if (terminate_receiver_) {
    TerminateConnection();
    return;
  }

  // Page index is zero-based whereas page number is one-based.
  uint32_t page_number = page_index + 1;
  if (page_number < rendering_delayed_until_page_number_) {
    DVLOG(2) << "Adding page " << page_number << " to delayed rendering queue";
    delayed_rendering_pages_.push(std::make_unique<RenderPrintedPageData>(
        document_cookie, page_index, page_data_type, std::move(serialized_page),
        page_size, page_content_rect, shrink_factor, std::move(callback)));
    return;
  }

  // Any previously delayed pages should now be rendered, before carrying on
  // with the page for this call.
  while (!delayed_rendering_pages_.empty()) {
    RenderPrintedPageData* page_data = delayed_rendering_pages_.front().get();
    DVLOG(2) << "Rendering deferred page " << (page_data->page_index + 1);
    PrintBackendServiceImpl::RenderPrintedPage(
        page_data->document_cookie, page_data->page_index,
        page_data->page_data_type, std::move(page_data->serialized_page),
        page_data->page_size, page_data->page_content_rect,
        page_data->shrink_factor, std::move(page_data->callback));
    delayed_rendering_pages_.pop();
  }

  DVLOG(2) << "Rendering page " << page_number;
  PrintBackendServiceImpl::RenderPrintedPage(
      document_cookie, page_index, page_data_type, std::move(serialized_page),
      page_size, page_content_rect, shrink_factor, std::move(callback));
}
#endif  // BUILDFLAG(IS_WIN)

void PrintBackendServiceTestImpl::TerminateConnection() {
  DLOG(ERROR) << "Terminating print backend service test connection";
  receiver_.reset();
}

// static
std::unique_ptr<PrintBackendServiceTestImpl>
PrintBackendServiceTestImpl::LaunchForTesting(
    mojo::Remote<mojom::PrintBackendService>& remote,
    scoped_refptr<TestPrintBackend> backend,
    bool sandboxed) {
  mojo::PendingReceiver<mojom::PrintBackendService> receiver =
      remote.BindNewPipeAndPassReceiver();

  // Private ctor.
  auto service = base::WrapUnique(
      new PrintBackendServiceTestImpl(std::move(receiver), std::move(backend)));
  service->Init(/*locale=*/std::string());

  // Register this test version of print backend service to be used instead of
  // launching instances out-of-process on-demand.
  if (sandboxed) {
    PrintBackendServiceManager::GetInstance().SetServiceForTesting(&remote);
  } else {
    PrintBackendServiceManager::GetInstance().SetServiceForFallbackTesting(
        &remote);
  }

  return service;
}

}  // namespace printing
