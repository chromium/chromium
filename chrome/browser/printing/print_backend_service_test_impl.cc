// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_backend_service_test_impl.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/printing/print_backend_service_manager.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

PrintBackendServiceTestImpl::PrintBackendServiceTestImpl(
    mojo::PendingReceiver<mojom::PrintBackendService> receiver)
    : PrintBackendServiceImpl(std::move(receiver)) {}

PrintBackendServiceTestImpl::~PrintBackendServiceTestImpl() = default;

void PrintBackendServiceTestImpl::Init(const std::string& locale) {
  DCHECK(test_print_backend_);
  print_backend_ = test_print_backend_;
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

// static
std::unique_ptr<PrintBackendServiceTestImpl>
PrintBackendServiceTestImpl::LaunchUninitialized(
    mojo::Remote<mojom::PrintBackendService>& remote) {
  // Launch the service running locally in-process.
  mojo::PendingReceiver<mojom::PrintBackendService> receiver =
      remote.BindNewPipeAndPassReceiver();
  return std::make_unique<PrintBackendServiceTestImpl>(std::move(receiver));
}

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
  std::unique_ptr<PrintBackendServiceTestImpl> service =
      LaunchUninitialized(remote);

  // Do the common initialization using the testing print backend.
  service->test_print_backend_ = backend;
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
