// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/services/printing/print_backend_service_impl.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/backend/test_print_backend.h"

namespace printing {

// `PrintBackendServiceTestImpl` uses a `TestPrintBackend` to enable testing
// of the `PrintBackendService` without relying upon the presence of real
// printer drivers.
class PrintBackendServiceTestImpl : public PrintBackendServiceImpl {
 public:
  explicit PrintBackendServiceTestImpl(
      mojo::PendingReceiver<mojom::PrintBackendService> receiver);
  PrintBackendServiceTestImpl(const PrintBackendServiceTestImpl&) = delete;
  PrintBackendServiceTestImpl& operator=(const PrintBackendServiceTestImpl&) =
      delete;
  ~PrintBackendServiceTestImpl() override;

  // Override which needs special handling for using `test_print_backend_`.
  void Init(const std::string& locale) override;

  // Overrides to support testing service termination scenarios.
  void EnumeratePrinters(
      mojom::PrintBackendService::EnumeratePrintersCallback callback) override;
  void GetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback)
      override;
  void GetPrinterSemanticCapsAndDefaults(
      const std::string& printer_name,
      mojom::PrintBackendService::GetPrinterSemanticCapsAndDefaultsCallback
          callback) override;
  void FetchCapabilities(
      const std::string& printer_name,
      mojom::PrintBackendService::FetchCapabilitiesCallback callback) override;
  void UpdatePrintSettings(
      base::Value::Dict job_settings,
      mojom::PrintBackendService::UpdatePrintSettingsCallback callback)
      override;

  // Cause the service to terminate on the next interaction it receives.  Once
  // terminated no further Mojo calls will be possible since there will not be
  // a receiver to handle them.
  void SetTerminateReceiverOnNextInteraction() { terminate_receiver_ = true; }

  // Launch the service in-process for testing using the provided backend.
  // `sandboxed` identifies if this service is potentially subject to
  // experiencing access-denied errors on some commands.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchForTesting(
      mojo::Remote<mojom::PrintBackendService>& remote,
      scoped_refptr<TestPrintBackend> backend,
      bool sandboxed);

 private:
  friend class PrintBackendBrowserTest;

  // Launch the service in-process for testing without initializing backend.
  static std::unique_ptr<PrintBackendServiceTestImpl> LaunchUninitialized(
      mojo::Remote<mojom::PrintBackendService>& remote);

  void OnDidGetDefaultPrinterName(
      mojom::PrintBackendService::GetDefaultPrinterNameCallback callback,
      mojom::DefaultPrinterNameResultPtr printer_name);

  void TerminateConnection();

  // When pretending to be sandboxed, have the possibility of getting access
  // denied errors.
  bool is_sandboxed_ = false;

  // Marker to signal service should terminate on next interaction.
  bool terminate_receiver_ = false;

  scoped_refptr<TestPrintBackend> test_print_backend_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_TEST_IMPL_H_
