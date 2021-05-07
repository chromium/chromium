// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace printing {

class PrintBackendServiceManager {
 public:
  PrintBackendServiceManager(const PrintBackendServiceManager&) = delete;
  PrintBackendServiceManager& operator=(const PrintBackendServiceManager&) =
      delete;

  // Acquires a remote handle to the Print Backend Service instance, launching a
  // process to host the service if necessary.
  const mojo::Remote<printing::mojom::PrintBackendService>& GetService(
      const std::string& locale,
      const std::string& printer_name);

  // Overrides the print backend service for testing.  Caller retains ownership
  // of `remote`.
  void SetServiceForTesting(
      mojo::Remote<printing::mojom::PrintBackendService>* remote);

  // There is to be at most one instance of this at a time.
  static PrintBackendServiceManager& GetInstance();

 private:
  friend base::NoDestructor<PrintBackendServiceManager>;

  PrintBackendServiceManager();
  ~PrintBackendServiceManager();

  using RemotesMap =
      base::flat_map<std::string,
                     mojo::Remote<printing::mojom::PrintBackendService>>;

  RemotesMap remotes_;

  // Override of service to use for testing.
  mojo::Remote<printing::mojom::PrintBackendService>* service_remote_for_test_ =
      nullptr;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_BACKEND_SERVICE_MANAGER_H_
