// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace printing {

class PrintBackendServiceManager {
 public:
  PrintBackendServiceManager(const PrintBackendServiceManager&) = delete;
  PrintBackendServiceManager& operator=(const PrintBackendServiceManager&) =
      delete;

  // Returns true if the print backend service should be sandboxed, false
  // otherwise.
  bool ShouldSandboxPrintBackendService() const;

  // Acquires a remote handle to the Print Backend Service instance, launching a
  // process to host the service if necessary.
  const mojo::Remote<printing::mojom::PrintBackendService>& GetService(
      const std::string& locale,
      const std::string& printer_name);

  // Query if printer driver has been found to require elevated privilege in
  // order to have print queries/commands succeed.
  bool PrinterDriverRequiresElevatedPrivilege(
      const std::string& printer_name) const;

  // Make note that `printer_name` has been detected as requiring elevated
  // privileges in order to operate.
  void SetPrinterDriverRequiresElevatedPrivilege(
      const std::string& printer_name);

  // Overrides the print backend service for testing.  Caller retains ownership
  // of `remote`.
  void SetServiceForTesting(
      mojo::Remote<printing::mojom::PrintBackendService>* remote);

  // Overrides the print backend service for testing when an alternate service
  // is required for fallback processing after an access denied error.  Caller
  // retains ownership of `remote`.
  void SetServiceForFallbackTesting(
      mojo::Remote<printing::mojom::PrintBackendService>* remote);

  // There is to be at most one instance of this at a time.
  static PrintBackendServiceManager& GetInstance();

  // Test support to revert to a fresh instance.
  static void ResetForTesting();

 private:
  friend base::NoDestructor<PrintBackendServiceManager>;

  PrintBackendServiceManager();
  ~PrintBackendServiceManager();

  // Callback when predetermined idle timeout occurs indicating no in-flight
  // messages for a short period of time.  `sandboxed` is used to distinguish
  // which mapping of remotes the timeout applies to.
  void OnIdleTimeout(bool sandboxed, const std::string& remote_id);

  // Callback when service has disconnected (e.g., process crashes).
  // `sandboxed` is used to distinguish which mapping of remotes the
  // disconnection applies to.
  void OnRemoteDisconnected(bool sandboxed, const std::string& remote_id);

  using RemotesMap =
      base::flat_map<std::string,
                     mojo::Remote<printing::mojom::PrintBackendService>>;

  // Keep separate mapping of remotes for sandboxed vs. unsandboxed services.
  RemotesMap sandboxed_remotes_;
  RemotesMap unsandboxed_remotes_;

  // Track if next service started should be sandboxed.
  bool is_sandboxed_service_ = true;

  // Set of printer drivers which require elevated permissions to operate.
  // It is expected that most print drivers will succeed with the preconfigured
  // sandbox permissions.  Should any drivers be discovered to require more than
  // that (and thus fail with access denied errors) then we need to fallback to
  // performing the operation with modified restrictions.
  base::flat_set<std::string> drivers_requiring_elevated_privilege_;

  // Override of service to use for testing.
  mojo::Remote<printing::mojom::PrintBackendService>*
      sandboxed_service_remote_for_test_ = nullptr;
  mojo::Remote<printing::mojom::PrintBackendService>*
      unsandboxed_service_remote_for_test_ = nullptr;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
